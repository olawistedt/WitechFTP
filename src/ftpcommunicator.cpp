#include "ftpcommunicator.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <functional>

FtpCommunicator::FtpCommunicator(QObject *parent)
    : QObject(parent)
    , m_controlSocket(new QTcpSocket(this))
    , m_dataSocket(new QTcpSocket(this))
    , m_controlStream(new QTextStream(m_controlSocket))
    , m_waitingForDataConnection(false)
    , m_keepAliveTimer(new QTimer(this))
    , m_isConnected(false)
    , m_lastCommand(FtpCommand::None)
    , m_fileToUpload(nullptr)
    , m_localFileSizeForVerify(0)
    , m_fileToDownload(nullptr)
    , m_remoteDeleteInProgress(false)
{
  m_keepAliveTimer->setInterval(60000);  // 60 seconds
  connect(m_keepAliveTimer, &QTimer::timeout, this, &FtpCommunicator::onKeepAliveTimeout);

  // Control socket connections
  connect(m_controlSocket, &QTcpSocket::connected, this, &FtpCommunicator::onControlConnected);
  connect(m_controlSocket, &QTcpSocket::readyRead, this, &FtpCommunicator::onControlReadyRead);
  connect(m_controlSocket, &QTcpSocket::disconnected, this, &FtpCommunicator::onControlDisconnected);
  connect(m_controlSocket,
          QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
          this,
          &FtpCommunicator::onControlError);

  // Data socket connections
  connect(m_dataSocket, &QTcpSocket::readyRead, this, &FtpCommunicator::onDataReadyRead);
  connect(m_dataSocket, &QTcpSocket::connected, this, &FtpCommunicator::onDataConnected);
  connect(m_dataSocket, &QTcpSocket::disconnected, this, &FtpCommunicator::onDataDisconnected);
  connect(m_dataSocket,
          QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
          this,
          &FtpCommunicator::onDataError);
}

FtpCommunicator::~FtpCommunicator()
{
  if (m_controlSocket)
  {
    m_controlSocket->blockSignals(true);
    m_controlSocket->disconnect(this);
    if (m_controlSocket->state() != QAbstractSocket::UnconnectedState)
      m_controlSocket->disconnectFromHost();
  }

  if (m_dataSocket)
  {
    m_dataSocket->blockSignals(true);
    m_dataSocket->disconnect(this);
    if (m_dataSocket->state() != QAbstractSocket::UnconnectedState)
      m_dataSocket->disconnectFromHost();
  }

  delete m_controlStream;
  m_controlStream = nullptr;

  if (m_fileToUpload)
  {
    m_fileToUpload->close();
    delete m_fileToUpload;
  }

  if (m_fileToDownload)
  {
    m_fileToDownload->close();
    delete m_fileToDownload;
  }
}

void
FtpCommunicator::connectToHost(const QString &host, const QString &username, const QString &password)
{
  m_controlSocket->connectToHost(host, 21);
  // Store credentials for later use - we'll pass them through slots
  m_controlSocket->setProperty("username", username);
  m_controlSocket->setProperty("password", password);
}

void
FtpCommunicator::disconnectFromHost()
{
  if (m_isConnected)
  {
    m_controlSocket->disconnectFromHost();
  }
}

void
FtpCommunicator::sendCommand(const QString &command)
{
  qDebug() << "C:" << command;
  *m_controlStream << command << "\r\n";
  m_controlStream->flush();
}

void
FtpCommunicator::onControlConnected()
{
  qDebug() << "Control connection established.";
  // The server will send a welcome message, which will be handled in onControlReadyRead
}

