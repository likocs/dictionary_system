#include "desktop_pet_widget.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtMath>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QImage>
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QStyle>


#include <algorithm>

namespace
{
    QPixmap loadChromaKeyed(const QString &path, int tolerance)
    {
        QImage img;
        if (!img.load(path))
        {
            return {};
        }

        img = img.convertToFormat(QImage::Format_ARGB32);
        const QColor key = img.pixelColor(0, 0);

        const int w = img.width();
        const int h = img.height();
        for (int y = 0; y < h; ++y)
        {
            QRgb *row = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < w; ++x)
            {
                const QColor c = QColor::fromRgba(row[x]);
                const int dr = qAbs(c.red() - key.red());
                const int dg = qAbs(c.green() - key.green());
                const int db = qAbs(c.blue() - key.blue());
                if (dr <= tolerance && dg <= tolerance && db <= tolerance)
                {
                    row[x] = qRgba(c.red(), c.green(), c.blue(), 0);
                }
            }
        }

        return QPixmap::fromImage(img);
    }
}

DesktopPetWidget::DesktopPetWidget(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);

    resize(140, 140);

    if (QSystemTrayIcon::isSystemTrayAvailable())
    {
        trayMenu_ = new QMenu();
        tray_ = new QSystemTrayIcon(this);
        tray_->setIcon(qApp->style()->standardIcon(QStyle::SP_ComputerIcon));
        tray_->setContextMenu(trayMenu_);
        connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
                {
                    if (reason == QSystemTrayIcon::DoubleClick)
                    {
                        emit activated();
                    }
                });
        tray_->show();
        updateTray();
    }

    t0_.start();
    animTimer_.setInterval(33);
    connect(&animTimer_, &QTimer::timeout, this, [this]()
            { tick(); });
    animTimer_.start();

    ensurePetImagesLoaded();

    idleTimer_.setSingleShot(true);
    connect(&idleTimer_, &QTimer::timeout, this, [this]()
            {
                if (petState_ != PetState::Searching && petState_ != PetState::Learning && petState_ != PetState::Success)
                {
                    setPetState(PetState::Idle);
                }
            });

    successTimer_.setSingleShot(true);
    connect(&successTimer_, &QTimer::timeout, this, [this]()
            {
                if (petState_ == PetState::Success)
                {
                    setPetState(afterSuccessState_);
                }
            });

    searchingTimer_.setSingleShot(true);
    connect(&searchingTimer_, &QTimer::timeout, this, [this]()
            {
                searchingLock_ = false;
                if (petState_ == PetState::Searching && pendingAfterSearchingState_ != PetState::Searching)
                {
                    const PetState next = pendingAfterSearchingState_;
                    pendingAfterSearchingState_ = PetState::Idle;
                    setPetState(next);
                }
            });

    setPetState(PetState::Init);
    idleTimer_.start(idleTimeoutMs_);
}

void DesktopPetWidget::setPetInfo(int level, int exp)
{
    level_ = std::max(1, level);
    exp_ = std::max(0, exp);
    const GrowthStage stage = growthStageForLevel(level_);
    const int pct = static_cast<int>(growthStageProgress(level_) * 100.0);
    setToolTip(QString("LV %1\nEXP %2\n阶段：%3（%4%）").arg(level_).arg(exp_).arg(growthStageName(stage)).arg(pct));
    updateTray();
    update();
}

int DesktopPetWidget::level() const
{
    return level_;
}

int DesktopPetWidget::exp() const
{
    return exp_;
}

void DesktopPetWidget::setPetState(PetState state)
{
    if (petState_ == state)
    {
        return;
    }

    if (petState_ == PetState::Success && state != PetState::Success)
    {
        afterSuccessState_ = state;
        if (successTimer_.isActive())
        {
            return;
        }
    }
    if (petState_ == PetState::Searching && searchingLock_ && state != PetState::Searching)
    {
        pendingAfterSearchingState_ = state;
        return;
    }

    petState_ = state;
    if (petState_ == PetState::Searching || petState_ == PetState::Learning)
    {
        idleTimer_.stop();
    }

    if (petState_ == PetState::Searching)
    {
        searchingLock_ = true;
        pendingAfterSearchingState_ = PetState::Idle;
        searchingTimer_.stop();
        searchingTimer_.start(searchingMinMs_);
    }

    update();
}

