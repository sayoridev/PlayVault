#include "App/PlayVaultApp.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        auto app = pv::PlayVaultApp::create();
        return app->run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Critical Error: " << e.what() << '\n';
        return 1; 
    }
}