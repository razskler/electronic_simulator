#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("elsim"));
    QApplication::setOrganizationName(QStringLiteral("elsim"));
    MainWindow w;
    w.show();
    return app.exec();
}
