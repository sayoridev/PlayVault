#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/paned.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/stringlist.h>
#include <gtkmm/spinner.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/scale.h>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include "../FTP/FtpClient.hpp"

namespace pv {

class MainWindow : public Gtk::Window {
public:
    MainWindow();
    virtual ~MainWindow();

protected:
    void on_connect_clicked();
    void loadGameList();
    void loadMediaGrid(const std::string& titleId);

private:
    Gtk::Box m_mainBox;
    Gtk::Box m_topBox;
    Gtk::Box m_contentBox;
    Gtk::Box m_sidebarBox;

    Gtk::Label m_ipLabel;
    Gtk::Entry m_ipEntry;
    Gtk::Button m_connectButton;
    Gtk::Label m_statusLabel;
    
    Gtk::Box m_gameList{Gtk::Orientation::VERTICAL, 4};
    Gtk::FlowBox m_mediaGrid;
    Gtk::ScrolledWindow m_mediaGridScroll;
    Gtk::DropDown m_filterDropDown;
    Gtk::Spinner m_spinner;
    Gtk::Scale m_zoomScale;

    std::unique_ptr<FtpClient> m_ftpClient;
    std::string m_lastTitleId;
    uint32_t m_currentFilterSortMode = 0;
    int m_currentThumbnailSize = 160;
    Glib::RefPtr<Gtk::CssProvider> m_cssProvider;
    
    std::map<std::string, std::string> m_resolvedTitlesCache;
    void fetchGameTitleAsync(const std::string& gameId, Gtk::Label* labelToUpdate);
};

} 

#endif