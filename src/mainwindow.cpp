#include "mainwindow.h"
#include "ftpcommunicator.h"

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
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QStyle>
#include <QTextStream>
#include <QTimer>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
  m_ftpCommunicator = new FtpCommunicator(this);

  // Connect FTP communicator signals
  connect(m_ftpCommunicator, &FtpCommunicator::connected, this, &MainWindow::onFtpConnected);
  connect(m_ftpCommunicator, &FtpCommunicator::disconnected, this, &MainWindow::onFtpDisconnected);
  connect(m_ftpCommunicator, &FtpCommunicator::connectionError, this, &MainWindow::onFtpConnectionError);
  connect(m_ftpCommunicator, &FtpCommunicator::statusUpdated, this, &MainWindow::onFtpStatusUpdated);
  connect(m_ftpCommunicator, &FtpCommunicator::directoryListReceived, this, &MainWindow::onFtpDirectoryListReceived);
  connect(m_ftpCommunicator, &FtpCommunicator::downloadComplete, this, &MainWindow::onFtpDownloadComplete);

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

  localListWidget = new QTreeWidget(localWidget);
  localListWidget->setHeaderLabels({"Namn", "Datum"});
  localListWidget->setColumnWidth(0, 200);
  localListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  localListWidget->setIconSize(QSize(16, 16));
  localListWidget->setRootIsDecorated(false);
  localLayout->addWidget(localListWidget);

  // Remote
  remoteListWidget = new QTreeWidget(splitter);
  remoteListWidget->setHeaderLabels({"Namn", "Datum"});
  remoteListWidget->setColumnWidth(0, 200);
  remoteListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  remoteListWidget->setIconSize(QSize(16, 16));
  remoteListWidget->setRootIsDecorated(false);

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
  connect(remoteListWidget, &QTreeWidget::itemDoubleClicked, this, &MainWindow::processItem);
  connect(localListWidget,
          &QTreeWidget::customContextMenuRequested,
          this,
          &MainWindow::showLocalContextMenu);
  connect(remoteListWidget,
          &QTreeWidget::customContextMenuRequested,
          this,
          &MainWindow::showRemoteContextMenu);
  connect(localListWidget, &QTreeWidget::itemDoubleClicked, this, &MainWindow::localItemDoubleClicked);

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
    QTreeWidgetItem *upItem = new QTreeWidgetItem(localListWidget);
    upItem->setText(0, "..");
    upItem->setData(0, Qt::UserRole, QString(".."));
    upItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
  }

  // Directories first
  for (const QFileInfo &info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
  {
    QTreeWidgetItem *item = new QTreeWidgetItem(localListWidget);
    item->setText(0, info.fileName());
    item->setText(1, info.lastModified().toString("yyyy-MM-dd HH:mm"));
    item->setData(0, Qt::UserRole, info.absoluteFilePath());
    item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
  }

  // Then files
  for (const QFileInfo &info : dir.entryInfoList(QDir::Files, QDir::Name))
  {
    QTreeWidgetItem *item = new QTreeWidgetItem(localListWidget);
    item->setText(0, info.fileName());
    item->setText(1, info.lastModified().toString("yyyy-MM-dd HH:mm"));
    item->setData(0, Qt::UserRole, info.absoluteFilePath());
    item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
  }
}