void
FtpCommunicator::onControlReadyRead()
{
  while (m_controlSocket->canReadLine())
  {
    QString response = m_controlSocket->readLine().trimmed();
    qDebug() << "S:" << response;

    int responseCode = response.left(3).toInt();

    if (responseCode == 220)
    {  // Welcome message
      QString username = m_controlSocket->property("username").toString();
      sendCommand(QString("USER %1").arg(username));
    }
    else if (responseCode == 331)
    {  // Password required
      QString password = m_controlSocket->property("password").toString();
      sendCommand(QString("PASS %1").arg(password));
    }
    else if (responseCode == 230)
    {  // Login successful
      m_isConnected = true;
      emit connected();
      emit statusUpdated("Inloggad");
      // Set binary mode before anything else
      m_lastCommand = FtpCommand::TypeI;
      sendCommand("TYPE I");
      m_keepAliveTimer->start();
    }
    else if (responseCode == 200 && m_lastCommand == FtpCommand::TypeI)
    {  // TYPE I acknowledged
      m_lastCommand = FtpCommand::Pwd;
      sendCommand("PWD");
    }
    else if (responseCode == 257 && m_lastCommand == FtpCommand::Pwd)
    {  // "PATHNAME" is current directory
      int firstQuote = response.indexOf('"');
      int secondQuote = response.indexOf('"', firstQuote + 1);
      if (firstQuote != -1 && secondQuote != -1 && secondQuote > firstQuote + 1)
      {
        m_currentPath = response.mid(firstQuote + 1, secondQuote - firstQuote - 1);
      }
      else
      {
        m_currentPath = "/";
      }

      m_lastCommand = FtpCommand::List;
      sendCommand("PASV");
    }
    else if (responseCode == 250 && m_lastCommand == FtpCommand::Cwd)
    {  // CWD successful
      m_currentPath = m_pendingPath;
      emit statusUpdated(QString("Öppnar mapp: %1").arg(m_currentPath));
      m_lastCommand = FtpCommand::List;
      sendCommand("PASV");  // Refresh list
    }
    else if (responseCode == 550 && m_lastCommand == FtpCommand::Cwd)
    {
      qDebug() << "CWD failed:" << response;
      m_lastCommand = FtpCommand::None;
    }
    else if (responseCode == 257 && m_lastCommand == FtpCommand::Mkd)
    {  // MKD success (from upload queue)
      m_uploadQueue.dequeue();
      processUploadQueue();
    }
    else if (responseCode == 550 && m_lastCommand == FtpCommand::Mkd)
    {  // MKD failed (maybe dir exists)
      qDebug() << "MKD failed, assuming directory exists:" << response;
      m_uploadQueue.dequeue();
      processUploadQueue();
    }
    else if (responseCode == 257 && m_lastCommand == FtpCommand::MkdManual)
    {  // MKD success (manual folder creation) – refresh listing
      emit folderCreated();
      m_lastCommand = FtpCommand::List;
      sendCommand("PASV");
    }
    else if (responseCode >= 500 && m_lastCommand == FtpCommand::MkdManual)
    {  // MKD failed
      emit statusUpdated(QString("Kunde inte skapa mapp: %1").arg(response));
      m_lastCommand = FtpCommand::None;
    }
    else if (responseCode == 227)
    {  // Entering Passive Mode
      if (m_lastCommand == FtpCommand::List || m_lastCommand == FtpCommand::Stor ||
          m_lastCommand == FtpCommand::Retr)
        handlePasvResponse(response);
    }
    else if (responseCode == 150)
    {  // File status okay; about to open data connection.
      if (m_lastCommand == FtpCommand::List)
        qDebug() << "Server is about to send the list.";
      else if (m_lastCommand == FtpCommand::Stor)
      {
        qDebug() << "Server is ready to receive file.";
        if (m_fileToUpload)
        {
          QByteArray fileContent = m_fileToUpload->readAll();
          m_dataSocket->write(fileContent);
          m_dataSocket->disconnectFromHost();  // Signal that we are done writing
          m_fileToUpload->close();
          delete m_fileToUpload;
          m_fileToUpload = nullptr;
        }
        else
        {
          m_dataSocket->disconnectFromHost();
        }
      }
      else if (m_lastCommand == FtpCommand::Retr)
      {
        qDebug() << "Server is about to send file.";
      }
    }
    else if (responseCode == 226)
    {  // Closing data connection.
      if (m_lastCommand == FtpCommand::List || m_lastCommand == FtpCommand::Stor ||
          m_lastCommand == FtpCommand::Retr || m_lastCommand == FtpCommand::ListForDelete)
      {
        qDebug() << "Server has closed data connection.";
        // The onDataDisconnected slot will handle the parsing
      }
      if (m_lastCommand == FtpCommand::Retr)
      {
        qDebug() << "File download successful.";
        if (m_fileToDownload)
        {
          m_fileToDownload->close();
          delete m_fileToDownload;
          m_fileToDownload = nullptr;
        }
        m_pendingFileNameForDownload.clear();
        emit downloadComplete();
        m_lastCommand = FtpCommand::None;
        // Refresh remote file list
        m_lastCommand = FtpCommand::List;
        sendCommand("PASV");
      }
    }
    else if (responseCode == 250 && m_lastCommand == FtpCommand::Dele)
    {  // DELE successful
      qDebug() << "File deleted successfully.";
      m_remoteFileToDelete.clear();
      m_lastCommand = FtpCommand::None;
      if (m_remoteDeleteInProgress)
      {
        processRemoteDeleteQueue();
      }
      else
      {
        emit deletionComplete();
        // Refresh remote file list
        m_lastCommand = FtpCommand::List;
        sendCommand("PASV");
      }
    }
    else if (responseCode == 250 && m_lastCommand == FtpCommand::Rmd)
    {  // RMD successful
      qDebug() << "Folder deleted successfully.";
      m_lastCommand = FtpCommand::None;
      if (m_remoteDeleteInProgress)
      {
        processRemoteDeleteQueue();
      }
      else
      {
        emit deletionComplete();
        // Refresh remote file list
        m_lastCommand = FtpCommand::List;
        sendCommand("PASV");
      }
    }
    else if (responseCode == 550 && m_lastCommand == FtpCommand::Dele)
    {  // DELE failed
      qDebug() << "Delete failed:" << response;
      m_remoteFileToDelete.clear();
      m_lastCommand = FtpCommand::None;
      if (m_remoteDeleteInProgress)
      {
        m_remoteDeleteInProgress = false;
        m_remoteDeleteQueue.clear();
        m_remoteDirsToList.clear();
        m_remoteDirsToDelete.clear();
      }
    }
    else if (responseCode == 550 && m_lastCommand == FtpCommand::Rmd)
    {  // RMD failed
      qDebug() << "Delete folder failed:" << response;
      m_lastCommand = FtpCommand::None;
      if (m_remoteDeleteInProgress)
      {
        // Directory might not be empty, retry after processing queue
        if (!m_remoteDirsToList.isEmpty() || !m_remoteDeleteQueue.isEmpty())
        {
          // There are still items to process
          QString failedDir = m_remoteDirsToDelete.pop();
          m_remoteDirsToDelete.push(failedDir);  // Put it back on the stack
          processRemoteDeleteQueue();
        }
        else
        {
          processRemoteDeleteQueue();
        }
      }
    }
    else if (responseCode == 213 && m_lastCommand == FtpCommand::Size)
    {  // SIZE response: "213 <bytes>"
      qint64 remoteSize = response.mid(4).trimmed().toLongLong();
      if (remoteSize == m_localFileSizeForVerify)
      {
        emit statusUpdated(QString("Verifiering OK: %1 (%2 byte)").arg(m_pendingRemotePathForUpload).arg(remoteSize));
      }
      else
      {
        emit statusUpdated(QString("VARNING: Storleksskillnad för %1 – lokal: %2 byte, server: %3 byte")
                               .arg(m_pendingRemotePathForUpload)
                               .arg(m_localFileSizeForVerify)
                               .arg(remoteSize));
      }
      m_uploadQueue.dequeue();
      m_pendingRemotePathForUpload.clear();
      m_localFileSizeForVerify = 0;
      m_lastCommand = FtpCommand::None;
      if (!m_uploadQueue.isEmpty())
        processUploadQueue();
      else
      {
        emit uploadComplete();
        m_lastCommand = FtpCommand::List;
        sendCommand("PASV");
      }
    }
    else if (responseCode == 550 && m_lastCommand == FtpCommand::Size)
    {  // SIZE not supported or file not found on server
      emit statusUpdated(QString("VARNING: Kunde inte verifiera %1 – SIZE-kommando stödjs ej av servern").arg(m_pendingRemotePathForUpload));
      m_uploadQueue.dequeue();
      m_pendingRemotePathForUpload.clear();
      m_localFileSizeForVerify = 0;
      m_lastCommand = FtpCommand::None;
      if (!m_uploadQueue.isEmpty())
        processUploadQueue();
      else
      {
        emit uploadComplete();
        m_lastCommand = FtpCommand::List;
        sendCommand("PASV");
      }
    }
  }
}

