#pragma once
#include <vector>
#include <functional>
#include <string> 

namespace pv {

struct FtpFileInfo {
    std::string name;
    bool isDirectory;
    size_t size; 
};

class FtpClient {
public:
    FtpClient(const std::string& ip);
    ~FtpClient();

    // pinned port is 2121
    bool connect();
    void disconnect();

    // asynchronous or synchronous (executed in threads)
    std::vector<FtpFileInfo> listDirectory(const std::string& path);
    bool downloadFile(const std::string& remotePath, const std::string& localPath, 
                      std::function<void(double)> progressCallback = nullptr);

private:
    std::string m_ip;
    const int m_port = 2121;
    void* m_curlHandle;
};

} 