void
MainWindow::connectOrDisconnect()
{
  if (m_ftpCommunicator->isConnected())
  {
    m_ftpCommunicator->disconnectFromHost();
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
  m_ftpCommunicator->connectToHost(hostLineEdit->text(), usernameLineEdit->text(), password);
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

// --- FTP Communicator Signal Handlers ---

void
MainWindow::onFtpConnected()
{
  connectButton->setText("Disconnect");
  hostLineEdit->setEnabled(true);
  usernameLineEdit->setEnabled(true);
  passwordLineEdit->setEnabled(true);
}

void
MainWindow::onFtpDisconnected()
{
  remoteListWidget->clear();
  connectButton->setText("Connect");
  hostLineEdit->setEnabled(true);
  usernameLineEdit->setEnabled(true);
  passwordLineEdit->setEnabled(true);
}

void
MainWindow::onFtpConnectionError(const QString &error)
{
  QMessageBox::critical(this, "Connection Error", error);
}

void
MainWindow::onFtpStatusUpdated(const QString &message)
{
  logStatus(message);
}

void
MainWindow::onFtpDirectoryListReceived()
{
  remoteListWidget->clear();
  m_currentRemotePath = m_ftpCommunicator->getCurrentPath();
  m_remoteFiles = m_ftpCommunicator->getRemoteFiles();

  // Add ".." to navigate up, unless we are at root
  if (m_currentRemotePath != "/")
  {
    QTreeWidgetItem *upItem = new QTreeWidgetItem(remoteListWidget);
    upItem->setText(0, "..");
    upItem->setData(0, Qt::UserRole, QString(".."));
    upItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
  }

  // Add all files and directories
  for (auto it = m_remoteFiles.constBegin(); it != m_remoteFiles.constEnd(); ++it)
  {
    const QString &name = it.key();
    const FtpCommunicator::RemoteFileInfo &info = it.value();

    QTreeWidgetItem *item = new QTreeWidgetItem(remoteListWidget);
    item->setText(0, name);
    item->setText(1, info.date);
    item->setData(0, Qt::UserRole, name);
    
    if (info.isDir)
    {
      item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    }
    else
    {
      item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    }
  }
}

void
MainWindow::downloadFolder(const QString &folderName)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  // Get the local directory where we'll save the folder
  QString localDir = m_localCurrentPath.isEmpty() ? QDir::currentPath() : m_localCurrentPath;

  // We should ideally check if the local folder exists, but let's let mkpath handle it.
  
  m_ftpCommunicator->downloadFolder(folderName, localDir);
}

void
MainWindow::onFtpDownloadComplete()
{
  logStatus("Download complete.");
  populateLocalList(m_localCurrentPath);
}

// --- Old FTP Connection Slots (removed) ---
// The following methods have been moved to FtpCommunicator and are no longer needed:
// onControlConnected(), onControlReadyRead(), onControlDisconnected(), onControlError()
// onDataReadyRead(), onDataConnected(), onDataDisconnected(), onDataError()


void
MainWindow::showLocalContextMenu(const QPoint &pos)
{
  QTreeWidgetItem *item = localListWidget->itemAt(pos);
  QString itemPath;
  if (item)
  {
    itemPath = item->data(0, Qt::UserRole).toString();
    if (itemPath == "..")
      itemPath.clear();
  }

  QMenu contextMenu(this);
  QAction *createFolderAction = contextMenu.addAction("Skapa Mapp");
  QAction *uploadAction = nullptr;
  QAction *deleteAction = nullptr;

  if (!itemPath.isEmpty())
  {
    contextMenu.addSeparator();
    QFileInfo info(itemPath);
    if (info.isDir())
    {
      uploadAction = contextMenu.addAction("Upload folder to server");
      deleteAction = contextMenu.addAction("Delete folder");
    }
    else
    {
      uploadAction = contextMenu.addAction("Upload file to server");
      deleteAction = contextMenu.addAction("Delete");
    }
  }

  QAction *selectedAction = contextMenu.exec(localListWidget->viewport()->mapToGlobal(pos));
  if (!selectedAction)
    return;

  if (selectedAction == createFolderAction)
  {
    createLocalFolder();
  }
  else if (selectedAction == uploadAction)
  {
    if (!m_ftpCommunicator->isConnected())
    {
      QMessageBox::warning(this, "Not Connected", "Connect to the server to upload.");
      return;
    }
    QFileInfo info(itemPath);
    if (info.isDir())
      uploadFolder(itemPath);
    else
      uploadFile(itemPath);
  }
  else if (selectedAction == deleteAction)
  {
    QFileInfo info(itemPath);
    QString typeStr = info.isDir() ? "Folder" : "File";
    QMessageBox::StandardButton reply =
        QMessageBox::question(this,
                              "Delete " + typeStr,
                              QString("Are you sure you want to delete '%1'?").arg(info.fileName()),
                              QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
    {
      bool success = false;
      if (info.isDir())
      {
        QDir dir(itemPath);
        success = dir.removeRecursively();
      }
      else
      {
        success = QFile::remove(itemPath);
      }

      if (!success)
        QMessageBox::critical(this, "Error", QString("Could not delete %1: %2").arg(typeStr.toLower(), info.fileName()));
      else
        populateLocalList(m_localCurrentPath);
    }
  }
}

void
MainWindow::createLocalFolder()
{
  bool ok;
  QString folderName = QInputDialog::getText(this,
                                             "Skapa Mapp",
                                             "Mappnamn:",
                                             QLineEdit::Normal,
                                             "",
                                             &ok);
  if (!ok || folderName.trimmed().isEmpty())
    return;

  QDir dir(m_localCurrentPath);
  if (dir.mkdir(folderName.trimmed()))
  {
    populateLocalList(m_localCurrentPath);
  }
  else
  {
    QMessageBox::critical(this, "Error", "Could not create folder.");
  }
}

void
MainWindow::showRemoteContextMenu(const QPoint &pos)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  QTreeWidgetItem *item = remoteListWidget->itemAt(pos);

  QString itemName;
  if (item)
  {
    itemName = item->data(0, Qt::UserRole).toString();
    if (itemName.isEmpty())
      itemName = item->text(0);
    if (itemName == "..")
      itemName.clear();
  }

  QMenu contextMenu(this);
  QAction *createFolderAction = contextMenu.addAction("Skapa ny mapp");
  QAction *downloadFileAction = nullptr;
  QAction *downloadFolderAction = nullptr;
  QAction *deleteAction = nullptr;
  QAction *deleteFolderAction = nullptr;

  if (!itemName.isEmpty())
  {
    contextMenu.addSeparator();
    if (!m_ftpCommunicator->isDirectory(itemName))
    {
      downloadFileAction = contextMenu.addAction("Download file");
      deleteAction = contextMenu.addAction("Ta bort fil");
    }
    else
    {
      downloadFolderAction = contextMenu.addAction("Download folder");
      deleteFolderAction = contextMenu.addAction("Ta bort mapp");
    }
  }

  QAction *selectedAction = contextMenu.exec(remoteListWidget->viewport()->mapToGlobal(pos));
  if (!selectedAction)
    return;

  if (selectedAction == createFolderAction)
  {
    createRemoteFolder();
  }
  else if (selectedAction == downloadFileAction)
  {
    downloadFile(itemName);
  }
  else if (selectedAction == downloadFolderAction)
  {
    downloadFolder(itemName);
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
  m_ftpCommunicator->deleteRemoteFile(fileName, m_ftpCommunicator->getCurrentPath());
}

void
MainWindow::createRemoteFolder()
{
  if (!m_ftpCommunicator->isConnected())
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

  m_ftpCommunicator->createRemoteFolder(folderName.trimmed(), m_ftpCommunicator->getCurrentPath());
}

void
MainWindow::deleteRemoteDirectoryConfirmed(const QString &dirName)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  m_ftpCommunicator->deleteRemoteDirectory(dirName, m_ftpCommunicator->getCurrentPath());
}

void
MainWindow::uploadFolder(const QString &localPath)
{
  if (!m_ftpCommunicator->isConnected())
  {
    QMessageBox::warning(this, "Not Connected", "Connect to the server to upload folders.");
    return;
  }

  m_ftpCommunicator->uploadFolder(localPath, m_ftpCommunicator->getCurrentPath());
}

void
MainWindow::localItemDoubleClicked(QTreeWidgetItem *item)
{
  QString itemPath = item->data(0, Qt::UserRole).toString();

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
  if (!m_ftpCommunicator->isConnected())
    return;

  QString fileName = info.fileName();
  QString currentPath = m_ftpCommunicator->getCurrentPath();
  QString remoteTargetPath = currentPath.endsWith('/') ? currentPath + fileName : currentPath + "/" + fileName;

  m_ftpCommunicator->uploadFile(itemPath, remoteTargetPath);
}

// --- Stubbed Functions ---

void
MainWindow::processItem(QTreeWidgetItem *item)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  QString name = item->data(0, Qt::UserRole).toString();
  if (name.isEmpty())
    name = item->text(0);

  if (name != ".." && !m_ftpCommunicator->isDirectory(name))
  {
    downloadFile(name);
    return;
  }

  // It's a directory - navigate to it
  QUrl url;
  QString currentPath = m_ftpCommunicator->getCurrentPath();
  
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
  QString newPath = newUrl.path();

  qDebug() << "CWD to" << newPath;
  m_ftpCommunicator->changeDirectory(newPath);
}

void
MainWindow::uploadFile()
{
  // To be implemented
}

void
MainWindow::uploadFile(const QString &filePath)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists())
  {
    QMessageBox::critical(this, "Error", "File does not exist.");
    return;
  }

  // Add to upload queue for the current remote directory
  QString currentPath = m_ftpCommunicator->getCurrentPath();
  QString remotePath = currentPath.endsWith('/') ? currentPath : currentPath + "/";
  remotePath += fileInfo.fileName();
  
  m_ftpCommunicator->uploadFile(filePath, remotePath);
}

void
MainWindow::downloadFile(const QString &fileName)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  // Get the local directory where we'll save the file
  QString localDir = m_localCurrentPath.isEmpty() ? QDir::currentPath() : m_localCurrentPath;

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

  m_ftpCommunicator->downloadFile(fileName, localDir);
}
