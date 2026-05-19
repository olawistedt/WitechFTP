#include "mainwindow.h"
#include "filetreewidget.h"

#include <QAbstractItemDelegate>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>

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
MainWindow::onLocalDirectoryChanged(const QString &path)
{
  if (path == m_localCurrentPath)
  {
    populateLocalList(path);
  }
}

void
MainWindow::showLocalContextMenu(const QPoint &pos)
{
  QString itemPath = pathFromItem(localListWidget->itemAt(pos));

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
    if (QFileInfo(itemPath).isDir())
      uploadFolder(itemPath);
    else
      uploadFile(itemPath);
  }
  else if (selectedAction == deleteAction)
  {
    deleteLocalPaths({ itemPath });
  }
}

void
MainWindow::createLocalFolder()
{
  if (m_localCurrentPath.isEmpty())
    return;

  QString folderName = promptForName(m_s->dlgCreateFolderTitle, m_s->dlgFolderNamePrompt);
  if (folderName.isEmpty())
    return;

  if (QDir(m_localCurrentPath).mkdir(folderName))
    populateLocalList(m_localCurrentPath);
  else
    QMessageBox::critical(this, m_s->dlgErrTitle, m_s->dlgErrCreateFolderMsg);
}

void
MainWindow::renameLocalItem(const QString &oldPath)
{
  QTreeWidgetItem *item = nullptr;
  for (int i = 0; i < localListWidget->topLevelItemCount(); ++i)
  {
    QTreeWidgetItem *c = localListWidget->topLevelItem(i);
    if (c->data(0, Qt::UserRole).toString() == oldPath)
    {
      item = c;
      break;
    }
  }
  if (!item)
    return;

  m_renamingItem  = item;
  m_renamingOldText = item->text(0);
  m_renamingOldPath = oldPath;

  if (m_renameConnection)
    disconnect(m_renameConnection);

  m_renameConnection = connect(
    localListWidget->itemDelegate(), &QAbstractItemDelegate::closeEditor,
    this, [this](QWidget *, QAbstractItemDelegate::EndEditHint) {
      disconnect(m_renameConnection);
      QTreeWidgetItem *item = m_renamingItem;
      m_renamingItem = nullptr;
      if (!item)
        return;

      item->setFlags(item->flags() & ~Qt::ItemIsEditable);

      QString newName = item->text(0).trimmed();
      if (newName.isEmpty() || newName == m_renamingOldText)
      {
        item->setText(0, m_renamingOldText);
        return;
      }

      QFileInfo fileInfo(m_renamingOldPath);
      QString newPath = fileInfo.dir().filePath(newName);

      if (QFile::exists(newPath))
      {
        item->setText(0, m_renamingOldText);
        QMessageBox::warning(this, m_s->dlgFileExistsTitle, m_s->dlgFileExistsMsg);
        return;
      }

      if (!QFile::rename(m_renamingOldPath, newPath))
      {
        item->setText(0, m_renamingOldText);
        QMessageBox::critical(this, m_s->dlgErrTitle, m_s->dlgErrRenameMsg);
        return;
      }

      item->setData(0, Qt::UserRole, newPath);
    });

  item->setFlags(item->flags() | Qt::ItemIsEditable);
  localListWidget->editItem(item, 0);
}

void
MainWindow::deleteLocalPaths(const QStringList &paths)
{
  if (paths.isEmpty())
    return;

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

  if (QMessageBox::question(this, title, msg, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    return;

  QStringList failures;
  for (const QString &path : paths)
  {
    QFileInfo info(path);
    QString typeWordDef = info.isDir() ? m_s->wordFolderDef : m_s->wordFileDef;
    bool success = info.isDir() ? QDir(path).removeRecursively() : QFile::remove(path);

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
MainWindow::deleteLocalItemDirectly()
{
  QStringList paths;
  for (QTreeWidgetItem *item : localListWidget->selectedItems())
  {
    QString p = pathFromItem(item);
    if (!p.isEmpty())
      paths.append(p);
  }
  deleteLocalPaths(paths);
}
