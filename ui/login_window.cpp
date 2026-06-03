#include "login_window.hpp"

#include <QtCore/QTimer>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "dic_client.hpp"
#include "dic_protocol.hpp"
#include "main_window.hpp"

LoginWindow::LoginWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("桌面宠物伴学单词系统 - 登录");
    setMinimumSize(520, 260);

    client_ = new DicClient(this);
    connect(client_, &DicClient::connected, this, &LoginWindow::onClientConnected);
    connect(client_, &DicClient::disconnected, this, &LoginWindow::onClientDisconnected);
    connect(client_, &DicClient::errorOccurred, this, &LoginWindow::onClientError);
    connect(client_, &DicClient::messageReceived, this, &LoginWindow::onClientMessage);

    hostEdit_ = new QLineEdit(this);
    hostEdit_->setText("192.168.124.132");

    portEdit_ = new QLineEdit(this);
    portEdit_->setText("9090");

    userEdit_ = new QLineEdit(this);
    userEdit_->setPlaceholderText("用户名（<= 19 字节）");

    passEdit_ = new QLineEdit(this);
    passEdit_->setPlaceholderText("密码（<= 127 字节）");
    passEdit_->setEchoMode(QLineEdit::Password);

    connectBtn_ = new QPushButton("连接服务器", this);
    registerBtn_ = new QPushButton("注册", this);
    loginBtn_ = new QPushButton("登录", this);
    connectBtn_->setProperty("kind", "secondary");
    registerBtn_->setProperty("kind", "secondary");
    loginBtn_->setProperty("kind", "primary");
    connectBtn_->setMinimumHeight(36);
    registerBtn_->setMinimumHeight(36);
    loginBtn_->setMinimumHeight(36);

    connect(connectBtn_, &QPushButton::clicked, this, &LoginWindow::onConnectClicked);
    connect(registerBtn_, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
    connect(loginBtn_, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);

    statusLabel_ = new QLabel(this);
    statusLabel_->setText("未连接");
    statusLabel_->setProperty("role", "subtitle");

    auto *title = new QLabel("桌面宠物伴学单词系统", this);
    title->setProperty("role", "title");
    auto *subtitle = new QLabel("连接服务器后登录/注册，开始和桌宠一起背单词。", this);
    subtitle->setProperty("role", "subtitle");

    auto *form = new QFormLayout();
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->addRow("服务器 IP：", hostEdit_);
    form->addRow("端口：", portEdit_);
    form->addRow("用户名：", userEdit_);
    form->addRow("密码：", passEdit_);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(connectBtn_);
    btnRow->addStretch(1);
    btnRow->addWidget(registerBtn_);
    btnRow->addWidget(loginBtn_);

    auto *root = new QVBoxLayout();
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);
    root->addWidget(title);
    root->addWidget(subtitle);
    root->addLayout(form);
    root->addLayout(btnRow);
    root->addWidget(statusLabel_);

    setLayout(root);
}

void LoginWindow::onConnectClicked()
{
    pendingOp_ = PendingOp::None;
    requestConnectIfNeeded();
}

void LoginWindow::onRegisterClicked()
{
    if (userEdit_->text().trimmed().isEmpty() || passEdit_->text().isEmpty())
    {
        QMessageBox::warning(this, "提示", "请输入用户名与密码");
        return;
    }

    pendingOp_ = PendingOp::Register;
    requestConnectIfNeeded();

    if (client_->isConnected())
    {
        client_->sendMsg(DicProtocol::R, userEdit_->text().trimmed(), passEdit_->text());
        showStatus("已发送注册请求…");
        pendingOp_ = PendingOp::None;
    }
}

void LoginWindow::onLoginClicked()
{
    if (userEdit_->text().trimmed().isEmpty() || passEdit_->text().isEmpty())
    {
        QMessageBox::warning(this, "提示", "请输入用户名与密码");
        return;
    }

    pendingOp_ = PendingOp::Login;
    requestConnectIfNeeded();

    if (client_->isConnected())
    {
        client_->sendMsg(DicProtocol::L, userEdit_->text().trimmed(), passEdit_->text());
        showStatus("已发送登录请求…");
        pendingOp_ = PendingOp::None;
    }
}

void LoginWindow::onClientConnected()
{
    showStatus(QString("已连接 %1:%2").arg(client_->host()).arg(client_->port()));

    if (pendingOp_ == PendingOp::Register)
    {
        client_->sendMsg(DicProtocol::R, userEdit_->text().trimmed(), passEdit_->text());
        showStatus("已发送注册请求…");
        pendingOp_ = PendingOp::None;
    }
    else if (pendingOp_ == PendingOp::Login)
    {
        client_->sendMsg(DicProtocol::L, userEdit_->text().trimmed(), passEdit_->text());
        showStatus("已发送登录请求…");
        pendingOp_ = PendingOp::None;
    }
}

void LoginWindow::onClientDisconnected()
{
    showStatus("已断开连接");
}

void LoginWindow::onClientError(const QString &message)
{
    showStatus("网络错误：" + message);
}

void LoginWindow::onClientMessage(const DicProtocol::ReceivedMessage &message)
{
    if (message.type == DicProtocol::R)
    {
        if (message.text == "**OK**")
        {
            QMessageBox::information(this, "注册成功", "注册成功，现在可以登录了。");
        }
        else if (message.text == "**EXISTS**")
        {
            QMessageBox::warning(this, "注册失败", "用户名已存在。");
        }
        else
        {
            QMessageBox::warning(this, "注册失败", message.text);
        }
        return;
    }

    if (message.type == DicProtocol::L)
    {
        if (message.text == "**OK**")
        {
            const QString username = userEdit_->text().trimmed();

            if (!mainWindow_)
            {
                mainWindow_ = new MainWindow(client_, username);
                mainWindow_->setAttribute(Qt::WA_DeleteOnClose, true);
                connect(mainWindow_, &QObject::destroyed, this, [this]()
                        { mainWindow_ = nullptr; show(); });
            }

            mainWindow_->show();
            hide();
            return;
        }

        if (message.text == "**EXISTS**")
        {
            QMessageBox::warning(this, "登录失败", "该账号已在线。");
            return;
        }

        QMessageBox::warning(this, "登录失败", "账号或密码错误。");
        return;
    }
}

void LoginWindow::setUiEnabled(bool enabled)
{
    hostEdit_->setEnabled(enabled);
    portEdit_->setEnabled(enabled);
    userEdit_->setEnabled(enabled);
    passEdit_->setEnabled(enabled);
    connectBtn_->setEnabled(enabled);
    registerBtn_->setEnabled(enabled);
    loginBtn_->setEnabled(enabled);
}

void LoginWindow::showStatus(const QString &text)
{
    statusLabel_->setText(text);
}

void LoginWindow::requestConnectIfNeeded()
{
    const QString host = hostEdit_->text().trimmed();
    const quint16 port = static_cast<quint16>(portEdit_->text().trimmed().toUShort());
    if (host.isEmpty() || port == 0)
    {
        QMessageBox::warning(this, "提示", "请输入正确的服务器 IP 与端口");
        return;
    }

    setUiEnabled(false);
    client_->connectToServer(host, port);

    QTimer::singleShot(800, this, [this]()
                       { setUiEnabled(true); });
}
