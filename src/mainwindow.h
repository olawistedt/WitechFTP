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

struct LangStrings;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

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
  void onFtpUploadComplete();

  void showLocalContextMenu(const QPoint &pos);
  void showRemoteContextMenu(const QPoint &pos);
  void uploadFolder(const QString &localPath);
  void onLocalItemClicked(QTreeWidgetItem *item);
  void renameRemoteItem(const QString &oldName);
  void renameLocalItem(const QString &oldPath);
  void createRemoteFolder();
  void createLocalFolder();

  void onSavedSiteSelected(int index);
  void onLocalDirectoryChanged(const QString &path);

  // Direct delete handlers (called by keyboard shortcut)
  void deleteLocalItemDirectly();
  void deleteRemoteItemDirectly();

  void onLanguageChanged(int index);

private:
  void createUi();
  void retranslateUi();
  void logStatus(const QString &message);
  void populateLocalList(const QString &path);
  void loadSavedSites();
  void saveCurrentSite();

  // Helpers
  static QString pathFromItem(QTreeWidgetItem *item);
  static QString remoteNameFromItem(QTreeWidgetItem *item);
  QString promptForName(const QString &title, const QString &prompt, const QString &initial = QString());
  void deleteLocalPaths(const QStringList &paths);
  void deleteRemoteNames(const QStringList &names);

  // Language
  int m_language; // 0 = Swedish, 1 = English
  const LangStrings *m_s;

  // Connection widgets
  QComboBox *savedSitesComboBox;
  QLineEdit *hostLineEdit;
  QLineEdit *usernameLineEdit;
  QLineEdit *passwordLineEdit;
  QPushButton *connectButton;
  QPushButton *stopButton;
  QLabel *connectionStatusIcon;
  QLabel *m_labelHost;
  QLabel *m_labelUsername;
  QLabel *m_labelPassword;
  QComboBox *m_langCombo;

  // File browsers
  QSplitter *splitter;
  QLineEdit *localPathEdit;
  QTreeWidget *localListWidget;
  QString m_localCurrentPath;
  QFileSystemWatcher *m_localWatcher;

  QLineEdit *remotePathEdit;
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
