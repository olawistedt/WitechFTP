#ifndef FTPCOMMUNICATOR_H
#define FTPCOMMUNICATOR_H

#include <QAbstractSocket>
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QQueue>
#include <QStack>
#include <QString>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QTcpSocket;
class QTextStream;
class QFile;
QT_END_NAMESPACE

class FtpCommunicator : public QObject
{
  Q_OBJECT

public:
  explicit FtpCommunicator(QObject *parent = nullptr);
  ~FtpCommunicator();

  // Connection management
  void connectToHost(const QString &host, const QString &username, const QString &password);
  void disconnectFromHost();
  bool isConnected() const { return m_isConnected; }

  // File operations
  void listRemoteDirectory(const QString &path);
  void changeDirectory(const QString &path);
  void uploadFile(const QString &localPath, const QString &remotePath);
  void uploadFolder(const QString &localPath, const QString &remotePath);
  void uploadItems(const QStringList &localPaths, const QString &remotePath);
  void downloadFile(const QString &fileName, const QString &localDir);
  void downloadFolder(const QString &remoteFolderName, const QString &localDir);
  void downloadItems(const QStringList &names, const QString &localDir);
  void deleteRemoteItems(const QStringList &files, const QStringList &dirs, const QString &currentPath);
  void createRemoteFolder(const QString &folderName, const QString &currentPath);
  void renameRemote(const QString &oldName, const QString &newName, const QString &currentPath);
  void abortTransfer();

  struct RemoteFileInfo
  {
    bool isDir;
    QString date;
    qint64 size;
    QString md5;
  };

  // Getters
  QString getCurrentPath() const { return m_currentPath; }
  bool isTransferInProgress() const { return !m_uploadQueue.isEmpty() || m_downloadInProgress || !m_downloadQueue.isEmpty(); }
  bool isDirectory(const QString &name) const { return m_remoteFiles.contains(name) && m_remoteFiles.value(name).isDir; }
  QHash<QString, RemoteFileInfo> getRemoteFiles() const { return m_remoteFiles; }

  // Join a remote parent path and child name with exactly one '/' between them.
  static QString joinPath(const QString &parent, const QString &child);

signals:
  // Connection signals
  void connected();
  void disconnected();
  void connectionError(const QString &error);

  // Status signals
  void statusUpdated(const QString &message);

  // Directory listing signals
  void directoryListReceived();

  // File operation signals
  void uploadComplete();
  void downloadComplete();
  void deletionComplete();
  void transferCountMismatch();
  void md5Received(const QString &fileName, const QString &md5);

private slots:
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

  void onKeepAliveTimeout();
  void processMd5Queue();

private:
  enum class FtpCommand
  {
    None,
    Cwd,
    List,
    Pwd,
    Mkd,
    MkdManual,
    Stor,
    Retr,
    Dele,
    Rmd,
    ListForDelete,
    ListForDownload,
    Size,
    TypeI,
    TypeIForRetr,
    TypeIForStor,
    Md5,
    Rnfr,
    Rnto
  };

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

  struct FtpDownloadCommand
  {
    enum CommandType
    {
      CreateLocalDirectory,
      DownloadFile
    };
    CommandType type;
    QString remotePath;
    QString localPath;
  };

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

  void sendCommand(const QString &command);
  void handlePasvResponse(const QString &response);
  void processUploadQueue();
  void processDownloadQueue();
  void emitCompletionStatus(const QString &verb);
  void processRemoteDeleteQueue();
  void finalizeDownload();

  // Sockets and streams
  QTcpSocket *m_controlSocket;
  QTcpSocket *m_dataSocket;
  QTextStream *m_controlStream;
  QByteArray m_dataBuffer;
  bool m_waitingForDataConnection;
  QTimer *m_keepAliveTimer;

  // Connection state
  bool m_isConnected;
  FtpCommand m_lastCommand;
  QString m_pendingPath;
  QString m_currentPath;
  QString m_pendingRenameTo;
  QHash<QString, RemoteFileInfo> m_remoteFiles;
  QQueue<QString> m_md5Queue;

  // Upload state
  QQueue<FtpUploadCommand> m_uploadQueue;
  QFile *m_fileToUpload;
  QString m_pendingRemotePathForUpload;
  qint64 m_localFileSizeForVerify;

  // Download state
  QFile *m_fileToDownload;
  QString m_pendingFileNameForDownload;
  QQueue<FtpDownloadCommand> m_downloadQueue;
  QStack<QString> m_remoteDirsToExploreForDownload;
  QString m_currentExploreDirForDownload;
  QString m_localBaseDirForDownload;
  QString m_baseRemotePathForDownload;
  bool m_downloadInProgress;

  // Transfer counters
  int m_filesToTransfer = 0;    // accumulates as files are discovered
  int m_expectedTransferCount = 0;  // snapshot taken when pre-count message is shown
  int m_filesTransferred = 0;
  bool m_downloadCountEmitted = false;
  bool m_control226Received;
  bool m_dataDisconnected;

  // Delete state
  QString m_remoteDirToDelete;
  QQueue<FtpDeleteCommand> m_remoteDeleteQueue;
  QStack<QString> m_remoteDirsToList;
  QStack<QString> m_remoteDirsToDelete;
  bool m_remoteDeleteInProgress;
  QString m_pendingDeleteListPath;
  QString m_currentDeleteDir;
};

#endif  // FTPCOMMUNICATOR_H
