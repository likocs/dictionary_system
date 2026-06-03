#pragma once

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtWidgets/QDialog>

#include "qwen_client.hpp"

class QPushButton;
class QTextEdit;
class QLineEdit;

class ChatDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ChatDialog(const QString &username, QWidget *parent = nullptr);

    void setPetInfo(int level, int exp);

private slots:
    void onSendClicked();
    void onModelReply(const QString &text);
    void onModelError(const QString &message);

private:
    void appendLine(const QString &speaker, const QString &text);
    void setBusy(bool busy);

    QString username_;
    int level_ = 1;
    int exp_ = 0;

    QPointer<QwenClient> qwen_;
    QTextEdit *chatView_ = nullptr;
    QLineEdit *input_ = nullptr;
    QPushButton *sendBtn_ = nullptr;

    QList<QwenClient::Message> history_;
};
