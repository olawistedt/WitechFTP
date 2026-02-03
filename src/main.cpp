#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QSslSocket>

int
main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  qDebug() << "SSL Library Build Version:" << QSslSocket::sslLibraryBuildVersionString();
  qDebug() << "SSL Library Version:" << QSslSocket::sslLibraryVersionString();
  MainWindow w;
  w.show();
  return a.exec();
}
