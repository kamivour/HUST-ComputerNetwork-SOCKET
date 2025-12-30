/**
 * @file MainWindow.h
 * @brief Main chat window with global and private chat support
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QSplitter>
#include <QMap>
#include <QDialog>

class ChatClient;

// Private chat tab widget
class ChatTab : public QWidget {
    Q_OBJECT

public:
    ChatTab(const QString& recipient, bool isGlobal = false, QWidget* parent = nullptr);

    void appendMessage(const QString& sender, const QString& content,
                      const QString& timestamp, bool isOwnMessage = false);
    QString getMessage() const;
    void clearInput();
    QString getRecipient() const { return recipient_; }
    bool isGlobal() const { return isGlobal_; }

signals:
    void sendMessageRequested(const QString& content);

private:
    QString recipient_;
    bool isGlobal_;
    QTextEdit* chatHistory_;
    QLineEdit* messageInput_;
    QPushButton* sendBtn_;
};

// Password change dialog
class ChangePasswordDialog : public QDialog {
    Q_OBJECT

public:
    explicit ChangePasswordDialog(QWidget* parent = nullptr);

    QString getOldPassword() const;
    QString getNewPassword() const;

private slots:
    void onOkClicked();

private:
    QLineEdit* oldPasswordEdit_;
    QLineEdit* newPasswordEdit_;
    QLineEdit* confirmPasswordEdit_;
};

// Main window
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(ChatClient* client, QWidget* parent = nullptr);
    ~MainWindow();

    void showAndReset();

signals:
    void loggedOut();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // Network events
    void onDisconnected();
    void onGlobalMessageReceived(const QString& sender, const QString& content, const QString& timestamp);
    void onPrivateMessageReceived(const QString& sender, const QString& receiver,
                                  const QString& content, const QString& timestamp);
    void onOnlineListReceived(const QStringList& users);
    void onUserOnline(const QString& username);
    void onUserOffline(const QString& username);
    void onErrorReceived(const QString& error);

    // User actions
    void onLogoutClicked();
    void onChangePasswordClicked();
    void onUserDoubleClicked(QListWidgetItem* item);
    void onReconnectClicked();
    void onTabCloseRequested(int index);

    // Password change
    void onPasswordChangeSuccess();
    void onPasswordChangeFailed(const QString& error);

    // Chat
    void onSendGlobalMessage(const QString& content);
    void onSendPrivateMessage(const QString& recipient, const QString& content);

    // Admin
    void onKicked(const QString& reason);
    void onBanned(const QString& reason);
    void onMuted(const QString& reason);
    void onUnmuted(const QString& reason);
    void onAdminActionSuccess(const QString& message);
    void onAdminActionFailed(const QString& error);
    void onUserListContextMenu(const QPoint& pos);
    void showAdminPanel();

private:
    void setupUI();
    void setupMenuBar();
    void updateConnectionStatus();
    void openPrivateChat(const QString& username);
    ChatTab* findPrivateChatTab(const QString& username);

    ChatClient* client_;

    // UI Elements
    QTabWidget* chatTabs_;
    ChatTab* globalChatTab_;
    QListWidget* userList_;
    QLabel* statusLabel_;
    QLabel* userInfoLabel_;
    QPushButton* reconnectBtn_;

    // Online users
    QStringList onlineUsers_;
};

#endif // MAINWINDOW_H
