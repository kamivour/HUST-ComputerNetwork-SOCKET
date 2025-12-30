/**
 * @file ServerWindow.h
 * @brief Qt GUI for Chat Server management
 */

#ifndef SERVER_WINDOW_H
#define SERVER_WINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QTimer>
#include <QThread>
#include <memory>

class Server;

/**
 * @brief Worker thread for running the server
 */
class ServerWorker : public QObject {
    Q_OBJECT

public:
    explicit ServerWorker(int port, QObject* parent = nullptr);
    ~ServerWorker();

    Server* getServer() const;

public slots:
    void startServer();
    void stopServer();
    void broadcastMessage(const QString& message);
    void sendMessageToClient(const QString& username, const QString& message);

signals:
    void serverStarted();
    void serverStopped();
    void serverError(const QString& error);
    void logMessage(const QString& message);

private:
    int port_;
    std::unique_ptr<Server> server_;
};

/**
 * @brief Main window for server GUI
 */
class ServerWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ServerWindow(QWidget* parent = nullptr);
    ~ServerWindow();

private slots:
    void onStartStopClicked();
    void onBroadcastClicked();
    void onSendToClientClicked();
    void onRefreshClicked();
    void onClientSelectionChanged();
    void updateClientList();
    void appendLog(const QString& message);
    void onServerStarted();
    void onServerStopped();
    void onServerError(const QString& error);

private:
    void setupUI();
    void createMenuBar();
    void updateServerStatus(bool running);

    // UI Components
    QWidget* centralWidget_;

    // Server controls
    QSpinBox* portSpinBox_;
    QPushButton* startStopButton_;
    QLabel* statusLabel_;

    // Client list
    QTableWidget* clientTable_;
    QPushButton* refreshButton_;

    // Log view
    QTextEdit* logView_;

    // Broadcast
    QLineEdit* broadcastInput_;
    QPushButton* broadcastButton_;

    // Private message to client
    QLabel* selectedClientLabel_;
    QLineEdit* privateMessageInput_;
    QPushButton* sendToClientButton_;

    // Server thread
    QThread* serverThread_;
    ServerWorker* serverWorker_;

    // Refresh timer
    QTimer* refreshTimer_;

    bool serverRunning_;
};

#endif // SERVER_WINDOW_H
