#include "main_window.hpp"

#include <QtCore/QRandomGenerator>
#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

#include "dic_client.hpp"
#include "dic_protocol.hpp"
#include "chat_dialog.hpp"
#include "desktop_pet_widget.hpp"

namespace
{
    QString normMeaning(const QString &s)
    {
        return s.simplified();
    }
}

MainWindow::MainWindow(DicClient *client, const QString &username, QWidget *parent)
    : QMainWindow(parent), client_(client), username_(username)
{
    setWindowTitle("桌面宠物伴学单词系统");
    setMinimumSize(820, 520);

    setupUi();

    desktopPet_ = new DesktopPetWidget();
    desktopPet_->setAttribute(Qt::WA_DeleteOnClose, true);
    desktopPet_->show();
    desktopPet_->setPetState(DesktopPetWidget::PetState::Init);

    if (tabs_)
    {
        connect(tabs_, &QTabWidget::currentChanged, this, [this](int idx)
                {
                    if (!desktopPet_)
                    {
                        return;
                    }
                    desktopPet_->notifyUserActivity();
                    if (idx == 1)
                    {
                        desktopPet_->setPetState(DesktopPetWidget::PetState::Learning);
                    }
                    else
                    {
                        desktopPet_->setPetState(DesktopPetWidget::PetState::Idle);
                    }
                });
    }

    connect(desktopPet_, &DesktopPetWidget::activated, this, [this]()
            {
                show();
                raise();
                activateWindow();
            });
    connect(desktopPet_, &DesktopPetWidget::chatRequested, this, [this]()
            {
                if (!chatDialog_)
                {
                    chatDialog_ = new ChatDialog(username_, desktopPet_);
                    chatDialog_->setAttribute(Qt::WA_DeleteOnClose, true);
                    connect(chatDialog_, &QObject::destroyed, this, [this]()
                            { chatDialog_ = nullptr; });
                }

                int level = 1;
                int exp = 0;
                if (petInfoLabel_ && parsePetLvExp(petInfoLabel_->text(), level, exp))
                {
                    chatDialog_->setPetInfo(level, exp);
                }
                chatDialog_->show();
                chatDialog_->raise();
                chatDialog_->activateWindow();
            });
    connect(desktopPet_, &DesktopPetWidget::quitRequested, this, [this]()
            { close(); });

    if (client_)
    {
        connect(client_, &DicClient::disconnected, this, &MainWindow::onClientDisconnected);
        connect(client_, &DicClient::errorOccurred, this, &MainWindow::onClientError);
        connect(client_, &DicClient::messageReceived, this, &MainWindow::onClientMessage);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (client_ && client_->isConnected())
    {
        client_->sendMsg(DicProtocol::Q, username_, "");
        client_->disconnectFromServer();
    }
    if (chatDialog_)
    {
        chatDialog_->close();
        chatDialog_ = nullptr;
    }
    if (desktopPet_)
    {
        desktopPet_->close();
        desktopPet_ = nullptr;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::onLogoutClicked()
{
    if (client_ && client_->isConnected())
    {
        client_->sendMsg(DicProtocol::Q, username_, "");
        client_->disconnectFromServer();
    }
    close();
}

void MainWindow::onSearchClicked()
{
    if (!client_ || !client_->isConnected())
    {
        QMessageBox::warning(this, "提示", "未连接到服务器");
        return;
    }
    const QString word = searchWordEdit_->text().trimmed();
    if (word.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请输入要查询的单词");
        return;
    }
    if (desktopPet_)
    {
        desktopPet_->notifyUserActivity();
        desktopPet_->setPetState(DesktopPetWidget::PetState::Searching);
    }
    client_->sendMsg(DicProtocol::S, username_, word);
    showTransientStatus("已发送查词请求…");
}

void MainWindow::onHistoryFetchClicked()
{
    if (!client_ || !client_->isConnected())
    {
        QMessageBox::warning(this, "提示", "未连接到服务器");
        return;
    }
    historyEdit_->clear();
    historyFetching_ = true;
    client_->sendMsg(DicProtocol::H, username_, "");
    showTransientStatus("正在拉取历史记录…");
}

void MainWindow::onPetRefreshClicked()
{
    if (!client_ || !client_->isConnected())
    {
        QMessageBox::warning(this, "提示", "未连接到服务器");
        return;
    }
    client_->sendMsg(DicProtocol::P, username_, "");
    showTransientStatus("正在刷新宠物信息…");
}

void MainWindow::onLearnNextClicked()
{
    if (!client_ || !client_->isConnected())
    {
        QMessageBox::warning(this, "提示", "未连接到服务器");
        return;
    }
    learnMeaningLabel_->setVisible(false);
    currentLearnWord_.clear();
    currentLearnMeaning_.clear();
    learnWordLabel_->setText("获取中…");
    if (desktopPet_)
    {
        desktopPet_->notifyUserActivity();
        desktopPet_->setPetState(DesktopPetWidget::PetState::Learning);
    }
    client_->sendMsg(DicProtocol::B, username_, "");
    showTransientStatus("请求下一个背诵单词…");
}

void MainWindow::onLearnShowAnswerClicked()
{
    if (currentLearnWord_.isEmpty())
    {
        return;
    }
    if (dictationActive_)
    {
        return;
    }
    learnMeaningLabel_->setVisible(true);
}

void MainWindow::onLearnRememberClicked()
{
    if (!client_ || !client_->isConnected())
    {
        QMessageBox::warning(this, "提示", "未连接到服务器");
        return;
    }
    if (currentLearnWord_.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请先获取一个要背的词");
        return;
    }
    if (dictationActive_)
    {
        return;
    }
    if (desktopPet_)
    {
        desktopPet_->notifyUserActivity();
        desktopPet_->showSuccess();
    }
    if (!currentLearnWord_.trimmed().isEmpty() && !currentLearnMeaning_.trimmed().isEmpty())
    {
        recentCorrectPairs_.push_back(qMakePair(currentLearnWord_.trimmed(), currentLearnMeaning_.trimmed()));
        while (recentCorrectPairs_.size() > 30)
        {
            recentCorrectPairs_.pop_front();
        }
    }
    client_->sendMsg(DicProtocol::M, username_, currentLearnWord_ + " 1");
    learnMeaningLabel_->setVisible(false);
    currentLearnWord_.clear();
    currentLearnMeaning_.clear();
    learnWordLabel_->setText("获取中…");
    client_->sendMsg(DicProtocol::B, username_, "");
    showTransientStatus("已提交：记住了，正在获取下一个单词…");
}

void MainWindow::onLearnForgetClicked()
{
    if (!client_ || !client_->isConnected())
    {
        QMessageBox::warning(this, "提示", "未连接到服务器");
        return;
    }
    if (currentLearnWord_.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请先获取一个要背的词");
        return;
    }
    if (dictationActive_)
    {
        return;
    }
    if (desktopPet_)
    {
        desktopPet_->notifyUserActivity();
        desktopPet_->setPetState(DesktopPetWidget::PetState::Learning);
    }
    client_->sendMsg(DicProtocol::M, username_, currentLearnWord_ + " 0");
    showTransientStatus("已提交：没记住");
}

void MainWindow::onDictationSubmitClicked()
{
    if (!client_ || !client_->isConnected())
    {
        QMessageBox::warning(this, "提示", "未连接到服务器");
        return;
    }
    if (!dictationActive_ || dictationId_ <= 0)
    {
        return;
    }
    const QString spelling = dictationInput_ ? dictationInput_->text().trimmed() : "";
    if (spelling.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请输入你的拼写");
        return;
    }
    if (desktopPet_)
    {
        desktopPet_->notifyUserActivity();
        desktopPet_->setPetState(DesktopPetWidget::PetState::Learning);
    }
    client_->sendMsg(DicProtocol::N, username_, QString("%1 %2").arg(dictationId_).arg(spelling));
    showTransientStatus("已提交默写答案…");
}

void MainWindow::onDictationHintClicked()
{
    if (!dictationActive_ || dictationId_ <= 0)
    {
        return;
    }
    if (dictationAnswer_.trimmed().isEmpty())
    {
        QMessageBox::information(this, "提示", "当前默写题暂时无法从本地匹配到答案。");
        return;
    }
    if (dictationInput_)
    {
        dictationInput_->setText(dictationAnswer_);
        dictationInput_->selectAll();
        dictationInput_->setFocus();
    }
}

void MainWindow::onClientDisconnected()
{
    showTransientStatus("已断开连接");
}

void MainWindow::onClientError(const QString &message)
{
    showTransientStatus("网络错误：" + message);
}

void MainWindow::onClientMessage(const DicProtocol::ReceivedMessage &message)
{
    if (message.type == DicProtocol::S)
    {
        searchResultEdit_->setPlainText(message.text);
        int level = 0;
        int exp = 0;
        int gain = 0;
        bool levelUp = false;
        if (parseGrowthInfo(message.text, level, exp, gain, levelUp))
        {
            updatePetLabels(level, exp, gain, levelUp);
        }
        if (desktopPet_)
        {
            desktopPet_->setPetState(DesktopPetWidget::PetState::Idle);
        }
        return;
    }

    if (message.type == DicProtocol::H)
    {
        if (message.text == "**OVER**")
        {
            historyFetching_ = false;
            showTransientStatus("历史记录拉取完成");
            return;
        }
        if (historyFetching_)
        {
            historyEdit_->append(message.text);
        }
        return;
    }

    if (message.type == DicProtocol::P)
    {
        if (message.text == "**FAIL**")
        {
            showTransientStatus("刷新宠物信息失败");
            return;
        }
        int level = 0;
        int exp = 0;
        if (parsePetLvExp(message.text, level, exp))
        {
            updatePetLabels(level, exp, 0, false);
        }
        else
        {
            setPetInfoText(message.text);
        }
        return;
    }

    if (message.type == DicProtocol::B)
    {
        if (desktopPet_)
        {
            desktopPet_->setPetState(DesktopPetWidget::PetState::Learning);
        }
        if (message.text == "**EMPTY**")
        {
            currentLearnWord_.clear();
            currentLearnMeaning_.clear();
            learnWordLabel_->setText("暂无可背单词");
            learnMeaningLabel_->setText("");
            learnMeaningLabel_->setVisible(false);
            return;
        }

        const int firstSpace = message.text.indexOf(' ');
        const QString word = (firstSpace >= 0) ? message.text.left(firstSpace) : message.text;
        const QString meaning = (firstSpace >= 0) ? message.text.mid(firstSpace + 1) : "";

        currentLearnWord_ = word.trimmed();
        currentLearnMeaning_ = meaning.trimmed();

        learnWordLabel_->setText(currentLearnWord_);
        learnMeaningLabel_->setText(currentLearnMeaning_);
        learnMeaningLabel_->setVisible(false);
        setDictationMode(false);
        return;
    }

    if (message.type == DicProtocol::D)
    {
        if (desktopPet_)
        {
            desktopPet_->setPetState(DesktopPetWidget::PetState::Learning);
        }
        if (message.text == "**NONE**")
        {
            setDictationMode(false);
            showTransientStatus("暂无默写题");
            return;
        }
        const int firstSpace = message.text.indexOf(' ');
        const QString idStr = (firstSpace >= 0) ? message.text.left(firstSpace) : "";
        const QString meaning = (firstSpace >= 0) ? message.text.mid(firstSpace + 1) : message.text;
        const int id = idStr.trimmed().toInt();
        if (id <= 0 || meaning.trimmed().isEmpty())
        {
            setDictationMode(false);
            showTransientStatus("默写题格式错误");
            return;
        }

        dictationId_ = id;
        dictationMeaning_ = meaning.trimmed();
        dictationAnswer_.clear();
        const QString m = normMeaning(dictationMeaning_);
        for (auto it = recentCorrectPairs_.crbegin(); it != recentCorrectPairs_.crend(); ++it)
        {
            const QString storedMeaning = normMeaning(it->second);
            if (!storedMeaning.isEmpty() && storedMeaning == m)
            {
                dictationAnswer_ = it->first;
                break;
            }
        }
        if (dictationAnswer_.isEmpty())
        {
            for (auto it = recentCorrectPairs_.crbegin(); it != recentCorrectPairs_.crend(); ++it)
            {
                const QString storedMeaning = normMeaning(it->second);
                if (!storedMeaning.isEmpty() && (storedMeaning.contains(m) || m.contains(storedMeaning)))
                {
                    dictationAnswer_ = it->first;
                    break;
                }
            }
        }
        setDictationMode(true);
        learnWordLabel_->setText(QString("默写题 #%1").arg(dictationId_));
        learnMeaningLabel_->setText(meaning.trimmed());
        learnMeaningLabel_->setVisible(true);
        if (dictationInput_)
        {
            dictationInput_->clear();
            dictationInput_->setFocus();
        }
        showTransientStatus("进入默写：请根据释义拼写单词");
        return;
    }

    if (message.type == DicProtocol::M)
    {
        if (message.text == "**FAIL**")
        {
            showTransientStatus("提交结果失败");
            return;
        }
        int level = 0;
        int exp = 0;
        int gain = 0;
        bool levelUp = false;
        if (parseGrowthInfo(message.text, level, exp, gain, levelUp))
        {
            updatePetLabels(level, exp, gain, levelUp);
        }
        return;
    }

    if (message.type == DicProtocol::N)
    {
        if (desktopPet_)
        {
            desktopPet_->setPetState(DesktopPetWidget::PetState::Learning);
        }
        if (message.text == "**FAIL**")
        {
            showTransientStatus("提交默写失败");
            return;
        }
        if (message.text == "**NONE**")
        {
            setDictationMode(false);
            showTransientStatus("默写题已失效");
            return;
        }

        int level = 0;
        int exp = 0;
        int gain = 0;
        bool levelUp = false;
        if (parseGrowthInfo(message.text, level, exp, gain, levelUp))
        {
            updatePetLabels(level, exp, gain, levelUp);
        }

        if (message.text.startsWith("DICT OK"))
        {
            if (!dictationMeaning_.trimmed().isEmpty() && dictationInput_ && !dictationInput_->text().trimmed().isEmpty())
            {
                recentCorrectPairs_.push_back(qMakePair(dictationInput_->text().trimmed(), dictationMeaning_.trimmed()));
                while (recentCorrectPairs_.size() > 30)
                {
                    recentCorrectPairs_.pop_front();
                }
            }
            showTransientStatus("默写正确，继续下一个单词…");
        }
        else if (message.text.startsWith("DICT FAIL"))
        {
            const int pos = message.text.indexOf("ans=");
            if (pos >= 0)
            {
                const QString ans = message.text.mid(pos + 4).split(' ').first().trimmed();
                if (!ans.isEmpty() && !dictationMeaning_.trimmed().isEmpty())
                {
                    recentCorrectPairs_.push_back(qMakePair(ans, dictationMeaning_.trimmed()));
                    while (recentCorrectPairs_.size() > 30)
                    {
                        recentCorrectPairs_.pop_front();
                    }
                }
            }
            showTransientStatus("默写错误，继续下一个单词…");
        }
        else
        {
            showTransientStatus("默写已提交，继续下一个单词…");
        }

        setDictationMode(false);
        learnWordLabel_->setText("获取中…");
        client_->sendMsg(DicProtocol::B, username_, "");
        return;
    }
}

void MainWindow::setupUi()
{
    auto *root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout();
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(12);

    auto *topRow = new QHBoxLayout();
    userLabel_ = new QLabel(QString("当前用户：%1").arg(username_), root);
    userLabel_->setProperty("role", "subtitle");
    logoutBtn_ = new QPushButton("退出登录", root);
    logoutBtn_->setProperty("kind", "danger");
    logoutBtn_->setMinimumHeight(36);
    connect(logoutBtn_, &QPushButton::clicked, this, &MainWindow::onLogoutClicked);
    topRow->addWidget(userLabel_);
    topRow->addStretch(1);
    topRow->addWidget(logoutBtn_);

    statusLabel_ = new QLabel("就绪", root);
    statusLabel_->setProperty("role", "subtitle");

    tabs_ = new QTabWidget(root);

    {
        auto *tab = new QWidget(tabs_);
        auto *layout = new QVBoxLayout();
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);

        auto *row = new QHBoxLayout();
        searchWordEdit_ = new QLineEdit(tab);
        searchWordEdit_->setPlaceholderText("输入单词，例如: apple");
        searchBtn_ = new QPushButton("查询", tab);
        searchBtn_->setProperty("kind", "primary");
        searchBtn_->setMinimumHeight(36);
        connect(searchBtn_, &QPushButton::clicked, this, &MainWindow::onSearchClicked);
        row->addWidget(searchWordEdit_, 1);
        row->addWidget(searchBtn_);

        searchResultEdit_ = new QTextEdit(tab);
        searchResultEdit_->setReadOnly(true);

        auto *petRow = new QHBoxLayout();
        petLvLabel1_ = new QLabel("LV 1", tab);
        petExpLabel1_ = new QLabel("EXP 0", tab);
        petHintLabel1_ = new QLabel("", tab);
        petHintLabel1_->setProperty("role", "subtitle");
        petRow->addWidget(new QLabel("宠物：", tab));
        petRow->addWidget(petLvLabel1_);
        petRow->addWidget(petExpLabel1_);
        petRow->addStretch(1);
        petRow->addWidget(petHintLabel1_);

        layout->addLayout(row);
        layout->addWidget(searchResultEdit_, 1);
        layout->addLayout(petRow);
        tab->setLayout(layout);
        tabs_->addTab(tab, "查词");
    }

    {
        auto *tab = new QWidget(tabs_);
        auto *layout = new QVBoxLayout();
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);

        auto *row = new QHBoxLayout();
        learnNextBtn_ = new QPushButton("下一个词", tab);
        learnShowAnswerBtn_ = new QPushButton("显示答案", tab);
        learnRememberBtn_ = new QPushButton("记住了", tab);
        learnForgetBtn_ = new QPushButton("没记住", tab);
        learnNextBtn_->setMinimumHeight(36);
        learnShowAnswerBtn_->setMinimumHeight(36);
        learnRememberBtn_->setMinimumHeight(36);
        learnForgetBtn_->setMinimumHeight(36);
        learnRememberBtn_->setProperty("kind", "primary");

        connect(learnNextBtn_, &QPushButton::clicked, this, &MainWindow::onLearnNextClicked);
        connect(learnShowAnswerBtn_, &QPushButton::clicked, this, &MainWindow::onLearnShowAnswerClicked);
        connect(learnRememberBtn_, &QPushButton::clicked, this, &MainWindow::onLearnRememberClicked);
        connect(learnForgetBtn_, &QPushButton::clicked, this, &MainWindow::onLearnForgetClicked);

        row->addWidget(learnNextBtn_);
        row->addWidget(learnShowAnswerBtn_);
        row->addStretch(1);
        row->addWidget(learnRememberBtn_);
        row->addWidget(learnForgetBtn_);

        learnWordLabel_ = new QLabel("点击“下一个词”开始", tab);
        learnWordLabel_->setProperty("role", "title");
        QFont wordFont = learnWordLabel_->font();
        wordFont.setPointSize(wordFont.pointSize() + 10);
        wordFont.setBold(true);
        learnWordLabel_->setFont(wordFont);
        learnWordLabel_->setAlignment(Qt::AlignCenter);

        learnMeaningLabel_ = new QLabel("", tab);
        learnMeaningLabel_->setProperty("role", "subtitle");
        learnMeaningLabel_->setAlignment(Qt::AlignCenter);
        learnMeaningLabel_->setVisible(false);

        auto *dictRow = new QHBoxLayout();
        dictationInput_ = new QLineEdit(tab);
        dictationInput_->setPlaceholderText("默写：请输入你认为正确的单词拼写");
        dictationHintBtn_ = new QPushButton("提示", tab);
        dictationHintBtn_->setMinimumHeight(36);
        dictationSubmitBtn_ = new QPushButton("提交默写", tab);
        dictationSubmitBtn_->setProperty("kind", "primary");
        dictationSubmitBtn_->setMinimumHeight(36);
        dictRow->addWidget(dictationInput_, 1);
        dictRow->addWidget(dictationHintBtn_);
        dictRow->addWidget(dictationSubmitBtn_);
        connect(dictationHintBtn_, &QPushButton::clicked, this, &MainWindow::onDictationHintClicked);
        connect(dictationSubmitBtn_, &QPushButton::clicked, this, &MainWindow::onDictationSubmitClicked);
        connect(dictationInput_, &QLineEdit::returnPressed, this, &MainWindow::onDictationSubmitClicked);

        auto *petRow = new QHBoxLayout();
        petLvLabel2_ = new QLabel("LV 1", tab);
        petExpLabel2_ = new QLabel("EXP 0", tab);
        petHintLabel2_ = new QLabel("", tab);
        petHintLabel2_->setProperty("role", "subtitle");
        petRow->addWidget(new QLabel("宠物：", tab));
        petRow->addWidget(petLvLabel2_);
        petRow->addWidget(petExpLabel2_);
        petRow->addStretch(1);
        petRow->addWidget(petHintLabel2_);

        layout->addLayout(row);
        layout->addStretch(1);
        layout->addWidget(learnWordLabel_);
        layout->addWidget(learnMeaningLabel_);
        layout->addLayout(dictRow);
        layout->addStretch(1);
        layout->addLayout(petRow);
        tab->setLayout(layout);
        tabs_->addTab(tab, "背词");
    }

    {
        auto *tab = new QWidget(tabs_);
        auto *layout = new QVBoxLayout();
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);

        historyFetchBtn_ = new QPushButton("拉取历史记录", tab);
        historyFetchBtn_->setProperty("kind", "primary");
        historyFetchBtn_->setMinimumHeight(36);
        connect(historyFetchBtn_, &QPushButton::clicked, this, &MainWindow::onHistoryFetchClicked);

        historyEdit_ = new QTextEdit(tab);
        historyEdit_->setReadOnly(true);

        layout->addWidget(historyFetchBtn_);
        layout->addWidget(historyEdit_, 1);
        tab->setLayout(layout);
        tabs_->addTab(tab, "历史");
    }

    {
        auto *tab = new QWidget(tabs_);
        auto *layout = new QVBoxLayout();
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);

        auto *row = new QHBoxLayout();
        petRefreshBtn_ = new QPushButton("刷新宠物信息", tab);
        petRefreshBtn_->setMinimumHeight(36);
        connect(petRefreshBtn_, &QPushButton::clicked, this, &MainWindow::onPetRefreshClicked);
        petInteractBtn_ = new QPushButton("互动", tab);
        petInteractBtn_->setMinimumHeight(36);
        petInteractBtn_->setProperty("kind", "primary");
        row->addWidget(petRefreshBtn_);
        row->addWidget(petInteractBtn_);
        row->addStretch(1);

        petInfoLabel_ = new QLabel("LV 1 EXP 0", tab);
        QFont petFont = petInfoLabel_->font();
        petFont.setPointSize(petFont.pointSize() + 4);
        petInfoLabel_->setFont(petFont);

        petSpeechLabel_ = new QLabel("点一下“互动”吧", tab);
        petSpeechLabel_->setWordWrap(true);
        petSpeechLabel_->setProperty("role", "subtitle");

        connect(petInteractBtn_, &QPushButton::clicked, this, [this]()
                {
                    const QStringList lines = {
                        "今天也要坚持背单词。",
                        "查词也能涨经验，继续加油。",
                        "记不住没关系，多来几次就会了。",
                        "保持节奏，宠物就会成长。"};
                    const int idx = static_cast<int>(QRandomGenerator::global()->bounded(lines.size()));
                    petSpeechLabel_->setText(lines.at(idx));
                });

        layout->addLayout(row);
        layout->addSpacing(12);
        layout->addWidget(petInfoLabel_);
        layout->addSpacing(12);
        layout->addWidget(petSpeechLabel_);
        layout->addStretch(1);
        tab->setLayout(layout);
        tabs_->addTab(tab, "宠物");
    }

    rootLayout->addLayout(topRow);
    rootLayout->addWidget(statusLabel_);
    rootLayout->addWidget(tabs_, 1);

    root->setLayout(rootLayout);
    setCentralWidget(root);

    setDictationMode(false);
}

