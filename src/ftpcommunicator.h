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
  void getCurrentDirectory();
  void uploadFile(const QString &localPath, const QString &remotePath);
  void uploadFolder(const QString &localPath, const QString &remotePath);
  void downloadFile(const QString &fileName, const QString &localDir);
  void deleteRemoteFile(const QString &fileName, const QString &currentPath);
  void deleteRemoteDirectory(const QString &dirName, const QString &currentPath);
  void createRemoteFolder(const QString &folderName, const QString &currentPath);

  // Getters
  QString getCurrentPath() const { return m_currentPath; }
  bool isDirectory(const QString &name) const { return m_isDirectory.value(name, false); }
  QHash<QString, bool> getDirectories() const { return m_isDirectory; }

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
  void uploadProgress(const QString &fileName);
  void uploadComplete();
  void downloadProgress(const QString &fileName);
  void downloadComplete();
  void deletionComplete();
  void folderCreated();

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
    Size,
    TypeI
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
  void processRemoteDeleteQueue();

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
  QHash<QString, bool> m_isDirectory;

  // Upload state
  QQueue<FtpUploadCommand> m_uploadQueue;
  QFile *m_fileToUpload;
  QString m_pendingRemotePathForUpload;
  qint64 m_localFileSizeForVerify;

  // Download state
  QFile *m_fileToDownload;
  QString m_pendingFileNameForDownload;

  // Delete state
  QString m_remoteFileToDelete;
  QQueue<FtpDeleteCommand> m_remoteDeleteQueue;
  QStack<QString> m_remoteDirsToList;
  QStack<QString> m_remoteDirsToDelete;
  bool m_remoteDeleteInProgress;
  QString m_pendingDeleteListPath;
  QString m_currentDeleteDir;
};

#endif  // FTPCOMMUNICATOR_H
