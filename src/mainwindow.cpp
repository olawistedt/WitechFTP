#include "mainwindow.h"
#include "ftpcommunicator.h"
#include "filetreewidget.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
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

struct LangStrings {
  const char *savedSites;
  const char *connectBtn;
  const char *disconnectBtn;
  const char *connectingBtn;
  const char *stopBtn;
  const char *labelHost;
  const char *labelUsername;
  const char *labelPassword;
  const char *tooltipConnected;
  const char *tooltipDisconnected;
  const char *placeholderLocalPath;
  const char *placeholderRemotePath;
  const char *placeholderStatusLog;
  const char *colName;
  const char *colSize;
  const char *colDate;
  const char *langBtnLabel;
  // Context menus
  const char *menuRefresh;
  const char *menuCreateFolder;
  const char *menuCreateNewFolder;
  const char *menuRename;
  const char *menuUploadFolder;
  const char *menuUploadFile;
  const char *menuDelete;
  const char *menuDeleteFolder;
  const char *menuDownloadFile;
  const char *menuDownloadFolder;
  const char *menuDeleteFile;
  // Dialogs – connection
  const char *dlgConnecting;
  const char *dlgFtpPasswordTitle;
  const char *dlgFtpPasswordPrompt;
  const char *dlgConnErrTitle;
  const char *dlgNotConnTitle;
  const char *dlgNotConnUploadMsg;
  const char *dlgNotConnUploadFolderMsg;
  // Dialogs – delete (remote, specific)
  const char *dlgDeleteFileTitle;
  const char *dlgDeleteFileMsg;
  const char *dlgDeleteFolderTitle;
  const char *dlgDeleteFolderMsg;
  // Dialogs – delete (local, generic with type word)
  const char *wordFolder;
  const char *wordFile;
  const char *wordFolderDef;
  const char *wordFileDef;
  const char *dlgDeleteTitle;
  const char *dlgDeleteMsg;
  const char *dlgDeleteMultiTitle;
  const char *dlgDeleteMultiMsg;
  // Dialogs – errors
  const char *dlgErrTitle;
  const char *dlgErrDeleteMsg;
  const char *dlgErrCreateFolderMsg;
  const char *dlgErrRenameMsg;
  // Dialogs – create folder
  const char *dlgCreateFolderTitle;
  const char *dlgFolderNamePrompt;
  // Dialogs – rename
  const char *dlgRenameTitle;
  const char *dlgNewNamePrompt;
  // Dialogs – file already exists (local rename)
  const char *dlgFileExistsTitle;
  const char *dlgFileExistsMsg;
  // Dialogs – overwrite on download
  const char *dlgOverwriteTitle;
  const char *dlgOverwriteMsg;
  // Dialogs – file not found on upload
  const char *dlgFileNotFoundTitle;
  const char *dlgFileNotFoundMsg;
  // Log
  const char *logDeleted;
};

static const LangStrings s_sv = {
  "Sparade sajter...",
  "Anslut",
  "Koppla från",
  "Ansluter...",
  "Stoppa",
  "Host:",
  "Användarnamn:",
  "Lösenord:",
  "Ansluten",
  "Frånkopplad",
  "Lokal sökväg...",
  "Serversökväg...",
  "Status log...",
  "Namn",
  "Storlek",
  "Datum",
  "English",
  "Uppdatera",
  "Skapa mapp",
  "Skapa ny mapp",
  "Byt namn",
  "Ladda upp mapp till server",
  "Ladda upp fil till server",
  "Ta bort",
  "Ta bort mapp",
  "Ladda ner fil",
  "Ladda ner mapp",
  "Ta bort fil",
  "Ansluter till %1:21...",
  "FTP-lösenord",
  "Ange FTP-lösenord:",
  "Anslutningsfel",
  "Ej ansluten",
  "Anslut till servern för att ladda upp.",
  "Anslut till servern för att ladda upp mappar.",
  "Ta bort fil",
  "Är du säker på att du vill ta bort filen '%1'?",
  "Ta bort mapp",
  "Är du säker på att du vill ta bort mappen '%1'?",
  "mapp",
  "fil",
  "mappen",
  "filen",
  "Ta bort %1",
  "Är du säker på att du vill ta bort %1 '%2'?",
  "Ta bort markerade",
  "Är du säker på att du vill ta bort %1 markerade objekt?",
  "Fel",
  "Kunde inte ta bort %1: %2",
  "Kunde inte skapa mappen.",
  "Kunde inte byta namn på objektet.",
  "Skapa mapp",
  "Mappnamn:",
  "Byt namn",
  "Nytt namn:",
  "Filen finns redan",
  "En fil eller mapp med det namnet finns redan.",
  "Filen finns redan",
  "Filen '%1' finns redan. Skriv över?",
  "Fel",
  "Filen finns inte.",
  "Tog bort %1: %2",
};