void
FtpCommunicator::onControlDisconnected()
{
  qDebug() << "Control connection disconnected.";
  emit statusUpdated("Frånkopplad från servern.");
  m_isConnected = false;
  m_keepAliveTimer->stop();
  emit disconnected();
}

void
FtpCommunicator::onControlError(QAbstractSocket::SocketError socketError)
{
  Q_UNUSED(socketError);
  qDebug() << "Control connection error:" << m_controlSocket->errorString();
  emit connectionError(m_controlSocket->errorString());
  onControlDisconnected();
}

void
FtpCommunicator::onDataReadyRead()
{
  if (m_lastCommand == FtpCommand::List || m_lastCommand == FtpCommand::ListForDelete)
  {
    m_dataBuffer.append(m_dataSocket->readAll());
  }
  else if (m_lastCommand == FtpCommand::Retr)
  {
    // Write downloaded data directly to file
    if (m_fileToDownload)
    {
      m_fileToDownload->write(m_dataSocket->readAll());
    }
  }
}

void
FtpCommunicator::onDataConnected()
{
  if (m_waitingForDataConnection && m_lastCommand == FtpCommand::List)
  {
    m_waitingForDataConnection = false;
    sendCommand("LIST");
  }
  else if (m_waitingForDataConnection && m_lastCommand == FtpCommand::ListForDelete)
  {
    m_waitingForDataConnection = false;
    sendCommand("LIST " + m_pendingDeleteListPath);
  }
  else if (m_waitingForDataConnection && m_lastCommand == FtpCommand::Stor)
  {
    m_waitingForDataConnection = false;
    emit statusUpdated(QString("Laddar upp till: %1").arg(m_pendingRemotePathForUpload));
    sendCommand("STOR " + m_pendingRemotePathForUpload);
  }
  else if (m_waitingForDataConnection && m_lastCommand == FtpCommand::Retr)
  {
    m_waitingForDataConnection = false;
    emit statusUpdated(QString("Laddar ner: %1").arg(m_pendingFileNameForDownload));
    sendCommand("RETR " + m_pendingFileNameForDownload);
  }
}

