/**
 * @file AuthDialog.cpp
 * @brief Implementation of Login and Registration dialog
 */

#include "AuthDialog.h"
#include "ChatClient.h"
#include <QMessageBox>
#include <QGroupBox>
#include <QHBoxLayout>

AuthDialog::AuthDialog(ChatClient* client, QWidget* parent)
    : QDialog(parent)
    , client_(client) {
    setupUI();

    // Connect client signals
    connect(client_, &ChatClient::connected, this, &AuthDialog::onConnected);
    connect(client_, &ChatClient::disconnected, this, &AuthDialog::onDisconnected);
    connect(client_, &ChatClient::connectionError, this, &AuthDialog::onConnectionError);
    connect(client_, &ChatClient::loginSuccess, this, &AuthDialog::onLoginSuccess);
    connect(client_, &ChatClient::loginFailed, this, &AuthDialog::onLoginFailed);
    connect(client_, &ChatClient::registerSuccess, this, &AuthDialog::onRegisterSuccess);
    connect(client_, &ChatClient::registerFailed, this, &AuthDialog::onRegisterFailed);
}

AuthDialog::~AuthDialog() {
}

void AuthDialog::setupUI() {
    setWindowTitle(tr("TCP Chat - Đăng nhập"));
    setFixedSize(400, 450);
    setModal(true);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // === Connection Group ===
    QGroupBox* connectionGroup = new QGroupBox(tr("Kết nối Server"), this);
    QVBoxLayout* connLayout = new QVBoxLayout(connectionGroup);

    QHBoxLayout* hostLayout = new QHBoxLayout();
    hostLayout->addWidget(new QLabel(tr("Server:"), this));
    hostEdit_ = new QLineEdit(this);
    hostEdit_->setText("127.0.0.1");
    hostEdit_->setPlaceholderText("IP hoặc hostname");
    hostLayout->addWidget(hostEdit_);

    hostLayout->addWidget(new QLabel(tr("Port:"), this));
    portEdit_ = new QLineEdit(this);
    portEdit_->setText("9000");
    portEdit_->setFixedWidth(60);
    portEdit_->setMaxLength(5);
    hostLayout->addWidget(portEdit_);

    connLayout->addLayout(hostLayout);

    QHBoxLayout* connBtnLayout = new QHBoxLayout();
    connectBtn_ = new QPushButton(tr("Kết nối"), this);
    connectBtn_->setMinimumHeight(35);
    connBtnLayout->addWidget(connectBtn_);
    connLayout->addLayout(connBtnLayout);

    connectionStatusLabel_ = new QLabel(this);
    connectionStatusLabel_->setAlignment(Qt::AlignCenter);
    connectionStatusLabel_->setStyleSheet("font-weight: bold;");
    connLayout->addWidget(connectionStatusLabel_);

    mainLayout->addWidget(connectionGroup);

    // === Tab Widget for Login/Register ===
    tabWidget_ = new QTabWidget(this);
    tabWidget_->setEnabled(false);  // Disabled until connected

    // --- Login Tab ---
    loginTab_ = new QWidget();
    QVBoxLayout* loginLayout = new QVBoxLayout(loginTab_);

    QFormLayout* loginForm = new QFormLayout();
    loginUsernameEdit_ = new QLineEdit(loginTab_);
    loginUsernameEdit_->setPlaceholderText(tr("Nhập tên đăng nhập"));
    loginForm->addRow(tr("Tên đăng nhập:"), loginUsernameEdit_);

    loginPasswordEdit_ = new QLineEdit(loginTab_);
    loginPasswordEdit_->setEchoMode(QLineEdit::Password);
    loginPasswordEdit_->setPlaceholderText(tr("Nhập mật khẩu"));
    loginForm->addRow(tr("Mật khẩu:"), loginPasswordEdit_);

    loginLayout->addLayout(loginForm);

    loginBtn_ = new QPushButton(tr("Đăng nhập"), loginTab_);
    loginBtn_->setMinimumHeight(40);
    loginBtn_->setStyleSheet("font-weight: bold; font-size: 14px;");
    loginLayout->addWidget(loginBtn_);

    loginStatusLabel_ = new QLabel(loginTab_);
    loginStatusLabel_->setAlignment(Qt::AlignCenter);
    loginStatusLabel_->setWordWrap(true);
    loginLayout->addWidget(loginStatusLabel_);

    loginLayout->addStretch();
    tabWidget_->addTab(loginTab_, tr("Đăng nhập"));

    // --- Register Tab ---
    registerTab_ = new QWidget();
    QVBoxLayout* registerLayout = new QVBoxLayout(registerTab_);

    QFormLayout* regForm = new QFormLayout();
    regUsernameEdit_ = new QLineEdit(registerTab_);
    regUsernameEdit_->setPlaceholderText(tr("3-20 ký tự"));
    regForm->addRow(tr("Tên đăng nhập:"), regUsernameEdit_);

    regPasswordEdit_ = new QLineEdit(registerTab_);
    regPasswordEdit_->setEchoMode(QLineEdit::Password);
    regPasswordEdit_->setPlaceholderText(tr("Ít nhất 4 ký tự"));
    regForm->addRow(tr("Mật khẩu:"), regPasswordEdit_);

    regConfirmPasswordEdit_ = new QLineEdit(registerTab_);
    regConfirmPasswordEdit_->setEchoMode(QLineEdit::Password);
    regConfirmPasswordEdit_->setPlaceholderText(tr("Nhập lại mật khẩu"));
    regForm->addRow(tr("Xác nhận:"), regConfirmPasswordEdit_);

    registerLayout->addLayout(regForm);

    registerBtn_ = new QPushButton(tr("Đăng ký"), registerTab_);
    registerBtn_->setMinimumHeight(40);
    registerBtn_->setStyleSheet("font-weight: bold; font-size: 14px;");
    registerLayout->addWidget(registerBtn_);

    registerStatusLabel_ = new QLabel(registerTab_);
    registerStatusLabel_->setAlignment(Qt::AlignCenter);
    registerStatusLabel_->setWordWrap(true);
    registerLayout->addWidget(registerStatusLabel_);

    registerLayout->addStretch();
    tabWidget_->addTab(registerTab_, tr("Đăng ký"));

    mainLayout->addWidget(tabWidget_);

    // Connect button signals
    connect(connectBtn_, &QPushButton::clicked, this, &AuthDialog::onConnectClicked);
    connect(loginBtn_, &QPushButton::clicked, this, &AuthDialog::onLoginClicked);
    connect(registerBtn_, &QPushButton::clicked, this, &AuthDialog::onRegisterClicked);

    // Enter key support
    connect(loginPasswordEdit_, &QLineEdit::returnPressed, this, &AuthDialog::onLoginClicked);
    connect(regConfirmPasswordEdit_, &QLineEdit::returnPressed, this, &AuthDialog::onRegisterClicked);
}

