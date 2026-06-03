#include "chat_dialog.hpp"

#include <QtCore/QProcessEnvironment>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <algorithm>

ChatDialog::ChatDialog(const QString &username, QWidget *parent) : QDialog(parent), username_(username)
{
    setWindowTitle("和宠物聊天");
    setMinimumSize(520, 420);

    qwen_ = new QwenClient(this);
    connect(qwen_, &QwenClient::responseReady, this, &ChatDialog::onModelReply);
    connect(qwen_, &QwenClient::errorOccurred, this, &ChatDialog::onModelError);

    QString apiKey = QProcessEnvironment::systemEnvironment().value("DASHSCOPE_API_KEY").trimmed();
    if (apiKey.isEmpty())
    {
        bool ok = false;
        apiKey = QInputDialog::getText(this,
                                       "配置千问 Key",
                                       "未在当前进程读取到 DASHSCOPE_API_KEY。\n可以直接在这里粘贴 Key（仅本次运行使用）：",
                                       QLineEdit::Password,
                                       "",
                                       &ok)
                     .trimmed();
        if (!ok)
        {
            QMessageBox::information(this, "提示", "如果你已在系统环境变量里配置了 Key，请关闭并重新启动程序/IDE 后再打开聊天。");
        }
    }
    qwen_->setApiKey(apiKey);

    chatView_ = new QTextEdit(this);
    chatView_->setReadOnly(true);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText("输入你想对宠物说的话…");

    sendBtn_ = new QPushButton("发送", this);
    sendBtn_->setProperty("kind", "primary");
    sendBtn_->setMinimumHeight(36);
    connect(sendBtn_, &QPushButton::clicked, this, &ChatDialog::onSendClicked);
    connect(input_, &QLineEdit::returnPressed, this, &ChatDialog::onSendClicked);

    auto *bottom = new QHBoxLayout();
    bottom->addWidget(input_, 1);
    bottom->addWidget(sendBtn_);

    auto *root = new QVBoxLayout();
    root->addWidget(chatView_, 1);
    root->addLayout(bottom);
    setLayout(root);

    const QString sys = QString("你是一个桌面宠物伙伴，名字叫“小词灵”。你要用简短、温暖的语气和用户聊天，同时鼓励他背单词。"
                                "用户当前登录名是 %1。当前宠物状态：LV %2 EXP %3。"
                                "你可以偶尔给出 1-3 个可执行的学习建议，尽量不啰嗦。")
                            .arg(username_)
                            .arg(level_)
                            .arg(exp_);
    history_.push_back({"system", sys});

    if (apiKey.isEmpty())
    {
        appendLine("系统", "千问未就绪：未读取到 DASHSCOPE_API_KEY。");
    }
    else
    {
        appendLine("系统", QString("千问已就绪（Key 长度 %1）").arg(apiKey.size()));
    }
    appendLine("宠物", "我在呢。点我聊天，双击我打开主界面。");
}

void ChatDialog::setPetInfo(int level, int exp)
{
    level_ = std::max(1, level);
    exp_ = std::max(0, exp);

    if (!history_.isEmpty() && history_.first().role == "system")
    {
        history_.first().content = QString("你是一个桌面宠物伙伴，名字叫“小词灵”。你要用简短、温暖的语气和用户聊天，同时鼓励他背单词。"
                                           "用户当前登录名是 %1。当前宠物状态：LV %2 EXP %3。"
                                           "你可以偶尔给出 1-3 个可执行的学习建议，尽量不啰嗦。")
                                       .arg(username_)
                                       .arg(level_)
                                       .arg(exp_);
    }
}

void ChatDialog::onSendClicked()
{
    const QString text = input_->text().trimmed();
    if (text.isEmpty() || !qwen_)
    {
        return;
    }

    appendLine("我", text);
    history_.push_back({"user", text});
    input_->clear();
    setBusy(true);
    qwen_->chat(history_);
}

void ChatDialog::onModelReply(const QString &text)
{
    appendLine("宠物", text);
    history_.push_back({"assistant", text});
    setBusy(false);
}

void ChatDialog::onModelError(const QString &message)
{
    appendLine("系统", message);
    setBusy(false);
}

void ChatDialog::appendLine(const QString &speaker, const QString &text)
{
    chatView_->append(QString("[%1] %2").arg(speaker, text));
}

void ChatDialog::setBusy(bool busy)
{
    input_->setEnabled(!busy);
    sendBtn_->setEnabled(!busy);
}
