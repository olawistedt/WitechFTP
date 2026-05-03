#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHash>
#include <QMainWindow>
#include <QString>

#include "ftpcommunicator.h"

#include <QFileSystemWatcher>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLineEdit;
class QPushButton;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QLabel;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void connectOrDisconnect();
  void onRemoteItemClicked(QTreeWidgetItem *item);
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
  void onLocalItemClicked(QTreeWidgetItem *item);
  void deleteRemoteFileConfirmed(const QString &fileName);
  void deleteRemoteDirectoryConfirmed(const QString &dirName);
  void renameRemoteItem(const QString &oldName);
  void createRemoteFolder();
  void createLocalFolder();
  
  void onSavedSiteSelected(int index);
  void onLocalDirectoryChanged(const QString &path);

private:
  void createUi();
  void logStatus(const QString &message);
  void populateLocalList(const QString &path);
  void loadSavedSites();
  void saveCurrentSite();

  // Connection widgets
  QComboBox *savedSitesComboBox;
  QLineEdit *hostLineEdit;
  QLineEdit *usernameLineEdit;
  QLineEdit *passwordLineEdit;
  QPushButton *connectButton;
  QPushButton *stopButton;
  QLabel *connectionStatusIcon;

  // File browsers
  QSplitter *splitter;
  QTreeWidget *localListWidget;
  QString m_localCurrentPath;
  QFileSystemWatcher *m_localWatcher;

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
