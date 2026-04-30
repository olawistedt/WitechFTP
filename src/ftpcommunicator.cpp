#include "ftpcommunicator.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
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
    , m_downloadInProgress(false)
    , m_control226Received(false)
    , m_dataDisconnected(false)
    , m_remoteDirToDelete("")
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
  if (command.startsWith("PASS ", Qt::CaseInsensitive))
    qDebug() << "C: PASS ******";
  else
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

    if (response.length() < 3)
      continue;

    // Check for multiline response continuation (e.g., "226-...")
    // Only the final line (e.g., "226 OK") should trigger actions.
    if (response.length() >= 4 && response.at(3) == '-')
      continue;

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
    else if (responseCode == 550 && m_lastCommand == FtpCommand::ListForDownload)
    {
      qDebug() << "LIST (download) failed:" << response;
      m_lastCommand = FtpCommand::None;
      processDownloadQueue();
    }
    else if (responseCode >= 400 && m_lastCommand == FtpCommand::Retr)
    {
      qDebug() << "RETR failed:" << response;
      emit statusUpdated(QString("Nedladdning misslyckades: %1").arg(response));
      if (m_fileToDownload)
      {
        m_fileToDownload->close();
        delete m_fileToDownload;
        m_fileToDownload = nullptr;
      }
      m_lastCommand = FtpCommand::None;
      if (m_downloadInProgress)
        processDownloadQueue();
      else
        emit downloadComplete();
    }
    else if (responseCode >= 400 && m_lastCommand == FtpCommand::Stor)
    {
      qDebug() << "STOR failed:" << response;
      emit statusUpdated(QString("Uppladdning misslyckades: %1").arg(response));
      if (m_fileToUpload)
      {
        m_fileToUpload->close();
        delete m_fileToUpload;
        m_fileToUpload = nullptr;
      }
      m_lastCommand = FtpCommand::None;
      if (!m_uploadQueue.isEmpty())
      {
        m_uploadQueue.dequeue();
        processUploadQueue();
      }
      else
        emit uploadComplete();
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
          m_lastCommand == FtpCommand::Retr || m_lastCommand == FtpCommand::ListForDelete ||
          m_lastCommand == FtpCommand::ListForDownload)
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
          m_lastCommand == FtpCommand::Retr || m_lastCommand == FtpCommand::ListForDelete ||
          m_lastCommand == FtpCommand::ListForDownload)
      {
        qDebug() << "Server has closed data connection (226).";
      }

      if (m_lastCommand == FtpCommand::Retr)
      {
        m_control226Received = true;
        if (m_dataDisconnected)
        {
          finalizeDownload();
        }
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
        // Directory might not be empty, try to list it and delete contents
        m_remoteDirsToList.push(m_remoteDirToDelete);
        processRemoteDeleteQueue();
      }
    }
    else if ((responseCode == 251 || responseCode == 200 || responseCode == 213) && m_lastCommand == FtpCommand::Md5)
    {
      // Parse MD5 from response using a regex to find the 32 character hex string
      QRegularExpression md5Regex("([a-fA-F0-9]{32})");
      QRegularExpressionMatch match = md5Regex.match(response);

      if (match.hasMatch())
      {
        QString md5 = match.captured(1).toLower();
        if (!m_md5Queue.isEmpty())
        {
          QString fileName = m_md5Queue.dequeue();
          m_remoteFiles[fileName].md5 = md5;
          emit md5Received(fileName, md5);
        }
      }
      else
      {
        if (!m_md5Queue.isEmpty())
          m_md5Queue.dequeue();
      }
      
      m_lastCommand = FtpCommand::None;
      processMd5Queue();
    }
    else if (responseCode >= 400 && m_lastCommand == FtpCommand::Md5)
    {
      // MD5 command failed or not supported
      if (!m_md5Queue.isEmpty())
        m_md5Queue.dequeue();
      m_lastCommand = FtpCommand::None;
      processMd5Queue();
    }
    else if (responseCode == 350 && m_lastCommand == FtpCommand::Rnfr)
    {  // RNFR successful, send RNTO
      m_lastCommand = FtpCommand::Rnto;
      sendCommand("RNTO " + m_pendingRenameTo);
    }
    else if (responseCode == 250 && m_lastCommand == FtpCommand::Rnto)
    {  // RNTO successful
      emit statusUpdated("Namnbyte lyckades.");
      m_pendingRenameTo.clear();
      m_lastCommand = FtpCommand::List;
      sendCommand("PASV");
    }
    else if (responseCode >= 400 && (m_lastCommand == FtpCommand::Rnfr || m_lastCommand == FtpCommand::Rnto))
    {
      emit statusUpdated(QString("Namnbyte misslyckades: %1").arg(response));
      m_pendingRenameTo.clear();
      m_lastCommand = FtpCommand::None;
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
  m_lastCommand = FtpCommand::None;
  m_md5Queue.clear();
  m_uploadQueue.clear();
  m_downloadQueue.clear();
  m_remoteDeleteQueue.clear();
  m_remoteDirsToList.clear();
  m_remoteDirsToDelete.clear();
  m_remoteDirsToExploreForDownload.clear();
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
  if (m_lastCommand == FtpCommand::List || m_lastCommand == FtpCommand::ListForDelete ||
      m_lastCommand == FtpCommand::ListForDownload)
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
  else if (m_waitingForDataConnection && m_lastCommand == FtpCommand::ListForDownload)
  {
    m_waitingForDataConnection = false;
    sendCommand("LIST " + m_currentExploreDirForDownload);
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

    m_remoteFiles.clear();
    m_md5Queue.clear();

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

      RemoteFileInfo info;
      info.isDir = parts[0].startsWith('d');
      info.size = parts[4].toLongLong();
      info.date = QString("%1 %2 %3").arg(parts[5], parts[6], parts[7]);
      info.md5 = ""; // Will be filled by MD5 command if supported

      m_remoteFiles[name] = info;

      if (!info.isDir) {
          m_md5Queue.enqueue(name);
      }
    }

    m_dataBuffer.clear();
    m_lastCommand = FtpCommand::None;
    emit directoryListReceived();

    // Start fetching MD5s
    QTimer::singleShot(100, this, &FtpCommunicator::processMd5Queue);
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

  if (m_lastCommand == FtpCommand::ListForDownload)
  {
    QString listing(m_dataBuffer);
    QStringList lines = listing.split('\n', Qt::SkipEmptyParts);

    // To map remote to local, we need a base remote dir.
    // However, for now let's just use the leaf folder name as the root locally.
    // Wait, m_localBaseDirForDownload already points to where we want it.
    
    // We need to determine the local path for m_currentExploreDirForDownload
    // If m_currentPath was /a/b and we download folder 'c', 
    // remotePath is /a/b/c and localBaseDir is (say) C:/downloads.
    // We want /a/b/c -> C:/downloads/c
    // /a/b/c/d -> C:/downloads/c/d
    
    // Let's find the relative path from the *parent* of the original download folder.
    // But it's easier to just pass the base remote path.
    // For now, let's assume m_currentPath was the parent at start.
    
    QString baseRemotePath = m_baseRemotePathForDownload;
    if (!baseRemotePath.endsWith('/')) baseRemotePath += "/";
    
    QString relPath = m_currentExploreDirForDownload;
    if (relPath.startsWith(baseRemotePath)) {
        relPath = relPath.mid(baseRemotePath.length());
    }
    
    QString localDirPath = QDir(m_localBaseDirForDownload).filePath(relPath);
    m_downloadQueue.enqueue({ FtpDownloadCommand::CreateLocalDirectory, "", localDirPath });

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

      QString remoteFullPath;
      if (m_currentExploreDirForDownload.endsWith('/'))
        remoteFullPath = m_currentExploreDirForDownload + name;
      else
        remoteFullPath = m_currentExploreDirForDownload + "/" + name;
      
      QString localFullPath = QDir(localDirPath).filePath(name);

      if (isDir)
      {
        m_remoteDirsToExploreForDownload.push(remoteFullPath);
      }
      else
      {
        m_downloadQueue.enqueue({ FtpDownloadCommand::DownloadFile, remoteFullPath, localFullPath });
      }
    }

    m_dataBuffer.clear();
    m_lastCommand = FtpCommand::None;
    processDownloadQueue();
  }

  if (m_lastCommand == FtpCommand::Retr)
  {
    qDebug() << "Data connection disconnected for RETR.";
    if (m_fileToDownload)
    {
      m_fileToDownload->write(m_dataSocket->readAll());
      m_fileToDownload->close();
      delete m_fileToDownload;
      m_fileToDownload = nullptr;
    }
    m_dataDisconnected = true;
    if (m_control226Received)
    {
      finalizeDownload();
    }
  }
}

