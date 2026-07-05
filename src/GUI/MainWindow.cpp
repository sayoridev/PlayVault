#include "MainWindow.hpp"
#include "../Logger/Logger.hpp"
#include <glibmm/fileutils.h>
#include <glibmm/main.h> 
#include <filesystem>
#include <algorithm>
#include <thread>
#include <mutex>
#include <sstream>
#include <curl/curl.h>
#include <gtkmm/stylecontext.h>
#include <gtkmm/alertdialog.h>
#include <gtkmm/settings.h>
#include <gtkmm/gestureclick.h> 

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

namespace pv {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

MainWindow::MainWindow()
    : m_mainBox(Gtk::Orientation::VERTICAL),
      m_topBox(Gtk::Orientation::HORIZONTAL, 12),
      m_contentBox(Gtk::Orientation::HORIZONTAL), 
      m_ipLabel("PS4 IP Address:"),
      m_connectButton("Connect"),
      m_statusLabel("Ready") {

    set_title("PlayVault - PS4 Media Sync");
    set_default_size(1100, 600);

    auto settings = Gtk::Settings::get_default();
    if (settings) {
        settings->property_gtk_application_prefer_dark_theme().set_value(true);
    }

    m_cssProvider = Gtk::CssProvider::create();
    try {
        m_cssProvider->load_from_path("resources/style.css");
        Gtk::StyleContext::add_provider_for_display(
            Gdk::Display::get_default(), m_cssProvider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        LOG_INFO("CSS applyed.");
    } catch (const std::exception& e) {
        LOG_ERROR("Impossible to load style.css: " + std::string(e.what()));
    }

    m_topBox.set_valign(Gtk::Align::START); 
    m_topBox.set_hexpand(true);
    m_topBox.set_vexpand(false);
    
    m_ipLabel.set_valign(Gtk::Align::CENTER);
    m_ipEntry.set_text("192.168.X.XXX");
    m_ipEntry.set_width_chars(12);
    m_ipEntry.set_valign(Gtk::Align::CENTER); 
    m_connectButton.set_valign(Gtk::Align::CENTER); 

    m_topBox.append(m_ipLabel);
    m_topBox.append(m_ipEntry);
    m_topBox.append(m_connectButton);

    m_spinner.set_valign(Gtk::Align::CENTER);
    m_spinner.set_margin_start(10);
    m_topBox.append(m_spinner);

    auto* spacer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    spacer->set_expand(true);
    m_topBox.append(*spacer);

    auto* zoomLabel = Gtk::make_managed<Gtk::Label>("Zoom:");
    zoomLabel->set_valign(Gtk::Align::CENTER);
    m_topBox.append(*zoomLabel);

    m_zoomScale.set_range(80, 300);
    m_zoomScale.set_value(160);     
    m_zoomScale.set_draw_value(false);
    m_zoomScale.set_size_request(100, -1); 
    m_zoomScale.set_valign(Gtk::Align::CENTER);
    m_zoomScale.set_margin_end(15);
    m_topBox.append(m_zoomScale);

    m_zoomScale.signal_value_changed().connect([this]() {
        m_currentThumbnailSize = static_cast<int>(m_zoomScale.get_value());
        if (!m_lastTitleId.empty()) {
            loadMediaGrid(m_lastTitleId);
        }
    });

    auto* filterLabel = Gtk::make_managed<Gtk::Label>("Filter:");
    filterLabel->set_valign(Gtk::Align::CENTER);
    m_topBox.append(*filterLabel);

    auto filterModel = Gtk::StringList::create({
        "All Media (Most Recent)",
        "All Media (Oldest)",
        "Photos Only",
        "Videos Only",
        "By File Size (Largest)"
    });
    m_filterDropDown.set_model(filterModel);
    m_filterDropDown.set_valign(Gtk::Align::CENTER); 
    m_filterDropDown.property_selected().signal_changed().connect([this]() {
        m_currentFilterSortMode = m_filterDropDown.get_selected();
        if (!m_lastTitleId.empty()) loadMediaGrid(m_lastTitleId);
    });
    m_topBox.append(m_filterDropDown);

    m_topBox.set_margin_start(15);
    m_topBox.set_margin_end(15);
    m_topBox.set_margin_top(10);
    m_topBox.set_margin_bottom(10);

    m_connectButton.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_connect_clicked));

