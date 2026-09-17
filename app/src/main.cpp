#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("elsim"));
    QApplication::setOrganizationName(QStringLiteral("elsim"));
    MainWindow w;
    if (QCoreApplication::arguments().contains(QLatin1String("--selftest"))) {
        int rc = w.selfTest();
        return rc;
    }
    w.show();
    return app.exec();
}
