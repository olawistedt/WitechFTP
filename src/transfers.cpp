#include "mainwindow.h"
#include "ftpcommunicator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>

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

void
MainWindow::onFtpTransferCountMismatch()
{
  QMessageBox::critical(this, m_s->dlgErrTitle, m_s->dlgTransferMismatchMsg);
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

  QString remotePath = FtpCommunicator::joinPath(m_ftpCommunicator->getCurrentPath(), fileInfo.fileName());
  m_ftpCommunicator->uploadFile(filePath, remotePath);
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
MainWindow::downloadFile(const QString &fileName)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  QString localDir = m_localCurrentPath.isEmpty() ? QDir::currentPath() : m_localCurrentPath;
  QString localFilePath = QDir(localDir).filePath(fileName);

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
MainWindow::downloadFolder(const QString &folderName)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  QString localDir = m_localCurrentPath.isEmpty() ? QDir::currentPath() : m_localCurrentPath;
  m_ftpCommunicator->downloadFolder(folderName, localDir);
}
