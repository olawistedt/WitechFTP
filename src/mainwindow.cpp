#include "mainwindow.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QStyle>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
  controlSocket = new QTcpSocket(this);
  controlStream = new QTextStream(controlSocket);
  dataSocket = new QTcpSocket(this);

  connect(controlSocket, &QTcpSocket::connected, this, &MainWindow::onControlConnected);
  connect(controlSocket, &QTcpSocket::readyRead, this, &MainWindow::onControlReadyRead);
  connect(controlSocket, &QTcpSocket::disconnected, this, &MainWindow::onControlDisconnected);
  connect(controlSocket,
          QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
          this,
          &MainWindow::onControlError);

  connect(dataSocket, &QTcpSocket::readyRead, this, &MainWindow::onDataReadyRead);
  connect(dataSocket, &QTcpSocket::connected, this, &MainWindow::onDataConnected);
  connect(dataSocket, &QTcpSocket::disconnected, this, &MainWindow::onDataDisconnected);
  connect(dataSocket,
          QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
          this,
          &MainWindow::onDataError);

  createUi();

  setWindowTitle("Qt FTP Client");
  setWindowIcon(QIcon(":/ftp-icon.png"));

  resize(800, 800);
  setGeometry(QStyle::alignedRect(Qt::LeftToRight,
                                  Qt::AlignCenter,
                                  size(),
                                  QGuiApplication::primaryScreen()->availableGeometry()));

  // Auto-connect on startup
  QTimer::singleShot(100, this, &MainWindow::connectOrDisconnect);
}

MainWindow::~MainWindow()
{
  if (controlSocket)
  {
    controlSocket->blockSignals(true);
    controlSocket->disconnect(this);
    if (controlSocket->state() != QAbstractSocket::UnconnectedState)
      controlSocket->disconnectFromHost();
  }

  if (dataSocket)
  {
    dataSocket->blockSignals(true);
    dataSocket->disconnect(this);
    if (dataSocket->state() != QAbstractSocket::UnconnectedState)
      dataSocket->disconnectFromHost();
  }

  delete controlStream;
  controlStream = nullptr;
}

void
MainWindow::createUi()
{
  // This function remains largely the same as it just sets up the UI.
  // --- Connection Bar ---
  QWidget *connectionWidget = new QWidget;
  QHBoxLayout *connectionLayout = new QHBoxLayout(connectionWidget);
  hostLineEdit = new QLineEdit("ftp.witech.se");
  hostLineEdit->setPlaceholderText("FTP Host");
  usernameLineEdit = new QLineEdit("witech.se");
  usernameLineEdit->setPlaceholderText("Username");
  passwordLineEdit = new QLineEdit;
  passwordLineEdit->setPlaceholderText("Password");
  passwordLineEdit->setEchoMode(QLineEdit::Password);

  // Read password from environment variable if it exists
  QString envPassword = QProcessEnvironment::systemEnvironment().value("FTP_PASSWORD", "");
  if (!envPassword.isEmpty())
  {
    passwordLineEdit->setText(envPassword);
  }

  connectButton = new QPushButton("Connect");

  connectionLayout->addWidget(new QLabel("Host:"));
  connectionLayout->addWidget(hostLineEdit);
  connectionLayout->addWidget(new QLabel("Username:"));
  connectionLayout->addWidget(usernameLineEdit);
  connectionLayout->addWidget(new QLabel("Password:"));
  connectionLayout->addWidget(passwordLineEdit);
  connectionLayout->addWidget(connectButton);
  connectionLayout->setContentsMargins(5, 5, 5, 5);
  connectionLayout->setSpacing(5);
  connectionWidget->setMaximumHeight(50);

  // --- File Browsers ---
  splitter = new QSplitter;
  splitter->setOrientation(Qt::Horizontal);

  // Local
  QString localStartPath =
      QProcessEnvironment::systemEnvironment().value("USERPROFILE", QDir::homePath());
  localModel = new QFileSystemModel;
  localModel->setRootPath(localStartPath);

  QWidget *localWidget = new QWidget(splitter);
  QVBoxLayout *localLayout = new QVBoxLayout(localWidget);
  localLayout->setContentsMargins(0, 0, 0, 0);

  QPushButton *upButton = new QPushButton("Up", localWidget);
  upButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogToParent));
  connect(upButton,
          &QPushButton::clicked,
          this,
          [this]()
          {
            QModelIndex current = localTreeView->rootIndex();
            if (current.isValid())
              localTreeView->setRootIndex(current.parent());
          });
  localLayout->addWidget(upButton);

  localTreeView = new QTreeView(localWidget);
  localTreeView->setModel(localModel);
  localTreeView->setRootIndex(localModel->index(localStartPath));
  localTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
  localTreeView->setColumnWidth(0, 1360);
  localTreeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  localLayout->addWidget(localTreeView);

  // Remote
  remoteListWidget = new QListWidget(splitter);
  remoteListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  remoteListWidget->setSpacing(0);
  remoteListWidget->setIconSize(QSize(16, 16));

  splitter->addWidget(localWidget);
  splitter->addWidget(remoteListWidget);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);

  // --- Main Layout ---
  QWidget *centralWidget = new QWidget;
  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  mainLayout->addWidget(connectionWidget);
  mainLayout->addWidget(splitter, 1);

  setCentralWidget(centralWidget);

  // --- Connections ---
  connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectOrDisconnect);
  connect(remoteListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::processItem);
  connect(localTreeView,
          &QTreeView::customContextMenuRequested,
          this,
          &MainWindow::showLocalContextMenu);
  connect(remoteListWidget,
          &QListWidget::customContextMenuRequested,
          this,
          &MainWindow::showRemoteContextMenu);
  connect(localTreeView, &QTreeView::doubleClicked, this, &MainWindow::localFileDoubleClicked);
}

