#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAbstractSocket>
#include <QByteArray>
#include <QHash>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QQueue>
#include <QStack>
#include <QString>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QTreeView;
class QFileSystemModel;
class QSplitter;
class QListWidget;
class QListWidgetItem;
class QTcpSocket;
class QTextStream;
class QFile;
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
  void processItem(QListWidgetItem *item);
  void uploadFile();
  void downloadFile(const QString &fileName);

  // Control connection slots
  void onControlConnected();
  void onControlReadyRead();
  void onControlDisconnected();
  void onControlError(QAbstractSocket::SocketError socketError);

  // Data connection slots
  void onDataReadyRead();
  void onDataConnected();
  void onDataDisconnected();
  void onDataError(QAbstractSocket::SocketError socketError);
  void showLocalContextMenu(const QPoint &pos);
  void showRemoteContextMenu(const QPoint &pos);
  void uploadFolder(const QString &localPath);
  void localFileDoubleClicked(const QModelIndex &index);
  void deleteRemoteFileConfirmed(const QString &fileName);
  void deleteRemoteDirectoryConfirmed(const QString &dirName);

private:
  void createUi();
  void logStatus(const QString &message);
  void sendCommand(const QString &command);
  void handlePasvResponse(const QString &response);
  void recursivelyPopulateUploadQueue(const QString &localPath, const QString &remotePath);
  void processUploadQueue();
  void processRemoteDeleteQueue();

  // Connection widgets
  QLineEdit *hostLineEdit;
  QLineEdit *usernameLineEdit;
  QLineEdit *passwordLineEdit;
  QPushButton *connectButton;

  // File browsers
  QSplitter *splitter;
  QTreeView *localTreeView;
  QFileSystemModel *localModel;

  QListWidget *remoteListWidget;

  // Status log
  QTextEdit *statusLog;

  // FTP
  QTcpSocket *controlSocket;
  QTcpSocket *dataSocket;
  QTextStream *controlStream;
  QByteArray dataBuffer;
  bool m_waitingForDataConnection = false;
  QTimer *m_keepAliveTimer = nullptr;

  enum class FtpCommand
  {
    None,
    Cwd,
    List,
    Pwd,
    Mkd,
    Stor,
    Retr,
    Dele,
    Rmd,
    ListForDelete,
    Size,
    TypeI
  };
  FtpCommand lastCommand = FtpCommand::None;
  QString pendingPath;

  QString currentPath;
  QHash<QString, bool> isDirectory;
  bool m_isConnected = false;

  // Upload feature
  struct FtpUploadCommand
  {
    enum CommandType
    {
      CreateDirectory,
      UploadFile
    };
    CommandType type;
    QString localPath;   // Full path to local file/dir
    QString remotePath;  // Path on server for creation
  };
  QQueue<FtpUploadCommand> m_uploadQueue;
  QFile *m_fileToUpload = nullptr;
  QString m_pendingRemotePathForUpload;
  qint64 m_localFileSizeForVerify = 0;
  QFile *m_fileToDownload = nullptr;
  QString m_pendingFileNameForDownload;
  QString m_remoteFileToDelete;
  QModelIndex m_localFileToDelete;
  bool m_deleteLocalFile = false;

  struct FtpDeleteCommand
  {
    enum CommandType
    {
      DeleteFile,
      DeleteDir
    };
    CommandType type;
    QString path;
  };
  QQueue<FtpDeleteCommand> m_remoteDeleteQueue;
  QStack<QString> m_remoteDirsToList;
  QStack<QString> m_remoteDirsToDelete;
  bool m_remoteDeleteInProgress = false;
  QString m_pendingDeleteListPath;
  QString m_currentDeleteDir;
};

#endif  // MAINWINDOW_H
