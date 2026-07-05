#include "FtpClient.hpp"
#include "../Logger/Logger.hpp"
#include <curl/curl.h>
#include <sstream>
#include <algorithm>
#include <fstream>

namespace pv {

static size_t localWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

static size_t fileWriteCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
    std::ofstream* out = static_cast<std::ofstream*>(stream);
    size_t totalSize = size * nmemb;
    if (out->is_open()) {
        out->write(static_cast<const char*>(ptr), totalSize);
        return totalSize;
    }
    return 0;
}

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

FtpClient::FtpClient(const std::string& ipAddress)
    : m_ip(ipAddress), m_curlHandle(nullptr) {
    LOG_INFO("FtpClient initialized for IP: " + m_ip);
}

FtpClient::~FtpClient() {
    disconnect();
}

bool FtpClient::connect() {
    if (m_curlHandle) return true;

    m_curlHandle = curl_easy_init();
    if (!m_curlHandle) {
        LOG_ERROR("Failed to initialize libcurl easy handle.");
        return false;
    }
    return true;
}

void FtpClient::disconnect() {
    if (m_curlHandle) {
        curl_easy_cleanup(m_curlHandle);
        m_curlHandle = nullptr;
        LOG_INFO("FtpClient disconnected and resources cleaned up.");
    }
}

std::vector<pv::FtpFileInfo> FtpClient::listDirectory(const std::string& remotePath) {
    std::vector<pv::FtpFileInfo> fileList;
    if (!m_curlHandle) return fileList;

    curl_easy_reset(m_curlHandle);

    LOG_INFO("Fetching real PS4 directory listing for: " + remotePath);

    std::string url = "ftp://" + m_ip + ":2121" + remotePath;
    if (url.back() != '/') {
        url += "/";
    }

    std::string responseString;
    
    curl_easy_setopt(m_curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curlHandle, CURLOPT_TIMEOUT, 15L); 
    curl_easy_setopt(m_curlHandle, CURLOPT_CONNECTTIMEOUT, 7L);
    
    curl_easy_setopt(m_curlHandle, CURLOPT_FTP_USE_EPSV, 0L);  // disable Extended Passive
    curl_easy_setopt(m_curlHandle, CURLOPT_FTP_USE_EPRT, 0L);  // disable Extended Active
    curl_easy_setopt(m_curlHandle, CURLOPT_FORBID_REUSE, 1L);   // close socket after every command

    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEFUNCTION, localWriteCallback);
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEDATA, &responseString);
    curl_easy_setopt(m_curlHandle, CURLOPT_DIRLISTONLY, 0L); 
    curl_easy_setopt(m_curlHandle, CURLOPT_FTP_FILEMETHOD, CURLFTPMETHOD_MULTICWD);

    CURLcode res = curl_easy_perform(m_curlHandle);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to list PS4 directory: " + std::string(curl_easy_strerror(res)));
        curl_easy_reset(m_curlHandle);
        return fileList;
    }

    std::stringstream ss(responseString);
    std::string line;
    while (std::getline(ss, line)) {
        line = trim(line);
        if (line.empty()) continue;

        pv::FtpFileInfo item;
        item.isDirectory = false;

        if (line[0] == 'd') {
            item.isDirectory = true;
        }

        std::stringstream lineTokens(line);
        std::string token;
        int tokenCount = 0;
        while (tokenCount < 8 && lineTokens >> token) {
            tokenCount++;
        }

        std::string entryName;
        std::getline(lineTokens, entryName);
        entryName = trim(entryName);

        if (entryName.empty()) {
            size_t lastSpace = line.find_last_of(" \t");
            if (lastSpace != std::string::npos) {
                entryName = line.substr(lastSpace + 1);
            } else {
                entryName = line;
            }
        }

        item.name = trim(entryName);

        size_t lastSlash = item.name.find_last_of('/');
        if (lastSlash != std::string::npos) {
            item.name = item.name.substr(lastSlash + 1);
        }

        if (!item.name.empty() && item.name != "." && item.name != "..") {
            fileList.push_back(item);
        }
    }

    return fileList;
}

bool FtpClient::downloadFile(const std::string& remotePath, const std::string& localPath, std::function<void(double)> progressCallback) {
    if (!m_curlHandle) return false;

    curl_easy_reset(m_curlHandle);

    LOG_INFO("Starting download from: " + remotePath + " to " + localPath);

    std::string url = "ftp://" + m_ip + ":2121" + remotePath;
    std::ofstream localFile(localPath, std::ios::binary);

    if (!localFile.is_open()) {
        LOG_ERROR("Failed to open local file for writing: " + localPath);
        return false;
    }

    curl_easy_setopt(m_curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curlHandle, CURLOPT_TIMEOUT, 60L); 
    curl_easy_setopt(m_curlHandle, CURLOPT_CONNECTTIMEOUT, 7L);
    
    curl_easy_setopt(m_curlHandle, CURLOPT_FTP_USE_EPSV, 0L);
    curl_easy_setopt(m_curlHandle, CURLOPT_FTP_USE_EPRT, 0L);
    curl_easy_setopt(m_curlHandle, CURLOPT_FORBID_REUSE, 1L);

    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEFUNCTION, fileWriteCallback);
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEDATA, &localFile);
    curl_easy_setopt(m_curlHandle, CURLOPT_FTP_FILEMETHOD, CURLFTPMETHOD_MULTICWD);

    if (progressCallback) {
        progressCallback(50.0);
    }

    CURLcode res = curl_easy_perform(m_curlHandle);
    localFile.close();

    if (res != CURLE_OK) {
        LOG_ERROR("Download failed: " + std::string(curl_easy_strerror(res)));
        curl_easy_reset(m_curlHandle);
        return false;
    }

    if (progressCallback) {
        progressCallback(100.0);
    }

    return true;
}

}