void
MainWindow::connectOrDisconnect()
{
  if (m_isConnected)
  {
    controlSocket->disconnectFromHost();
    return;
  }

  // Check if password field is empty, if so, prompt user
  QString password = passwordLineEdit->text();
  if (password.isEmpty())
  {
    bool ok;
    password = QInputDialog::getText(this,
                                     "FTP Password",
                                     "Enter FTP password:",
                                     QLineEdit::Password,
                                     "",
                                     &ok);
    if (!ok || password.isEmpty())
    {
      return;  // User cancelled or didn't enter password
    }
    passwordLineEdit->setText(password);
  }

  controlSocket->connectToHost(hostLineEdit->text(), 21);
  connectButton->setText("Connecting...");
  hostLineEdit->setEnabled(false);
  usernameLineEdit->setEnabled(false);
  passwordLineEdit->setEnabled(false);
}

void
MainWindow::sendCommand(const QString &command)
{
  qDebug() << "C:" << command;
  *controlStream << command << "\r\n";
  controlStream->flush();
}

void
MainWindow::handlePasvResponse(const QString &response)
{
  // Response is like: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
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
  dataBuffer.clear();
  if (dataSocket->state() != QAbstractSocket::UnconnectedState)
  {
    dataSocket->abort();
  }
  m_waitingForDataConnection = true;
  dataSocket->connectToHost(pasvHost, pasvPort);
}

// --- Control Connection Slots ---

void
MainWindow::onControlConnected()
{
  qDebug() << "Control connection established.";
  // The server will send a welcome message, which will be handled in onControlReadyRead
}