void MainWindow::setDictationMode(bool enabled)
{
    dictationActive_ = enabled;
    if (!enabled)
    {
        dictationId_ = 0;
        dictationMeaning_.clear();
        dictationAnswer_.clear();
    }

    if (dictationInput_)
    {
        dictationInput_->setVisible(enabled);
        dictationInput_->setEnabled(enabled);
    }
    if (dictationHintBtn_)
    {
        dictationHintBtn_->setVisible(enabled);
        dictationHintBtn_->setEnabled(enabled);
    }
    if (dictationSubmitBtn_)
    {
        dictationSubmitBtn_->setVisible(enabled);
        dictationSubmitBtn_->setEnabled(enabled);
    }

    if (learnShowAnswerBtn_)
    {
        learnShowAnswerBtn_->setEnabled(!enabled);
    }
    if (learnRememberBtn_)
    {
        learnRememberBtn_->setEnabled(!enabled);
    }
    if (learnForgetBtn_)
    {
        learnForgetBtn_->setEnabled(!enabled);
    }

    if (!enabled && dictationInput_)
    {
        dictationInput_->clear();
    }
}

void MainWindow::updatePetLabels(int level, int exp, int gain, bool levelUp)
{
    const QString lvText = QString("LV %1").arg(level);
    const QString expText = QString("EXP %1").arg(exp);

    petLvLabel1_->setText(lvText);
    petExpLabel1_->setText(expText);
    petLvLabel2_->setText(lvText);
    petExpLabel2_->setText(expText);
    petInfoLabel_->setText(QString("LV %1 EXP %2").arg(level).arg(exp));
    if (desktopPet_)
    {
        desktopPet_->setPetInfo(level, exp);
    }
    if (chatDialog_)
    {
        chatDialog_->setPetInfo(level, exp);
    }

    QString hint;
    if (gain > 0)
    {
        hint = QString("+%1").arg(gain);
    }
    if (levelUp)
    {
        hint = hint.isEmpty() ? "UP" : (hint + "  UP");
    }

    petHintLabel1_->setText(hint);
    petHintLabel2_->setText(hint);
}

