#include "mainwindow.h"
#include "ftpcommunicator.h"

#include <QCryptographicHash>
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
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QTextStream>
#include <QTimer>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QComboBox>
#include <QVariantMap>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
  m_ftpCommunicator = new FtpCommunicator(this);

  // Connect FTP communicator signals
  connect(m_ftpCommunicator, &FtpCommunicator::connected, this, &MainWindow::onFtpConnected);
  connect(m_ftpCommunicator, &FtpCommunicator::disconnected, this, &MainWindow::onFtpDisconnected);
  connect(m_ftpCommunicator, &FtpCommunicator::connectionError, this, &MainWindow::onFtpConnectionError);
  connect(m_ftpCommunicator, &FtpCommunicator::statusUpdated, this, &MainWindow::onFtpStatusUpdated);
  connect(m_ftpCommunicator, &FtpCommunicator::directoryListReceived, this, &MainWindow::onFtpDirectoryListReceived);
  connect(m_ftpCommunicator, &FtpCommunicator::md5Received, this, &MainWindow::onFtpMd5Received);
  connect(m_ftpCommunicator, &FtpCommunicator::downloadComplete, this, &MainWindow::onFtpDownloadComplete);

  createUi();

  setWindowTitle("Witech FTP");
  setWindowIcon(QIcon(":/ftp-icon.png"));

  QSettings settings("Witech", "WitechFTP");
  hostLineEdit->setText(settings.value("lastHost", "ftp.witech.se").toString());
  usernameLineEdit->setText(settings.value("lastUsername", "witech.se").toString());

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
  settings.setValue("lastHost", hostLineEdit->text());
  settings.setValue("lastUsername", usernameLineEdit->text());

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
  // --- Title ---
  QWidget *titleWidget = new QWidget;
  QHBoxLayout *titleLayout = new QHBoxLayout(titleWidget);
  
  QLabel *titleLabel = new QLabel("WitechFTP v1.2");
  QFont titleFont("Arial", 24, QFont::Bold);
  titleLabel->setFont(titleFont);
  titleLabel->setAlignment(Qt::AlignCenter);
  
  QVBoxLayout *rightLayout = new QVBoxLayout;
  QPushButton *clearLogButton = new QPushButton("Rensa logg");
  connect(clearLogButton, &QPushButton::clicked, [this]() { statusLog->clear(); });

  savedSitesComboBox = new QComboBox;
  savedSitesComboBox->addItem("Sparade sajter...");
  loadSavedSites();
  connect(savedSitesComboBox, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onSavedSiteSelected);

  rightLayout->addWidget(clearLogButton);
  rightLayout->addWidget(savedSitesComboBox);

  titleLayout->addStretch();
  titleLayout->addWidget(titleLabel);
  titleLayout->addStretch();
  titleLayout->addLayout(rightLayout);
  titleWidget->setStyleSheet("padding: 5px;");

  // --- Connection Bar ---
  QWidget *connectionWidget = new QWidget;
  QHBoxLayout *connectionLayout = new QHBoxLayout(connectionWidget);
  
  connectionStatusIcon = new QLabel("🔴"); // Disconnected by default
  connectionStatusIcon->setToolTip("Frånkopplad");

  hostLineEdit = new QLineEdit;
  hostLineEdit->setPlaceholderText("FTP Host");
  usernameLineEdit = new QLineEdit;
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

  connectButton = new QPushButton("Anslut");
  stopButton = new QPushButton("Stoppa");
  stopButton->setEnabled(false);
  connect(stopButton, &QPushButton::clicked, m_ftpCommunicator, &FtpCommunicator::abortTransfer);

  connectionLayout->addWidget(connectionStatusIcon);
  connectionLayout->addWidget(new QLabel("Host:"));
  connectionLayout->addWidget(hostLineEdit);
  connectionLayout->addWidget(new QLabel("Username:"));
  connectionLayout->addWidget(usernameLineEdit);
  connectionLayout->addWidget(new QLabel("Password:"));
  connectionLayout->addWidget(passwordLineEdit);
  connectionLayout->addWidget(connectButton);
  connectionLayout->addWidget(stopButton);
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
  localListWidget->setHeaderLabels({"Namn", "Storlek", "Datum", "MD5"});
  localListWidget->setColumnWidth(0, 200);
  localListWidget->setColumnWidth(1, 80);
  localListWidget->setColumnWidth(2, 150);
  localListWidget->setColumnWidth(3, 250);
  localListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  localListWidget->setIconSize(QSize(16, 16));
  localListWidget->setRootIsDecorated(false);
  localLayout->addWidget(localListWidget);

  // Remote
  remoteListWidget = new QTreeWidget(splitter);
  remoteListWidget->setHeaderLabels({"Namn", "Storlek", "Datum", "MD5"});
  remoteListWidget->setColumnWidth(0, 200);
  remoteListWidget->setColumnWidth(1, 80);
  remoteListWidget->setColumnWidth(2, 150);
  remoteListWidget->setColumnWidth(3, 250);
  remoteListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  remoteListWidget->setIconSize(QSize(16, 16));
  remoteListWidget->setRootIsDecorated(false);

  splitter->addWidget(localWidget);
  splitter->addWidget(remoteListWidget);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 1);

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
  mainLayout->addWidget(titleWidget);
  mainLayout->addWidget(connectionWidget);
  mainLayout->addWidget(verticalSplitter, 1);

  setCentralWidget(centralWidget);

  // --- Connections ---
  connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectOrDisconnect);
  connect(remoteListWidget, &QTreeWidget::itemActivated, this, &MainWindow::onRemoteItemClicked);
  connect(localListWidget,
          &QTreeWidget::customContextMenuRequested,
          this,
          &MainWindow::showLocalContextMenu);
  connect(remoteListWidget,
          &QTreeWidget::customContextMenuRequested,
          this,
          &MainWindow::showRemoteContextMenu);
  connect(localListWidget, &QTreeWidget::itemActivated, this, &MainWindow::onLocalItemClicked);

  // Keyboard support for Delete
  QShortcut *localDeleteShortcut = new QShortcut(QKeySequence::Delete, localListWidget);
  connect(localDeleteShortcut, &QShortcut::activated, [this]() {
    QTreeWidgetItem *item = localListWidget->currentItem();
    if (item) {
        // Trigger deletion logic (simplified for this example)
        // In a real app, we'd refactor the context menu logic into helper methods
        showLocalContextMenu(localListWidget->visualItemRect(item).center());
    }
  });

  QShortcut *remoteDeleteShortcut = new QShortcut(QKeySequence::Delete, remoteListWidget);
  connect(remoteDeleteShortcut, &QShortcut::activated, [this]() {
    QTreeWidgetItem *item = remoteListWidget->currentItem();
    if (item) {
        showRemoteContextMenu(remoteListWidget->visualItemRect(item).center());
    }
  });

  // Keyboard support for Refresh (F5)
  QShortcut *localRefreshShortcut = new QShortcut(QKeySequence("F5"), localListWidget);
  connect(localRefreshShortcut, &QShortcut::activated, [this]() {
    populateLocalList(m_localCurrentPath);
  });

  QShortcut *remoteRefreshShortcut = new QShortcut(QKeySequence("F5"), remoteListWidget);
  connect(remoteRefreshShortcut, &QShortcut::activated, [this]() {
    if (m_ftpCommunicator->isConnected())
        m_ftpCommunicator->listRemoteDirectory(m_ftpCommunicator->getCurrentPath());
  });

  populateLocalList(localStartPath);
}