void
MainWindow::onControlReadyRead()
{
  while (controlSocket->canReadLine())
  {
    QString response = controlSocket->readLine().trimmed();
    qDebug() << "S:" << response;

    int responseCode = response.left(3).toInt();

    if (responseCode == 220)
    {  // Welcome message
      sendCommand(QString("USER %1").arg(usernameLineEdit->text()));
    }
    else if (responseCode == 331)
    {  // Password required
      sendCommand(QString("PASS %1").arg(passwordLineEdit->text()));
    }
    else if (responseCode == 230)
    {  // Login successful
      m_isConnected = true;
      connectButton->setText("Disconnect");
      // Ask server for current directory so we can build correct paths
      lastCommand = FtpCommand::Pwd;
      sendCommand("PWD");
    }
    else if (responseCode == 257 && lastCommand == FtpCommand::Pwd)
    {  // "PATHNAME" is current directory
      int firstQuote = response.indexOf('"');
      int secondQuote = response.indexOf('"', firstQuote + 1);
      if (firstQuote != -1 && secondQuote != -1 && secondQuote > firstQuote + 1)
      {
        currentPath = response.mid(firstQuote + 1, secondQuote - firstQuote - 1);
      }
      else
      {
        currentPath = "/";
      }

      lastCommand = FtpCommand::List;
      sendCommand("PASV");
    }
    else if (responseCode == 250 && lastCommand == FtpCommand::Cwd)
    {  // CWD successful
      currentPath = pendingPath;
      lastCommand = FtpCommand::List;
      sendCommand("PASV");  // Refresh list
    }
    else if (responseCode == 550 && lastCommand == FtpCommand::Cwd)
    {
      qDebug() << "CWD failed:" << response;
      lastCommand = FtpCommand::None;
    }
    else if (responseCode == 257 && lastCommand == FtpCommand::Mkd)
    {  // MKD success
      m_uploadQueue.dequeue();
      processUploadQueue();
    }
    else if (responseCode == 550 && lastCommand == FtpCommand::Mkd)
    {  // MKD failed (maybe dir exists)
      qDebug() << "MKD failed, assuming directory exists:" << response;
      m_uploadQueue.dequeue();
      processUploadQueue();
    }
    else if (responseCode == 227)
    {  // Entering Passive Mode
      if (lastCommand == FtpCommand::List || lastCommand == FtpCommand::Stor ||
          lastCommand == FtpCommand::Retr)
        handlePasvResponse(response);
    }
    else if (responseCode == 150)
    {  // File status okay; about to open data connection.
      if (lastCommand == FtpCommand::List)
        qDebug() << "Server is about to send the list.";
      else if (lastCommand == FtpCommand::Stor)
      {
        qDebug() << "Server is ready to receive file.";
        QByteArray fileContent = m_fileToUpload->readAll();
        dataSocket->write(fileContent);
        dataSocket->disconnectFromHost();  // Signal that we are done writing
        m_fileToUpload->close();
        delete m_fileToUpload;
        m_fileToUpload = nullptr;
      }
      else if (lastCommand == FtpCommand::Retr)
      {
        qDebug() << "Server is about to send file.";
      }
    }
    else if (responseCode == 226)
    {  // Closing data connection.
      if (lastCommand == FtpCommand::List || lastCommand == FtpCommand::Stor ||
          lastCommand == FtpCommand::Retr || lastCommand == FtpCommand::ListForDelete)
      {
        qDebug() << "Server has closed data connection.";
        // The onDataDisconnected slot will handle the parsing
      }
      if (lastCommand == FtpCommand::Retr)
      {
        qDebug() << "File download successful.";
        if (m_fileToDownload)
        {
          m_fileToDownload->close();
          delete m_fileToDownload;
          m_fileToDownload = nullptr;
        }
        m_pendingFileNameForDownload.clear();
        lastCommand = FtpCommand::None;
        // Refresh remote file list
        lastCommand = FtpCommand::List;
        sendCommand("PASV");
      }
    }
    else if (responseCode == 250 && lastCommand == FtpCommand::Dele)
    {  // DELE successful
      qDebug() << "File deleted successfully.";
      m_remoteFileToDelete.clear();
      lastCommand = FtpCommand::None;
      if (m_remoteDeleteInProgress)
      {
        processRemoteDeleteQueue();
      }
      else
      {
        // Refresh remote file list
        lastCommand = FtpCommand::List;
        sendCommand("PASV");
      }
    }
    else if (responseCode == 250 && lastCommand == FtpCommand::Rmd)
    {  // RMD successful
      qDebug() << "Folder deleted successfully.";
      lastCommand = FtpCommand::None;
      if (m_remoteDeleteInProgress)
      {
        processRemoteDeleteQueue();
      }
      else
      {
        // Refresh remote file list
        lastCommand = FtpCommand::List;
        sendCommand("PASV");
      }
    }
    else if (responseCode == 550 && lastCommand == FtpCommand::Dele)
    {  // DELE failed
      qDebug() << "Delete failed:" << response;
      QMessageBox::critical(this, "Delete Failed", "Failed to delete remote file.");
      m_remoteFileToDelete.clear();
      lastCommand = FtpCommand::None;
      if (m_remoteDeleteInProgress)
      {
        m_remoteDeleteInProgress = false;
        m_remoteDeleteQueue.clear();
        m_remoteDirsToList.clear();
        m_remoteDirsToDelete.clear();
      }
    }
    else if (responseCode == 550 && lastCommand == FtpCommand::Rmd)
    {  // RMD failed
      qDebug() << "Delete folder failed:" << response;
      lastCommand = FtpCommand::None;
      if (m_remoteDeleteInProgress)
      {
        // Directory might not be empty, retry after processing queue
        // Re-queue the directory to try again later
        if (!m_remoteDirsToList.isEmpty() || !m_remoteDeleteQueue.isEmpty())
        {
          // There are still items to process, put this dir back and try again
          QString failedDir = m_remoteDirsToDelete.pop();
          m_remoteDirsToDelete.push(failedDir);  // Put it back on the stack
          processRemoteDeleteQueue();
        }
        else
        {
          // All contents should be deleted, try RMD one more time
          processRemoteDeleteQueue();
        }
      }
      else
      {
        QMessageBox::critical(this, "Delete Failed", "Failed to delete remote folder.");
      }
    }
  }
}