void MainWindow::setPetInfoText(const QString &text)
{
    petInfoLabel_->setText(text);
    int level = 0;
    int exp = 0;
    if (desktopPet_ && parsePetLvExp(text, level, exp))
    {
        desktopPet_->setPetInfo(level, exp);
    }
    if (chatDialog_ && parsePetLvExp(text, level, exp))
    {
        chatDialog_->setPetInfo(level, exp);
    }
}

void MainWindow::showTransientStatus(const QString &text)
{
    statusLabel_->setText(text);
    QTimer::singleShot(3000, this, [this, text]()
                       {
                           if (statusLabel_ && statusLabel_->text() == text)
                           {
                               statusLabel_->setText("就绪");
                           }
                       });
}

bool MainWindow::parsePetLvExp(const QString &text, int &level, int &exp) const
{
    static const QRegularExpression re(R"(LV\s+(\d+)\s+EXP\s+(\d+))");
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch())
    {
        return false;
    }
    level = m.captured(1).toInt();
    exp = m.captured(2).toInt();
    return true;
}

bool MainWindow::parseGrowthInfo(const QString &text, int &level, int &exp, int &gain, bool &levelUp) const
{
    static const QRegularExpression re(R"(LV\s+(\d+)\s+EXP\s+(\d+)\s*\(\+(\d+)\)\s*(UP)?)");
    const QRegularExpressionMatch m = re.match(text);
    if (m.hasMatch())
    {
        level = m.captured(1).toInt();
        exp = m.captured(2).toInt();
        gain = m.captured(3).toInt();
        levelUp = !m.captured(4).isEmpty() || text.contains(" UP");
        return true;
    }

    const int pos = text.indexOf("LV ");
    if (pos >= 0)
    {
        const QRegularExpressionMatch m2 = re.match(text.mid(pos));
        if (m2.hasMatch())
        {
            level = m2.captured(1).toInt();
            exp = m2.captured(2).toInt();
            gain = m2.captured(3).toInt();
            levelUp = !m2.captured(4).isEmpty() || text.contains(" UP");
            return true;
        }
    }
    return false;
}
