#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHash>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QSplitter;
class QListWidget;
class QListWidgetItem;
class QTextEdit;
QT_END_NAMESPACE

class FtpCommunicator;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void connectOrDisconnect();
  void processItem(QListWidgetItem *item);
  void uploadFile();
  void uploadFile(const QString &filePath);
  void downloadFile(const QString &fileName);

  // FTP communicator signal handlers
  void onFtpConnected();
  void onFtpDisconnected();
  void onFtpConnectionError(const QString &error);
  void onFtpStatusUpdated(const QString &message);
  void onFtpDirectoryListReceived();

  void showLocalContextMenu(const QPoint &pos);
  void showRemoteContextMenu(const QPoint &pos);
  void uploadFolder(const QString &localPath);
  void localItemDoubleClicked(QListWidgetItem *item);
  void deleteRemoteFileConfirmed(const QString &fileName);
  void deleteRemoteDirectoryConfirmed(const QString &dirName);
  void createRemoteFolder();

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
  QListWidget *localListWidget;
  QString m_localCurrentPath;

  QListWidget *remoteListWidget;

  // Status log
  QTextEdit *statusLog;

  // FTP communicator
  FtpCommunicator *m_ftpCommunicator;

  // UI state
  QString m_currentRemotePath;
  QHash<QString, bool> m_remoteIsDirectory;
};

#endif  // MAINWINDOW_H