void
MainWindow::populateLocalList(const QString &path)
{
  m_localCurrentPath = path;
  localListWidget->clear();

  if (path.isEmpty())
  {
    for (const QFileInfo &drive : QDir::drives())
    {
      QTreeWidgetItem *item = new QTreeWidgetItem(localListWidget);
      item->setText(0, drive.absoluteFilePath());
      item->setData(0, Qt::UserRole, drive.absoluteFilePath());
      item->setIcon(0, style()->standardIcon(QStyle::SP_DriveHDIcon));
    }
    return;
  }

  QDir dir(path);

  // Add ".." to navigate up to parent or drive list
  QTreeWidgetItem *upItem = new QTreeWidgetItem(localListWidget);
  upItem->setText(0, "..");
  upItem->setData(0, Qt::UserRole, QString(".."));
  upItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));

  auto formatSize = [](qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024);
    return QString("%1 MB").arg(bytes / (1024 * 1024));
  };

  // Directories first
  for (const QFileInfo &info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase))
  {
    QTreeWidgetItem *item = new QTreeWidgetItem(localListWidget);
    item->setText(0, info.fileName());
    item->setText(2, info.lastModified().toString("yyyy-MM-dd HH:mm"));
    item->setData(0, Qt::UserRole, info.absoluteFilePath());
    item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
  }

  // Then files
  for (const QFileInfo &info : dir.entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase))
  {
    QTreeWidgetItem *item = new QTreeWidgetItem(localListWidget);
    item->setText(0, info.fileName());
    item->setText(1, formatSize(info.size()));
    item->setText(2, info.lastModified().toString("yyyy-MM-dd HH:mm"));

    // MD5 calculation for local file - skip if > 10MB to avoid UI hang
    if (info.size() < 10 * 1024 * 1024)
    {
        QFile file(info.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly))
        {
          QCryptographicHash hash(QCryptographicHash::Md5);
          if (hash.addData(&file))
          {
            item->setText(3, hash.result().toHex());
          }
        }
    }
    else
    {
        item->setText(3, "(För stor)");
    }

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
  saveCurrentSite();
  connectButton->setText("Koppla från");
  hostLineEdit->setEnabled(false);
  usernameLineEdit->setEnabled(false);
  passwordLineEdit->setEnabled(false);
  stopButton->setEnabled(true);
  connectionStatusIcon->setText("🟢");
  connectionStatusIcon->setToolTip("Ansluten");
}