void
FtpCommunicator::onDataDisconnected()
{
  if (m_lastCommand == FtpCommand::List)
  {
    qDebug() << "Data connection disconnected. Processing list.";

    m_isDirectory.clear();

    QString listing(m_dataBuffer);
    QStringList lines = listing.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines)
    {
      QString trimmedLine = line.trimmed();
      QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
      if (parts.size() < 9)
        continue;

      QString name = parts.sliced(8).join(' ');
      if (name == "." || name == "..")
        continue;

      bool isDir = parts[0].startsWith('d');
      m_isDirectory[name] = isDir;
    }

    m_dataBuffer.clear();
    m_lastCommand = FtpCommand::None;
    emit directoryListReceived();
    return;
  }

  if (m_lastCommand == FtpCommand::Stor)
  {
    qDebug() << "File upload complete. Sending SIZE for verification.";
    m_dataBuffer.clear();
    // Send SIZE to verify the uploaded file matches the local file
    m_lastCommand = FtpCommand::Size;
    sendCommand("SIZE " + m_pendingRemotePathForUpload);
    return;
  }

  if (m_lastCommand == FtpCommand::ListForDelete)
  {
    QString listing(m_dataBuffer);
    QStringList lines = listing.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines)
    {
      QString trimmedLine = line.trimmed();
      QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
      if (parts.size() < 9)
        continue;

      QString name = parts.sliced(8).join(' ');
      if (name == "." || name == "..")
        continue;

      bool isDir = parts[0].startsWith('d');

      QString fullPath;
      if (m_currentDeleteDir.endsWith('/'))
        fullPath = m_currentDeleteDir + name;
      else
        fullPath = m_currentDeleteDir + "/" + name;

      if (isDir)
      {
        m_remoteDirsToList.push(fullPath);
      }
      else
      {
        m_remoteDeleteQueue.enqueue({ FtpDeleteCommand::DeleteFile, fullPath });
      }
    }

    m_remoteDirsToDelete.push(m_currentDeleteDir);
    m_dataBuffer.clear();
    m_lastCommand = FtpCommand::None;
    processRemoteDeleteQueue();
  }
}

void
FtpCommunicator::onDataError(QAbstractSocket::SocketError socketError)
{
  if (socketError == QAbstractSocket::RemoteHostClosedError)
  {
    // Normal for FTP data connections: server closes after sending data
    return;
  }
  qDebug() << "Data connection error:" << m_dataSocket->errorString();
}

void
FtpCommunicator::onKeepAliveTimeout()
{
  if (m_isConnected && m_lastCommand == FtpCommand::None)
    sendCommand("NOOP");
}