void
FtpCommunicator::finalizeDownload()
{
  qDebug() << "Finalizing download.";
  m_pendingFileNameForDownload.clear();
  m_control226Received = false;
  m_dataDisconnected = false;

  if (m_downloadInProgress)
  {
    processDownloadQueue();
  }
  else
  {
    emit downloadComplete();
    m_lastCommand = FtpCommand::List;
    sendCommand("PASV");
  }
}

void
FtpCommunicator::abortTransfer()
{
  qDebug() << "Aborting transfers and clearing queues.";
  
  if (m_dataSocket->state() != QAbstractSocket::UnconnectedState)
  {
    m_dataSocket->abort();
  }

  if (m_fileToUpload)
  {
    m_fileToUpload->close();
    delete m_fileToUpload;
    m_fileToUpload = nullptr;
  }

  if (m_fileToDownload)
  {
    m_fileToDownload->close();
    delete m_fileToDownload;
    m_fileToDownload = nullptr;
  }

  m_uploadQueue.clear();
  m_downloadQueue.clear();
  m_downloadInProgress = false;
  m_remoteDeleteInProgress = false;
  m_remoteDeleteQueue.clear();
  m_remoteDirsToList.clear();
  m_remoteDirsToDelete.clear();
  m_remoteDirsToExploreForDownload.clear();
  
  m_lastCommand = FtpCommand::None;
  emit statusUpdated("Överföringar avbrutna.");
  
  // Refresh listing to be safe
  if (m_isConnected)
  {
      m_lastCommand = FtpCommand::List;
      sendCommand("PASV");
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
  m_control226Received = false;
  m_dataDisconnected = false;

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

  emit statusUpdated(QString("Add directory to delete queue: %1").arg(remoteDirPath));
  m_remoteDeleteQueue.enqueue({ FtpDeleteCommand::DeleteDir, remoteDirPath });

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
FtpCommunicator::renameRemote(const QString &oldName, const QString &newName, const QString &currentPath)
{
  QString oldPath;
  QString newPath;
  if (currentPath.endsWith('/'))
  {
    oldPath = currentPath + oldName;
    newPath = currentPath + newName;
  }
  else
  {
    oldPath = currentPath + "/" + oldName;
    newPath = currentPath + "/" + newName;
  }

  m_pendingRenameTo = newPath;
  m_lastCommand = FtpCommand::Rnfr;
  sendCommand("RNFR " + oldPath);
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
      m_remoteDirToDelete = cmd.path;
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
    m_remoteDirToDelete = dirPath;
    m_lastCommand = FtpCommand::Rmd;
    sendCommand("RMD " + dirPath);
    return;
  }

  m_remoteDeleteInProgress = false;
  m_currentDeleteDir.clear();
  m_pendingDeleteListPath.clear();
  m_remoteDirToDelete.clear();
  emit deletionComplete();
  m_lastCommand = FtpCommand::List;
  sendCommand("PASV");
}

void
FtpCommunicator::processMd5Queue()
{
  if (!m_isConnected || m_md5Queue.isEmpty() || m_lastCommand != FtpCommand::None)
    return;

  QString fileName = m_md5Queue.head();
  m_lastCommand = FtpCommand::Md5;
  
  // Use just the filename as we are in the correct directory
  sendCommand("MD5 " + fileName);
}

void
FtpCommunicator::downloadFolder(const QString &remoteFolderName, const QString &localDir)
{
  QString remotePath;
  if (m_currentPath.endsWith('/'))
    remotePath = m_currentPath + remoteFolderName;
  else
    remotePath = m_currentPath + "/" + remoteFolderName;

  m_downloadInProgress = true;
  m_downloadQueue.clear();
  m_remoteDirsToExploreForDownload.clear();
  m_localBaseDirForDownload = localDir;
  m_baseRemotePathForDownload = m_currentPath;

  // Start by exploring the top directory
  m_remoteDirsToExploreForDownload.push(remotePath);
  processDownloadQueue();
}

void
FtpCommunicator::processDownloadQueue()
{
  if (!m_downloadInProgress)
    return;

  // 1. Process files in the download queue
  if (!m_downloadQueue.isEmpty())
  {
    FtpDownloadCommand cmd = m_downloadQueue.dequeue();
    if (cmd.type == FtpDownloadCommand::CreateLocalDirectory)
    {
      QDir().mkpath(cmd.localPath);
      processDownloadQueue();
      return;
    }
    else if (cmd.type == FtpDownloadCommand::DownloadFile)
    {
      m_fileToDownload = new QFile(cmd.localPath);
      if (!m_fileToDownload->open(QIODevice::WriteOnly))
      {
        qDebug() << "Could not open local file for writing:" << cmd.localPath;
        delete m_fileToDownload;
        m_fileToDownload = nullptr;
        processDownloadQueue();
        return;
      }
      m_pendingFileNameForDownload = cmd.remotePath;
      m_control226Received = false;
      m_dataDisconnected = false;
      m_lastCommand = FtpCommand::Retr;
      sendCommand("PASV");
      return;
    }
  }

  // 2. If no files to download, explore more directories
  if (!m_remoteDirsToExploreForDownload.isEmpty())
  {
    m_currentExploreDirForDownload = m_remoteDirsToExploreForDownload.pop();

    // Map remote path to local path
    // We need to know where the "root" of this download is.
    // Let's assume the first dir pushed is the base.
    // Actually, we can just use relative paths if we know the base remote dir.
    // But for simplicity, let's just use the current dir name.

    m_lastCommand = FtpCommand::ListForDownload;
    sendCommand("PASV");
    return;
  }

  // 3. Finished!
  m_downloadInProgress = false;
  emit downloadComplete();
  m_lastCommand = FtpCommand::List;
  sendCommand("PASV");
}