static const LangStrings s_en = {
  "Saved sites...",
  "Connect",
  "Disconnect",
  "Connecting...",
  "Stop",
  "Host:",
  "Username:",
  "Password:",
  "Connected",
  "Disconnected",
  "Local path...",
  "Server path...",
  "Status log...",
  "Name",
  "Size",
  "Date",
  "Español",
  "Refresh",
  "Create folder",
  "Create new folder",
  "Rename",
  "Upload folder to server",
  "Upload file to server",
  "Delete",
  "Delete folder",
  "Download file",
  "Download folder",
  "Delete file",
  "Connecting to %1:21...",
  "FTP Password",
  "Enter FTP password:",
  "Connection Error",
  "Not connected",
  "Connect to the server to upload.",
  "Connect to the server to upload folders.",
  "Delete file",
  "Are you sure you want to delete the file '%1'?",
  "Delete folder",
  "Are you sure you want to delete the folder '%1'?",
  "folder",
  "file",
  "the folder",
  "the file",
  "Delete %1",
  "Are you sure you want to delete %1 '%2'?",
  "Delete selected",
  "Are you sure you want to delete %1 selected items?",
  "Error",
  "Could not delete %1: %2",
  "Could not create the folder.",
  "Could not rename the item.",
  "Create folder",
  "Folder name:",
  "Rename",
  "New name:",
  "File already exists",
  "A file or folder with that name already exists.",
  "File exists",
  "File '%1' already exists. Overwrite?",
  "Error",
  "File does not exist.",
  "Deleted %1: %2",
};

static const LangStrings s_es = {
  "Sitios guardados...",
  "Conectar",
  "Desconectar",
  "Conectando...",
  "Detener",
  "Host:",
  "Usuario:",
  "Contraseña:",
  "Conectado",
  "Desconectado",
  "Ruta local...",
  "Ruta del servidor...",
  "Registro de estado...",
  "Nombre",
  "Tamaño",
  "Fecha",
  "Svenska",
  "Actualizar",
  "Crear carpeta",
  "Crear nueva carpeta",
  "Renombrar",
  "Subir carpeta al servidor",
  "Subir archivo al servidor",
  "Eliminar",
  "Eliminar carpeta",
  "Descargar archivo",
  "Descargar carpeta",
  "Eliminar archivo",
  "Conectando a %1:21...",
  "Contraseña FTP",
  "Introduce la contraseña FTP:",
  "Error de conexión",
  "No conectado",
  "Conéctate al servidor para subir.",
  "Conéctate al servidor para subir carpetas.",
  "Eliminar archivo",
  "¿Estás seguro de que quieres eliminar el archivo '%1'?",
  "Eliminar carpeta",
  "¿Estás seguro de que quieres eliminar la carpeta '%1'?",
  "carpeta",
  "archivo",
  "la carpeta",
  "el archivo",
  "Eliminar %1",
  "¿Estás seguro de que quieres eliminar %1 '%2'?",
  "Eliminar seleccionados",
  "¿Estás seguro de que quieres eliminar %1 elementos seleccionados?",
  "Error",
  "No se pudo eliminar %1: %2",
  "No se pudo crear la carpeta.",
  "No se pudo renombrar el elemento.",
  "Crear carpeta",
  "Nombre de carpeta:",
  "Renombrar",
  "Nuevo nombre:",
  "El archivo ya existe",
  "Ya existe un archivo o carpeta con ese nombre.",
  "Archivo existente",
  "El archivo '%1' ya existe. ¿Sobrescribir?",
  "Error",
  "El archivo no existe.",
  "Eliminado %1: %2",
};

static const LangStrings s_ja = {
  "保存済みサイト...",
  "接続",
  "切断",
  "接続中...",
  "停止",
  "ホスト:",
  "ユーザー名:",
  "パスワード:",
  "接続済み",
  "未接続",
  "ローカルパス...",
  "サーバーパス...",
  "ステータスログ...",
  "名前",
  "サイズ",
  "日付",
  "",
  "更新",
  "フォルダを作成",
  "新しいフォルダを作成",
  "名前の変更",
  "フォルダをサーバーにアップロード",
  "ファイルをサーバーにアップロード",
  "削除",
  "フォルダを削除",
  "ファイルをダウンロード",
  "フォルダをダウンロード",
  "ファイルを削除",
  "%1:21 に接続中...",
  "FTPパスワード",
  "FTPパスワードを入力:",
  "接続エラー",
  "未接続",
  "アップロードするにはサーバーに接続してください。",
  "フォルダをアップロードするにはサーバーに接続してください。",
  "ファイルを削除",
  "ファイル '%1' を削除してもよろしいですか?",
  "フォルダを削除",
  "フォルダ '%1' を削除してもよろしいですか?",
  "フォルダ",
  "ファイル",
  "フォルダ",
  "ファイル",
  "%1を削除",
  "%1 '%2' を削除してもよろしいですか?",
  "選択項目を削除",
  "選択された%1個のアイテムを削除してもよろしいですか?",
  "エラー",
  "%1を削除できませんでした: %2",
  "フォルダを作成できませんでした。",
  "名前を変更できませんでした。",
  "フォルダを作成",
  "フォルダ名:",
  "名前の変更",
  "新しい名前:",
  "ファイルが既に存在します",
  "同じ名前のファイルまたはフォルダが既に存在します。",
  "ファイルが存在します",
  "ファイル '%1' は既に存在します。上書きしますか?",
  "エラー",
  "ファイルが存在しません。",
  "%1を削除しました: %2",
};

