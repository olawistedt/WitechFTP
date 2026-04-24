#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHash>
#include <QMainWindow>
#include <QString>

#include "ftpcommunicator.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void connectOrDisconnect();
  void processItem(QTreeWidgetItem *item);
  void uploadFile();
  void uploadFile(const QString &filePath);
  void downloadFile(const QString &fileName);
  void downloadFolder(const QString &folderName);

  // FTP communicator signal handlers
  void onFtpConnected();
  void onFtpDisconnected();
  void onFtpConnectionError(const QString &error);
  void onFtpStatusUpdated(const QString &message);
  void onFtpDirectoryListReceived();
  void onFtpMd5Received(const QString &fileName, const QString &md5);
  void onFtpDownloadComplete();

  void showLocalContextMenu(const QPoint &pos);
  void showRemoteContextMenu(const QPoint &pos);
  void uploadFolder(const QString &localPath);
  void localItemDoubleClicked(QTreeWidgetItem *item);
  void deleteRemoteFileConfirmed(const QString &fileName);
  void deleteRemoteDirectoryConfirmed(const QString &dirName);
  void createRemoteFolder();
  void createLocalFolder();

private:
  void createUi();
  void logStatus(const QString &message);
  void populateLocalList(const QString &path);

  // Connection widgets
  QLineEdit *hostLineEdit;
  QLineEdit *usernameLineEdit;
  QLineEdit *passwordLineEdit;
  QPushButton *connectButton;

  // File browsers
  QSplitter *splitter;
  QTreeWidget *localListWidget;
  QString m_localCurrentPath;

  QTreeWidget *remoteListWidget;

  // Status log
  QTextEdit *statusLog;

  // FTP communicator
  FtpCommunicator *m_ftpCommunicator;

  // UI state
  QString m_currentRemotePath;
  QHash<QString, FtpCommunicator::RemoteFileInfo> m_remoteFiles;
};

#endif  // MAINWINDOW_H