    m_contentBox.set_valign(Gtk::Align::FILL);
    m_contentBox.set_halign(Gtk::Align::FILL);
    m_contentBox.set_hexpand(true);
    m_contentBox.set_vexpand(true);

    auto* paneContainer = Gtk::make_managed<Gtk::Paned>(Gtk::Orientation::HORIZONTAL);
    paneContainer->set_hexpand(true);
    paneContainer->set_vexpand(true);
    paneContainer->set_position(250); 

    m_sidebarBox.set_orientation(Gtk::Orientation::VERTICAL);
    m_sidebarBox.set_spacing(8);
    m_sidebarBox.set_valign(Gtk::Align::FILL); 
    m_sidebarBox.set_halign(Gtk::Align::FILL);
    m_sidebarBox.set_vexpand(true); 

    m_sidebarBox.set_margin_start(15);
    m_sidebarBox.set_margin_end(5);
    m_sidebarBox.set_margin_top(5);
    m_sidebarBox.set_margin_bottom(5);

    auto* sidebarTitle = Gtk::make_managed<Gtk::Label>();
    sidebarTitle->set_markup("<b>Games</b>");
    sidebarTitle->set_halign(Gtk::Align::START);
    m_sidebarBox.append(*sidebarTitle);

    m_gameList.set_hexpand(true);
    m_gameList.set_vexpand(true);
    m_gameList.set_valign(Gtk::Align::FILL);

    auto* sidebarScroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    sidebarScroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    sidebarScroll->set_hexpand(true);
    sidebarScroll->set_vexpand(true);
    sidebarScroll->set_valign(Gtk::Align::FILL);
    sidebarScroll->set_child(m_gameList);
    m_sidebarBox.append(*sidebarScroll);

    m_mediaGrid.set_row_spacing(6);         
    m_mediaGrid.set_column_spacing(6);      
    m_mediaGrid.set_valign(Gtk::Align::START); 
    m_mediaGrid.set_halign(Gtk::Align::FILL);
    m_mediaGrid.set_hexpand(true);
    m_mediaGrid.set_vexpand(true); 

    m_mediaGridScroll.set_child(m_mediaGrid);
    m_mediaGridScroll.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_mediaGridScroll.set_valign(Gtk::Align::FILL);
    m_mediaGridScroll.set_halign(Gtk::Align::FILL);
    m_mediaGridScroll.set_hexpand(true); 
    m_mediaGridScroll.set_vexpand(true); 
    m_mediaGridScroll.set_margin_end(15);
    m_mediaGridScroll.set_margin_top(5);
    m_mediaGridScroll.set_margin_bottom(5);

    paneContainer->set_start_child(m_sidebarBox);
    paneContainer->set_end_child(m_mediaGridScroll);

    m_contentBox.append(*paneContainer);

    m_statusLabel.set_halign(Gtk::Align::START);
    m_statusLabel.set_valign(Gtk::Align::END);
    m_statusLabel.set_margin_start(15);
    m_statusLabel.set_margin_bottom(8);
    m_statusLabel.set_margin_top(5);

    m_mainBox.set_valign(Gtk::Align::FILL);
    m_mainBox.set_halign(Gtk::Align::FILL);
    m_mainBox.set_hexpand(true);
    m_mainBox.set_vexpand(true);

    m_mainBox.append(m_topBox);       
    m_mainBox.append(m_contentBox);   
    m_mainBox.append(m_statusLabel);  
        
    set_child(m_mainBox);
    m_currentThumbnailSize = static_cast<int>(m_zoomScale.get_value());
}

