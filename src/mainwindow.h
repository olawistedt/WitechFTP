#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QAbstractSocket>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QTreeView;
class QFileSystemModel;
class QSplitter;
class QListWidget;
class QListWidgetItem;
class QTcpSocket;
class QTextStream;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void connectOrDisconnect();
  void processItem(QListWidgetItem *item);
  void uploadFile();
  void downloadFile(const QString &fileName);

  // Control connection slots
  void onControlConnected();
  void onControlReadyRead();
  void onControlDisconnected();
  void onControlError(QAbstractSocket::SocketError socketError);

  // Data connection slots
  void onDataReadyRead();
  void onDataDisconnected();
  void onDataError(QAbstractSocket::SocketError socketError);


private:
  void createUi();
  void sendCommand(const QString &command);

  // Connection widgets
  QLineEdit *hostLineEdit;
  QLineEdit *usernameLineEdit;
  QLineEdit *passwordLineEdit;
  QPushButton *connectButton;

  // File browsers
  QSplitter *splitter;
  QTreeView *localTreeView;
  QFileSystemModel *localModel;

  QListWidget *remoteListWidget;

  // FTP
  QTcpSocket *controlSocket;
  QTcpSocket *dataSocket;
  QTextStream *controlStream;
  QByteArray dataBuffer;

  enum class FtpCommand
  {
    None,
    Cwd,
    List
  };
  FtpCommand lastCommand = FtpCommand::None;
  QString pendingPath;

  QString currentPath;
  QHash<QString, bool> isDirectory;
  bool m_isConnected = false;
};

#endif  // MAINWINDOW_H
