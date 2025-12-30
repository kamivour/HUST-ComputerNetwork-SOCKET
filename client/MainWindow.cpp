/**
 * @file MainWindow.cpp
 * @brief Implementation of main chat window
 */

#include "MainWindow.h"
#include "ChatClient.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QMessageBox>
#include <QCloseEvent>
#include <QScrollBar>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDateTime>

// ============ ChatTab Implementation ============

ChatTab::ChatTab(const QString& recipient, bool isGlobal, QWidget* parent)
    : QWidget(parent)
    , recipient_(recipient)
    , isGlobal_(isGlobal) {

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);

    // Chat history
    chatHistory_ = new QTextEdit(this);
    chatHistory_->setReadOnly(true);
    chatHistory_->setStyleSheet(
        "QTextEdit { background-color: #f5f5f5; border: 1px solid #ddd; "
        "border-radius: 5px; padding: 5px; font-family: 'Segoe UI', Arial; font-size: 13px; }"
    );
    layout->addWidget(chatHistory_, 1);

    // Input area
    QHBoxLayout* inputLayout = new QHBoxLayout();

    messageInput_ = new QLineEdit(this);
    messageInput_->setPlaceholderText(isGlobal ? tr("Nhập tin nhắn cho mọi người...") :
                                                  tr("Nhập tin nhắn cho %1...").arg(recipient));
    messageInput_->setMinimumHeight(35);
    messageInput_->setStyleSheet(
        "QLineEdit { border: 1px solid #ccc; border-radius: 5px; padding: 5px 10px; font-size: 13px; }"
    );
    inputLayout->addWidget(messageInput_);

    sendBtn_ = new QPushButton(tr("Gửi"), this);
    sendBtn_->setMinimumSize(80, 35);
    sendBtn_->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; border: none; "
        "border-radius: 5px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:pressed { background-color: #3d8b40; }"
    );
    inputLayout->addWidget(sendBtn_);

    layout->addLayout(inputLayout);

    // Connections
    connect(sendBtn_, &QPushButton::clicked, [this]() {
        QString content = getMessage();
        if (!content.isEmpty()) {
            emit sendMessageRequested(content);
            clearInput();
        }
    });

    connect(messageInput_, &QLineEdit::returnPressed, [this]() {
        QString content = getMessage();
        if (!content.isEmpty()) {
            emit sendMessageRequested(content);
            clearInput();
        }
    });
}

