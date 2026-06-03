#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QHash>
#include <QtCore/QPoint>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariant>
#include <QtGui/QEnterEvent>
#include <QtGui/QPixmap>
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QWidget>

class DesktopPetWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopPetWidget(QWidget *parent = nullptr);

    enum class PetState
    {
        Init,
        Idle,
        Searching,
        Learning,
        Success,
    };

    enum class GrowthStage
    {
        Baby,
        Teen,
        Adult,
        Awaken,
    };

    void setPetInfo(int level, int exp);
    int level() const;
    int exp() const;

    void setPetState(PetState state);
    void notifyUserActivity();
    void showSuccess();

signals:
    void activated();
    void chatRequested();
    void quitRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void tick();
    void snapIntoScreen();
    QPoint screenClampedTopLeft(const QPoint &desiredTopLeft) const;
    QPoint screenDockedTopLeft(const QPoint &desiredTopLeft);
    void setClickThroughEnabled(bool enabled);
    void updateTray();
    void ensurePetImagesLoaded();
    QPixmap currentStatePixmap() const;
    GrowthStage growthStageForLevel(int level) const;
    QString growthStageName(GrowthStage stage) const;
    double growthStageProgress(int level) const;

    enum class DockEdge
    {
        Left,
        Right,
        Top,
        Bottom,
    };

    DockEdge dockEdge_ = DockEdge::Bottom;
    bool dockToEdge_ = true;
    bool clickThrough_ = false;
    int dockThresholdPx_ = 40;

    QTimer animTimer_;
    QElapsedTimer t0_;
    QTimer idleTimer_;
    QTimer successTimer_;
    QTimer searchingTimer_;
    int idleTimeoutMs_ = 15000;
    int searchingMinMs_ = 2500;
    int successDurationMs_ = 3500;
    int chromaKeyTolerance_ = 24;

    QPoint dragOffset_;
    bool dragging_ = false;
    bool pressed_ = false;
    QPoint pressGlobalPos_;

    QPointF velocity_{1.2, -0.8};
    bool autoMove_ = true;

    QPointer<QSystemTrayIcon> tray_;
    QPointer<QMenu> trayMenu_;

    int level_ = 1;
    int exp_ = 0;

    QString petImageDir_;
    QHash<int, QPixmap> petStateImages_;
    bool petImagesLoaded_ = false;

    PetState petState_ = PetState::Init;
    PetState afterSuccessState_ = PetState::Idle;
    PetState pendingAfterSearchingState_ = PetState::Idle;
    bool searchingLock_ = false;
};