static const LangStrings *s_langs[] = {&s_sv, &s_en, &s_es, &s_ja};

class FileTreeItem : public QTreeWidgetItem
{
public:
  enum ItemType { ParentDir = 0, Directory = 1, File = 2 };

  FileTreeItem(QTreeWidget *parent, ItemType type)
      : QTreeWidgetItem(parent), m_type(type) {}

  bool operator<(const QTreeWidgetItem &other) const override
  {
    const auto *o = static_cast<const FileTreeItem *>(&other);
    if (m_type != o->m_type) {
      if (m_type == ParentDir || o->m_type == ParentDir) {
        Qt::SortOrder ord = treeWidget()->header()->sortIndicatorOrder();
        return ord == Qt::AscendingOrder ? m_type == ParentDir : m_type != ParentDir;
      }
      return m_type < o->m_type;
    }
    int col = treeWidget()->sortColumn();
    if (col == 1)
      return data(1, Qt::UserRole + 1).toLongLong() < o->data(1, Qt::UserRole + 1).toLongLong();
    return text(col).compare(o->text(col), Qt::CaseInsensitive) < 0;
  }

private:
  ItemType m_type;
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
  m_ftpCommunicator = new FtpCommunicator(this);
  m_localWatcher = new QFileSystemWatcher(this);

  QDir().mkpath(QDir::homePath() + "/.WitechFTP");
  {
    QSettings s(QDir::homePath() + "/.WitechFTP/settings.ini", QSettings::IniFormat);
    m_language = s.value("language", 1).toInt();
  }
  m_s = s_langs[m_language];

  // Connect FTP communicator signals
  connect(m_ftpCommunicator, &FtpCommunicator::connected, this, &MainWindow::onFtpConnected);
  connect(m_ftpCommunicator, &FtpCommunicator::disconnected, this, &MainWindow::onFtpDisconnected);
  connect(m_ftpCommunicator, &FtpCommunicator::connectionError, this, &MainWindow::onFtpConnectionError);
  connect(m_ftpCommunicator, &FtpCommunicator::statusUpdated, this, &MainWindow::onFtpStatusUpdated);
  connect(m_ftpCommunicator, &FtpCommunicator::directoryListReceived, this, &MainWindow::onFtpDirectoryListReceived);
  connect(m_ftpCommunicator, &FtpCommunicator::md5Received, this, &MainWindow::onFtpMd5Received);
  connect(m_ftpCommunicator, &FtpCommunicator::downloadComplete, this, &MainWindow::onFtpDownloadComplete);
  connect(m_ftpCommunicator, &FtpCommunicator::uploadComplete, this, &MainWindow::onFtpUploadComplete);

  // Connect local watcher
  connect(m_localWatcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::onLocalDirectoryChanged);

  createUi();

  setWindowTitle("Witech FTP");
  setWindowIcon(QIcon(":/ftp-icon.png"));

  QSettings settings(QDir::homePath() + "/.WitechFTP/settings.ini", QSettings::IniFormat);
  hostLineEdit->setText(settings.value("lastHost", "").toString());
  usernameLineEdit->setText(settings.value("lastUsername", "").toString());
  if (passwordLineEdit->text().isEmpty())
    passwordLineEdit->setText(settings.value("lastPassword").toString());

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
  QSettings settings(QDir::homePath() + "/.WitechFTP/settings.ini", QSettings::IniFormat);
  settings.setValue("windowGeometry", saveGeometry());
  settings.setValue("lastHost", hostLineEdit->text());
  settings.setValue("lastUsername", usernameLineEdit->text());
  settings.setValue("lastPassword", passwordLineEdit->text());
  settings.setValue("language", m_language);

  if (!m_localCurrentPath.isEmpty())
    settings.setValue("lastLocalPath", m_localCurrentPath);
}

