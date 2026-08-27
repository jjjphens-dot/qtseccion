#include "mainwindow.h"

#include <QApplication>
#include <QGridLayout>
#include <QFont>
#include <QFile>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFile qssFile(":/style.qss");
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QByteArray content = qssFile.readAll();
        a.setStyleSheet(QString::fromUtf8(content));
        qssFile.close();
    } else {
        qDebug() << "QSS打开失败!";
    }

    MainWindow w;
    w.show();
    return a.exec();
}
