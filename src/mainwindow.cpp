#include "mainwindow.h"
#include "ftpcommunicator.h"
#include "langstrings.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QIcon>
#include <QInputDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>

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
  connect(m_ftpCommunicator, &FtpCommunicator::transferCountMismatch, this, &MainWindow::onFtpTransferCountMismatch);

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

QString
MainWindow::formatSize(qint64 bytes)
{
  if (bytes < 1024) return QString("%1 B").arg(bytes);
  if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024);
  return QString("%1 MB").arg(bytes / (1024 * 1024));
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

QString
MainWindow::pathFromItem(QTreeWidgetItem *item)
{
  if (!item)
    return QString();
  QString p = item->data(0, Qt::UserRole).toString();
  return (p == "..") ? QString() : p;
}

QString
MainWindow::remoteNameFromItem(QTreeWidgetItem *item)
{
  if (!item)
    return QString();
  QString name = item->data(0, Qt::UserRole).toString();
  if (name.isEmpty())
    name = item->text(0);
  return (name == "..") ? QString() : name;
}

void
MainWindow::closeEvent(QCloseEvent *event)
{
  if (m_ftpCommunicator->isTransferInProgress())
  {
    auto reply = QMessageBox::question(this,
                                       m_s->dlgQuitCopyingTitle,
                                       m_s->dlgQuitCopyingMsg,
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::No)
    {
      event->ignore();
      return;
    }
  }
  QMainWindow::closeEvent(event);
}

QString
MainWindow::promptForName(const QString &title, const QString &prompt, const QString &initial)
{
  bool ok = false;
  QString name = QInputDialog::getText(this, title, prompt, QLineEdit::Normal, initial, &ok);
  if (!ok)
    return QString();
  return name.trimmed();
}
