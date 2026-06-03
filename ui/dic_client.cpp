#include "dic_client.hpp"

#include <QtCore/QByteArray>
#include <QtCore/QMetaType>
#include <QtNetwork/QNetworkProxy>

#include <cstring>

DicClient::DicClient(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<DicProtocol::ReceivedMessage>();

    socket_ = new QTcpSocket(this);
    socket_->setProxy(QNetworkProxy::NoProxy);
    connect(socket_, &QTcpSocket::connected, this, &DicClient::onConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &DicClient::onDisconnected);
    connect(socket_, &QTcpSocket::readyRead, this, &DicClient::onReadyRead);
    connect(socket_, &QTcpSocket::errorOccurred, this, &DicClient::onSocketError);
}

void DicClient::connectToServer(const QString &host, quint16 port)
{
    host_ = host;
    port_ = port;

    if (socket_->state() == QAbstractSocket::ConnectedState)
    {
        if (socket_->peerName() == host && socket_->peerPort() == port)
        {
            return;
        }
        socket_->disconnectFromHost();
    }

    if (socket_->state() == QAbstractSocket::ConnectingState)
    {
        socket_->abort();
    }

    socket_->connectToHost(host, port);
}

void DicClient::disconnectFromServer()
{
    pendingWrites_.clear();
    buffer_.clear();
    if (socket_->state() == QAbstractSocket::ConnectedState || socket_->state() == QAbstractSocket::ConnectingState)
    {
        socket_->disconnectFromHost();
    }
}

bool DicClient::isConnected() const
{
    return socket_->state() == QAbstractSocket::ConnectedState;
}

QString DicClient::host() const
{
    return host_;
}

quint16 DicClient::port() const
{
    return port_;
}

void DicClient::sendMsg(int type, const QString &name, const QString &text)
{
    const DicProtocol::Msg outgoing = DicProtocol::makeOutgoingMsg(type, name, text);
    QByteArray bytes(reinterpret_cast<const char *>(&outgoing), static_cast<int>(sizeof(outgoing)));

    if (socket_->state() == QAbstractSocket::ConnectedState)
    {
        socket_->write(bytes);
        return;
    }

    pendingWrites_.push_back(bytes);
    if (!host_.isEmpty() && port_ != 0 && socket_->state() == QAbstractSocket::UnconnectedState)
    {
        socket_->connectToHost(host_, port_);
    }
}

void DicClient::onConnected()
{
    flushPendingWrites();
    emit connected();
}

void DicClient::onDisconnected()
{
    pendingWrites_.clear();
    buffer_.clear();
    emit disconnected();
}

void DicClient::onReadyRead()
{
    buffer_.append(socket_->readAll());

    while (buffer_.size() >= static_cast<int>(sizeof(DicProtocol::Msg)))
    {
        const QByteArray chunk = buffer_.left(static_cast<int>(sizeof(DicProtocol::Msg)));
        buffer_.remove(0, static_cast<int>(sizeof(DicProtocol::Msg)));

        DicProtocol::Msg raw{};
        std::memcpy(&raw, chunk.constData(), sizeof(raw));

        DicProtocol::ReceivedMessage msg;
        msg.type = DicProtocol::hostTypeFromNetwork(raw.type);
        msg.name = DicProtocol::readFixedString(raw.name, static_cast<int>(sizeof(raw.name)));
        msg.text = DicProtocol::readFixedString(raw.text, static_cast<int>(sizeof(raw.text)));

        emit messageReceived(msg);
    }
}

void DicClient::onSocketError(QAbstractSocket::SocketError)
{
    emit errorOccurred(socket_->errorString());
}

void DicClient::flushPendingWrites()
{
    if (socket_->state() != QAbstractSocket::ConnectedState)
    {
        return;
    }
    for (const QByteArray &bytes : pendingWrites_)
    {
        socket_->write(bytes);
    }
    pendingWrites_.clear();
}

