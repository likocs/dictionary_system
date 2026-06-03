#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QString>
#include "login_window.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(0x12, 0x14, 0x18));
    pal.setColor(QPalette::WindowText, QColor(0xEA, 0xEA, 0xEA));
    pal.setColor(QPalette::Base, QColor(0x1A, 0x1D, 0x23));
    pal.setColor(QPalette::AlternateBase, QColor(0x16, 0x18, 0x1D));
    pal.setColor(QPalette::Text, QColor(0xEA, 0xEA, 0xEA));
    pal.setColor(QPalette::Button, QColor(0x26, 0x2B, 0x36));
    pal.setColor(QPalette::ButtonText, QColor(0xEA, 0xEA, 0xEA));
    pal.setColor(QPalette::Highlight, QColor(0x2F, 0x6B, 0xFF));
    pal.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
    pal.setColor(QPalette::ToolTipBase, QColor(0x1A, 0x1D, 0x23));
    pal.setColor(QPalette::ToolTipText, QColor(0xEA, 0xEA, 0xEA));
    app.setPalette(pal);

    const QString qss = QString::fromUtf8(R"(
* {
  font-family: "Microsoft YaHei UI";
  font-size: 13px;
}

QWidget {
  background: #121418;
  color: #EAEAEA;
}

QLabel[role="title"] {
  font-size: 22px;
  font-weight: 700;
}

QLabel[role="subtitle"] {
  color: rgba(234, 234, 234, 0.72);
}

QLineEdit, QTextEdit {
  background: #1A1D23;
  border: 1px solid #2A2F3A;
  border-radius: 10px;
  padding: 8px 10px;
  selection-background-color: #2F6BFF;
}

QLineEdit:focus, QTextEdit:focus {
  border: 1px solid rgba(47, 107, 255, 0.85);
}

QLineEdit::placeholder {
  color: rgba(234, 234, 234, 0.45);
}

QPushButton {
  background: #262B36;
  border: 1px solid #2F3542;
  border-radius: 10px;
  padding: 8px 14px;
}

QPushButton:hover {
  background: #2B3140;
}

QPushButton:pressed {
  background: #202534;
}

QPushButton:disabled {
  color: rgba(234, 234, 234, 0.35);
  background: rgba(38, 43, 54, 0.5);
  border: 1px solid rgba(47, 53, 66, 0.4);
}

QPushButton[kind="primary"] {
  background: #2F6BFF;
  border: 1px solid #2F6BFF;
  color: #FFFFFF;
}

QPushButton[kind="primary"]:hover {
  background: #3D7BFF;
}

QPushButton[kind="primary"]:pressed {
  background: #285CE0;
}

QPushButton[kind="danger"] {
  background: #E74C3C;
  border: 1px solid #E74C3C;
  color: #FFFFFF;
}

QPushButton[kind="danger"]:hover {
  background: #EF5A4B;
}

QTabWidget::pane {
  border: 1px solid #2A2F3A;
  border-radius: 12px;
  top: -1px;
}

QTabBar::tab {
  background: transparent;
  border: 1px solid transparent;
  padding: 10px 14px;
  margin: 4px 4px 0 4px;
  color: rgba(234, 234, 234, 0.75);
}

QTabBar::tab:selected {
  color: #FFFFFF;
  background: #1A1D23;
  border: 1px solid #2A2F3A;
  border-top-left-radius: 10px;
  border-top-right-radius: 10px;
}

QScrollBar:vertical {
  background: transparent;
  width: 10px;
  margin: 0px;
}

QScrollBar::handle:vertical {
  background: rgba(234, 234, 234, 0.25);
  min-height: 24px;
  border-radius: 5px;
}

QScrollBar::handle:vertical:hover {
  background: rgba(234, 234, 234, 0.35);
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
  height: 0px;
}

QScrollBar:horizontal {
  background: transparent;
  height: 10px;
  margin: 0px;
}

QScrollBar::handle:horizontal {
  background: rgba(234, 234, 234, 0.25);
  min-width: 24px;
  border-radius: 5px;
}

QScrollBar::handle:horizontal:hover {
  background: rgba(234, 234, 234, 0.35);
}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
  width: 0px;
}
)");
    app.setStyleSheet(qss);

    LoginWindow w;
    w.show();
    return app.exec();
}
