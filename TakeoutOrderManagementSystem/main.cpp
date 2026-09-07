#include "mainwindow.h"

#include <QApplication>
#include "app/appcontext.h"
#include "app/theme.h"
#include <QCommandLineParser>
#include <QTemporaryDir>
#include <QTimer>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    takeout::applyApplicationTheme(a);
    QCoreApplication::setOrganizationName("QtTraining");
    QCoreApplication::setApplicationName("TakeoutOrderManagementSystem");
    QCoreApplication::setApplicationVersion("0.5.0");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"data-dir", "Override local data directory", "directory"});
    parser.addOption({"smoke-test", "Open architecture shell and exit using an isolated temporary directory"});
    parser.process(a);
    QTemporaryDir smokeDirectory;
    if (parser.isSet("smoke-test") && !smokeDirectory.isValid()) return 2;
    const auto paths = takeout::AppPaths::resolve(parser.isSet("smoke-test")
        ? smokeDirectory.path() : parser.value("data-dir"));
    takeout::AppContext context(paths);
    const auto startup = context.initialize();
    MainWindow w(context, startup);
    w.show();
    if (parser.isSet("smoke-test")) {
        QTimer::singleShot(200, &a, [&a, &startup] { a.exit(startup.ok() ? 0 : 1); });
    }
    return a.exec();
}
