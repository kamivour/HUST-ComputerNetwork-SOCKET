/**
 * @file ServerWindow.cpp
 * @brief Implementation of Server GUI
 */

#include "ServerWindow.h"
#include "../server/Server.h"
#include "../server/Database.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QDateTime>
#include <QApplication>
#include <QSplitter>

// ========== ServerWorker Implementation ==========

ServerWorker::ServerWorker(int port, QObject* parent)
    : QObject(parent)
    , port_(port)
    , server_(nullptr) {
}

ServerWorker::~ServerWorker() {
    stopServer();
}

void ServerWorker::startServer() {
    try {
        server_ = std::make_unique<Server>(port_);
        if (server_->start()) {
            emit serverStarted();
            emit logMessage("Server started on port " + QString::number(port_));
        } else {
            emit serverError("Failed to start server on port " + QString::number(port_));
            server_.reset();
        }
    } catch (const std::exception& e) {
        emit serverError(QString("Exception: ") + e.what());
        server_.reset();
    }
}

void ServerWorker::stopServer() {
    if (server_) {
        server_->stop();
        server_.reset();
        emit serverStopped();
        emit logMessage("Server stopped");
    }
}

Server* ServerWorker::getServer() const {
    return server_.get();
}

void ServerWorker::broadcastMessage(const QString& message) {
    if (server_) {
        server_->broadcastServerMessage(message.toStdString());
    }
}

void ServerWorker::sendMessageToClient(const QString& username, const QString& message) {
    if (server_) {
        server_->sendServerMessageToUser(username.toStdString(), message.toStdString());
    }
}

// ========== ServerWindow Implementation ==========

ServerWindow::ServerWindow(QWidget* parent)
    : QMainWindow(parent)
    , serverThread_(nullptr)
    , serverWorker_(nullptr)
    , refreshTimer_(nullptr)
    , serverRunning_(false) {
    setupUI();
    createMenuBar();

    setWindowTitle("Chat Server");
    resize(800, 600);

    // Setup refresh timer
    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &ServerWindow::updateClientList);
}

ServerWindow::~ServerWindow() {
    if (serverRunning_) {
        onStartStopClicked();  // Stop server if running
    }

    if (serverThread_) {
        serverThread_->quit();
        serverThread_->wait();
    }
}

