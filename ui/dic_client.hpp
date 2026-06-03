#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QTcpSocket>

#include "dic_protocol.hpp"

class DicClient final : public QObject
{
    Q_OBJECT

public:
    explicit DicClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();

    bool isConnected() const;
    QString host() const;
    quint16 port() const;

    void sendMsg(int type, const QString &name, const QString &text);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void messageReceived(const DicProtocol::ReceivedMessage &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    void flushPendingWrites();

    QTcpSocket *socket_ = nullptr;
    QByteArray buffer_;
    QString host_;
    quint16 port_ = 0;
    QList<QByteArray> pendingWrites_;
};