void
MainWindow::createUi()
{
  // --- Title ---
  QWidget *titleWidget = new QWidget;
  QGridLayout *titleGrid = new QGridLayout(titleWidget);
  titleGrid->setContentsMargins(5, 5, 5, 5);

  QLabel *titleLabel = new QLabel("WitechFTP v1.4-beta1");
  QFont titleFont("Arial", 24, QFont::Bold);
  titleLabel->setFont(titleFont);
  titleLabel->setAlignment(Qt::AlignCenter);

  QHBoxLayout *rightLayout = new QHBoxLayout;

  m_langCombo = new QComboBox;
  m_langCombo->setMinimumWidth(200);
  m_langCombo->addItems({"Choose language", "Svenska", "English", "Español", "日本語"});
  // English (1) is the "Choose language" default; any other saved language is shown explicitly
  m_langCombo->setCurrentIndex(m_language == 1 ? 0 : m_language + 1);
  connect(m_langCombo, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onLanguageChanged);

  savedSitesComboBox = new QComboBox;
  savedSitesComboBox->addItem(m_s->savedSites);
  loadSavedSites();
  connect(savedSitesComboBox, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onSavedSiteSelected);

  rightLayout->addWidget(m_langCombo);
  rightLayout->addWidget(savedSitesComboBox);

  QWidget *rightWidget = new QWidget;
  rightWidget->setLayout(rightLayout);

  // Title spans full width and is truly centered; right controls sit on top-right
  titleGrid->addWidget(titleLabel,  0, 0, 1, 2, Qt::AlignCenter);
  titleGrid->addWidget(rightWidget, 0, 1,       Qt::AlignRight);
  titleWidget->setStyleSheet("padding: 5px;");

  // --- Connection Bar ---
  QWidget *connectionWidget = new QWidget;
  QHBoxLayout *connectionLayout = new QHBoxLayout(connectionWidget);
  
  connectionStatusIcon = new QLabel("🔴");
  connectionStatusIcon->setToolTip(m_s->tooltipDisconnected);

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
    passwordLineEdit->setText(envPassword);

  connectButton = new QPushButton(m_s->connectBtn);
  stopButton = new QPushButton(m_s->stopBtn);
  stopButton->setEnabled(false);
  connect(stopButton, &QPushButton::clicked, m_ftpCommunicator, &FtpCommunicator::abortTransfer);

  m_labelHost     = new QLabel(m_s->labelHost);
  m_labelUsername = new QLabel(m_s->labelUsername);
  m_labelPassword = new QLabel(m_s->labelPassword);

  connectionLayout->addWidget(connectionStatusIcon);
  connectionLayout->addWidget(m_labelHost);
  connectionLayout->addWidget(hostLineEdit);
  connectionLayout->addWidget(m_labelUsername);
  connectionLayout->addWidget(usernameLineEdit);
  connectionLayout->addWidget(m_labelPassword);
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
  QSettings settings(QDir::homePath() + "/.WitechFTP/settings.ini", QSettings::IniFormat);
  QString localStartPath = settings.value("lastLocalPath").toString();
  if (localStartPath.isEmpty() || !QDir(localStartPath).exists())
  {
    localStartPath =
        QProcessEnvironment::systemEnvironment().value("USERPROFILE", QDir::homePath());
  }

  QWidget *localWidget = new QWidget(splitter);
  QVBoxLayout *localLayout = new QVBoxLayout(localWidget);
  localLayout->setContentsMargins(0, 0, 0, 0);
  localLayout->setSpacing(0);

  localPathEdit = new QLineEdit(localWidget);
  localPathEdit->setPlaceholderText(m_s->placeholderLocalPath);
  localPathEdit->setText(localStartPath);
  localLayout->addWidget(localPathEdit);

  auto *localTree = new FileTreeWidget(FileTreeWidget::Local, localWidget);
  localListWidget = localTree;
  localListWidget->setHeaderLabels({m_s->colName, m_s->colSize, m_s->colDate, "MD5"});
  localListWidget->setColumnWidth(0, 200);
  localListWidget->setColumnWidth(1, 80);
  localListWidget->setColumnWidth(2, 150);
  localListWidget->setColumnWidth(3, 250);
  localListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  localListWidget->setIconSize(QSize(16, 16));
  localListWidget->setRootIsDecorated(false);
  localListWidget->setSortingEnabled(true);
  localListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
  localListWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  localListWidget->header()->setSortIndicatorShown(true);
  localListWidget->sortByColumn(0, Qt::AscendingOrder);
  localLayout->addWidget(localListWidget);

  // Remote
  QWidget *remoteWidget = new QWidget(splitter);
  QVBoxLayout *remoteLayout = new QVBoxLayout(remoteWidget);
  remoteLayout->setContentsMargins(0, 0, 0, 0);
  remoteLayout->setSpacing(0);

  remotePathEdit = new QLineEdit(remoteWidget);
  remotePathEdit->setPlaceholderText(m_s->placeholderRemotePath);
  remoteLayout->addWidget(remotePathEdit);

  auto *remoteTree = new FileTreeWidget(FileTreeWidget::Remote, remoteWidget);
  remoteListWidget = remoteTree;
  remoteListWidget->setHeaderLabels({m_s->colName, m_s->colSize, m_s->colDate, "MD5"});
  remoteListWidget->setColumnWidth(0, 200);
  remoteListWidget->setColumnWidth(1, 80);
  remoteListWidget->setColumnWidth(2, 150);
  remoteListWidget->setColumnWidth(3, 250);
  remoteListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  remoteListWidget->setIconSize(QSize(16, 16));
  remoteListWidget->setRootIsDecorated(false);
  remoteListWidget->setSortingEnabled(true);
  remoteListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
  remoteListWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  remoteListWidget->header()->setSortIndicatorShown(true);
  remoteListWidget->sortByColumn(0, Qt::AscendingOrder);
  remoteLayout->addWidget(remoteListWidget);

  splitter->addWidget(localWidget);
  splitter->addWidget(remoteWidget);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 1);

  // --- Status Log ---
  statusLog = new QTextEdit;
  statusLog->setReadOnly(true);
  statusLog->setMinimumHeight(40);
  statusLog->setFont(QFont("Courier New", 8));
  statusLog->setPlaceholderText(m_s->placeholderStatusLog);

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
  connect(remoteListWidget, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onRemoteItemClicked);
  connect(localListWidget,
          &QTreeWidget::customContextMenuRequested,
          this,
          &MainWindow::showLocalContextMenu);
  connect(remoteListWidget,
          &QTreeWidget::customContextMenuRequested,
          this,
          &MainWindow::showRemoteContextMenu);
  connect(localListWidget, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onLocalItemClicked);

  connect(localPathEdit, &QLineEdit::returnPressed, this, [this]() {
    QString path = localPathEdit->text().trimmed();
    if (path.isEmpty() || QDir(path).exists())
      populateLocalList(path);
    else
      localPathEdit->setText(m_localCurrentPath);
  });

  connect(remotePathEdit, &QLineEdit::returnPressed, this, [this]() {
    if (m_ftpCommunicator->isConnected())
      m_ftpCommunicator->changeDirectory(remotePathEdit->text().trimmed());
    else
      remotePathEdit->setText(m_currentRemotePath);
  });

  // Install event filter on main window to catch Delete key press
  installEventFilter(this);

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

  // Keyboard support for Rename (F2)
  QShortcut *localRenameShortcut = new QShortcut(QKeySequence("F2"), localListWidget);
  connect(localRenameShortcut, &QShortcut::activated, [this]() {
    QTreeWidgetItem *item = localListWidget->currentItem();
    if (item) {
        QString itemPath = item->data(0, Qt::UserRole).toString();
        if (itemPath != ".." && !itemPath.isEmpty())
            renameLocalItem(itemPath);
    }
  });

  QShortcut *remoteRenameShortcut = new QShortcut(QKeySequence("F2"), remoteListWidget);
  connect(remoteRenameShortcut, &QShortcut::activated, [this]() {
    QTreeWidgetItem *item = remoteListWidget->currentItem();
    if (item && m_ftpCommunicator->isConnected()) {
        QString itemName = item->data(0, Qt::UserRole).toString();
        if (itemName != ".." && !itemName.isEmpty())
            renameRemoteItem(itemName);
    }
  });

  // Drag-and-drop transfers between the two trees
  connect(remoteTree, &FileTreeWidget::localItemsDropped, this,
          [this](const QStringList &paths) {
            if (!m_ftpCommunicator->isConnected())
            {
              QMessageBox::warning(this, m_s->dlgNotConnTitle, m_s->dlgNotConnUploadMsg);
              return;
            }
            m_ftpCommunicator->uploadItems(paths, m_ftpCommunicator->getCurrentPath());
          });

  connect(localTree, &FileTreeWidget::remoteItemsDropped, this,
          [this](const QStringList &names) {
            if (!m_ftpCommunicator->isConnected())
              return;
            QString localDir = m_localCurrentPath.isEmpty() ? QDir::currentPath() : m_localCurrentPath;
            m_ftpCommunicator->downloadItems(names, localDir);
          });

  populateLocalList(localStartPath);
}