void AuthDialog::onConnectClicked() {
    clearMessages();

    if (client_->isConnected()) {
        client_->disconnectFromServer();
    } else {
        QString host = hostEdit_->text().trimmed();
        quint16 port = portEdit_->text().toUShort();

        if (host.isEmpty()) {
            showMessage(connectionStatusLabel_, tr("Vui lòng nhập địa chỉ server"), true);
            return;
        }

        if (port == 0) {
            showMessage(connectionStatusLabel_, tr("Port không hợp lệ"), true);
            return;
        }

        connectBtn_->setEnabled(false);
        connectionStatusLabel_->setText(tr("Đang kết nối..."));
        connectionStatusLabel_->setStyleSheet("color: blue; font-weight: bold;");

        client_->connectToServer(host, port);
    }
}

void AuthDialog::onLoginClicked() {
    clearMessages();

    QString username = loginUsernameEdit_->text().trimmed();
    QString password = loginPasswordEdit_->text();

    if (username.isEmpty() || password.isEmpty()) {
        showMessage(loginStatusLabel_, tr("Vui lòng nhập đầy đủ thông tin"), true);
        return;
    }

    loginBtn_->setEnabled(false);
    loginStatusLabel_->setText(tr("Đang đăng nhập..."));
    loginStatusLabel_->setStyleSheet("color: blue;");

    client_->login(username, password);
}

