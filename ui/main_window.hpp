#pragma once

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtWidgets/QMainWindow>

class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QTextEdit;
class DesktopPetWidget;
class ChatDialog;

class DicClient;

namespace DicProtocol
{
struct ReceivedMessage;
}

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(DicClient *client, const QString &username, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onLogoutClicked();

    void onSearchClicked();
    void onHistoryFetchClicked();
    void onPetRefreshClicked();

    void onLearnNextClicked();
    void onLearnShowAnswerClicked();
    void onLearnRememberClicked();
    void onLearnForgetClicked();
    void onDictationSubmitClicked();
    void onDictationHintClicked();

    void onClientDisconnected();
    void onClientError(const QString &message);
    void onClientMessage(const DicProtocol::ReceivedMessage &message);

private:
    void setupUi();
    void updatePetLabels(int level, int exp, int gain, bool levelUp);
    void setPetInfoText(const QString &text);
    void showTransientStatus(const QString &text);
    void setDictationMode(bool enabled);

    bool parsePetLvExp(const QString &text, int &level, int &exp) const;
    bool parseGrowthInfo(const QString &text, int &level, int &exp, int &gain, bool &levelUp) const;

    QPointer<DicClient> client_;
    QString username_;

    QLabel *userLabel_ = nullptr;
    QPushButton *logoutBtn_ = nullptr;
    QLabel *statusLabel_ = nullptr;

    QTabWidget *tabs_ = nullptr;

    QLineEdit *searchWordEdit_ = nullptr;
    QPushButton *searchBtn_ = nullptr;
    QTextEdit *searchResultEdit_ = nullptr;
    QLabel *petLvLabel1_ = nullptr;
    QLabel *petExpLabel1_ = nullptr;
    QLabel *petHintLabel1_ = nullptr;

    QPushButton *learnNextBtn_ = nullptr;
    QPushButton *learnShowAnswerBtn_ = nullptr;
    QPushButton *learnRememberBtn_ = nullptr;
    QPushButton *learnForgetBtn_ = nullptr;
    QLabel *learnWordLabel_ = nullptr;
    QLabel *learnMeaningLabel_ = nullptr;
    QLineEdit *dictationInput_ = nullptr;
    QPushButton *dictationHintBtn_ = nullptr;
    QPushButton *dictationSubmitBtn_ = nullptr;
    bool dictationActive_ = false;
    int dictationId_ = 0;
    QString dictationMeaning_;
    QString dictationAnswer_;
    QList<QPair<QString, QString>> recentCorrectPairs_;
    QLabel *petLvLabel2_ = nullptr;
    QLabel *petExpLabel2_ = nullptr;
    QLabel *petHintLabel2_ = nullptr;

    QPushButton *historyFetchBtn_ = nullptr;
    QTextEdit *historyEdit_ = nullptr;
    bool historyFetching_ = false;

    QPushButton *petRefreshBtn_ = nullptr;
    QLabel *petInfoLabel_ = nullptr;
    QPushButton *petInteractBtn_ = nullptr;
    QLabel *petSpeechLabel_ = nullptr;

    QPointer<DesktopPetWidget> desktopPet_;
    QPointer<ChatDialog> chatDialog_;

    QString currentLearnWord_;
    QString currentLearnMeaning_;
};

