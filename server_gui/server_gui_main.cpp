/**
 * @file server_gui_main.cpp
 * @brief Entry point for Chat Server GUI application
 */

#include "ServerWindow.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("Chat Server");
    app.setOrganizationName("TCPChat");
    app.setApplicationVersion("1.0");

    ServerWindow window;
    window.show();

    return app.exec();
}
