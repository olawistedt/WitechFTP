#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QSslSocket>

int main(int argc, char *argv[])
{
    qDebug() << "SSL Library Build Version:" << QSslSocket::sslLibraryBuildVersionString();
    qDebug() << "SSL Library Version:" << QSslSocket::sslLibraryVersionString();

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
