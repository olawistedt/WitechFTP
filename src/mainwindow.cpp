#include "mainwindow.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>
#include <QTcpSocket>
#include <QTextStream>
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
  resize(800, 600);
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

  // --- File Browsers ---
  splitter = new QSplitter;

  // Local
  localModel = new QFileSystemModel;
  localModel->setRootPath(QDir::currentPath());
  localTreeView = new QTreeView(splitter);
  localTreeView->setModel(localModel);
  localTreeView->setRootIndex(localModel->index(QDir::currentPath()));

  // Remote
  remoteListWidget = new QListWidget(splitter);

  splitter->addWidget(localTreeView);
  splitter->addWidget(remoteListWidget);

  // --- Main Layout ---
  QWidget *centralWidget = new QWidget;
  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->addWidget(connectionWidget);
  mainLayout->addWidget(splitter);

  setCentralWidget(centralWidget);

  // --- Connections ---
  connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectOrDisconnect);
  connect(remoteListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::processItem);
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
    else if (responseCode == 227)
    {  // Entering Passive Mode
      if (lastCommand != FtpCommand::List)
        return;
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

      QString pasvHost =
          QString("%1.%2.%3.%4").arg(parts[0]).arg(parts[1]).arg(parts[2]).arg(parts[3]);
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
    else if (responseCode == 150)
    {  // File status okay; about to open data connection.
      qDebug() << "Server is about to send the list.";
      // Nothing to do here, we just wait for the data connection to close
    }
    else if (responseCode == 226)
    {  // Closing data connection.
      qDebug() << "Server has sent the list.";
      lastCommand = FtpCommand::None;
      // The onDataDisconnected slot will handle the parsing
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
  dataBuffer.append(dataSocket->readAll());
}

void
MainWindow::onDataConnected()
{
  if (m_waitingForDataConnection && lastCommand == FtpCommand::List)
  {
    m_waitingForDataConnection = false;
    sendCommand("LIST");
  }
}

void
MainWindow::onDataDisconnected()
{
  qDebug() << "Data connection disconnected. Processing list.";
  if (lastCommand != FtpCommand::List)
    return;


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


// --- Stubbed Functions ---

void
MainWindow::processItem(QListWidgetItem *item)
{
  if (!m_isConnected)
    return;

  QString name = item->text();
  if (!isDirectory.value(name, false))
  {
    qDebug() << "Item is a file, not navigating.";
    return;  // It's a file, do nothing for now
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
  // To be implemented
}
