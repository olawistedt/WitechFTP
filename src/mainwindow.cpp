#include "mainwindow.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
#include <QSettings>
#include <QSplitter>
#include <QStyle>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
  controlSocket = new QTcpSocket(this);
  controlStream = new QTextStream(controlSocket);
  dataSocket = new QTcpSocket(this);

  m_keepAliveTimer = new QTimer(this);
  m_keepAliveTimer->setInterval(60000);  // 60 seconds
  connect(m_keepAliveTimer, &QTimer::timeout, this, [this]()
  {
    if (m_isConnected && lastCommand == FtpCommand::None)
      sendCommand("NOOP");
  });

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

  setWindowTitle("Witech FTP");
  setWindowIcon(QIcon(":/ftp-icon.png"));

  QSettings settings("Witech", "WitechFTP");
  QByteArray savedGeometry = settings.value("windowGeometry").toByteArray();
  if (!savedGeometry.isEmpty())
  {
    restoreGeometry(savedGeometry);
  }
  else
  {
    resize(800, 800);
    setGeometry(QStyle::alignedRect(Qt::LeftToRight,
                                    Qt::AlignCenter,
                                    size(),
                                    QGuiApplication::primaryScreen()->availableGeometry()));
  }

  // Auto-connect on startup
  QTimer::singleShot(100, this, &MainWindow::connectOrDisconnect);
}

MainWindow::~MainWindow()
{
  QSettings settings("Witech", "WitechFTP");
  settings.setValue("windowGeometry", saveGeometry());

  if (!m_localCurrentPath.isEmpty())
  {
    QString configDir = QDir::homePath() + "/.witech_ftp";
    QDir().mkpath(configDir);
    QFile file(configDir + "/last_local_path");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
      QTextStream out(&file);
      out << m_localCurrentPath;
    }
  }

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
  // --- Title ---
  QLabel *titleLabel = new QLabel("WitechFTP v1.1");
  QFont titleFont("Arial", 24, QFont::Bold);
  titleLabel->setFont(titleFont);
  titleLabel->setAlignment(Qt::AlignLeft);
  titleLabel->setStyleSheet("padding: 10px;");

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
  QString localStartPath;
  QString configDir = QDir::homePath() + "/.witech_ftp";
  QFile file(configDir + "/last_local_path");
  if (file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    localStartPath = QTextStream(&file).readAll().trimmed();
  }
  if (localStartPath.isEmpty() || !QDir(localStartPath).exists())
  {
    localStartPath =
        QProcessEnvironment::systemEnvironment().value("USERPROFILE", QDir::homePath());
  }

  QWidget *localWidget = new QWidget(splitter);
  QVBoxLayout *localLayout = new QVBoxLayout(localWidget);
  localLayout->setContentsMargins(0, 0, 0, 0);

  localListWidget = new QListWidget(localWidget);
  localListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  localListWidget->setSpacing(0);
  localListWidget->setIconSize(QSize(16, 16));
  localLayout->addWidget(localListWidget);

  // Remote
  remoteListWidget = new QListWidget(splitter);
  remoteListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  remoteListWidget->setSpacing(0);
  remoteListWidget->setIconSize(QSize(16, 16));

  splitter->addWidget(localWidget);
  splitter->addWidget(remoteListWidget);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);

  // --- Status Log ---
  statusLog = new QTextEdit;
  statusLog->setReadOnly(true);
  statusLog->setMinimumHeight(40);
  statusLog->setFont(QFont("Courier New", 8));
  statusLog->setPlaceholderText("Status log...");

  // --- Vertical splitter: file browsers on top, status log on bottom ---
  QSplitter *verticalSplitter = new QSplitter(Qt::Vertical);
  verticalSplitter->addWidget(splitter);
  verticalSplitter->addWidget(statusLog);
  verticalSplitter->setStretchFactor(0, 3);
  verticalSplitter->setStretchFactor(1, 1);

  // --- Main Layout ---
  QWidget *centralWidget = new QWidget;
  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  mainLayout->addWidget(titleLabel);
  mainLayout->addWidget(connectionWidget);
  mainLayout->addWidget(verticalSplitter, 1);

  setCentralWidget(centralWidget);

  // --- Connections ---
  connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectOrDisconnect);
  connect(remoteListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::processItem);
  connect(localListWidget,
          &QListWidget::customContextMenuRequested,
          this,
          &MainWindow::showLocalContextMenu);
  connect(remoteListWidget,
          &QListWidget::customContextMenuRequested,
          this,
          &MainWindow::showRemoteContextMenu);
  connect(localListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::localItemDoubleClicked);

  populateLocalList(localStartPath);
}