void
MainWindow::onControlDisconnected()
{
  qDebug() << "Control connection disconnected.";
  m_isConnected = false;
  remoteListWidget->clear();
  connectButton->setText("Connect");
  hostLineEdit->setEnabled(true);
  usernameLineEdit->setEnabled(true);
  passwordLineEdit->setEnabled(true);
}

void
MainWindow::onControlError(QAbstractSocket::SocketError socketError)
{
  Q_UNUSED(socketError);
  qDebug() << "Control connection error:" << controlSocket->errorString();
  QMessageBox::critical(this, "Connection Error", controlSocket->errorString());
  onControlDisconnected();  // Reset UI
}

// --- Data Connection Slots ---

void
MainWindow::onDataReadyRead()
{
  if (lastCommand == FtpCommand::List || lastCommand == FtpCommand::ListForDelete)
  {
    dataBuffer.append(dataSocket->readAll());
  }
  else if (lastCommand == FtpCommand::Retr)
  {
    // Write downloaded data directly to file
    if (m_fileToDownload)
    {
      m_fileToDownload->write(dataSocket->readAll());
    }
  }
}

void
MainWindow::onDataConnected()
{
  if (m_waitingForDataConnection && lastCommand == FtpCommand::List)
  {
    m_waitingForDataConnection = false;
    sendCommand("LIST");
  }
  else if (m_waitingForDataConnection && lastCommand == FtpCommand::ListForDelete)
  {
    m_waitingForDataConnection = false;
    sendCommand("LIST " + m_pendingDeleteListPath);
  }
  else if (m_waitingForDataConnection && lastCommand == FtpCommand::Stor)
  {
    m_waitingForDataConnection = false;
    sendCommand("STOR " + m_pendingRemotePathForUpload);
  }
  else if (m_waitingForDataConnection && lastCommand == FtpCommand::Retr)
  {
    m_waitingForDataConnection = false;
    sendCommand("RETR " + m_pendingFileNameForDownload);
  }
}

void
MainWindow::onDataDisconnected()
{
  if (lastCommand == FtpCommand::List)
  {
    qDebug() << "Data connection disconnected. Processing list.";

    remoteListWidget->clear();
    isDirectory.clear();

    // Add ".." to navigate up, unless we are at root
    if (currentPath != "/")
    {
      QListWidgetItem *upItem = new QListWidgetItem("..");
      upItem->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
      remoteListWidget->addItem(upItem);
      isDirectory[".."] = true;
    }

    QString listing(dataBuffer);
    QStringList lines = listing.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines)
    {
      // This is a very basic UNIX `ls -l` parser, not robust!
      QString trimmedLine = line.trimmed();
      QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
      if (parts.size() < 9)
        continue;

      QString name = parts.sliced(8).join(' ');
      if (name == "." || name == "..")
        continue;  // Already handled

      bool isDir = parts[0].startsWith('d');
      isDirectory[name] = isDir;

      QListWidgetItem *item = new QListWidgetItem(name);
      if (isDir)
      {
        item->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
      }
      else
      {
        item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
      }
      remoteListWidget->addItem(item);
    }

    dataBuffer.clear();
    lastCommand = FtpCommand::None;
    return;
  }

  if (lastCommand == FtpCommand::Stor)
  {
    qDebug() << "File upload complete. Dequeuing and refreshing list.";
    m_uploadQueue.dequeue();
    m_pendingRemotePathForUpload.clear();

    dataBuffer.clear();
    lastCommand = FtpCommand::None;

    // Process next upload or refresh the list
    if (m_uploadQueue.isEmpty())
    {
      // All uploads done, refresh the remote file list
      lastCommand = FtpCommand::List;
      sendCommand("PASV");
    }
    else
    {
      // Process next upload
      processUploadQueue();
    }
    return;
  }

  if (lastCommand == FtpCommand::ListForDelete)
  {
    QString listing(dataBuffer);
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
    dataBuffer.clear();
    lastCommand = FtpCommand::None;
    processRemoteDeleteQueue();
  }
}