void
FtpCommunicator::handlePasvResponse(const QString &response)
{
  int openParen = response.indexOf('(');
  int closeParen = response.indexOf(')');
  if (openParen == -1 || closeParen == -1)
  {
    qDebug() << "Could not parse PASV response";
    return;
  }
  QString numbers = response.mid(openParen + 1, closeParen - openParen - 1);
  QStringList parts = numbers.split(',');
  if (parts.size() < 6)
  {
    qDebug() << "Could not parse PASV response parts";
    return;
  }

  QString pasvHost = QString("%1.%2.%3.%4").arg(parts[0]).arg(parts[1]).arg(parts[2]).arg(parts[3]);
  int pasvPort = (parts[4].toInt() * 256) + parts[5].toInt();

  qDebug() << "Attempting data connection to" << pasvHost << pasvPort;
  m_dataBuffer.clear();
  if (m_dataSocket->state() != QAbstractSocket::UnconnectedState)
  {
    m_dataSocket->abort();
  }
  m_waitingForDataConnection = true;
  m_dataSocket->connectToHost(pasvHost, pasvPort);
}

void
FtpCommunicator::listRemoteDirectory(const QString &path)
{
  m_pendingPath = path;
  m_lastCommand = FtpCommand::Cwd;
  sendCommand(QString("CWD %1").arg(path));
}

void
FtpCommunicator::changeDirectory(const QString &path)
{
  m_pendingPath = path;
  m_lastCommand = FtpCommand::Cwd;
  sendCommand(QString("CWD %1").arg(path));
}

void
FtpCommunicator::getCurrentDirectory()
{
  m_lastCommand = FtpCommand::Pwd;
  sendCommand("PWD");
}

void
FtpCommunicator::uploadFile(const QString &localPath, const QString &remotePath)
{
  m_uploadQueue.clear();
  m_uploadQueue.enqueue({ FtpUploadCommand::UploadFile, localPath, remotePath });
  processUploadQueue();
}

void
FtpCommunicator::uploadFolder(const QString &localPath, const QString &remotePath)
{
  m_uploadQueue.clear();
  QFileInfo fileInfo(localPath);
  QString remoteFolderPath;
  if (remotePath.endsWith('/'))
  {
    remoteFolderPath = remotePath + fileInfo.fileName();
  }
  else
  {
    remoteFolderPath = remotePath + "/" + fileInfo.fileName();
  }

  // Helper lambda to recursively populate upload queue
  std::function<void(const QString &, const QString &)> populateQueue =
      [this, &populateQueue](const QString &localDir, const QString &remoteDir)
  {
    QDir dir(localDir);
    if (!dir.exists())
      return;

    // Command to create this directory
    m_uploadQueue.enqueue({ FtpUploadCommand::CreateDirectory, localDir, remoteDir });

    // Enqueue files for upload
    for (const QFileInfo &file : dir.entryInfoList(QDir::Files))
    {
      m_uploadQueue.enqueue(
          { FtpUploadCommand::UploadFile, file.filePath(), remoteDir + "/" + file.fileName() });
    }

    // Recurse into subdirectories
    for (const QFileInfo &subDir : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
      populateQueue(subDir.filePath(), remoteDir + "/" + subDir.fileName());
    }
  };

  populateQueue(localPath, remoteFolderPath);
  processUploadQueue();
}

void
FtpCommunicator::downloadFile(const QString &fileName, const QString &localDir)
{
  QString localFilePath = localDir + "/" + fileName;
  emit statusUpdated(QString("Laddar ner: %1 → %2").arg(fileName, localFilePath));

  // Construct remote file path
  QString remoteFilePath;
  if (m_currentPath.endsWith('/'))
  {
    remoteFilePath = m_currentPath + fileName;
  }
  else
  {
    remoteFilePath = m_currentPath + "/" + fileName;
  }

  // Set up the file for download
  m_fileToDownload = new QFile(localFilePath);
  if (!m_fileToDownload->open(QIODevice::WriteOnly))
  {
    qDebug() << "Could not open local file for writing:" << localFilePath;
    delete m_fileToDownload;
    m_fileToDownload = nullptr;
    return;
  }

  m_pendingFileNameForDownload = remoteFilePath;

  // Initiate download via PASV
  m_lastCommand = FtpCommand::Retr;
  sendCommand("PASV");
}

void
FtpCommunicator::deleteRemoteFile(const QString &fileName, const QString &currentPath)
{
  m_remoteFileToDelete = fileName;

  // Construct remote file path
  QString remoteFilePath;
  if (currentPath.endsWith('/'))
  {
    remoteFilePath = currentPath + fileName;
  }
  else
  {
    remoteFilePath = currentPath + "/" + fileName;
  }

  emit statusUpdated(QString("Raderar fil: %1").arg(remoteFilePath));
  m_lastCommand = FtpCommand::Dele;
  sendCommand("DELE " + remoteFilePath);
}