void DesktopPetWidget::notifyUserActivity()
{
    if (petState_ == PetState::Init)
    {
        setPetState(PetState::Idle);
    }
    if (petState_ != PetState::Searching && petState_ != PetState::Learning && petState_ != PetState::Success)
    {
        idleTimer_.start(idleTimeoutMs_);
    }
}

void DesktopPetWidget::showSuccess()
{
    afterSuccessState_ = (petState_ == PetState::Learning) ? PetState::Learning : PetState::Idle;
    setPetState(PetState::Success);
    successTimer_.stop();
    successTimer_.start(successDurationMs_);
}

void DesktopPetWidget::enterEvent(QEnterEvent *event)
{
    notifyUserActivity();
    if (petState_ == PetState::Idle)
    {
        idleTimer_.stop();
        setPetState(PetState::Init);
    }
    QWidget::enterEvent(event);
}

void DesktopPetWidget::leaveEvent(QEvent *event)
{
    if (petState_ == PetState::Init)
    {
        setPetState(PetState::Idle);
        idleTimer_.start(idleTimeoutMs_);
    }
    QWidget::leaveEvent(event);
}

void DesktopPetWidget::ensurePetImagesLoaded()
{
    if (petImagesLoaded_)
    {
        return;
    }

    const QStringList fileNames = {"init.jpg", "daiji.jpg", "ing.jpg", "sikao.jpg", "success.jpg"};

    auto tryResolveDir = [&](const QString &base) -> QString
    {
        QDir d(base);
        for (int i = 0; i < 32; ++i)
        {
            const QString c1 = d.filePath("src/image");
            const QString c2 = d.filePath("ui/src/image");
            if (QFileInfo::exists(QDir(c1).filePath(fileNames.front())))
            {
                return QDir(c1).absolutePath();
            }
            if (QFileInfo::exists(QDir(c2).filePath(fileNames.front())))
            {
                return QDir(c2).absolutePath();
            }
            if (!d.cdUp())
            {
                break;
            }
        }
        return {};
    };

    const QString srcBase = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath();
    const QStringList bases = {QCoreApplication::applicationDirPath(), QDir::currentPath(), srcBase};
    for (const QString &b : bases)
    {
        const QString found = tryResolveDir(b);
        if (!found.isEmpty())
        {
            petImageDir_ = found;
            break;
        }
    }

    auto load = [&](PetState state, const QString &name)
    {
        const QString path = petImageDir_.isEmpty() ? QString() : QDir(petImageDir_).filePath(name);
        const QPixmap px = path.isEmpty() ? QPixmap() : loadChromaKeyed(path, chromaKeyTolerance_);
        petStateImages_.insert(static_cast<int>(state), px);
    };

    load(PetState::Init, "init.jpg");
    load(PetState::Idle, "daiji.jpg");
    load(PetState::Searching, "ing.jpg");
    load(PetState::Learning, "sikao.jpg");
    load(PetState::Success, "success.jpg");

    petImagesLoaded_ = true;
}

QPixmap DesktopPetWidget::currentStatePixmap() const
{
    const auto it = petStateImages_.find(static_cast<int>(petState_));
    if (it == petStateImages_.end())
    {
        return {};
    }
    return it.value();
}

DesktopPetWidget::GrowthStage DesktopPetWidget::growthStageForLevel(int level) const
{
    const int lv = std::max(1, level);
    if (lv >= 10)
    {
        return GrowthStage::Awaken;
    }
    if (lv >= 6)
    {
        return GrowthStage::Adult;
    }
    if (lv >= 3)
    {
        return GrowthStage::Teen;
    }
    return GrowthStage::Baby;
}

QString DesktopPetWidget::growthStageName(GrowthStage stage) const
{
    switch (stage)
    {
    case GrowthStage::Baby:
        return "幼年";
    case GrowthStage::Teen:
        return "成长期";
    case GrowthStage::Adult:
        return "成熟期";
    case GrowthStage::Awaken:
        return "觉醒";
    }
    return "未知";
}

double DesktopPetWidget::growthStageProgress(int level) const
{
    const int lv = std::max(1, level);
    if (lv >= 10)
    {
        return 1.0;
    }
    if (lv >= 6)
    {
        return (lv - 6) / 4.0;
    }
    if (lv >= 3)
    {
        return (lv - 3) / 3.0;
    }
    return (lv - 1) / 2.0;
}

void DesktopPetWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal t = t0_.elapsed() / 1000.0;
    const GrowthStage stage = growthStageForLevel(level_);
    const qreal stageMul = (stage == GrowthStage::Baby) ? 0.95 : (stage == GrowthStage::Teen) ? 1.05
                                              : (stage == GrowthStage::Adult)   ? 1.12
                                                                               : 1.2;
    const qreal bob = qSin(t * 2.2) * (5.0 * stageMul);
    const qreal breathe = 1.0 + qSin(t * 1.6) * (0.03 * stageMul);
    const bool walking = autoMove_ && !dragging_;
    const qreal walk = walking ? qSin(t * 8.0) : 0.0;

    const QRectF bounds = rect();
    const QPointF center(bounds.center().x(), bounds.center().y() + bob);

    ensurePetImagesLoaded();
    const QPixmap pix = currentStatePixmap();
    if (!pix.isNull())
    {
        QSizeF target = pix.size();
        target.scale(bounds.size() * 0.82, Qt::KeepAspectRatio);
        const qreal scale = breathe * stageMul * (walking ? (1.0 + 0.02 * qSin(t * 8.0)) : 1.0);
        const qreal w = target.width() * scale;
        const qreal h = target.height() * scale;

        const qreal tiltBase = (walking ? 4.0 : 1.6);
        const qreal tilt = (walking ? (qSin(t * 6.5) * tiltBase) : (qSin(t * 1.3) * tiltBase)) * stageMul;

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 70));
        p.drawEllipse(QPointF(center.x(), bounds.bottom() - 18), w * 0.33, 10.0 + 2.0 * qAbs(walk));

        p.save();
        p.translate(center);
        p.rotate(tilt);
        const QRectF dst(-w / 2.0, -h / 2.0, w, h);
        p.drawPixmap(dst, pix, QRectF(pix.rect()));
        p.restore();

        int hue = 120;
        if (stage == GrowthStage::Awaken)
        {
            hue = 285;
        }
        else if (stage == GrowthStage::Adult)
        {
            hue = 210;
        }
        else if (stage == GrowthStage::Teen)
        {
            hue = 35;
        }
        const qreal expPulse = 0.25 + 0.25 * (1.0 + qSin(t * 3.0));
        QColor ring = QColor::fromHsv(hue, 140, 255);
        ring.setAlpha(static_cast<int>(110 + 80 * expPulse));
        p.setPen(QPen(ring, (stage == GrowthStage::Awaken) ? 8 : 6));
        p.setBrush(Qt::NoBrush);
        const qreal r = std::min(w, h) * 0.52;
        p.drawEllipse(center, r, r);

        const QString stageText = growthStageName(stage);
        QFont f = p.font();
        f.setBold(true);
        f.setPointSize(std::max(9, f.pointSize() - 1));
        p.setFont(f);

        const QRectF badgeRect(bounds.left() + 10, bounds.top() + 10, bounds.width() - 20, 28);
        p.setPen(Qt::NoPen);
        QColor badgeBg(0, 0, 0, 120);
        p.setBrush(badgeBg);
        p.drawRoundedRect(badgeRect, 10, 10);
        p.setPen(QColor(255, 255, 255, 230));
        p.drawText(badgeRect, Qt::AlignCenter, stageText);

        if (stage == GrowthStage::Awaken)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 215, 0, static_cast<int>(140 + 80 * expPulse)));
            for (int i = 0; i < 6; ++i)
            {
                const qreal a = t * 1.2 + i * (M_PI / 3.0);
                const qreal rr = r * (1.05 + 0.05 * qSin(t * 2.4));
                const QPointF pt(center.x() + rr * qCos(a), center.y() + rr * qSin(a));
                p.drawEllipse(pt, 3.2, 3.2);
            }
        }
        return;
    }

    const int baseSize = 88 + std::min(24, (level_ - 1) * 2);
    const qreal size = baseSize * breathe;
    const QRectF bodyRect(center.x() - size / 2.0, center.y() - size / 2.0, size, size);

    int hue = 120;
    if (level_ >= 8)
    {
        hue = 285;
    }
    else if (level_ >= 5)
    {
        hue = 210;
    }
    else if (level_ >= 3)
    {
        hue = 35;
    }
    QColor bodyColor = QColor::fromHsv(hue, 140, 255);
    bodyColor.setAlpha(235);

    QColor outline(20, 20, 20, 190);

    p.setPen(QPen(outline, 3));
    p.setBrush(bodyColor);
    p.drawEllipse(bodyRect);

    const qreal earW = size * 0.22;
    const qreal earH = size * 0.22;
    const qreal earY = center.y() - size * 0.48 + walk * 2.0;
    p.setPen(QPen(outline, 3));
    p.setBrush(bodyColor.darker(105));
    p.drawEllipse(QRectF(center.x() - size * 0.38, earY, earW, earH));
    p.drawEllipse(QRectF(center.x() + size * 0.16, earY, earW, earH));

    const qreal eyeY = center.y() - size * 0.12;
    const qreal eyeXOffset = size * 0.18;
    const qreal eyeR = size * 0.055;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 20, 230));
    p.drawEllipse(QPointF(center.x() - eyeXOffset, eyeY), eyeR, eyeR);
    p.drawEllipse(QPointF(center.x() + eyeXOffset, eyeY), eyeR, eyeR);

    p.setBrush(QColor(255, 255, 255, 220));
    p.drawEllipse(QPointF(center.x() - eyeXOffset + eyeR * 0.35, eyeY - eyeR * 0.35), eyeR * 0.35, eyeR * 0.35);
    p.drawEllipse(QPointF(center.x() + eyeXOffset + eyeR * 0.35, eyeY - eyeR * 0.35), eyeR * 0.35, eyeR * 0.35);

    const qreal mouthY = center.y() + size * 0.06;
    const qreal mouthW = size * 0.22;
    const qreal mouthH = size * 0.12;

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(30, 30, 30, 200), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QRectF mouthRect(center.x() - mouthW / 2.0, mouthY - mouthH / 2.0, mouthW, mouthH);
    p.drawArc(mouthRect, 200 * 16, 140 * 16);

    p.setPen(QPen(outline, 3));
    p.setBrush(bodyColor.darker(110));
    const qreal legY = center.y() + size * 0.35;
    const qreal legW = size * 0.12;
    const qreal legH = size * 0.18;
    const qreal step = walk * size * 0.06;
    p.drawRoundedRect(QRectF(center.x() - size * 0.28, legY + step, legW, legH), legW * 0.4, legW * 0.4);
    p.drawRoundedRect(QRectF(center.x() + size * 0.16, legY - step, legW, legH), legW * 0.4, legW * 0.4);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 140, 150, 120));
    p.drawEllipse(QPointF(center.x() - size * 0.28, center.y() + size * 0.02), size * 0.09, size * 0.06);
    p.drawEllipse(QPointF(center.x() + size * 0.28, center.y() + size * 0.02), size * 0.09, size * 0.06);

    if (level_ >= 10)
    {
        p.setPen(QPen(outline, 3));
        p.setBrush(QColor(255, 215, 0, 220));
        const qreal crownW = size * 0.46;
        const qreal crownH = size * 0.22;
        QPolygonF crown;
        crown << QPointF(center.x() - crownW / 2.0, center.y() - size * 0.56);
        crown << QPointF(center.x() - crownW * 0.25, center.y() - size * 0.72);
        crown << QPointF(center.x(), center.y() - size * 0.56 - crownH);
        crown << QPointF(center.x() + crownW * 0.25, center.y() - size * 0.72);
        crown << QPointF(center.x() + crownW / 2.0, center.y() - size * 0.56);
        crown << QPointF(center.x() + crownW / 2.0, center.y() - size * 0.56 + crownH);
        crown << QPointF(center.x() - crownW / 2.0, center.y() - size * 0.56 + crownH);
        p.drawPolygon(crown);
    }

    const qreal expPulse = 0.25 + 0.25 * (1.0 + qSin(t * 3.0));
    QColor ring = QColor::fromHsv(hue, 140, 255);
    ring.setAlpha(static_cast<int>(110 + 80 * expPulse));
    p.setPen(QPen(ring, 6));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(bodyRect.adjusted(-8, -8, 8, 8));
}

void DesktopPetWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        notifyUserActivity();
        pressed_ = true;
        dragging_ = false;
        pressGlobalPos_ = event->globalPosition().toPoint();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void DesktopPetWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (pressed_ && !dragging_)
    {
        const QPoint now = event->globalPosition().toPoint();
        const int dist = (now - pressGlobalPos_).manhattanLength();
        if (dist >= 4)
        {
            dragging_ = true;
            dragOffset_ = pressGlobalPos_ - frameGeometry().topLeft();
            autoMove_ = false;
            setClickThroughEnabled(false);
        }
    }

    if (dragging_)
    {
        const QPoint desired = event->globalPosition().toPoint() - dragOffset_;
        move(screenClampedTopLeft(desired));
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void DesktopPetWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && pressed_)
    {
        notifyUserActivity();
        const bool wasDragging = dragging_;
        pressed_ = false;
        dragging_ = false;

        if (wasDragging)
        {
            snapIntoScreen();
        }
        else
        {
            emit chatRequested();
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void DesktopPetWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        notifyUserActivity();
        emit activated();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void DesktopPetWidget::contextMenuEvent(QContextMenuEvent *event)
{
    notifyUserActivity();
    QMenu menu(this);
    QAction *chat = menu.addAction("和我聊天");
    QAction *openMain = menu.addAction("打开主界面");
    QAction *toggleMove = menu.addAction(autoMove_ ? "暂停移动" : "继续移动");
    QAction *toggleDock = menu.addAction(dockToEdge_ ? "取消贴边" : "开启贴边");
    QAction *toggleClickThrough = menu.addAction(clickThrough_ ? "关闭点击穿透" : "开启点击穿透");
    QAction *hidePet = menu.addAction("隐藏桌宠");
    QAction *quit = menu.addAction("退出程序");

    QAction *picked = menu.exec(event->globalPos());
    if (picked == chat)
    {
        emit chatRequested();
    }
    else if (picked == openMain)
    {
        emit activated();
    }
    else if (picked == toggleMove)
    {
        autoMove_ = !autoMove_;
    }
    else if (picked == toggleDock)
    {
        dockToEdge_ = !dockToEdge_;
        snapIntoScreen();
    }
    else if (picked == toggleClickThrough)
    {
        setClickThroughEnabled(!clickThrough_);
    }
    else if (picked == hidePet)
    {
        hide();
    }
    else if (picked == quit)
    {
        emit quitRequested();
    }
    updateTray();
}

void DesktopPetWidget::tick()
{
    if (dragging_)
    {
        update();
        return;
    }

    if (autoMove_)
    {
        const QPoint topLeft = frameGeometry().topLeft();
        QPoint delta(0, 0);
        if (dockToEdge_)
        {
            if (dockEdge_ == DockEdge::Top || dockEdge_ == DockEdge::Bottom)
            {
                delta.setX(static_cast<int>(velocity_.x()));
            }
            else
            {
                delta.setY(static_cast<int>(velocity_.y()));
            }
            const QPoint desired = topLeft + delta;
            const QPoint docked = screenDockedTopLeft(desired);
            if (docked.x() != desired.x())
            {
                velocity_.setX(-velocity_.x());
            }
            if (docked.y() != desired.y())
            {
                velocity_.setY(-velocity_.y());
            }
            move(screenDockedTopLeft(frameGeometry().topLeft() + delta));
        }
        else
        {
            const QPoint desired = topLeft + QPoint(static_cast<int>(velocity_.x()), static_cast<int>(velocity_.y()));
            const QPoint clamped = screenClampedTopLeft(desired);
            if (clamped.x() != desired.x())
            {
                velocity_.setX(-velocity_.x());
            }
            if (clamped.y() != desired.y())
            {
                velocity_.setY(-velocity_.y());
            }
            move(screenClampedTopLeft(frameGeometry().topLeft() + QPoint(static_cast<int>(velocity_.x()), static_cast<int>(velocity_.y()))));
        }
    }
    update();
}

void DesktopPetWidget::snapIntoScreen()
{
    const QPoint topLeft = frameGeometry().topLeft();
    if (dockToEdge_)
    {
        QScreen *screen = QGuiApplication::screenAt(topLeft);
        if (!screen)
        {
            screen = QGuiApplication::primaryScreen();
        }
        if (!screen)
        {
            move(topLeft);
            return;
        }

        const QRect avail = screen->availableGeometry();
        const QSize s = frameGeometry().size();

        const int left = avail.left();
        const int right = avail.right() - s.width();
        const int top = avail.top();
        const int bottom = avail.bottom() - s.height();

        const QPoint clamped(std::clamp(topLeft.x(), left, right), std::clamp(topLeft.y(), top, bottom));

        const int dxL = std::abs(clamped.x() - left);
        const int dxR = std::abs(clamped.x() - right);
        const int dyT = std::abs(clamped.y() - top);
        const int dyB = std::abs(clamped.y() - bottom);
        const int minD = std::min(std::min(dxL, dxR), std::min(dyT, dyB));

        if (minD <= dockThresholdPx_)
        {
            move(screenDockedTopLeft(clamped));
        }
        else
        {
            move(clamped);
        }
    }
    else
    {
        move(screenClampedTopLeft(topLeft));
    }
}

QPoint DesktopPetWidget::screenClampedTopLeft(const QPoint &desiredTopLeft) const
{
    QScreen *screen = QGuiApplication::screenAt(desiredTopLeft);
    if (!screen)
    {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen)
    {
        return desiredTopLeft;
    }

    const QRect avail = screen->availableGeometry();
    const QSize s = frameGeometry().size();

    const int x = std::clamp(desiredTopLeft.x(), avail.left(), avail.right() - s.width());
    const int y = std::clamp(desiredTopLeft.y(), avail.top(), avail.bottom() - s.height());
    return QPoint(x, y);
}

QPoint DesktopPetWidget::screenDockedTopLeft(const QPoint &desiredTopLeft)
{
    QScreen *screen = QGuiApplication::screenAt(desiredTopLeft);
    if (!screen)
    {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen)
    {
        return desiredTopLeft;
    }

    const QRect avail = screen->availableGeometry();
    const QSize s = frameGeometry().size();

    const int left = avail.left();
    const int right = avail.right() - s.width();
    const int top = avail.top();
    const int bottom = avail.bottom() - s.height();

    const int dxL = std::abs(desiredTopLeft.x() - left);
    const int dxR = std::abs(desiredTopLeft.x() - right);
    const int dyT = std::abs(desiredTopLeft.y() - top);
    const int dyB = std::abs(desiredTopLeft.y() - bottom);

    int minD = dxL;
    dockEdge_ = DockEdge::Left;
    if (dxR < minD)
    {
        minD = dxR;
        dockEdge_ = DockEdge::Right;
    }
    if (dyT < minD)
    {
        minD = dyT;
        dockEdge_ = DockEdge::Top;
    }
    if (dyB < minD)
    {
        minD = dyB;
        dockEdge_ = DockEdge::Bottom;
    }

    int x = std::clamp(desiredTopLeft.x(), left, right);
    int y = std::clamp(desiredTopLeft.y(), top, bottom);

    if (dockEdge_ == DockEdge::Left)
    {
        x = left;
    }
    else if (dockEdge_ == DockEdge::Right)
    {
        x = right;
    }
    else if (dockEdge_ == DockEdge::Top)
    {
        y = top;
    }
    else
    {
        y = bottom;
    }

    return QPoint(x, y);
}

void DesktopPetWidget::setClickThroughEnabled(bool enabled)
{
    clickThrough_ = enabled;
    setAttribute(Qt::WA_TransparentForMouseEvents, clickThrough_);
}

void DesktopPetWidget::updateTray()
{
    if (!tray_ || !trayMenu_)
    {
        return;
    }

    trayMenu_->clear();
    QAction *chat = trayMenu_->addAction("和我聊天");
    QAction *openMain = trayMenu_->addAction("打开主界面");
    QAction *showPet = trayMenu_->addAction(isVisible() ? "隐藏桌宠" : "显示桌宠");
    QAction *toggleMove = trayMenu_->addAction(autoMove_ ? "暂停移动" : "继续移动");
    QAction *toggleDock = trayMenu_->addAction(dockToEdge_ ? "取消贴边" : "开启贴边");
    QAction *toggleClickThrough = trayMenu_->addAction(clickThrough_ ? "关闭点击穿透" : "开启点击穿透");
    trayMenu_->addSeparator();
    QAction *quit = trayMenu_->addAction("退出程序");

    connect(chat, &QAction::triggered, this, [this]()
            { emit chatRequested(); });
    connect(openMain, &QAction::triggered, this, [this]()
            { emit activated(); });
    connect(showPet, &QAction::triggered, this, [this]()
            {
                if (isVisible())
                {
                    hide();
                }
                else
                {
                    show();
                    raise();
                }
                updateTray();
            });
    connect(toggleMove, &QAction::triggered, this, [this]()
            {
                autoMove_ = !autoMove_;
                updateTray();
            });
    connect(toggleDock, &QAction::triggered, this, [this]()
            {
                dockToEdge_ = !dockToEdge_;
                snapIntoScreen();
                updateTray();
            });
    connect(toggleClickThrough, &QAction::triggered, this, [this]()
            {
                setClickThroughEnabled(!clickThrough_);
                updateTray();
            });
    connect(quit, &QAction::triggered, this, [this]()
            { emit quitRequested(); });

    const GrowthStage stage = growthStageForLevel(level_);
    const int pct = static_cast<int>(growthStageProgress(level_) * 100.0);
    tray_->setToolTip(QString("桌面宠物\nLV %1  EXP %2\n阶段：%3（%4%）").arg(level_).arg(exp_).arg(growthStageName(stage)).arg(pct));
}