void
MainWindow::onDataError(QAbstractSocket::SocketError socketError)
{
  if (socketError == QAbstractSocket::RemoteHostClosedError)
  {
    // Normal for FTP data connections: server closes after sending data
    return;
  }
  qDebug() << "Data connection error:" << dataSocket->errorString();
}


void
MainWindow::showLocalContextMenu(const QPoint &pos)
{
  QModelIndex index = localTreeView->indexAt(pos);
  if (!index.isValid())
  {
    return;
  }

  QMenu contextMenu(this);

  // Check if it's a directory
  if (localModel->isDir(index))
  {
    QAction *uploadAction = contextMenu.addAction("Upload folder to server");
    QAction *deleteFolderAction = contextMenu.addAction("Delete folder");

    QAction *selectedAction = contextMenu.exec(localTreeView->viewport()->mapToGlobal(pos));

    if (selectedAction == uploadAction)
    {
      if (!m_isConnected)
      {
        QMessageBox::warning(this, "Not Connected", "Connect to the server to upload folders.");
        return;
      }
      QString localPath = localModel->filePath(index);
      uploadFolder(localPath);
    }
    else if (selectedAction == deleteFolderAction)
    {
      QString localPath = localModel->filePath(index);
      QFileInfo fileInfo(localPath);
      QString dirName = fileInfo.fileName();

      QMessageBox::StandardButton reply =
          QMessageBox::question(this,
                                "Delete Folder",
                                QString("Are you sure you want to delete '%1'?").arg(dirName),
                                QMessageBox::Yes | QMessageBox::No);

      if (reply == QMessageBox::Yes)
      {
        QDir dir(localPath);
        if (dir.removeRecursively())
        {
          qDebug() << "Folder deleted:" << localPath;
        }
        else
        {
          QMessageBox::critical(this, "Error", QString("Could not delete folder: %1").arg(dirName));
        }
      }
    }
  }
  else
  {
    // It's a file - add delete option
    QAction *deleteAction = contextMenu.addAction("Delete");

    QAction *selectedAction = contextMenu.exec(localTreeView->viewport()->mapToGlobal(pos));

    if (selectedAction == deleteAction)
    {
      m_localFileToDelete = index;
      QFileInfo fileInfo(localModel->filePath(index));
      QString fileName = fileInfo.fileName();

      QMessageBox::StandardButton reply =
          QMessageBox::question(this,
                                "Delete File",
                                QString("Are you sure you want to delete '%1'?").arg(fileName),
                                QMessageBox::Yes | QMessageBox::No);

      if (reply == QMessageBox::Yes)
      {
        QString filePath = localModel->filePath(index);
        if (QFile::remove(filePath))
        {
          qDebug() << "File deleted:" << filePath;
          // File deletion doesn't require refreshing the tree view in most cases,
          // but Qt's file system model should handle it automatically
        }
        else
        {
          QMessageBox::critical(this, "Error", QString("Could not delete file: %1").arg(fileName));
        }
      }
    }
  }
}