MainWindow::~MainWindow() {
    if (m_ftpClient) {
        m_ftpClient->disconnect();
    }
}

void MainWindow::on_connect_clicked() {
    std::string ip = m_ipEntry.get_text();
    if (ip.empty()) {
        m_statusLabel.set_text("Error: Please enter a valid IP address.");
        return;
    }

    if (m_ftpClient) {
        m_ftpClient->disconnect();
        m_ftpClient.reset();
    }

    while (auto* child = m_gameList.get_first_child()) m_gameList.remove(*child);
    while (auto* child = m_mediaGrid.get_first_child()) m_mediaGrid.remove(*child);
    m_lastTitleId = "";

    m_statusLabel.set_text("Connecting to " + ip + "...");
    m_spinner.start();

    m_ftpClient = std::make_unique<FtpClient>(ip);
    
    if (m_ftpClient->connect()) {
        m_statusLabel.set_text("Connected successfully!");
        loadGameList(); 
    } else {
        m_statusLabel.set_text("Connection failed.");
        m_spinner.stop();
    }
}

void MainWindow::loadGameList() {
    if (!m_ftpClient) return;

    auto* systemButton = Gtk::make_managed<Gtk::Button>();
    systemButton->set_has_frame(false);
    systemButton->set_halign(Gtk::Align::FILL);
    
    auto* systemLabel = Gtk::make_managed<Gtk::Label>("System");
    systemLabel->set_ellipsize(Pango::EllipsizeMode::END);
    systemLabel->set_max_width_chars(32); 
    systemButton->set_child(*systemLabel);

    systemButton->signal_clicked().connect([this]() {
        m_statusLabel.set_text("Selected Category: System");
        loadMediaGrid("NPXS20001");
    });
    m_gameList.append(*systemButton);

    std::vector<std::string> mediaRoots = {
        "/user/av_contents/thumbnails/photo/NPXS20001",
        "/user/av_contents/thumbnails/video/NPXS20001"
    };

    std::vector<std::string> discoveredCusas;

    for (const auto& root : mediaRoots) {
        auto files = m_ftpClient->listDirectory(root);
        for (const auto& file : files) {
            if (!file.isDirectory) continue;
            std::string name = file.name;
            if (name.find("CUSA") == 0 && name.length() == 9) {
                if (std::find(discoveredCusas.begin(), discoveredCusas.end(), name) == discoveredCusas.end()) {
                    discoveredCusas.push_back(name);
                }
            }
        }
    }

    std::sort(discoveredCusas.begin(), discoveredCusas.end());

    for (const auto& gameId : discoveredCusas) {
        auto* gameButton = Gtk::make_managed<Gtk::Button>();
        gameButton->set_has_frame(false);
        gameButton->set_halign(Gtk::Align::FILL);
        
        auto* gameLabel = Gtk::make_managed<Gtk::Label>(gameId);
        gameLabel->set_ellipsize(Pango::EllipsizeMode::END);
        gameLabel->set_max_width_chars(32);
        gameButton->set_child(*gameLabel);
        
        gameButton->signal_clicked().connect([this, gameId]() {
            m_statusLabel.set_text("Selected Game: " + gameId);
            loadMediaGrid(gameId); 
        });
        
        m_gameList.append(*gameButton);
        fetchGameTitleAsync(gameId, gameLabel);
    }

    m_spinner.stop();
}

