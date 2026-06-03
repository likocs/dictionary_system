#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QtEndian>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace DicProtocol
{
constexpr int R = 1;
constexpr int L = 2;
constexpr int Q = 3;
constexpr int S = 4;
constexpr int H = 5;
constexpr int P = 6;
constexpr int B = 7;
constexpr int M = 8;
constexpr int D = 9;
constexpr int N = 10;

struct Msg
{
    qint32 type;
    char name[20];
    char text[128];
};

static_assert(sizeof(Msg) == 152);

struct ReceivedMessage
{
    int type = 0;
    QString name;
    QString text;
};

inline QString readFixedString(const char *buf, int capacity)
{
    int n = 0;
    while (n < capacity && buf[n] != '\0')
    {
        ++n;
    }
    return QString::fromUtf8(QByteArray(buf, n));
}

inline void writeFixedString(char *dst, int capacity, const QString &src)
{
    std::memset(dst, 0, static_cast<size_t>(capacity));
    const QByteArray bytes = src.toUtf8();
    const int n = std::min(capacity - 1, static_cast<int>(bytes.size()));
    if (n > 0)
    {
        std::memcpy(dst, bytes.constData(), static_cast<size_t>(n));
    }
}

inline Msg makeOutgoingMsg(int hostType, const QString &name, const QString &text)
{
    Msg m{};
    m.type = static_cast<qint32>(qToBigEndian(static_cast<quint32>(hostType)));
    writeFixedString(m.name, static_cast<int>(sizeof(m.name)), name);
    writeFixedString(m.text, static_cast<int>(sizeof(m.text)), text);
    return m;
}

inline int hostTypeFromNetwork(qint32 networkType)
{
    return static_cast<int>(qFromBigEndian(static_cast<quint32>(networkType)));
}
}

Q_DECLARE_METATYPE(DicProtocol::ReceivedMessage)