void
MainWindow::showRemoteContextMenu(const QPoint &pos)
{
  if (!m_isConnected)
    return;

  QListWidgetItem *item = remoteListWidget->itemAt(pos);
  if (!item)
    return;

  QString itemName = item->text();

  // Skip ".." directory in context menu
  if (itemName == "..")
    return;

  QMenu contextMenu(this);

  // Check if it's a directory or file
  if (!isDirectory.value(itemName, false))
  {
    // It's a file - add delete option
    QAction *deleteAction = contextMenu.addAction("Delete");

    QAction *selectedAction = contextMenu.exec(remoteListWidget->viewport()->mapToGlobal(pos));

    if (selectedAction == deleteAction)
    {
      QMessageBox::StandardButton reply =
          QMessageBox::question(this,
                                "Delete File",
                                QString("Are you sure you want to delete '%1'?").arg(itemName),
                                QMessageBox::Yes | QMessageBox::No);

      if (reply == QMessageBox::Yes)
      {
        deleteRemoteFileConfirmed(itemName);
      }
    }
  }
  else
  {
    QAction *deleteFolderAction = contextMenu.addAction("Delete folder");

    QAction *selectedAction = contextMenu.exec(remoteListWidget->viewport()->mapToGlobal(pos));

    if (selectedAction == deleteFolderAction)
    {
      QMessageBox::StandardButton reply =
          QMessageBox::question(this,
                                "Delete Folder",
                                QString("Are you sure you want to delete '%1'?").arg(itemName),
                                QMessageBox::Yes | QMessageBox::No);

      if (reply == QMessageBox::Yes)
      {
        deleteRemoteDirectoryConfirmed(itemName);
      }
    }
  }
}

void
MainWindow::deleteRemoteFileConfirmed(const QString &fileName)
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

  // Send DELE command
  lastCommand = FtpCommand::Dele;
  sendCommand("DELE " + remoteFilePath);
}

