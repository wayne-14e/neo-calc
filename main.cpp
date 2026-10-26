#include "calculator.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QStyleFactory>

#ifndef NEO_VERSION
#define NEO_VERSION "1.0.0"
#endif

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Neo"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(NEO_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("Neo"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Neo"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.neo.Neo"));

    // Fusion gives QSS a consistent rendering across Linux, Windows, and macOS.
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        QApplication::setStyle(fusion);
    }

    QFont applicationFont;
    applicationFont.setFamilies({QStringLiteral("Classic Console"),
        QStringLiteral("gallant12x22"), QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("monospace")});
    applicationFont.setStyleHint(QFont::Monospace);
    applicationFont.setPixelSize(13);
    application.setFont(applicationFont);

    Calculator window;
    window.show();

    return application.exec();
}