void
MainWindow::retranslateUi()
{
  savedSitesComboBox->setItemText(0, m_s->savedSites);
  m_labelHost->setText(m_s->labelHost);
  m_labelUsername->setText(m_s->labelUsername);
  m_labelPassword->setText(m_s->labelPassword);
  bool connected = m_ftpCommunicator->isConnected();
  connectButton->setText(connected ? m_s->disconnectBtn : m_s->connectBtn);
  stopButton->setText(m_s->stopBtn);
  connectionStatusIcon->setToolTip(connected ? m_s->tooltipConnected : m_s->tooltipDisconnected);
  localPathEdit->setPlaceholderText(m_s->placeholderLocalPath);
  remotePathEdit->setPlaceholderText(m_s->placeholderRemotePath);
  statusLog->setPlaceholderText(m_s->placeholderStatusLog);
  localListWidget->setHeaderLabels({m_s->colName, m_s->colSize, m_s->colDate, "MD5"});
  remoteListWidget->setHeaderLabels({m_s->colName, m_s->colSize, m_s->colDate, "MD5"});
  m_langCombo->setCurrentIndex(m_language + 1);
}

void
MainWindow::onLanguageChanged(int index)
{
  constexpr int numLangs = sizeof(s_langs) / sizeof(s_langs[0]);
  int lang = index - 1; // offset: index 0 is "Choose language"
  if (lang < 0 || lang >= numLangs)
    return;
  m_language = lang;
  m_s = s_langs[m_language];
  retranslateUi();
}