void
MainWindow::deleteRemoteDirectoryConfirmed(const QString &dirName)
{
  if (!m_isConnected)
    return;

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
MainWindow::processRemoteDeleteQueue()
{
  if (!m_remoteDeleteInProgress)
    return;

  if (!m_remoteDeleteQueue.isEmpty())
  {
    FtpDeleteCommand cmd = m_remoteDeleteQueue.dequeue();
    if (cmd.type == FtpDeleteCommand::DeleteFile)
    {
      lastCommand = FtpCommand::Dele;
      sendCommand("DELE " + cmd.path);
      return;
    }
    if (cmd.type == FtpDeleteCommand::DeleteDir)
    {
      lastCommand = FtpCommand::Rmd;
      sendCommand("RMD " + cmd.path);
      return;
    }
  }

  if (!m_remoteDirsToList.isEmpty())
  {
    m_currentDeleteDir = m_remoteDirsToList.pop();
    m_pendingDeleteListPath = m_currentDeleteDir;
    lastCommand = FtpCommand::ListForDelete;
    sendCommand("PASV");
    return;
  }

  if (!m_remoteDirsToDelete.isEmpty())
  {
    QString dirPath = m_remoteDirsToDelete.pop();
    lastCommand = FtpCommand::Rmd;
    sendCommand("RMD " + dirPath);
    return;
  }

  m_remoteDeleteInProgress = false;
  m_currentDeleteDir.clear();
  m_pendingDeleteListPath.clear();
  lastCommand = FtpCommand::List;
  sendCommand("PASV");
}

void
MainWindow::uploadFolder(const QString &localPath)
{
  if (!m_uploadQueue.isEmpty())
  {
    QMessageBox::warning(this, "Upload in Progress", "An upload is already in progress.");
    return;
  }

  QFileInfo fileInfo(localPath);
  QString remoteDirName = fileInfo.fileName();

  QString remoteTargetPath;
  if (currentPath.endsWith('/'))
  {
    remoteTargetPath = currentPath + remoteDirName;
  }
  else
  {
    remoteTargetPath = currentPath + "/" + remoteDirName;
  }

  m_uploadQueue.clear();
  recursivelyPopulateUploadQueue(localPath, remoteTargetPath);
  processUploadQueue();
}

void
MainWindow::recursivelyPopulateUploadQueue(const QString &localPath, const QString &remotePath)
{
  QDir localDir(localPath);
  if (!localDir.exists())
    return;

  // Command to create this directory
  m_uploadQueue.enqueue({ FtpUploadCommand::CreateDirectory, localPath, remotePath });

  // Enqueue files for upload
  for (const QFileInfo &file : localDir.entryInfoList(QDir::Files))
  {
    m_uploadQueue.enqueue(
        { FtpUploadCommand::UploadFile, file.filePath(), remotePath + "/" + file.fileName() });
  }

  // Recurse into subdirectories
  for (const QFileInfo &dir : localDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
  {
    recursivelyPopulateUploadQueue(dir.filePath(), remotePath + "/" + dir.fileName());
  }
}

void
MainWindow::processUploadQueue()
{
  if (m_uploadQueue.isEmpty())
  {
    qDebug() << "Upload queue finished.";
    // Refresh remote file list
    lastCommand = FtpCommand::List;
    sendCommand("PASV");
    return;
  }

  FtpUploadCommand command = m_uploadQueue.head();  // Peek at the command

  if (command.type == FtpUploadCommand::CreateDirectory)
  {
    lastCommand = FtpCommand::Mkd;
    sendCommand("MKD " + command.remotePath);
  }
  else if (command.type == FtpUploadCommand::UploadFile)
  {
    // For STOR, we also need to enter passive mode
    lastCommand = FtpCommand::Stor;
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
    m_pendingRemotePathForUpload = command.remotePath;
    sendCommand("PASV");
  }
}

void
MainWindow::localFileDoubleClicked(const QModelIndex &index)
{
  if (!m_isConnected)
    return;

  if (!m_uploadQueue.isEmpty())
  {
    QMessageBox::warning(this, "Upload in Progress", "An upload is already in progress.");
    return;
  }

  QString localPath = localModel->filePath(index);
  if (localModel->isDir(index))
  {
    // If it's a directory, maybe we want to navigate into it locally
    // or offer to upload the whole directory. For now, do nothing.
    localTreeView->setRootIndex(index);
    return;
  }

  QFileInfo fileInfo(localPath);
  QString fileName = fileInfo.fileName();

  QString remoteTargetPath;
  if (currentPath.endsWith('/'))
  {
    remoteTargetPath = currentPath + fileName;
  }
  else
  {
    remoteTargetPath = currentPath + "/" + fileName;
  }

  m_uploadQueue.clear();
  m_uploadQueue.enqueue({ FtpUploadCommand::UploadFile, localPath, remoteTargetPath });
  processUploadQueue();
}

// --- Stubbed Functions ---

void
MainWindow::processItem(QListWidgetItem *item)
{
  if (!m_isConnected)
    return;

  QString name = item->text();
  if (!isDirectory.value(name, false))
  {
    downloadFile(name);
    return;
  }

  QUrl url;
  // Ensure currentPath ends with a slash if it's not the root, for correct resolution
  if (currentPath.endsWith('/') || currentPath.isEmpty())
  {
    url.setPath(currentPath);
  }
  else
  {
    url.setPath(currentPath + '/');
  }

  QUrl newUrl = url.resolved(QUrl(name));
  pendingPath = newUrl.path();

  qDebug() << "CWD to" << pendingPath;
  lastCommand = FtpCommand::Cwd;
  sendCommand(QString("CWD %1").arg(pendingPath));
}

void
MainWindow::uploadFile()
{
  // To be implemented
}

void
MainWindow::downloadFile(const QString &fileName)
{
  if (!m_isConnected)
    return;

  // Get the local directory where we'll save the file
  QString localDir = localModel->filePath(localTreeView->currentIndex());
  if (localModel->isDir(localTreeView->currentIndex()))
  {
    // If a file is selected, use its directory; if a directory is selected, use it
    if (!localModel->isDir(localTreeView->currentIndex()))
    {
      QFileInfo fileInfo(localDir);
      localDir = fileInfo.absolutePath();
    }
  }
  else
  {
    localDir = QDir::currentPath();
  }

  QString localFilePath = localDir + "/" + fileName;

  // Check if file already exists
  if (QFile::exists(localFilePath))
  {
    QMessageBox::StandardButton reply =
        QMessageBox::question(this,
                              "File Exists",
                              QString("File '%1' already exists. Overwrite?").arg(fileName),
                              QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
      return;
  }

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

  // Set up the file for download
  m_fileToDownload = new QFile(localFilePath);
  if (!m_fileToDownload->open(QIODevice::WriteOnly))
  {
    qDebug() << "Could not open local file for writing:" << localFilePath;
    QMessageBox::critical(this,
                          "Error",
                          QString("Could not open file for writing: %1").arg(localFilePath));
    delete m_fileToDownload;
    m_fileToDownload = nullptr;
    return;
  }

  m_pendingFileNameForDownload = remoteFilePath;

  // Initiate download via PASV
  lastCommand = FtpCommand::Retr;
  sendCommand("PASV");
}