void
MainWindow::populateLocalList(const QString &path)
{
  m_localCurrentPath = path;
  localListWidget->clear();

  QDir dir(path);

  // Add ".." unless already at filesystem root
  QDir parent = dir;
  if (parent.cdUp())
  {
    QListWidgetItem *upItem = new QListWidgetItem("..");
    upItem->setData(Qt::UserRole, QString(".."));
    upItem->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    localListWidget->addItem(upItem);
  }

  // Directories first
  for (const QFileInfo &info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
  {
    QListWidgetItem *item = new QListWidgetItem(info.fileName());
    item->setData(Qt::UserRole, info.absoluteFilePath());
    item->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    localListWidget->addItem(item);
  }

  // Then files
  for (const QFileInfo &info : dir.entryInfoList(QDir::Files, QDir::Name))
  {
    QListWidgetItem *item = new QListWidgetItem(info.fileName());
    item->setData(Qt::UserRole, info.absoluteFilePath());
    item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    localListWidget->addItem(item);
  }
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

  logStatus(QString("Ansluter till %1:21...").arg(hostLineEdit->text()));
  controlSocket->connectToHost(hostLineEdit->text(), 21);
  connectButton->setText("Connecting...");
  hostLineEdit->setEnabled(false);
  usernameLineEdit->setEnabled(false);
  passwordLineEdit->setEnabled(false);
}

void
MainWindow::logStatus(const QString &message)
{
  QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
  statusLog->append(QString("[%1] %2").arg(timestamp, message));
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
      logStatus(QString("Inloggad som %1").arg(usernameLineEdit->text()));
      // Set binary mode before anything else
      lastCommand = FtpCommand::TypeI;
      sendCommand("TYPE I");
      m_keepAliveTimer->start();
    }
    else if (responseCode == 200 && lastCommand == FtpCommand::TypeI)
    {  // TYPE I acknowledged
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
      logStatus(QString("\u00d6ppnar mapp: %1").arg(currentPath));
      lastCommand = FtpCommand::List;
      sendCommand("PASV");  // Refresh list
    }
    else if (responseCode == 550 && lastCommand == FtpCommand::Cwd)
    {
      qDebug() << "CWD failed:" << response;
      lastCommand = FtpCommand::None;
    }
    else if (responseCode == 257 && lastCommand == FtpCommand::Mkd)
    {  // MKD success (from upload queue)
      m_uploadQueue.dequeue();
      processUploadQueue();
    }
    else if (responseCode == 550 && lastCommand == FtpCommand::Mkd)
    {  // MKD failed (maybe dir exists)
      qDebug() << "MKD failed, assuming directory exists:" << response;
      m_uploadQueue.dequeue();
      processUploadQueue();
    }
    else if (responseCode == 257 && lastCommand == FtpCommand::MkdManual)
    {  // MKD success (manual folder creation) – refresh listing
      lastCommand = FtpCommand::List;
      sendCommand("PASV");
    }
    else if (responseCode >= 500 && lastCommand == FtpCommand::MkdManual)
    {  // MKD failed
      logStatus(QString("Kunde inte skapa mapp: %1").arg(response));
      lastCommand = FtpCommand::None;
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
        if (m_fileToUpload)
        {
          QByteArray fileContent = m_fileToUpload->readAll();
          dataSocket->write(fileContent);
          dataSocket->disconnectFromHost();  // Signal that we are done writing
          m_fileToUpload->close();
          delete m_fileToUpload;
          m_fileToUpload = nullptr;
        }
        else
        {
          dataSocket->disconnectFromHost();
        }
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
    else if (responseCode == 213 && lastCommand == FtpCommand::Size)
    {  // SIZE response: "213 <bytes>"
      qint64 remoteSize = response.mid(4).trimmed().toLongLong();
      if (remoteSize == m_localFileSizeForVerify)
      {
        logStatus(QString("Verifiering OK: %1 (%2 byte)").arg(m_pendingRemotePathForUpload).arg(remoteSize));
      }
      else
      {
        logStatus(QString("VARNING: Storleksskillnad för %1 – lokal: %2 byte, server: %3 byte")
                      .arg(m_pendingRemotePathForUpload)
                      .arg(m_localFileSizeForVerify)
                      .arg(remoteSize));
        QMessageBox::warning(this,
                             "Uppladdningsfel",
                             QString("Fil %1 verifierades inte.\nLokal storlek: %2 byte\nServerstorlek: %3 byte")
                                 .arg(m_pendingRemotePathForUpload)
                                 .arg(m_localFileSizeForVerify)
                                 .arg(remoteSize));
      }
      m_uploadQueue.dequeue();
      m_pendingRemotePathForUpload.clear();
      m_localFileSizeForVerify = 0;
      lastCommand = FtpCommand::None;
      if (!m_uploadQueue.isEmpty())
        processUploadQueue();
      else
      {
        lastCommand = FtpCommand::List;
        sendCommand("PASV");
      }
    }
    else if (responseCode == 550 && lastCommand == FtpCommand::Size)
    {  // SIZE not supported or file not found on server
      logStatus(QString("VARNING: Kunde inte verifiera %1 – SIZE-kommando stödjs ej av servern").arg(m_pendingRemotePathForUpload));
      m_uploadQueue.dequeue();
      m_pendingRemotePathForUpload.clear();
      m_localFileSizeForVerify = 0;
      lastCommand = FtpCommand::None;
      if (!m_uploadQueue.isEmpty())
        processUploadQueue();
      else
      {
        lastCommand = FtpCommand::List;
        sendCommand("PASV");
      }
    }
  }
}

