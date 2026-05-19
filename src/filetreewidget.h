#ifndef FILETREEWIDGET_H
#define FILETREEWIDGET_H

#include <QHeaderView>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>

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

class FileTreeWidget : public QTreeWidget
{
  Q_OBJECT

public:
  enum Kind { Local, Remote };

  explicit FileTreeWidget(Kind kind, QWidget *parent = nullptr);

signals:
  // Emitted when remote items are dropped onto a Local widget (-> download)
  void remoteItemsDropped(const QStringList &remoteNames);
  // Emitted when local paths are dropped onto a Remote widget (-> upload)
  void localItemsDropped(const QStringList &localPaths);

protected:
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  Kind m_kind;
  QList<QTreeWidgetItem *> m_dragSnapshot;
};

#endif  // FILETREEWIDGET_H
