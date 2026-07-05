#pragma once
#include <gtkmm.h>
#include <memory>

namespace pv {

class MainWindow;

class PlayVaultApp : public Gtk::Application {
protected:
    PlayVaultApp();

public:
    static Glib::RefPtr<PlayVaultApp> create();

protected:
    void on_activate() override; 

private:
    std::unique_ptr<MainWindow> m_mainWindow;
};

} 