void
MainWindow::onControlDisconnected()
{
  qDebug() << "Control connection disconnected.";
  logStatus("Frånkopplad från servern.");
  m_isConnected = false;
  m_keepAliveTimer->stop();
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
    logStatus(QString("Laddar upp till: %1").arg(m_pendingRemotePathForUpload));
    sendCommand("STOR " + m_pendingRemotePathForUpload);
  }
  else if (m_waitingForDataConnection && lastCommand == FtpCommand::Retr)
  {
    m_waitingForDataConnection = false;
    logStatus(QString("Laddar ner: %1").arg(m_pendingFileNameForDownload));
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
      upItem->setData(Qt::UserRole, QString(".."));
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

      QString sizeStr;
      if (isDir)
      {
        sizeStr = "<mapp>";
      }
      else
      {
        qint64 size = parts[4].toLongLong();
        if (size < 1024)
          sizeStr = QString("%1 B").arg(size);
        else if (size < 1024 * 1024)
          sizeStr = QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
        else if (size < 1024LL * 1024 * 1024)
          sizeStr = QString("%1 MB").arg(size / (1024.0 * 1024), 0, 'f', 1);
        else
          sizeStr = QString("%1 GB").arg(size / (1024.0 * 1024 * 1024), 0, 'f', 1);
      }
      QString dateStr = parts[5] + " " + parts[6] + " " + parts[7];
      QString displayText = name + "    " + sizeStr + "    " + dateStr;

      QListWidgetItem *item = new QListWidgetItem(displayText);
      item->setData(Qt::UserRole, name);
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
    qDebug() << "File upload complete. Sending SIZE for verification.";
    dataBuffer.clear();
    // Send SIZE to verify the uploaded file matches the local file
    lastCommand = FtpCommand::Size;
    sendCommand("SIZE " + m_pendingRemotePathForUpload);
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
  QListWidgetItem *item = localListWidget->itemAt(pos);
  if (!item)
    return;

  QString itemPath = item->data(Qt::UserRole).toString();
  if (itemPath == "..")
    return;

  QMenu contextMenu(this);
  QFileInfo info(itemPath);

  if (info.isDir())
  {
    QAction *uploadAction = contextMenu.addAction("Upload folder to server");
    QAction *deleteFolderAction = contextMenu.addAction("Delete folder");

    QAction *selectedAction = contextMenu.exec(localListWidget->viewport()->mapToGlobal(pos));

    if (selectedAction == uploadAction)
    {
      if (!m_isConnected)
      {
        QMessageBox::warning(this, "Not Connected", "Connect to the server to upload folders.");
        return;
      }
      uploadFolder(itemPath);
    }
    else if (selectedAction == deleteFolderAction)
    {
      QMessageBox::StandardButton reply =
          QMessageBox::question(this,
                                "Delete Folder",
                                QString("Are you sure you want to delete '%1'?").arg(info.fileName()),
                                QMessageBox::Yes | QMessageBox::No);
      if (reply == QMessageBox::Yes)
      {
        QDir dir(itemPath);
        if (!dir.removeRecursively())
          QMessageBox::critical(this, "Error", QString("Could not delete folder: %1").arg(info.fileName()));
        else
          populateLocalList(m_localCurrentPath);
      }
    }
  }
  else
  {
    QAction *deleteAction = contextMenu.addAction("Delete");

    QAction *selectedAction = contextMenu.exec(localListWidget->viewport()->mapToGlobal(pos));

    if (selectedAction == deleteAction)
    {
      QMessageBox::StandardButton reply =
          QMessageBox::question(this,
                                "Delete File",
                                QString("Are you sure you want to delete '%1'?").arg(info.fileName()),
                                QMessageBox::Yes | QMessageBox::No);
      if (reply == QMessageBox::Yes)
      {
        if (!QFile::remove(itemPath))
          QMessageBox::critical(this, "Error", QString("Could not delete file: %1").arg(info.fileName()));
        else
          populateLocalList(m_localCurrentPath);
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

  QString itemName;
  if (item)
  {
    itemName = item->data(Qt::UserRole).toString();
    if (itemName.isEmpty())
      itemName = item->text();
    if (itemName == "..")
      itemName.clear();
  }

  QMenu contextMenu(this);
  QAction *createFolderAction = contextMenu.addAction("Skapa ny mapp");
  QAction *deleteAction = nullptr;
  QAction *deleteFolderAction = nullptr;

  if (!itemName.isEmpty())
  {
    contextMenu.addSeparator();
    if (!isDirectory.value(itemName, false))
      deleteAction = contextMenu.addAction("Ta bort fil");
    else
      deleteFolderAction = contextMenu.addAction("Ta bort mapp");
  }

  QAction *selectedAction = contextMenu.exec(remoteListWidget->viewport()->mapToGlobal(pos));
  if (!selectedAction)
    return;

  if (selectedAction == createFolderAction)
  {
    createRemoteFolder();
  }
  else if (selectedAction == deleteAction)
  {
    QMessageBox::StandardButton reply =
        QMessageBox::question(this,
                              "Ta bort fil",
                              QString("Är du säker på att du vill ta bort '%1'?").arg(itemName),
                              QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
      deleteRemoteFileConfirmed(itemName);
  }
  else if (selectedAction == deleteFolderAction)
  {
    QMessageBox::StandardButton reply =
        QMessageBox::question(this,
                              "Ta bort mapp",
                              QString("Är du säker på att du vill ta bort '%1'?").arg(itemName),
                              QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
      deleteRemoteDirectoryConfirmed(itemName);
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

  logStatus(QString("Raderar fil: %1").arg(remoteFilePath));
  // Send DELE command
  lastCommand = FtpCommand::Dele;
  sendCommand("DELE " + remoteFilePath);
}

void
MainWindow::createRemoteFolder()
{
  if (!m_isConnected)
    return;

  bool ok;
  QString folderName = QInputDialog::getText(this,
                                             "Skapa ny mapp",
                                             "Mappnamn:",
                                             QLineEdit::Normal,
                                             "",
                                             &ok);
  if (!ok || folderName.trimmed().isEmpty())
    return;

  QString remotePath;
  if (currentPath.endsWith('/'))
    remotePath = currentPath + folderName.trimmed();
  else
    remotePath = currentPath + "/" + folderName.trimmed();

  logStatus(QString("Skapar mapp: %1").arg(remotePath));
  lastCommand = FtpCommand::MkdManual;
  sendCommand("MKD " + remotePath);
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
      logStatus(QString("Raderar fil: %1").arg(cmd.path));
      lastCommand = FtpCommand::Dele;
      sendCommand("DELE " + cmd.path);
      return;
    }
    if (cmd.type == FtpDeleteCommand::DeleteDir)
    {
      logStatus(QString("Raderar mapp: %1").arg(cmd.path));
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
    logStatus(QString("Raderar mapp: %1").arg(dirPath));
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
    logStatus(QString("Skapar mapp: %1").arg(command.remotePath));
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
    logStatus(QString("Förbereder uppladdning: %1 → %2").arg(command.localPath, command.remotePath));
    m_pendingRemotePathForUpload = command.remotePath;
    m_localFileSizeForVerify = m_fileToUpload->size();
    sendCommand("PASV");
  }
}

void
MainWindow::localItemDoubleClicked(QListWidgetItem *item)
{
  QString itemPath = item->data(Qt::UserRole).toString();

  // Navigate up
  if (itemPath == "..")
  {
    QDir dir(m_localCurrentPath);
    if (dir.cdUp())
      populateLocalList(dir.absolutePath());
    return;
  }

  QFileInfo info(itemPath);
  if (info.isDir())
  {
    populateLocalList(itemPath);
    return;
  }

  // It's a file — upload it
  if (!m_isConnected)
    return;

  if (!m_uploadQueue.isEmpty())
  {
    QMessageBox::warning(this, "Upload in Progress", "An upload is already in progress.");
    return;
  }

  QString fileName = info.fileName();
  QString remoteTargetPath;
  if (currentPath.endsWith('/'))
    remoteTargetPath = currentPath + fileName;
  else
    remoteTargetPath = currentPath + "/" + fileName;

  m_uploadQueue.clear();
  m_uploadQueue.enqueue({ FtpUploadCommand::UploadFile, itemPath, remoteTargetPath });
  processUploadQueue();
}

// --- Stubbed Functions ---

void
MainWindow::processItem(QListWidgetItem *item)
{
  if (!m_isConnected)
    return;

  QString name = item->data(Qt::UserRole).toString();
  if (name.isEmpty())
    name = item->text();
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
  QString localDir = m_localCurrentPath.isEmpty() ? QDir::currentPath() : m_localCurrentPath;

  QString localFilePath = localDir + "/" + fileName;
  logStatus(QString("Laddar ner: %1 → %2").arg(fileName, localFilePath));

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