void
MainWindow::onFtpDisconnected()
{
  remoteListWidget->clear();
  connectButton->setText("Anslut");
  hostLineEdit->setEnabled(true);
  usernameLineEdit->setEnabled(true);
  passwordLineEdit->setEnabled(true);
  stopButton->setEnabled(false);
  connectionStatusIcon->setText("🔴");
  connectionStatusIcon->setToolTip("Frånkopplad");
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

  // Separate and sort directories and files
  QStringList dirNames;
  QStringList fileNames;
  for (auto it = m_remoteFiles.constBegin(); it != m_remoteFiles.constEnd(); ++it)
  {
    if (it.value().isDir)
      dirNames.append(it.key());
    else
      fileNames.append(it.key());
  }

  dirNames.sort(Qt::CaseInsensitive);
  fileNames.sort(Qt::CaseInsensitive);

  auto formatSize = [](qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024);
    return QString("%1 MB").arg(bytes / (1024 * 1024));
  };

  auto addItem = [&](const QString &name) {
    const FtpCommunicator::RemoteFileInfo &info = m_remoteFiles.value(name);
    QTreeWidgetItem *item = new QTreeWidgetItem(remoteListWidget);
    item->setText(0, name);
    if (!info.isDir)
        item->setText(1, formatSize(info.size));
    item->setText(2, info.date);
    item->setText(3, info.md5);
    item->setData(0, Qt::UserRole, name);
    
    if (info.isDir)
    {
      item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    }
    else
    {
      item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    }
  };

  // Add directories first, then files
  for (const QString &name : dirNames)
  {
    addItem(name);
  }
  for (const QString &name : fileNames)
  {
    addItem(name);
  }
}