void
MainWindow::populateLocalList(const QString &path)
{
  int sortCol = localListWidget->header()->sortIndicatorSection();
  Qt::SortOrder sortOrder = localListWidget->header()->sortIndicatorOrder();
  localListWidget->setSortingEnabled(false);

  m_localCurrentPath = path;
  localPathEdit->setText(path);

  // Update watcher to monitor the current directory
  if (!m_localWatcher->directories().isEmpty())
    m_localWatcher->removePaths(m_localWatcher->directories());

  if (!path.isEmpty() && QDir(path).exists())
    m_localWatcher->addPath(path);

  localListWidget->clear();

  if (path.isEmpty())
  {
    for (const QFileInfo &drive : QDir::drives())
    {
      auto *item = new FileTreeItem(localListWidget, FileTreeItem::Directory);
      item->setText(0, drive.absoluteFilePath());
      item->setData(0, Qt::UserRole, drive.absoluteFilePath());
      item->setIcon(0, style()->standardIcon(QStyle::SP_DriveHDIcon));
    }
    localListWidget->setSortingEnabled(true);
    localListWidget->sortByColumn(sortCol, sortOrder);
    return;
  }

  QDir dir(path);

  // Add ".." to navigate up to parent or drive list
  auto *upItem = new FileTreeItem(localListWidget, FileTreeItem::ParentDir);
  upItem->setText(0, "..");
  upItem->setData(0, Qt::UserRole, QString(".."));
  upItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));

  auto formatSize = [](qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024);
    return QString("%1 MB").arg(bytes / (1024 * 1024));
  };

  // Directories
  for (const QFileInfo &info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden))
  {
    auto *item = new FileTreeItem(localListWidget, FileTreeItem::Directory);
    item->setText(0, info.fileName());
    item->setText(2, info.lastModified().toString("yyyy-MM-dd HH:mm"));
    item->setData(0, Qt::UserRole, info.absoluteFilePath());
    item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
  }

  // Files
  for (const QFileInfo &info : dir.entryInfoList(QDir::Files | QDir::Hidden))
  {
    auto *item = new FileTreeItem(localListWidget, FileTreeItem::File);
    item->setText(0, info.fileName());
    item->setText(1, formatSize(info.size()));
    item->setText(2, info.lastModified().toString("yyyy-MM-dd HH:mm"));
    item->setData(1, Qt::UserRole + 1, info.size());

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

  localListWidget->setSortingEnabled(true);
  localListWidget->sortByColumn(sortCol, sortOrder);
}

void
MainWindow::connectOrDisconnect()
{
  if (m_ftpCommunicator->isConnected())
  {
    m_ftpCommunicator->disconnectFromHost();
    return;
  }

  QString password = passwordLineEdit->text();

  logStatus(QString(m_s->dlgConnecting).arg(hostLineEdit->text()));
  m_ftpCommunicator->connectToHost(hostLineEdit->text(), usernameLineEdit->text(), password);
  connectButton->setText(m_s->connectingBtn);
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
  connectButton->setText(m_s->disconnectBtn);
  hostLineEdit->setEnabled(false);
  usernameLineEdit->setEnabled(false);
  passwordLineEdit->setEnabled(false);
  stopButton->setEnabled(true);
  connectionStatusIcon->setText("🟢");
  connectionStatusIcon->setToolTip(m_s->tooltipConnected);
}

void
MainWindow::onFtpDisconnected()
{
  remoteListWidget->clear();
  remotePathEdit->clear();
  connectButton->setText(m_s->connectBtn);
  hostLineEdit->setEnabled(true);
  usernameLineEdit->setEnabled(true);
  passwordLineEdit->setEnabled(true);
  stopButton->setEnabled(false);
  connectionStatusIcon->setText("🔴");
  connectionStatusIcon->setToolTip(m_s->tooltipDisconnected);
}

void
MainWindow::onFtpConnectionError(const QString &error)
{
  if (hostLineEdit->text().trimmed().isEmpty())
    return;
  QMessageBox::critical(this, m_s->dlgConnErrTitle, error);
}

void
MainWindow::onFtpStatusUpdated(const QString &message)
{
  logStatus(message);
}

