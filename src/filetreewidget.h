#ifndef FILETREEWIDGET_H
#define FILETREEWIDGET_H

#include <QStringList>
#include <QTreeWidget>

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
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  Kind m_kind;
};

#endif  // FILETREEWIDGET_H
