#include "PlayVaultApp.hpp"
#include "../GUI/MainWindow.hpp" 
#include "../Logger/Logger.hpp"   

namespace pv {

PlayVaultApp::PlayVaultApp()
    : Gtk::Application("com.playvault.app") {
}

Glib::RefPtr<PlayVaultApp> PlayVaultApp::create() {
    return Glib::make_refptr_for_instance<PlayVaultApp>(new PlayVaultApp());
}

void PlayVaultApp::on_activate() {
    LOG_INFO("user interface initialization...");
    m_mainWindow = std::make_unique<MainWindow>();
    add_window(*m_mainWindow);
    m_mainWindow->present();
}

} 