#include "theme.h"
#include <QApplication>
#include <QFile>
#include <QPalette>

void initializeTakeoutResources() { Q_INIT_RESOURCE(resources); }

namespace takeout {
void applyApplicationTheme(QApplication& application) {
    initializeTakeoutResources();
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f4f6fa"));
    palette.setColor(QPalette::WindowText, QColor("#182230"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#f7f9fc"));
    palette.setColor(QPalette::ToolTipBase, QColor("#182230"));
    palette.setColor(QPalette::ToolTipText, QColor("#ffffff"));
    palette.setColor(QPalette::Text, QColor("#182230"));
    palette.setColor(QPalette::Button, QColor("#ffffff"));
    palette.setColor(QPalette::ButtonText, QColor("#182230"));
    palette.setColor(QPalette::Highlight, QColor("#1769aa"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, QColor("#667085"));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#747b86"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#747b86"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#747b86"));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor("#edf0f4"));
    application.setPalette(palette);
    QFile stylesheet(QStringLiteral(":/style.qss"));
    if (stylesheet.open(QIODevice::ReadOnly)) application.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
}
} // namespace takeout
