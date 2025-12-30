/**
 * @file AuthDialog.h
 * @brief Login and Registration dialog
 */

#ifndef AUTHDIALOG_H
#define AUTHDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QFormLayout>

class ChatClient;

class AuthDialog : public QDialog {
    Q_OBJECT

public:
    explicit AuthDialog(ChatClient* client, QWidget* parent = nullptr);
    ~AuthDialog();

    QString getServerHost() const { return hostEdit_->text(); }
    quint16 getServerPort() const { return portEdit_->text().toUShort(); }

signals:
    void loginSuccessful();

private slots:
    void onConnectClicked();
    void onLoginClicked();
    void onRegisterClicked();
    void onConnected();
    void onDisconnected();
    void onConnectionError(const QString& error);
    void onLoginSuccess(const QString& username, const QString& displayName);
    void onLoginFailed(const QString& error);
    void onRegisterSuccess();
    void onRegisterFailed(const QString& error);

private:
    void setupUI();
    void updateConnectionUI(bool connected);
    void showMessage(QLabel* label, const QString& message, bool isError);
    void clearMessages();

    ChatClient* client_;

    // Connection group
    QLineEdit* hostEdit_;
    QLineEdit* portEdit_;
    QPushButton* connectBtn_;
    QLabel* connectionStatusLabel_;

    // Tab widget
    QTabWidget* tabWidget_;

    // Login tab
    QWidget* loginTab_;
    QLineEdit* loginUsernameEdit_;
    QLineEdit* loginPasswordEdit_;
    QPushButton* loginBtn_;
    QLabel* loginStatusLabel_;

    // Register tab
    QWidget* registerTab_;
    QLineEdit* regUsernameEdit_;
    QLineEdit* regPasswordEdit_;
    QLineEdit* regConfirmPasswordEdit_;
    QPushButton* registerBtn_;
    QLabel* registerStatusLabel_;
};

#endif // AUTHDIALOG_H