void AuthDialog::onRegisterClicked() {
    clearMessages();

    QString username = regUsernameEdit_->text().trimmed();
    QString password = regPasswordEdit_->text();
    QString confirmPassword = regConfirmPasswordEdit_->text();

    if (username.isEmpty() || password.isEmpty()) {
        showMessage(registerStatusLabel_, tr("Vui lòng nhập đầy đủ thông tin"), true);
        return;
    }

    if (username.length() < 3 || username.length() > 20) {
        showMessage(registerStatusLabel_, tr("Tên đăng nhập phải từ 3-20 ký tự"), true);
        return;
    }

    if (password.length() < 4) {
        showMessage(registerStatusLabel_, tr("Mật khẩu phải có ít nhất 4 ký tự"), true);
        return;
    }

    if (password != confirmPassword) {
        showMessage(registerStatusLabel_, tr("Mật khẩu xác nhận không khớp"), true);
        return;
    }

    registerBtn_->setEnabled(false);
    registerStatusLabel_->setText(tr("Đang đăng ký..."));
    registerStatusLabel_->setStyleSheet("color: blue;");

    client_->registerUser(username, password);
}

void AuthDialog::onConnected() {
    updateConnectionUI(true);
    showMessage(connectionStatusLabel_, tr("Đã kết nối thành công!"), false);
}

void AuthDialog::onDisconnected() {
    updateConnectionUI(false);
    showMessage(connectionStatusLabel_, tr("Đã ngắt kết nối"), true);
}

void AuthDialog::onConnectionError(const QString& error) {
    updateConnectionUI(false);
    showMessage(connectionStatusLabel_, tr("Lỗi kết nối: %1").arg(error), true);
}

void AuthDialog::onLoginSuccess(const QString& username, const QString& displayName) {
    Q_UNUSED(username)
    Q_UNUSED(displayName)
    showMessage(loginStatusLabel_, tr("Đăng nhập thành công!"), false);
    emit loginSuccessful();
    accept();
}

void AuthDialog::onLoginFailed(const QString& error) {
    loginBtn_->setEnabled(true);
    showMessage(loginStatusLabel_, error, true);
}

void AuthDialog::onRegisterSuccess() {
    registerBtn_->setEnabled(true);
    showMessage(registerStatusLabel_, tr("Đăng ký thành công! Hãy đăng nhập."), false);

    // Copy username to login tab
    loginUsernameEdit_->setText(regUsernameEdit_->text());
    tabWidget_->setCurrentWidget(loginTab_);

    // Clear register fields
    regUsernameEdit_->clear();
    regPasswordEdit_->clear();
    regConfirmPasswordEdit_->clear();
}

void AuthDialog::onRegisterFailed(const QString& error) {
    registerBtn_->setEnabled(true);
    showMessage(registerStatusLabel_, error, true);
}

void AuthDialog::updateConnectionUI(bool connected) {
    connectBtn_->setEnabled(true);

    if (connected) {
        connectBtn_->setText(tr("Ngắt kết nối"));
        tabWidget_->setEnabled(true);
        hostEdit_->setEnabled(false);
        portEdit_->setEnabled(false);
    } else {
        connectBtn_->setText(tr("Kết nối"));
        tabWidget_->setEnabled(false);
        hostEdit_->setEnabled(true);
        portEdit_->setEnabled(true);
        loginBtn_->setEnabled(true);
        registerBtn_->setEnabled(true);
    }
}

void AuthDialog::showMessage(QLabel* label, const QString& message, bool isError) {
    label->setText(message);
    if (isError) {
        label->setStyleSheet("color: red; font-weight: bold;");
    } else {
        label->setStyleSheet("color: green; font-weight: bold;");
    }
}

void AuthDialog::clearMessages() {
    loginStatusLabel_->clear();
    registerStatusLabel_->clear();
}
