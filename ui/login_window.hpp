#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class DicClient;
class MainWindow;

namespace DicProtocol
{
struct ReceivedMessage;
}

class LoginWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);

private slots:
    void onConnectClicked();
    void onRegisterClicked();
    void onLoginClicked();

    void onClientConnected();
    void onClientDisconnected();
    void onClientError(const QString &message);
    void onClientMessage(const DicProtocol::ReceivedMessage &message);

private:
    void setUiEnabled(bool enabled);
    void showStatus(const QString &text);
    void requestConnectIfNeeded();

    enum class PendingOp
    {
        None,
        Register,
        Login,
    };

    PendingOp pendingOp_ = PendingOp::None;

    QPointer<DicClient> client_;
    QPointer<MainWindow> mainWindow_;

    QLineEdit *hostEdit_ = nullptr;
    QLineEdit *portEdit_ = nullptr;
    QLineEdit *userEdit_ = nullptr;
    QLineEdit *passEdit_ = nullptr;

    QPushButton *connectBtn_ = nullptr;
    QPushButton *registerBtn_ = nullptr;
    QPushButton *loginBtn_ = nullptr;

    QLabel *statusLabel_ = nullptr;
};