void
MainWindow::onFtpMd5Received(const QString &fileName, const QString &md5)
{
  if (m_remoteFiles.contains(fileName))
  {
    m_remoteFiles[fileName].md5 = md5;
  }

  for (int i = 0; i < remoteListWidget->topLevelItemCount(); ++i)
  {
    QTreeWidgetItem *item = remoteListWidget->topLevelItem(i);
    if (item->text(0) == fileName)
    {
      item->setText(2, md5);
      break;
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
  QAction *refreshAction = contextMenu.addAction("Uppdatera");
  QAction *createFolderAction = contextMenu.addAction("Skapa mapp");
  if (m_localCurrentPath.isEmpty())
    createFolderAction->setEnabled(false);

  QAction *uploadAction = nullptr;
  QAction *deleteAction = nullptr;

  if (!itemPath.isEmpty())
  {
    contextMenu.addSeparator();
    QFileInfo info(itemPath);
    if (info.isDir())
    {
      uploadAction = contextMenu.addAction("Ladda upp mapp till server");
      deleteAction = contextMenu.addAction("Ta bort mapp");
    }
    else
    {
      uploadAction = contextMenu.addAction("Ladda upp fil till server");
      deleteAction = contextMenu.addAction("Ta bort");
    }
  }

  QAction *selectedAction = contextMenu.exec(localListWidget->viewport()->mapToGlobal(pos));
  if (!selectedAction)
    return;

  if (selectedAction == refreshAction)
  {
    populateLocalList(m_localCurrentPath);
  }
  else if (selectedAction == createFolderAction)
  {
    createLocalFolder();
  }
  else if (selectedAction == uploadAction)
  {
    if (!m_ftpCommunicator->isConnected())
    {
      QMessageBox::warning(this, "Ej ansluten", "Anslut till servern för att ladda upp.");
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
    QString typeStr = info.isDir() ? "mappen" : "filen";
    QMessageBox::StandardButton reply =
        QMessageBox::question(this,
                              QString("Ta bort %1").arg(info.isDir() ? "mapp" : "fil"),
                              QString("Är du säker på att du vill ta bort %1 '%2'?").arg(typeStr, info.fileName()),
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
        QMessageBox::critical(this, "Fel", QString("Kunde inte ta bort %1: %2").arg(typeStr, info.fileName()));
      else
        populateLocalList(m_localCurrentPath);
    }
  }
}

void
MainWindow::createLocalFolder()
{
  if (m_localCurrentPath.isEmpty())
    return;

  bool ok;
  QString folderName = QInputDialog::getText(this,
                                             "Skapa mapp",
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
    QMessageBox::critical(this, "Fel", "Kunde inte skapa mappen.");
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
  QAction *refreshAction = contextMenu.addAction("Uppdatera");
  QAction *createFolderAction = contextMenu.addAction("Skapa ny mapp");
  QAction *downloadFileAction = nullptr;
  QAction *downloadFolderAction = nullptr;
  QAction *renameAction = nullptr;
  QAction *deleteAction = nullptr;
  QAction *deleteFolderAction = nullptr;

  if (!itemName.isEmpty())
  {
    contextMenu.addSeparator();
    renameAction = contextMenu.addAction("Byt namn");
    if (!m_ftpCommunicator->isDirectory(itemName))
    {
      downloadFileAction = contextMenu.addAction("Ladda ner fil");
      deleteAction = contextMenu.addAction("Ta bort fil");
    }
    else
    {
      downloadFolderAction = contextMenu.addAction("Ladda ner mapp");
      deleteFolderAction = contextMenu.addAction("Ta bort mapp");
    }
  }

  QAction *selectedAction = contextMenu.exec(remoteListWidget->viewport()->mapToGlobal(pos));
  if (!selectedAction)
    return;

  if (selectedAction == refreshAction)
  {
    m_ftpCommunicator->listRemoteDirectory(m_ftpCommunicator->getCurrentPath());
  }
  else if (selectedAction == createFolderAction)
  {
    createRemoteFolder();
  }
  else if (selectedAction == renameAction)
  {
    renameRemoteItem(itemName);
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
                              QString("Är du säker på att du vill ta bort filen '%1'?").arg(itemName),
                              QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
      deleteRemoteFileConfirmed(itemName);
  }
  else if (selectedAction == deleteFolderAction)
  {
    QMessageBox::StandardButton reply =
        QMessageBox::question(this,
                              "Ta bort mapp",
                              QString("Är du säker på att du vill ta bort mappen '%1'?").arg(itemName),
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
MainWindow::renameRemoteItem(const QString &oldName)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  bool ok;
  QString newName = QInputDialog::getText(this,
                                          "Byt namn",
                                          "Nytt namn:",
                                          QLineEdit::Normal,
                                          oldName,
                                          &ok);
  if (!ok || newName.trimmed().isEmpty() || newName == oldName)
    return;

  m_ftpCommunicator->renameRemote(oldName, newName.trimmed(), m_ftpCommunicator->getCurrentPath());
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
MainWindow::onLocalItemClicked(QTreeWidgetItem *item)
{
  QString itemPath = item->data(0, Qt::UserRole).toString();

  // Navigate up
  if (itemPath == "..")
  {
    QDir dir(m_localCurrentPath);
    if (dir.cdUp())
      populateLocalList(dir.absolutePath());
    else
      populateLocalList(""); // Show drives
    return;
  }

  QFileInfo info(itemPath);
  if (info.isDir())
  {
    populateLocalList(itemPath);
    return;
  }
}

void
MainWindow::onRemoteItemClicked(QTreeWidgetItem *item)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  QString name = item->data(0, Qt::UserRole).toString();
  if (name.isEmpty())
    name = item->text(0);

  if (name == "..") {
      // Navigate up
      QUrl url;
      QString currentPath = m_ftpCommunicator->getCurrentPath();
      if (currentPath.endsWith('/') || currentPath.isEmpty())
          url.setPath(currentPath);
      else
          url.setPath(currentPath + '/');

      QUrl newUrl = url.resolved(QUrl(name));
      m_ftpCommunicator->changeDirectory(newUrl.path());
      return;
  }

  if (m_ftpCommunicator->isDirectory(name))
  {
      // Navigate into directory
      QUrl url;
      QString currentPath = m_ftpCommunicator->getCurrentPath();
      if (currentPath.endsWith('/') || currentPath.isEmpty())
          url.setPath(currentPath);
      else
          url.setPath(currentPath + '/');

      QUrl newUrl = url.resolved(QUrl(name));
      m_ftpCommunicator->changeDirectory(newUrl.path());
  }
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

void
MainWindow::loadSavedSites()
{
  QSettings settings("Witech", "WitechFTP");
  int size = settings.beginReadArray("SavedSites");
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QString host = settings.value("host").toString();
    QString username = settings.value("username").toString();
    QString password = settings.value("password").toString();
    
    QVariantMap siteData;
    siteData["host"] = host;
    siteData["username"] = username;
    siteData["password"] = password;
    
    savedSitesComboBox->addItem(QString("%1 (%2)").arg(host, username), siteData);
  }
  settings.endArray();
}

void
MainWindow::saveCurrentSite()
{
  QString currentHost = hostLineEdit->text().trimmed();
  QString currentUsername = usernameLineEdit->text().trimmed();
  QString currentPassword = passwordLineEdit->text();
  
  if (currentHost.isEmpty()) return;

  QSettings settings("Witech", "WitechFTP");
  
  // Read existing
  QList<QVariantMap> sites;
  int size = settings.beginReadArray("SavedSites");
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QVariantMap siteData;
    siteData["host"] = settings.value("host").toString();
    siteData["username"] = settings.value("username").toString();
    siteData["password"] = settings.value("password").toString();
    sites.append(siteData);
  }
  settings.endArray();
  
  // Check if it already exists
  bool exists = false;
  for (int i = 0; i < sites.size(); ++i) {
    if (sites[i]["host"].toString() == currentHost && sites[i]["username"].toString() == currentUsername) {
      // Update password if changed
      sites[i]["password"] = currentPassword;
      exists = true;
      
      // Update combo box
      for (int j = 1; j < savedSitesComboBox->count(); ++j) {
        QVariantMap data = savedSitesComboBox->itemData(j).toMap();
        if (data["host"].toString() == currentHost && data["username"].toString() == currentUsername) {
          savedSitesComboBox->setItemData(j, sites[i]);
          break;
        }
      }
      break;
    }
  }
  
  if (!exists) {
    QVariantMap newSite;
    newSite["host"] = currentHost;
    newSite["username"] = currentUsername;
    newSite["password"] = currentPassword;
    sites.append(newSite);
    
    // Add to UI combobox as well
    savedSitesComboBox->addItem(QString("%1 (%2)").arg(currentHost, currentUsername), newSite);
  }
  
  // Save back
  settings.beginWriteArray("SavedSites");
  for (int i = 0; i < sites.size(); ++i) {
    settings.setArrayIndex(i);
    settings.setValue("host", sites[i]["host"]);
    settings.setValue("username", sites[i]["username"]);
    settings.setValue("password", sites[i]["password"]);
  }
  settings.endArray();
}

void
MainWindow::onSavedSiteSelected(int index)
{
  if (index == 0) return; // "Sparade sajter..."
  
  QVariant data = savedSitesComboBox->itemData(index);
  QVariantMap siteData = data.toMap();
  
  if (!siteData.isEmpty()) {
    hostLineEdit->setText(siteData["host"].toString());
    usernameLineEdit->setText(siteData["username"].toString());
    passwordLineEdit->setText(siteData["password"].toString());
    
    if (m_ftpCommunicator->isConnected()) {
      // Koppla från automatiskt
      connectOrDisconnect();
      
      // Vänta en halv sekund så anslutningen hinner stängas, anslut sedan igen
      QTimer::singleShot(500, this, &MainWindow::connectOrDisconnect);
    } else {
      // Auto-connect!
      connectOrDisconnect();
    }
  }
  
  // Återställ rullgardinsmenyn så den fungerar som en åtgärdsmeny
  savedSitesComboBox->setCurrentIndex(0);
}
