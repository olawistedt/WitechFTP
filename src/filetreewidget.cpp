#include "filetreewidget.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QSet>
#include <QUrl>

static const char *kLocalMime  = "application/x-witechftp-local";
static const char *kRemoteMime = "application/x-witechftp-remote";

FileTreeWidget::FileTreeWidget(Kind kind, QWidget *parent)
    : QTreeWidget(parent)
    , m_kind(kind)
{
  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);
  setDragDropMode(QAbstractItemView::DragDrop);
  setDefaultDropAction(Qt::CopyAction);
}

QStringList
FileTreeWidget::mimeTypes() const
{
  return QStringList() << kLocalMime << kRemoteMime << "text/uri-list";
}

QMimeData *
FileTreeWidget::mimeData(const QList<QTreeWidgetItem *> &items) const
{
  // Use the snapshot captured at press time so the full selection is included
  // even if Qt has already narrowed selectedItems() to only the dragged item.
  QList<QTreeWidgetItem *> source = m_dragSnapshot.isEmpty() ? selectedItems() : m_dragSnapshot;
  if (source.isEmpty())
    source = items;

  QStringList values;
  QList<QUrl> urls;
  QSet<QString> seen;

  for (QTreeWidgetItem *item : source)
  {
    QString val = item->data(0, Qt::UserRole).toString();
    if (val.isEmpty() || val == ".." || seen.contains(val))
      continue;
    seen.insert(val);
    values.append(val);
    if (m_kind == Local)
      urls.append(QUrl::fromLocalFile(val));
  }

  if (values.isEmpty())
    return nullptr;

  QMimeData *data = new QMimeData();
  QByteArray payload = values.join('\n').toUtf8();
  if (m_kind == Local)
  {
    data->setData(kLocalMime, payload);
    data->setUrls(urls);
  }
  else
  {
    data->setData(kRemoteMime, payload);
  }
  return data;
}

void
FileTreeWidget::mousePressEvent(QMouseEvent *event)
{
  // Snapshot the selection before Qt has a chance to change it.
  // mimeData() will use this snapshot so a drag carries the full selection
  // regardless of which item the user grabbed.
  if (event->button() == Qt::LeftButton && itemAt(event->pos()))
    m_dragSnapshot = selectedItems();
  else
    m_dragSnapshot.clear();

  QTreeWidget::mousePressEvent(event);
}

void
FileTreeWidget::mouseReleaseEvent(QMouseEvent *event)
{
  m_dragSnapshot.clear();
  QTreeWidget::mouseReleaseEvent(event);
}

static bool
acceptableFor(FileTreeWidget::Kind kind, const QMimeData *mime)
{
  if (kind == FileTreeWidget::Local)
    return mime->hasFormat(kRemoteMime);
  // Remote target: accept local-internal drops or external file drags
  return mime->hasFormat(kLocalMime) || mime->hasUrls();
}

void
FileTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
  if (acceptableFor(m_kind, event->mimeData()))
  {
    event->setDropAction(Qt::CopyAction);
    event->accept();
  }
  else
  {
    event->ignore();
  }
}

void
FileTreeWidget::dragMoveEvent(QDragMoveEvent *event)
{
  if (acceptableFor(m_kind, event->mimeData()))
  {
    event->setDropAction(Qt::CopyAction);
    event->accept();
  }
  else
  {
    event->ignore();
  }
}

void
FileTreeWidget::dropEvent(QDropEvent *event)
{
  const QMimeData *mime = event->mimeData();

  if (m_kind == Local && mime->hasFormat(kRemoteMime))
  {
    QStringList names =
        QString::fromUtf8(mime->data(kRemoteMime)).split('\n', Qt::SkipEmptyParts);
    if (!names.isEmpty())
      emit remoteItemsDropped(names);
    event->setDropAction(Qt::CopyAction);
    event->accept();
    return;
  }

  if (m_kind == Remote && mime->hasFormat(kLocalMime))
  {
    QStringList paths =
        QString::fromUtf8(mime->data(kLocalMime)).split('\n', Qt::SkipEmptyParts);
    if (!paths.isEmpty())
      emit localItemsDropped(paths);
    event->setDropAction(Qt::CopyAction);
    event->accept();
    return;
  }

  if (m_kind == Remote && mime->hasUrls())
  {
    QStringList paths;
    for (const QUrl &url : mime->urls())
    {
      if (url.isLocalFile())
        paths.append(url.toLocalFile());
    }
    if (!paths.isEmpty())
      emit localItemsDropped(paths);
    event->setDropAction(Qt::CopyAction);
    event->accept();
    return;
  }

  event->ignore();
}
