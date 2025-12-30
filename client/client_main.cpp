/**
 * @file client_main.cpp
 * @brief Main entry point for Chat Client
 */

#include <QApplication>
#include <QStyleFactory>
#include "ChatClient.h"
#include "AuthDialog.h"
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Set application info
    app.setApplicationName("TCP Chat Client");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("MMT");

    // Set Fusion style for consistent look
    app.setStyle(QStyleFactory::create("Fusion"));

    // Set stylesheet for modern look
    app.setStyleSheet(R"(
        QMainWindow, QDialog {
            background-color: #fafafa;
        }
        QGroupBox {
            font-weight: bold;
            border: 1px solid #ddd;
            border-radius: 5px;
            margin-top: 10px;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
        QPushButton {
            background-color: #2196F3;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #1976D2;
        }
        QPushButton:pressed {
            background-color: #0D47A1;
        }
        QPushButton:disabled {
            background-color: #bdbdbd;
        }
        QLineEdit {
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 6px;
            background-color: white;
        }
        QLineEdit:focus {
            border-color: #2196F3;
        }
        QTabWidget::pane {
            border: 1px solid #ddd;
            border-radius: 5px;
            background-color: white;
        }
        QTabBar::tab {
            background-color: #e0e0e0;
            border: 1px solid #ccc;
            padding: 8px 16px;
            margin-right: 2px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        QTabBar::tab:selected {
            background-color: white;
            border-bottom-color: white;
        }
        QTabBar::tab:hover {
            background-color: #f5f5f5;
        }
    )");

    // Create chat client
    ChatClient client;

    // Create windows
    AuthDialog authDialog(&client);
    MainWindow mainWindow(&client);

    // Connect auth dialog success to show main window
    QObject::connect(&authDialog, &AuthDialog::loginSuccessful, [&]() {
        authDialog.hide();
        mainWindow.showAndReset();
    });

    // Connect main window logout to show auth dialog
    QObject::connect(&mainWindow, &MainWindow::loggedOut, [&]() {
        mainWindow.hide();
        client.disconnectFromServer();
        authDialog.show();
    });

    // Show auth dialog first
    authDialog.show();

    return app.exec();
}