void ChatTab::appendMessage(const QString& sender, const QString& content,
                            const QString& timestamp, bool isOwnMessage) {
    QString html;
    QString senderColor = isOwnMessage ? "#2196F3" : "#4CAF50";
    QString bgColor = isOwnMessage ? "#e3f2fd" : "#e8f5e9";

    html = QString(
        "<div style='margin: 5px 0; padding: 8px; background-color: %1; border-radius: 8px;'>"
        "<span style='color: %2; font-weight: bold;'>%3</span> "
        "<span style='color: #888; font-size: 11px;'>[%4]</span><br/>"
        "<span style='color: #333;'>%5</span>"
        "</div>"
    ).arg(bgColor, senderColor, sender.toHtmlEscaped(), timestamp.toHtmlEscaped(),
          content.toHtmlEscaped().replace("\n", "<br/>"));

    chatHistory_->append(html);

    // Auto scroll to bottom
    QScrollBar* scrollBar = chatHistory_->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

QString ChatTab::getMessage() const {
    return messageInput_->text().trimmed();
}

void ChatTab::clearInput() {
    messageInput_->clear();
}

// ============ ChangePasswordDialog Implementation ============

ChangePasswordDialog::ChangePasswordDialog(QWidget* parent)
    : QDialog(parent) {

    setWindowTitle(tr("Đổi mật khẩu"));
    setFixedSize(300, 200);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QFormLayout* form = new QFormLayout();

    oldPasswordEdit_ = new QLineEdit(this);
    oldPasswordEdit_->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Mật khẩu cũ:"), oldPasswordEdit_);

    newPasswordEdit_ = new QLineEdit(this);
    newPasswordEdit_->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Mật khẩu mới:"), newPasswordEdit_);

    confirmPasswordEdit_ = new QLineEdit(this);
    confirmPasswordEdit_->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Xác nhận:"), confirmPasswordEdit_);

    layout->addLayout(form);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &ChangePasswordDialog::onOkClicked);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString ChangePasswordDialog::getOldPassword() const {
    return oldPasswordEdit_->text();
}

QString ChangePasswordDialog::getNewPassword() const {
    return newPasswordEdit_->text();
}

void ChangePasswordDialog::onOkClicked() {
    if (oldPasswordEdit_->text().isEmpty()) {
        QMessageBox::warning(this, tr("Lỗi"), tr("Vui lòng nhập mật khẩu cũ"));
        return;
    }

    if (newPasswordEdit_->text().length() < 4) {
        QMessageBox::warning(this, tr("Lỗi"), tr("Mật khẩu mới phải có ít nhất 4 ký tự"));
        return;
    }

    if (newPasswordEdit_->text() != confirmPasswordEdit_->text()) {
        QMessageBox::warning(this, tr("Lỗi"), tr("Mật khẩu xác nhận không khớp"));
        return;
    }

    accept();
}

// ============ MainWindow Implementation ============

MainWindow::MainWindow(ChatClient* client, QWidget* parent)
    : QMainWindow(parent)
    , client_(client) {

    setupUI();
    setupMenuBar();

    // Connect client signals
    connect(client_, &ChatClient::disconnected, this, &MainWindow::onDisconnected);
    connect(client_, &ChatClient::globalMessageReceived, this, &MainWindow::onGlobalMessageReceived);
    connect(client_, &ChatClient::privateMessageReceived, this, &MainWindow::onPrivateMessageReceived);
    connect(client_, &ChatClient::onlineListReceived, this, &MainWindow::onOnlineListReceived);
    connect(client_, &ChatClient::userOnline, this, &MainWindow::onUserOnline);
    connect(client_, &ChatClient::userOffline, this, &MainWindow::onUserOffline);
    connect(client_, &ChatClient::errorReceived, this, &MainWindow::onErrorReceived);
    connect(client_, &ChatClient::logoutSuccess, this, [this]() {
        emit loggedOut();
    });
    connect(client_, &ChatClient::passwordChangeSuccess, this, &MainWindow::onPasswordChangeSuccess);
    connect(client_, &ChatClient::passwordChangeFailed, this, &MainWindow::onPasswordChangeFailed);

    // Admin signals
    connect(client_, &ChatClient::kicked, this, &MainWindow::onKicked);
    connect(client_, &ChatClient::banned, this, &MainWindow::onBanned);
    connect(client_, &ChatClient::muted, this, &MainWindow::onMuted);
    connect(client_, &ChatClient::unmuted, this, &MainWindow::onUnmuted);
    connect(client_, &ChatClient::adminActionSuccess, this, &MainWindow::onAdminActionSuccess);
    connect(client_, &ChatClient::adminActionFailed, this, &MainWindow::onAdminActionFailed);
}

MainWindow::~MainWindow() {
}

void MainWindow::setupUI() {
    setWindowTitle(tr("TCP Chat Client"));
    resize(900, 600);

    // Central widget
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // Top info bar
    QHBoxLayout* topBar = new QHBoxLayout();

    userInfoLabel_ = new QLabel(this);
    userInfoLabel_->setStyleSheet("font-weight: bold; font-size: 14px; color: #333;");
    topBar->addWidget(userInfoLabel_);

    topBar->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet("font-weight: bold;");
    topBar->addWidget(statusLabel_);

    reconnectBtn_ = new QPushButton(tr("Kết nối lại"), this);
    reconnectBtn_->setVisible(false);
    reconnectBtn_->setStyleSheet(
        "QPushButton { background-color: #ff9800; color: white; border: none; "
        "border-radius: 3px; padding: 5px 10px; }"
    );
    connect(reconnectBtn_, &QPushButton::clicked, this, &MainWindow::onReconnectClicked);
    topBar->addWidget(reconnectBtn_);

    mainLayout->addLayout(topBar);

    // Main splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    // Chat area (left)
    chatTabs_ = new QTabWidget(this);
    chatTabs_->setTabsClosable(true);
    chatTabs_->setMovable(true);
    connect(chatTabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    // Global chat tab
    globalChatTab_ = new ChatTab("", true, this);
    chatTabs_->addTab(globalChatTab_, tr("Chat nhóm"));
    // Don't allow closing global tab
    chatTabs_->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
    chatTabs_->tabBar()->setTabButton(0, QTabBar::LeftSide, nullptr);

    connect(globalChatTab_, &ChatTab::sendMessageRequested, this, &MainWindow::onSendGlobalMessage);

    splitter->addWidget(chatTabs_);

    // Online users panel (right)
    QWidget* rightPanel = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* usersLabel = new QLabel(tr("Người dùng online:"), this);
    usersLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #333;");
    rightLayout->addWidget(usersLabel);

    userList_ = new QListWidget(this);
    userList_->setStyleSheet(
        "QListWidget { border: 1px solid #ddd; border-radius: 5px; }"
        "QListWidget::item { padding: 8px; }"
        "QListWidget::item:hover { background-color: #e3f2fd; }"
        "QListWidget::item:selected { background-color: #bbdefb; }"
    );
    userList_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(userList_, &QListWidget::itemDoubleClicked, this, &MainWindow::onUserDoubleClicked);
    connect(userList_, &QListWidget::customContextMenuRequested, this, &MainWindow::onUserListContextMenu);
    rightLayout->addWidget(userList_);

    QLabel* hintLabel = new QLabel(tr("(Double-click để chat riêng)"), this);
    hintLabel->setStyleSheet("color: #888; font-size: 11px;");
    hintLabel->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(hintLabel);

    splitter->addWidget(rightPanel);

    // Set splitter sizes
    splitter->setSizes({700, 200});
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    mainLayout->addWidget(splitter);
}

void MainWindow::setupMenuBar() {
    QMenuBar* menuBar = this->menuBar();

    // Account menu
    QMenu* accountMenu = menuBar->addMenu(tr("Tài khoản"));

    QAction* changePasswordAction = accountMenu->addAction(tr("Đổi mật khẩu"));
    connect(changePasswordAction, &QAction::triggered, this, &MainWindow::onChangePasswordClicked);

    accountMenu->addSeparator();

    QAction* logoutAction = accountMenu->addAction(tr("Đăng xuất"));
    connect(logoutAction, &QAction::triggered, this, &MainWindow::onLogoutClicked);

    // Help menu
    QMenu* helpMenu = menuBar->addMenu(tr("Trợ giúp"));

    QAction* aboutAction = helpMenu->addAction(tr("Giới thiệu"));
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, tr("Giới thiệu"),
            tr("TCP Chat Client v1.0\n\n"
               "Ứng dụng chat client-server sử dụng TCP Socket.\n"
               "Hỗ trợ chat nhóm và chat riêng."));
    });
}