void MainWindow::fetchGameTitleAsync(const std::string& gameId, Gtk::Label* labelToUpdate) {
    if (m_resolvedTitlesCache.find(gameId) != m_resolvedTitlesCache.end()) {
        labelToUpdate->set_text(m_resolvedTitlesCache[gameId]);
        return;
    }

    std::thread([this, gameId, labelToUpdate]() {
        std::string finalTitle = gameId; 

        CURL* curl = curl_easy_init();
        if (curl) {
            std::string readBuffer;
            std::string url = "https://orbispatches.com/" + gameId;
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) PlayVault/1.0");

            CURLcode res = curl_easy_perform(curl);
            long response_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            if (res == CURLE_OK && response_code == 200) {
                size_t titleStart = readBuffer.find("<title>");
                if (titleStart != std::string::npos) {
                    size_t titleEnd = readBuffer.find("</title>", titleStart);
                    if (titleEnd != std::string::npos) {
                        std::string fullTitleTag = readBuffer.substr(titleStart + 7, titleEnd - (titleStart + 7));
                        
                        // extract name game (es. "CUSA00717: Game name")
                        size_t semiColonPos = fullTitleTag.find(":");
                        if (semiColonPos != std::string::npos) {
                            fullTitleTag = fullTitleTag.substr(semiColonPos + 1);
                            if (!fullTitleTag.empty() && fullTitleTag[0] == ' ') {
                                fullTitleTag.erase(0, 1);
                            }
                        }

                        size_t pipePos = fullTitleTag.find(" |");
                        if (pipePos != std::string::npos) {
                            fullTitleTag = fullTitleTag.substr(0, pipePos);
                        }

                        if (!fullTitleTag.empty()) {
                            finalTitle = fullTitleTag + " (" + gameId + ")";
                            LOG_INFO("OrbisPatches mapping completed: " + gameId + " -> " + fullTitleTag);
                        }
                    }
                }
            } else {
                LOG_ERROR("OrbisPatches call failed for " + gameId + ".HTTP Code: " + std::to_string(response_code));
            }
            curl_easy_cleanup(curl);
        }

        m_resolvedTitlesCache[gameId] = finalTitle;

        Glib::MainContext::get_default()->signal_idle().connect([labelToUpdate, finalTitle]() {
            labelToUpdate->set_text(finalTitle);
            return false;
        });
    }).detach();
}

