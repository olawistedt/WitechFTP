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
  // Always use the full current selection so the drag includes every
  // selected item even if Qt only passed a subset.
  QList<QTreeWidgetItem *> source = selectedItems();
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
  // If the user clicks on an unselected item while a multi-selection is
  // active, Qt would normally clear the selection and only keep the
  // clicked item - causing a subsequent drag to transfer only that one.
  // Preserve the previous selection (plus the clicked item) so drag-and-drop
  // works regardless of which item is grabbed.
  QList<QTreeWidgetItem *> previous;
  bool restore = false;
  if (event->button() == Qt::LeftButton &&
      !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
  {
    QTreeWidgetItem *item = itemAt(event->pos());
    if (item && !item->isSelected() && selectedItems().size() > 1)
    {
      previous = selectedItems();
      previous.append(item);
      restore = true;
    }
  }

  QTreeWidget::mousePressEvent(event);

  if (restore)
  {
    for (QTreeWidgetItem *item : previous)
      item->setSelected(true);
  }
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