void ServerWindow::setupUI() {
    centralWidget_ = new QWidget(this);
    setCentralWidget(centralWidget_);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget_);

    // ===== Server Control Group =====
    QGroupBox* controlGroup = new QGroupBox("Server Control", this);
    QHBoxLayout* controlLayout = new QHBoxLayout(controlGroup);

    QLabel* portLabel = new QLabel("Port:", this);
    portSpinBox_ = new QSpinBox(this);
    portSpinBox_->setRange(1024, 65535);
    portSpinBox_->setValue(9000);

    startStopButton_ = new QPushButton("Start Server", this);
    startStopButton_->setMinimumWidth(120);

    statusLabel_ = new QLabel("Status: Stopped", this);
    statusLabel_->setStyleSheet("color: red; font-weight: bold;");

    controlLayout->addWidget(portLabel);
    controlLayout->addWidget(portSpinBox_);
    controlLayout->addWidget(startStopButton_);
    controlLayout->addStretch();
    controlLayout->addWidget(statusLabel_);

    connect(startStopButton_, &QPushButton::clicked, this, &ServerWindow::onStartStopClicked);

    // ===== Splitter for client list and log =====
    QSplitter* splitter = new QSplitter(Qt::Vertical, this);

    // ===== Connected Clients Group =====
    QGroupBox* clientGroup = new QGroupBox("Connected Clients", this);
    QVBoxLayout* clientLayout = new QVBoxLayout(clientGroup);

    clientTable_ = new QTableWidget(this);
    clientTable_->setColumnCount(4);
    clientTable_->setHorizontalHeaderLabels({"Username", "Display Name", "IP Address", "Role"});
    clientTable_->horizontalHeader()->setStretchLastSection(true);
    clientTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    clientTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    clientTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    refreshButton_ = new QPushButton("Refresh", this);
    connect(refreshButton_, &QPushButton::clicked, this, &ServerWindow::onRefreshClicked);

    QHBoxLayout* clientButtonLayout = new QHBoxLayout();
    clientButtonLayout->addStretch();
    clientButtonLayout->addWidget(refreshButton_);

    clientLayout->addWidget(clientTable_);
    clientLayout->addLayout(clientButtonLayout);

    // ===== Server Log Group =====
    QGroupBox* logGroup = new QGroupBox("Server Log", this);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);

    logView_ = new QTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setFont(QFont("Consolas", 9));

    logLayout->addWidget(logView_);

    splitter->addWidget(clientGroup);
    splitter->addWidget(logGroup);
    splitter->setSizes({300, 200});

    // ===== Messaging Group =====
    QGroupBox* messagingGroup = new QGroupBox("Messaging", this);
    QVBoxLayout* messagingLayout = new QVBoxLayout(messagingGroup);

    // Broadcast section
    QHBoxLayout* broadcastLayout = new QHBoxLayout();
    QLabel* broadcastLabel = new QLabel("Broadcast:", this);
    broadcastInput_ = new QLineEdit(this);
    broadcastInput_->setPlaceholderText("Message to all clients...");
    broadcastButton_ = new QPushButton("Send to All", this);
    broadcastButton_->setEnabled(false);

    connect(broadcastButton_, &QPushButton::clicked, this, &ServerWindow::onBroadcastClicked);
    connect(broadcastInput_, &QLineEdit::returnPressed, this, &ServerWindow::onBroadcastClicked);

    broadcastLayout->addWidget(broadcastLabel);
    broadcastLayout->addWidget(broadcastInput_, 1);
    broadcastLayout->addWidget(broadcastButton_);

    // Private message section
    QHBoxLayout* privateLayout = new QHBoxLayout();
    selectedClientLabel_ = new QLabel("Selected: (none)", this);
    selectedClientLabel_->setMinimumWidth(150);
    privateMessageInput_ = new QLineEdit(this);
    privateMessageInput_->setPlaceholderText("Message to selected client...");
    privateMessageInput_->setEnabled(false);
    sendToClientButton_ = new QPushButton("Send to Selected", this);
    sendToClientButton_->setEnabled(false);

    connect(sendToClientButton_, &QPushButton::clicked, this, &ServerWindow::onSendToClientClicked);
    connect(privateMessageInput_, &QLineEdit::returnPressed, this, &ServerWindow::onSendToClientClicked);
    connect(clientTable_, &QTableWidget::itemSelectionChanged, this, &ServerWindow::onClientSelectionChanged);

    privateLayout->addWidget(selectedClientLabel_);
    privateLayout->addWidget(privateMessageInput_, 1);
    privateLayout->addWidget(sendToClientButton_);

    messagingLayout->addLayout(broadcastLayout);
    messagingLayout->addLayout(privateLayout);

    // Add all to main layout
    mainLayout->addWidget(controlGroup);
    mainLayout->addWidget(splitter, 1);
    mainLayout->addWidget(messagingGroup);
}

void ServerWindow::createMenuBar() {
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // File menu
    QMenu* fileMenu = menuBar->addMenu("&File");

    QAction* exitAction = new QAction("E&xit", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    fileMenu->addAction(exitAction);

    // Help menu
    QMenu* helpMenu = menuBar->addMenu("&Help");

    QAction* aboutAction = new QAction("&About", this);
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "About Chat Server",
            "TCP Chat Server GUI\n\n"
            "A simple server management interface for the chat application.");
    });
    helpMenu->addAction(aboutAction);
}

void ServerWindow::onStartStopClicked() {
    if (serverRunning_) {
        // Stop server
        if (serverWorker_) {
            QMetaObject::invokeMethod(serverWorker_, "stopServer", Qt::QueuedConnection);
        }
        refreshTimer_->stop();
    } else {
        // Start server
        int port = portSpinBox_->value();

        serverThread_ = new QThread(this);
        serverWorker_ = new ServerWorker(port);
        serverWorker_->moveToThread(serverThread_);

        connect(serverThread_, &QThread::started, serverWorker_, &ServerWorker::startServer);
        connect(serverWorker_, &ServerWorker::serverStarted, this, &ServerWindow::onServerStarted);
        connect(serverWorker_, &ServerWorker::serverStopped, this, &ServerWindow::onServerStopped);
        connect(serverWorker_, &ServerWorker::serverError, this, &ServerWindow::onServerError);
        connect(serverWorker_, &ServerWorker::logMessage, this, &ServerWindow::appendLog);

        serverThread_->start();
        appendLog("Starting server on port " + QString::number(port) + "...");
    }
}

void ServerWindow::onServerStarted() {
    serverRunning_ = true;
    updateServerStatus(true);
    refreshTimer_->start(2000);  // Refresh every 2 seconds
    updateClientList();
}

void ServerWindow::onServerStopped() {
    serverRunning_ = false;
    updateServerStatus(false);

    if (serverThread_) {
        serverThread_->quit();
        serverThread_->wait();
        delete serverWorker_;
        serverWorker_ = nullptr;
        serverThread_ = nullptr;
    }

    clientTable_->setRowCount(0);
}

