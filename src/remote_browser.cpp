#include "mainwindow.h"
#include "filetreewidget.h"
#include "ftpcommunicator.h"

#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>

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
MainWindow::onRemoteItemClicked(QTreeWidgetItem *item)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  QString rawName = item->data(0, Qt::UserRole).toString();
  if (rawName.isEmpty())
    rawName = item->text(0);

  QString currentPath = m_ftpCommunicator->getCurrentPath();
  bool isUp = (rawName == "..");
  if (!isUp && !m_ftpCommunicator->isDirectory(rawName))
    return;

  QUrl base;
  base.setPath(currentPath.isEmpty() || currentPath.endsWith('/') ? currentPath : currentPath + '/');
  m_ftpCommunicator->changeDirectory(base.resolved(QUrl(rawName)).path());
}

void
MainWindow::showRemoteContextMenu(const QPoint &pos)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  QString itemName = remoteNameFromItem(remoteListWidget->itemAt(pos));

  QMenu contextMenu(this);
  QAction *refreshAction = contextMenu.addAction(m_s->menuRefresh);
  QAction *createFolderAction = contextMenu.addAction(m_s->menuCreateNewFolder);
  QAction *downloadFileAction = nullptr;
  QAction *downloadFolderAction = nullptr;
  QAction *renameAction = nullptr;
  QAction *deleteAction = nullptr;

  if (!itemName.isEmpty())
  {
    contextMenu.addSeparator();
    renameAction = contextMenu.addAction(m_s->menuRename);
    bool isDir = m_ftpCommunicator->isDirectory(itemName);
    if (isDir)
    {
      downloadFolderAction = contextMenu.addAction(m_s->menuDownloadFolder);
      deleteAction = contextMenu.addAction(m_s->menuDeleteFolder);
    }
    else
    {
      downloadFileAction = contextMenu.addAction(m_s->menuDownloadFile);
      deleteAction = contextMenu.addAction(m_s->menuDeleteFile);
    }
  }

  QAction *selectedAction = contextMenu.exec(remoteListWidget->viewport()->mapToGlobal(pos));
  if (!selectedAction)
    return;

  if (selectedAction == refreshAction)
    m_ftpCommunicator->listRemoteDirectory(m_ftpCommunicator->getCurrentPath());
  else if (selectedAction == createFolderAction)
    createRemoteFolder();
  else if (selectedAction == renameAction)
    renameRemoteItem(itemName);
  else if (selectedAction == downloadFileAction)
    downloadFile(itemName);
  else if (selectedAction == downloadFolderAction)
    downloadFolder(itemName);
  else if (selectedAction == deleteAction)
    deleteRemoteNames({ itemName });
}

void
MainWindow::createRemoteFolder()
{
  if (!m_ftpCommunicator->isConnected())
    return;

  QString folderName = promptForName(m_s->dlgCreateFolderTitle, m_s->dlgFolderNamePrompt);
  if (folderName.isEmpty())
    return;

  m_ftpCommunicator->createRemoteFolder(folderName, m_ftpCommunicator->getCurrentPath());
}

void
MainWindow::renameRemoteItem(const QString &oldName)
{
  if (!m_ftpCommunicator->isConnected())
    return;

  QString newName = promptForName(m_s->dlgRenameTitle, m_s->dlgNewNamePrompt, oldName);
  if (newName.isEmpty() || newName == oldName)
    return;

  m_ftpCommunicator->renameRemote(oldName, newName, m_ftpCommunicator->getCurrentPath());
}

void
MainWindow::deleteRemoteNames(const QStringList &names)
{
  if (names.isEmpty() || !m_ftpCommunicator->isConnected())
    return;

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

  if (QMessageBox::question(this, title, msg, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    return;

  QStringList files, dirs;
  for (const QString &name : names)
  {
    if (m_ftpCommunicator->isDirectory(name))
      dirs.append(name);
    else
      files.append(name);
  }
  m_ftpCommunicator->deleteRemoteItems(files, dirs, m_ftpCommunicator->getCurrentPath());
}

void
MainWindow::deleteRemoteItemDirectly()
{
  QStringList names;
  for (QTreeWidgetItem *item : remoteListWidget->selectedItems())
  {
    QString n = remoteNameFromItem(item);
    if (!n.isEmpty())
      names.append(n);
  }
  deleteRemoteNames(names);
}
