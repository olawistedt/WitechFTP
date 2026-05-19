#include "mainwindow.h"
#include "filetreewidget.h"
#include "ftpcommunicator.h"
#include "langstrings.h"

#include <QComboBox>
#include <QDir>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QFont>

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
    QString p = pathFromItem(localListWidget->currentItem());
    if (!p.isEmpty())
      renameLocalItem(p);
  });

  QShortcut *remoteRenameShortcut = new QShortcut(QKeySequence("F2"), remoteListWidget);
  connect(remoteRenameShortcut, &QShortcut::activated, [this]() {
    if (!m_ftpCommunicator->isConnected())
      return;
    QString n = remoteNameFromItem(remoteListWidget->currentItem());
    if (!n.isEmpty())
      renameRemoteItem(n);
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
  int lang = index - 1; // offset: index 0 is "Choose language"
  if (lang < 0 || lang >= kNumLangs)
    return;
  m_language = lang;
  m_s = s_langs[m_language];
  retranslateUi();
}