void
FtpCommunicator::deleteRemoteDirectory(const QString &dirName, const QString &currentPath)
{
  QString remoteDirPath;
  if (currentPath.endsWith('/'))
  {
    remoteDirPath = currentPath + dirName;
  }
  else
  {
    remoteDirPath = currentPath + "/" + dirName;
  }

  m_remoteDeleteInProgress = true;
  m_remoteDeleteQueue.clear();
  m_remoteDirsToList.clear();
  m_remoteDirsToDelete.clear();

  m_remoteDirsToList.push(remoteDirPath);
  processRemoteDeleteQueue();
}

void
FtpCommunicator::createRemoteFolder(const QString &folderName, const QString &currentPath)
{
  QString remotePath;
  if (currentPath.endsWith('/'))
    remotePath = currentPath + folderName;
  else
    remotePath = currentPath + "/" + folderName;

  emit statusUpdated(QString("Skapar mapp: %1").arg(remotePath));
  m_lastCommand = FtpCommand::MkdManual;
  sendCommand("MKD " + remotePath);
}

void
FtpCommunicator::processUploadQueue()
{
  if (m_uploadQueue.isEmpty())
  {
    qDebug() << "Upload queue finished.";
    emit uploadComplete();
    // Refresh remote file list
    m_lastCommand = FtpCommand::List;
    sendCommand("PASV");
    return;
  }

  FtpUploadCommand command = m_uploadQueue.head();  // Peek at the command

  if (command.type == FtpUploadCommand::CreateDirectory)
  {
    emit statusUpdated(QString("Skapar mapp: %1").arg(command.remotePath));
    m_lastCommand = FtpCommand::Mkd;
    sendCommand("MKD " + command.remotePath);
  }
  else if (command.type == FtpUploadCommand::UploadFile)
  {
    // For STOR, we also need to enter passive mode
    m_lastCommand = FtpCommand::Stor;
    m_fileToUpload = new QFile(command.localPath);
    if (!m_fileToUpload->open(QIODevice::ReadOnly))
    {
      qDebug() << "Could not open local file for reading:" << command.localPath;
      // Dequeue and try next
      m_uploadQueue.dequeue();
      delete m_fileToUpload;
      m_fileToUpload = nullptr;
      processUploadQueue();  // process next
      return;
    }
    emit statusUpdated(QString("Förbereder uppladdning: %1 → %2").arg(command.localPath, command.remotePath));
    m_pendingRemotePathForUpload = command.remotePath;
    m_localFileSizeForVerify = m_fileToUpload->size();
    sendCommand("PASV");
  }
}

void
FtpCommunicator::processRemoteDeleteQueue()
{
  if (!m_remoteDeleteInProgress)
    return;

  if (!m_remoteDeleteQueue.isEmpty())
  {
    FtpDeleteCommand cmd = m_remoteDeleteQueue.dequeue();
    if (cmd.type == FtpDeleteCommand::DeleteFile)
    {
      emit statusUpdated(QString("Raderar fil: %1").arg(cmd.path));
      m_lastCommand = FtpCommand::Dele;
      sendCommand("DELE " + cmd.path);
      return;
    }
    if (cmd.type == FtpDeleteCommand::DeleteDir)
    {
      emit statusUpdated(QString("Raderar mapp: %1").arg(cmd.path));
      m_lastCommand = FtpCommand::Rmd;
      sendCommand("RMD " + cmd.path);
      return;
    }
  }

  if (!m_remoteDirsToList.isEmpty())
  {
    m_currentDeleteDir = m_remoteDirsToList.pop();
    m_pendingDeleteListPath = m_currentDeleteDir;
    m_lastCommand = FtpCommand::ListForDelete;
    sendCommand("PASV");
    return;
  }

  if (!m_remoteDirsToDelete.isEmpty())
  {
    QString dirPath = m_remoteDirsToDelete.pop();
    emit statusUpdated(QString("Raderar mapp: %1").arg(dirPath));
    m_lastCommand = FtpCommand::Rmd;
    sendCommand("RMD " + dirPath);
    return;
  }

  m_remoteDeleteInProgress = false;
  m_currentDeleteDir.clear();
  m_pendingDeleteListPath.clear();
  emit deletionComplete();
  m_lastCommand = FtpCommand::List;
  sendCommand("PASV");
}
