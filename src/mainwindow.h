#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>

#include <QNetworkReply>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QTreeView;
class QFileSystemModel;
class QSplitter;
class QNetworkAccessManager;
class QListWidget;
class QListWidgetItem;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void connectOrDisconnect();
    void onFtpReply(QNetworkReply *reply);
    void processItem(QListWidgetItem *item);
    void uploadFile();
    void downloadFile(const QString &fileName);

private:
    void createUi();

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
    QNetworkAccessManager *ftp;
    QString currentPath;
    QHash<QString, bool> isDirectory;
};

#endif // MAINWINDOW_H