void ServerWindow::onServerError(const QString& error) {
    appendLog("ERROR: " + error);
    QMessageBox::critical(this, "Server Error", error);

    serverRunning_ = false;
    updateServerStatus(false);

    if (serverThread_) {
        serverThread_->quit();
        serverThread_->wait();
        delete serverWorker_;
        serverWorker_ = nullptr;
        serverThread_ = nullptr;
    }
}

void ServerWindow::updateServerStatus(bool running) {
    if (running) {
        statusLabel_->setText("Status: Running");
        statusLabel_->setStyleSheet("color: green; font-weight: bold;");
        startStopButton_->setText("Stop Server");
        portSpinBox_->setEnabled(false);
        broadcastButton_->setEnabled(true);
    } else {
        statusLabel_->setText("Status: Stopped");
        statusLabel_->setStyleSheet("color: red; font-weight: bold;");
        startStopButton_->setText("Start Server");
        portSpinBox_->setEnabled(true);
        broadcastButton_->setEnabled(false);
    }
}

void ServerWindow::onBroadcastClicked() {
    if (!serverRunning_ || broadcastInput_->text().isEmpty()) {
        return;
    }

    QString message = broadcastInput_->text();
    appendLog("[BROADCAST] Server: " + message);

    // Send broadcast through server worker
    if (serverWorker_) {
        QMetaObject::invokeMethod(serverWorker_, "broadcastMessage",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, message));
    }

    broadcastInput_->clear();
}

void ServerWindow::onSendToClientClicked() {
    if (!serverRunning_ || privateMessageInput_->text().isEmpty()) {
        return;
    }

    // Get selected client username
    QList<QTableWidgetItem*> selectedItems = clientTable_->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    int row = selectedItems.first()->row();
    QTableWidgetItem* usernameItem = clientTable_->item(row, 0);
    if (!usernameItem) {
        return;
    }

    QString username = usernameItem->text();
    if (username == "(not logged in)") {
        appendLog("[ERROR] Cannot send message to unauthenticated client");
        return;
    }

    QString message = privateMessageInput_->text();
    appendLog("[PRIVATE -> " + username + "] Server: " + message);

    // Send message through server worker
    if (serverWorker_) {
        QMetaObject::invokeMethod(serverWorker_, "sendMessageToClient",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, username),
                                  Q_ARG(QString, message));
    }

    privateMessageInput_->clear();
}

void ServerWindow::onClientSelectionChanged() {
    QList<QTableWidgetItem*> selectedItems = clientTable_->selectedItems();

    if (selectedItems.isEmpty()) {
        selectedClientLabel_->setText("Selected: (none)");
        privateMessageInput_->setEnabled(false);
        sendToClientButton_->setEnabled(false);
        return;
    }

    int row = selectedItems.first()->row();
    QTableWidgetItem* usernameItem = clientTable_->item(row, 0);

    if (usernameItem) {
        QString username = usernameItem->text();
        selectedClientLabel_->setText("Selected: " + username);

        bool canSend = serverRunning_ && (username != "(not logged in)");
        privateMessageInput_->setEnabled(canSend);
        sendToClientButton_->setEnabled(canSend);
    }
}

void ServerWindow::onRefreshClicked() {
    updateClientList();
}

void ServerWindow::updateClientList() {
    if (!serverRunning_ || !serverWorker_) {
        return;
    }

    Server* server = serverWorker_->getServer();
    if (!server) {
        return;
    }

    // Get connected clients from server
    std::vector<ClientInfo> clients = server->getConnectedClients();

    // Update table
    clientTable_->setRowCount(static_cast<int>(clients.size()));

    int row = 0;
    for (const auto& client : clients) {
        QString username = client.isAuthenticated ?
            QString::fromStdString(client.username) : "(not logged in)";
        QString displayName = QString::fromStdString(client.displayName);
        QString address = QString::fromStdString(client.address);
        QString role = client.role == 1 ? "Admin" : "Member";

        if (!client.isAuthenticated) {
            displayName = "-";
            role = "-";
        }

        clientTable_->setItem(row, 0, new QTableWidgetItem(username));
        clientTable_->setItem(row, 1, new QTableWidgetItem(displayName));
        clientTable_->setItem(row, 2, new QTableWidgetItem(address));
        clientTable_->setItem(row, 3, new QTableWidgetItem(role));

        row++;
    }
}

void ServerWindow::appendLog(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss] ");
    logView_->append(timestamp + message);
}