void
MainWindow::onFtpDirectoryListReceived()
{
  int sortCol = remoteListWidget->header()->sortIndicatorSection();
  Qt::SortOrder sortOrder = remoteListWidget->header()->sortIndicatorOrder();
  remoteListWidget->setSortingEnabled(false);
  remoteListWidget->clear();

  m_currentRemotePath = m_ftpCommunicator->getCurrentPath();
  remotePathEdit->setText(m_currentRemotePath);
  m_remoteFiles = m_ftpCommunicator->getRemoteFiles();

  // Add ".." to navigate up, unless we are at root
  if (m_currentRemotePath != "/")
  {
    auto *upItem = new FileTreeItem(remoteListWidget, FileTreeItem::ParentDir);
    upItem->setText(0, "..");
    upItem->setData(0, Qt::UserRole, QString(".."));
    upItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
  }

  auto formatSize = [](qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024);
    return QString("%1 MB").arg(bytes / (1024 * 1024));
  };

  auto addItem = [&](const QString &name) {
    const FtpCommunicator::RemoteFileInfo &info = m_remoteFiles.value(name);
    auto *item = new FileTreeItem(remoteListWidget,
                                  info.isDir ? FileTreeItem::Directory : FileTreeItem::File);
    item->setText(0, name);
    if (!info.isDir)
    {
      item->setText(1, formatSize(info.size));
      item->setData(1, Qt::UserRole + 1, info.size);
    }
    item->setText(2, info.date);
    item->setText(3, info.md5);
    item->setData(0, Qt::UserRole, name);
    item->setIcon(0, style()->standardIcon(info.isDir ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
  };

  for (auto it = m_remoteFiles.constBegin(); it != m_remoteFiles.constEnd(); ++it)
    addItem(it.key());

  remoteListWidget->setSortingEnabled(true);
  remoteListWidget->sortByColumn(sortCol, sortOrder);
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
  logStatus("Finished!");
  populateLocalList(m_localCurrentPath);
}

void
MainWindow::onFtpUploadComplete()
{
  logStatus("Finished!");
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
  QAction *refreshAction = contextMenu.addAction(m_s->menuRefresh);
  QAction *createFolderAction = contextMenu.addAction(m_s->menuCreateFolder);
  if (m_localCurrentPath.isEmpty())
    createFolderAction->setEnabled(false);

  QAction *renameAction = nullptr;
  QAction *uploadAction = nullptr;
  QAction *deleteAction = nullptr;

  if (!itemPath.isEmpty())
  {
    contextMenu.addSeparator();
    renameAction = contextMenu.addAction(m_s->menuRename);
    QFileInfo info(itemPath);
    if (info.isDir())
    {
      uploadAction = contextMenu.addAction(m_s->menuUploadFolder);
      deleteAction = contextMenu.addAction(m_s->menuDeleteFolder);
    }
    else
    {
      uploadAction = contextMenu.addAction(m_s->menuUploadFile);
      deleteAction = contextMenu.addAction(m_s->menuDelete);
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
  else if (selectedAction == renameAction)
  {
    renameLocalItem(itemPath);
  }
  else if (selectedAction == uploadAction)
  {
    if (!m_ftpCommunicator->isConnected())
    {
      QMessageBox::warning(this, m_s->dlgNotConnTitle, m_s->dlgNotConnUploadMsg);
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
    QString typeWord    = info.isDir() ? m_s->wordFolder    : m_s->wordFile;
    QString typeWordDef = info.isDir() ? m_s->wordFolderDef : m_s->wordFileDef;
    QMessageBox::StandardButton reply =
        QMessageBox::question(this,
                              QString(m_s->dlgDeleteTitle).arg(typeWord),
                              QString(m_s->dlgDeleteMsg).arg(typeWordDef, info.fileName()),
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
        QMessageBox::critical(this, m_s->dlgErrTitle, QString(m_s->dlgErrDeleteMsg).arg(typeWordDef, info.fileName()));
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
                                             m_s->dlgCreateFolderTitle,
                                             m_s->dlgFolderNamePrompt,
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
    QMessageBox::critical(this, m_s->dlgErrTitle, m_s->dlgErrCreateFolderMsg);
  }
}

void
MainWindow::renameLocalItem(const QString &oldPath)
{
  QFileInfo fileInfo(oldPath);
  QString oldName = fileInfo.fileName();

  bool ok;
  QString newName = QInputDialog::getText(this,
                                          m_s->dlgRenameTitle,
                                          m_s->dlgNewNamePrompt,
                                          QLineEdit::Normal,
                                          oldName,
                                          &ok);
  if (!ok || newName.trimmed().isEmpty() || newName == oldName)
    return;

  QString newPath = fileInfo.dir().filePath(newName.trimmed());

  if (QFile::exists(newPath))
  {
    QMessageBox::warning(this, m_s->dlgFileExistsTitle, m_s->dlgFileExistsMsg);
    return;
  }

  if (QFile::rename(oldPath, newPath))
  {
    populateLocalList(m_localCurrentPath);
  }
  else
  {
    QMessageBox::critical(this, m_s->dlgErrTitle, m_s->dlgErrRenameMsg);
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
  QAction *refreshAction = contextMenu.addAction(m_s->menuRefresh);
  QAction *createFolderAction = contextMenu.addAction(m_s->menuCreateNewFolder);
  QAction *downloadFileAction = nullptr;
  QAction *downloadFolderAction = nullptr;
  QAction *renameAction = nullptr;
  QAction *deleteAction = nullptr;
  QAction *deleteFolderAction = nullptr;

  if (!itemName.isEmpty())
  {
    contextMenu.addSeparator();
    renameAction = contextMenu.addAction(m_s->menuRename);
    if (!m_ftpCommunicator->isDirectory(itemName))
    {
      downloadFileAction = contextMenu.addAction(m_s->menuDownloadFile);
      deleteAction = contextMenu.addAction(m_s->menuDeleteFile);
    }
    else
    {
      downloadFolderAction = contextMenu.addAction(m_s->menuDownloadFolder);
      deleteFolderAction = contextMenu.addAction(m_s->menuDeleteFolder);
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
                              m_s->dlgDeleteFileTitle,
                              QString(m_s->dlgDeleteFileMsg).arg(itemName),
                              QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
      deleteRemoteFileConfirmed(itemName);
  }
  else if (selectedAction == deleteFolderAction)
  {
    QMessageBox::StandardButton reply =
        QMessageBox::question(this,
                              m_s->dlgDeleteFolderTitle,
                              QString(m_s->dlgDeleteFolderMsg).arg(itemName),
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
                                             m_s->dlgCreateFolderTitle,
                                             m_s->dlgFolderNamePrompt,
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
                                          m_s->dlgRenameTitle,
                                          m_s->dlgNewNamePrompt,
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
    QMessageBox::warning(this, m_s->dlgNotConnTitle, m_s->dlgNotConnUploadFolderMsg);
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
    QMessageBox::critical(this, m_s->dlgFileNotFoundTitle, m_s->dlgFileNotFoundMsg);
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
                              m_s->dlgOverwriteTitle,
                              QString(m_s->dlgOverwriteMsg).arg(fileName),
                              QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
      return;
  }

  m_ftpCommunicator->downloadFile(fileName, localDir);
}

void
MainWindow::loadSavedSites()
{
  QSettings settings(QDir::homePath() + "/.WitechFTP/settings.ini", QSettings::IniFormat);
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

  QSettings settings(QDir::homePath() + "/.WitechFTP/settings.ini", QSettings::IniFormat);
  
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

void
MainWindow::onLocalDirectoryChanged(const QString &path)
{
  if (path == m_localCurrentPath)
  {
    populateLocalList(path);
  }
}

bool
MainWindow::eventFilter(QObject *obj, QEvent *event)
{
  if (event->type() == QEvent::KeyPress)
  {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    
    if (keyEvent->key() == Qt::Key_Delete)
    {
      // Check which widget has focus
      QWidget *focusWidget = QApplication::focusWidget();
      
      if (focusWidget == localListWidget || localListWidget->isAncestorOf(focusWidget))
      {
        deleteLocalItemDirectly();
        return true;  // Mark event as handled
      }
      else if (focusWidget == remoteListWidget || remoteListWidget->isAncestorOf(focusWidget))
      {
        deleteRemoteItemDirectly();
        return true;  // Mark event as handled
      }
    }
  }

  return QMainWindow::eventFilter(obj, event);
}

void
MainWindow::deleteLocalItemDirectly()
{
  QList<QTreeWidgetItem *> selected = localListWidget->selectedItems();

  // Collect valid paths, skipping ".." navigation items
  QStringList paths;
  for (QTreeWidgetItem *item : selected)
  {
    QString itemPath = item->data(0, Qt::UserRole).toString();
    if (!itemPath.isEmpty() && itemPath != "..")
      paths.append(itemPath);
  }

  if (paths.isEmpty())
    return;

  // Build confirmation dialog
  QString title;
  QString msg;
  if (paths.size() == 1)
  {
    QFileInfo info(paths.first());
    QString typeWord    = info.isDir() ? m_s->wordFolder    : m_s->wordFile;
    QString typeWordDef = info.isDir() ? m_s->wordFolderDef : m_s->wordFileDef;
    title = QString(m_s->dlgDeleteTitle).arg(typeWord);
    msg   = QString(m_s->dlgDeleteMsg).arg(typeWordDef, info.fileName());
  }
  else
  {
    title = m_s->dlgDeleteMultiTitle;
    msg   = QString(m_s->dlgDeleteMultiMsg).arg(paths.size());
  }

  QMessageBox::StandardButton reply =
      QMessageBox::question(this, title, msg, QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes)
    return;

  QStringList failures;
  for (const QString &path : paths)
  {
    QFileInfo info(path);
    QString typeWordDef = info.isDir() ? m_s->wordFolderDef : m_s->wordFileDef;

    bool success = false;
    if (info.isDir())
    {
      QDir dir(path);
      success = dir.removeRecursively();
    }
    else
    {
      success = QFile::remove(path);
    }

    if (success)
      logStatus(QString(m_s->logDeleted).arg(typeWordDef, info.fileName()));
    else
      failures.append(info.fileName());
  }

  if (!failures.isEmpty())
  {
    QMessageBox::critical(this,
                          m_s->dlgErrTitle,
                          QString(m_s->dlgErrDeleteMsg).arg("", failures.join(", ")));
  }

  populateLocalList(m_localCurrentPath);
}

void
MainWindow::deleteRemoteItemDirectly()
{
  if (!m_ftpCommunicator->isConnected())
    return;

  QList<QTreeWidgetItem *> selected = remoteListWidget->selectedItems();

  // Collect valid names, skipping ".." navigation items
  QStringList names;
  for (QTreeWidgetItem *item : selected)
  {
    QString itemName = item->data(0, Qt::UserRole).toString();
    if (itemName.isEmpty())
      itemName = item->text(0);
    if (!itemName.isEmpty() && itemName != "..")
      names.append(itemName);
  }

  if (names.isEmpty())
    return;

  // Build confirmation dialog
  QString title;
  QString msg;
  if (names.size() == 1)
  {
    bool isDirectory    = m_ftpCommunicator->isDirectory(names.first());
    QString typeWord    = isDirectory ? m_s->wordFolder    : m_s->wordFile;
    QString typeWordDef = isDirectory ? m_s->wordFolderDef : m_s->wordFileDef;
    title = QString(m_s->dlgDeleteTitle).arg(typeWord);
    msg   = QString(m_s->dlgDeleteMsg).arg(typeWordDef, names.first());
  }
  else
  {
    title = m_s->dlgDeleteMultiTitle;
    msg   = QString(m_s->dlgDeleteMultiMsg).arg(names.size());
  }

  QMessageBox::StandardButton reply =
      QMessageBox::question(this, title, msg, QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes)
    return;

  // Queue all deletes - FtpCommunicator processes them sequentially
  for (const QString &name : names)
  {
    if (m_ftpCommunicator->isDirectory(name))
      deleteRemoteDirectoryConfirmed(name);
    else
      deleteRemoteFileConfirmed(name);
  }
}