void MainWindow::showAndReset() {
    // Update user info
    userInfoLabel_->setText(tr("Đã đăng nhập: %1").arg(client_->currentUsername()));
    updateConnectionStatus();

    // Clear chat history
    // Note: We keep chat history for now, only clear if needed

    show();
}

void MainWindow::updateConnectionStatus() {
    if (client_->isConnected()) {
        if (client_->isAuthenticated()) {
            statusLabel_->setText(tr("Trạng thái: Đã kết nối"));
            statusLabel_->setStyleSheet("color: green; font-weight: bold;");
        } else {
            statusLabel_->setText(tr("Trạng thái: Chưa đăng nhập"));
            statusLabel_->setStyleSheet("color: orange; font-weight: bold;");
        }
        reconnectBtn_->setVisible(false);
    } else {
        statusLabel_->setText(tr("Trạng thái: Mất kết nối"));
        statusLabel_->setStyleSheet("color: red; font-weight: bold;");
        reconnectBtn_->setVisible(true);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    int result = QMessageBox::question(this, tr("Xác nhận"),
        tr("Bạn có chắc muốn thoát?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result == QMessageBox::Yes) {
        if (client_->isAuthenticated()) {
            client_->logout();
        }
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::onDisconnected() {
    updateConnectionStatus();
    QMessageBox::warning(this, tr("Mất kết nối"),
        tr("Kết nối đến server đã bị ngắt."));
}

void MainWindow::onGlobalMessageReceived(const QString& sender, const QString& content, const QString& timestamp) {
    bool isOwn = (sender == client_->currentUsername());
    globalChatTab_->appendMessage(sender, content, timestamp, isOwn);

    // Flash tab if not active
    if (chatTabs_->currentWidget() != globalChatTab_) {
        chatTabs_->setTabText(0, tr("Chat nhóm *"));
    }
}

void MainWindow::onPrivateMessageReceived(const QString& sender, const QString& receiver,
                                          const QString& content, const QString& timestamp) {
    // Determine the other party
    QString otherUser = (sender == client_->currentUsername()) ? receiver : sender;
    bool isOwn = (sender == client_->currentUsername());

    // Find or create private chat tab
    ChatTab* tab = findPrivateChatTab(otherUser);
    if (!tab) {
        openPrivateChat(otherUser);
        tab = findPrivateChatTab(otherUser);
    }

    if (tab) {
        tab->appendMessage(sender, content, timestamp, isOwn);

        // Flash tab if not active
        if (chatTabs_->currentWidget() != tab) {
            int index = chatTabs_->indexOf(tab);
            chatTabs_->setTabText(index, otherUser + " *");
        }
    }
}

void MainWindow::onOnlineListReceived(const QStringList& users) {
    onlineUsers_ = users;
    userList_->clear();

    for (const QString& user : users) {
        if (user != client_->currentUsername()) {
            QListWidgetItem* item = new QListWidgetItem(user, userList_);
            item->setIcon(QIcon::fromTheme("user-available", QIcon(":/icons/user.png")));
        }
    }
}

void MainWindow::onUserOnline(const QString& username) {
    if (username == client_->currentUsername()) {
        return;
    }

    if (!onlineUsers_.contains(username)) {
        onlineUsers_.append(username);
        QListWidgetItem* item = new QListWidgetItem(username, userList_);
        item->setIcon(QIcon::fromTheme("user-available"));

        // Notify in global chat
        globalChatTab_->appendMessage(tr("Hệ thống"),
            tr("%1 đã online").arg(username),
            QDateTime::currentDateTime().toString("hh:mm:ss"), false);
    }
}

void MainWindow::onUserOffline(const QString& username) {
    onlineUsers_.removeAll(username);

    // Remove from list widget
    for (int i = 0; i < userList_->count(); ++i) {
        if (userList_->item(i)->text() == username) {
            delete userList_->takeItem(i);
            break;
        }
    }

    // Notify in global chat
    globalChatTab_->appendMessage(tr("Hệ thống"),
        tr("%1 đã offline").arg(username),
        QDateTime::currentDateTime().toString("hh:mm:ss"), false);
}

void MainWindow::onErrorReceived(const QString& error) {
    QMessageBox::warning(this, tr("Lỗi"), error);
}

void MainWindow::onLogoutClicked() {
    int result = QMessageBox::question(this, tr("Đăng xuất"),
        tr("Bạn có chắc muốn đăng xuất?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result == QMessageBox::Yes) {
        client_->logout();
    }
}

void MainWindow::onChangePasswordClicked() {
    ChangePasswordDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        client_->changePassword(dialog.getOldPassword(), dialog.getNewPassword());
    }
}

void MainWindow::onUserDoubleClicked(QListWidgetItem* item) {
    QString username = item->text();
    openPrivateChat(username);
}

void MainWindow::onReconnectClicked() {
    // Emit signal to show login dialog again
    emit loggedOut();
}

void MainWindow::onTabCloseRequested(int index) {
    // Don't allow closing global tab (index 0)
    if (index > 0) {
        QWidget* widget = chatTabs_->widget(index);
        chatTabs_->removeTab(index);
        delete widget;
    }
}

void MainWindow::onPasswordChangeSuccess() {
    QMessageBox::information(this, tr("Thành công"),
        tr("Đổi mật khẩu thành công!"));
}

void MainWindow::onPasswordChangeFailed(const QString& error) {
    QMessageBox::warning(this, tr("Lỗi"), error);
}

void MainWindow::onSendGlobalMessage(const QString& content) {
    client_->sendGlobalMessage(content);
}

void MainWindow::onSendPrivateMessage(const QString& recipient, const QString& content) {
    client_->sendPrivateMessage(recipient, content);
}

void MainWindow::openPrivateChat(const QString& username) {
    // Check if tab already exists
    ChatTab* existing = findPrivateChatTab(username);
    if (existing) {
        chatTabs_->setCurrentWidget(existing);
        return;
    }

    // Create new tab
    ChatTab* tab = new ChatTab(username, false, this);
    int index = chatTabs_->addTab(tab, username);
    chatTabs_->setCurrentIndex(index);

    // Connect send signal
    connect(tab, &ChatTab::sendMessageRequested, [this, username](const QString& content) {
        onSendPrivateMessage(username, content);
    });
}

ChatTab* MainWindow::findPrivateChatTab(const QString& username) {
    for (int i = 1; i < chatTabs_->count(); ++i) {
        ChatTab* tab = qobject_cast<ChatTab*>(chatTabs_->widget(i));
        if (tab && tab->getRecipient() == username) {
            return tab;
        }
    }
    return nullptr;
}

// ========== Admin Functions ==========

void MainWindow::onKicked(const QString& reason) {
    QMessageBox::warning(this, tr("Bị đuổi"),
        tr("Bạn đã bị đuổi khỏi server.\n%1").arg(reason));
    emit loggedOut();
}

void MainWindow::onBanned(const QString& reason) {
    QMessageBox::critical(this, tr("Bị cấm"),
        tr("Tài khoản của bạn đã bị cấm.\n%1").arg(reason));
    emit loggedOut();
}

void MainWindow::onMuted(const QString& reason) {
    QMessageBox::warning(this, tr("Bị cấm chat"),
        tr("Bạn đã bị cấm gửi tin nhắn.\n%1").arg(reason));
    globalChatTab_->appendMessage(tr("Hệ thống"),
        tr("Bạn đã bị cấm gửi tin nhắn"),
        QDateTime::currentDateTime().toString("hh:mm:ss"), false);
}

void MainWindow::onUnmuted(const QString& reason) {
    QMessageBox::information(this, tr("Được bỏ cấm chat"),
        tr("Bạn đã được bỏ cấm gửi tin nhắn.\n%1").arg(reason));
    globalChatTab_->appendMessage(tr("Hệ thống"),
        tr("Bạn đã được bỏ cấm gửi tin nhắn"),
        QDateTime::currentDateTime().toString("hh:mm:ss"), false);
}

void MainWindow::onAdminActionSuccess(const QString& message) {
    QMessageBox::information(this, tr("Thành công"), message);
}

void MainWindow::onAdminActionFailed(const QString& error) {
    QMessageBox::warning(this, tr("Lỗi"), error);
}

void MainWindow::onUserListContextMenu(const QPoint& pos) {
    QListWidgetItem* item = userList_->itemAt(pos);
    if (!item) return;

    QString username = item->text();

    QMenu menu(this);

    // Chat action (always available)
    QAction* chatAction = menu.addAction(tr("Chat riêng"));
    connect(chatAction, &QAction::triggered, [this, username]() {
        openPrivateChat(username);
    });

    // Admin actions (only if current user is admin)
    if (client_->isAdmin()) {
        menu.addSeparator();

        QAction* kickAction = menu.addAction(tr("Kick (Đuổi)"));
        connect(kickAction, &QAction::triggered, [this, username]() {
            int result = QMessageBox::question(this, tr("Xác nhận"),
                tr("Bạn có chắc muốn đuổi %1?").arg(username),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (result == QMessageBox::Yes) {
                client_->kickUser(username);
            }
        });

        QAction* banAction = menu.addAction(tr("Ban (Cấm)"));
        connect(banAction, &QAction::triggered, [this, username]() {
            int result = QMessageBox::question(this, tr("Xác nhận"),
                tr("Bạn có chắc muốn cấm %1?\nUser sẽ không thể đăng nhập lại.").arg(username),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (result == QMessageBox::Yes) {
                client_->banUser(username);
            }
        });

        QAction* muteAction = menu.addAction(tr("Mute (Cấm chat)"));
        connect(muteAction, &QAction::triggered, [this, username]() {
            client_->muteUser(username);
        });

        QAction* unmuteAction = menu.addAction(tr("Unmute (Bỏ cấm chat)"));
        connect(unmuteAction, &QAction::triggered, [this, username]() {
            client_->unmuteUser(username);
        });
    }

    menu.exec(userList_->mapToGlobal(pos));
}

void MainWindow::showAdminPanel() {
    // Can be expanded later for a full admin panel dialog
}