void MainWindow::loadMediaGrid(const std::string& titleId) {
    if (!m_ftpClient) return;
    m_lastTitleId = titleId; 
    
    m_spinner.start();

    while (auto* child = m_mediaGrid.get_first_child()) {
        m_mediaGrid.remove(*child);
    }

    std::vector<std::string> foldersToScan = {
        "/user/av_contents/thumbnails/photo/NPXS20001/" + titleId,
        "/user/av_contents/thumbnails/video/NPXS20001/" + titleId
    };

    struct MediaFileEntry {
        std::string folderPath;
        std::string fileName;
        bool isVideo;
        size_t fileSize;
    };
    std::vector<MediaFileEntry> masterFileList;

    for (const auto& targetFolder : foldersToScan) {
        bool isVideoPath = (targetFolder.find("/thumbnails/video/") != std::string::npos);
        
        if (m_currentFilterSortMode == 2 && isVideoPath) continue;  
        if (m_currentFilterSortMode == 3 && !isVideoPath) continue; 

        auto files = m_ftpClient->listDirectory(targetFolder);
        if (files.empty()) continue;

        for (const auto& file : files) {
            if (file.isDirectory) {
                std::string deepFolder = targetFolder + "/" + file.name;
                auto deepFiles = m_ftpClient->listDirectory(deepFolder);
                for (const auto& deepFile : deepFiles) {
                    if (!deepFile.isDirectory) {
                        masterFileList.push_back({deepFolder, deepFile.name, isVideoPath, deepFile.size});
                    }
                }
            } else {
                masterFileList.push_back({targetFolder, file.name, isVideoPath, file.size});
            }
        }
    }

    if (m_currentFilterSortMode == 0 || m_currentFilterSortMode == 2 || m_currentFilterSortMode == 3) {
        std::sort(masterFileList.begin(), masterFileList.end(), [](const MediaFileEntry& a, const MediaFileEntry& b) {
            return a.fileName > b.fileName; 
        });
    } else if (m_currentFilterSortMode == 1) {
        std::sort(masterFileList.begin(), masterFileList.end(), [](const MediaFileEntry& a, const MediaFileEntry& b) {
            return a.fileName < b.fileName; 
        });
    } else if (m_currentFilterSortMode == 4) {
        std::sort(masterFileList.begin(), masterFileList.end(), [](const MediaFileEntry& a, const MediaFileEntry& b) {
            return a.fileSize > b.fileSize; 
        });
    }

    bool elementsFound = false;

    for (const auto& entry : masterFileList) {
        std::string lowerName = entry.fileName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        if (lowerName.find(".jpg") == std::string::npos && lowerName.find(".png") == std::string::npos) {
            continue;
        }

        elementsFound = true;
        std::string remoteFilePath = entry.folderPath + "/" + entry.fileName;

        auto* itemBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0); 
        itemBox->set_margin_start(2);
        itemBox->set_margin_end(2);
        itemBox->set_margin_top(2);
        itemBox->set_margin_bottom(2);
        itemBox->set_valign(Gtk::Align::START); 
        itemBox->set_halign(Gtk::Align::CENTER);
        itemBox->set_vexpand(false);

        auto* imageWidget = Gtk::make_managed<Gtk::Image>();
        imageWidget->set_from_icon_name(entry.isVideo ? "media-playback-start" : "image-x-generic");
        
        imageWidget->set_pixel_size(m_currentThumbnailSize); 
        imageWidget->set_valign(Gtk::Align::START);
        imageWidget->set_vexpand(false);

        std::string cacheDir = std::string(g_get_user_cache_dir()) + "/PlayVault/cache";
        std::filesystem::create_directories(cacheDir);
        std::string localThumbPath = cacheDir + "/" + titleId + "_" + entry.fileName;

        if (std::filesystem::exists(localThumbPath)) {
            imageWidget->set(localThumbPath);
        } else {
            std::string ip = m_ipEntry.get_text();
            std::thread([ip, remoteFilePath, localThumbPath, imageWidget]() {
                FtpClient backgroundClient(ip);
                if (backgroundClient.connect()) {
                    if (backgroundClient.downloadFile(remoteFilePath, localThumbPath, nullptr)) {
                        Glib::MainContext::get_default()->signal_idle().connect([imageWidget, localThumbPath]() {
                            imageWidget->set(localThumbPath);
                            return false; 
                        });
                    }
                }
            }).detach();
        }

        std::string cleanDisplayName = entry.fileName;
        
        size_t doubleJpg = cleanDisplayName.find(".jpg.jpg");
        if (doubleJpg != std::string::npos) cleanDisplayName.replace(doubleJpg, 8, "");
        
        size_t mp4Jpg = cleanDisplayName.find(".mp4.jpg");
        if (mp4Jpg != std::string::npos) cleanDisplayName.replace(mp4Jpg, 8, "");
        
        size_t singleJpg = cleanDisplayName.find(".jpg");
        if (singleJpg != std::string::npos) cleanDisplayName.replace(singleJpg, 4, "");
        
        size_t singleMp4 = cleanDisplayName.find(".mp4");
        if (singleMp4 != std::string::npos) cleanDisplayName.replace(singleMp4, 4, "");

        if (cleanDisplayName.length() >= 15 && cleanDisplayName[8] == '_') {
            std::string year = cleanDisplayName.substr(0, 4);
            std::string month = cleanDisplayName.substr(4, 2);
            std::string day = cleanDisplayName.substr(6, 2);
            std::string hour = cleanDisplayName.substr(9, 2);
            std::string minute = cleanDisplayName.substr(11, 2);
            cleanDisplayName = day + "/" + month + "/" + year + " " + hour + ":" + minute;
        }

        auto* nameLabel = Gtk::make_managed<Gtk::Label>();
        if (entry.isVideo) {
            nameLabel->set_markup("<span foreground='#3584e4'><b>🎬 Video</b></span>\n" + cleanDisplayName);
        } else {
            nameLabel->set_markup("<span foreground='#8a8a8a'>📷 Foto</span>\n" + cleanDisplayName);
        }
        
        nameLabel->set_justify(Gtk::Justification::CENTER);
        nameLabel->set_single_line_mode(false);
        nameLabel->set_ellipsize(Pango::EllipsizeMode::NONE);
        nameLabel->set_max_width_chars(18);
        nameLabel->set_valign(Gtk::Align::START);
        nameLabel->set_margin_top(4);
        nameLabel->set_margin_bottom(4);
        nameLabel->set_vexpand(false);

        itemBox->append(*imageWidget);
        itemBox->append(*nameLabel);

        auto gesture = Gtk::GestureClick::create();
        gesture->set_button(1); 
        gesture->signal_pressed().connect([this, remoteFilePath, filename = entry.fileName, isVideoPath = entry.isVideo, titleId](int n_press, double, double) {
            if (n_press != 2) return; 

            m_spinner.start(); 
            std::string mediaType = isVideoPath ? "video" : "photo";
            std::string cleanFileName = filename;
            
            if (isVideoPath) {
                size_t mp4Pos = cleanFileName.find(".mp4");
                if (mp4Pos != std::string::npos) {
                    cleanFileName = cleanFileName.substr(0, mp4Pos + 4);
                } else {
                    size_t dotPos = cleanFileName.find_last_of(".");
                    if (dotPos != std::string::npos) cleanFileName = cleanFileName.substr(0, dotPos) + ".mp4";
                }
            } else {
                size_t jpgPos = cleanFileName.find(".jpg.jpg");
                if (jpgPos != std::string::npos) {
                    cleanFileName.replace(jpgPos, 8, ".jpg");
                }
            }

            std::string subFolder = "";
            size_t titlePos = remoteFilePath.find("/" + titleId + "/");
            if (titlePos != std::string::npos) {
                std::string trailingPath = remoteFilePath.substr(titlePos + titleId.length() + 2);
                size_t slashPos = trailingPath.find("/");
                if (slashPos != std::string::npos) {
                    subFolder = trailingPath.substr(0, slashPos) + "/";
                }
            }

            std::string realRemotePath = "/user/av_contents/" + mediaType + "/NPXS20001/" + titleId + "/" + subFolder + cleanFileName;
            std::string homeDir = g_get_home_dir();
            std::string downloadPath = homeDir + "/" + cleanFileName;

            m_statusLabel.set_text("Downloading " + cleanFileName + "...");
            
            std::string ip = m_ipEntry.get_text();
            std::thread([this, ip, realRemotePath, downloadPath, cleanFileName]() {
                FtpClient downloadClient(ip);
                if (downloadClient.connect()) {
                    if (downloadClient.downloadFile(realRemotePath, downloadPath, nullptr)) {
                        Glib::MainContext::get_default()->signal_idle().connect([this, cleanFileName]() {
                            m_statusLabel.set_text("Download completato: " + cleanFileName);
                            m_spinner.stop();

                            auto dialog = Gtk::AlertDialog::create("Download Completato");
                            dialog->set_detail("Il file " + cleanFileName + " è stato salvato nella tua cartella Home.");
                            dialog->show(*this);

                            return false;
                        });
                    } else {
                        Glib::MainContext::get_default()->signal_idle().connect([this]() {
                            m_statusLabel.set_text("Errore durante il download.");
                            m_spinner.stop();
                            return false;
                        });
                    }
                }
            }).detach();
        });
        itemBox->add_controller(gesture);

        m_mediaGrid.append(*itemBox);
    }

    if (!elementsFound) {
        auto* noMediaLabel = Gtk::make_managed<Gtk::Label>("Nessun contenuto multimediale corrisponde ai filtri impostati.");
        m_mediaGrid.append(*noMediaLabel);
        m_statusLabel.set_text("Nessun contenuto trovato.");
    }
    
    m_spinner.stop(); 
}

}