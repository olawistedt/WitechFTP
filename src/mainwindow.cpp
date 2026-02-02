#include "mainwindow.h"

#include <QtWidgets>
#include <QtNetwork>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ftp(nullptr)
{
    createUi();
    setWindowTitle("Qt FTP Client");
    resize(800, 600);
}

MainWindow::~MainWindow()
{
}

void MainWindow::createUi()
{
    // --- Connection Bar ---
    QWidget *connectionWidget = new QWidget;
    QHBoxLayout *connectionLayout = new QHBoxLayout(connectionWidget);
    hostLineEdit = new QLineEdit("ftp.qt.io");
    hostLineEdit->setPlaceholderText("FTP Host");
    usernameLineEdit = new QLineEdit("anonymous");
    usernameLineEdit->setPlaceholderText("Username");
    passwordLineEdit = new QLineEdit;
    passwordLineEdit->setPlaceholderText("Password");
    passwordLineEdit->setEchoMode(QLineEdit::Password);
    connectButton = new QPushButton("Connect");

    connectionLayout->addWidget(new QLabel("Host:"));
    connectionLayout->addWidget(hostLineEdit);
    connectionLayout->addWidget(new QLabel("Username:"));
    connectionLayout->addWidget(usernameLineEdit);
    connectionLayout->addWidget(new QLabel("Password:"));
    connectionLayout->addWidget(passwordLineEdit);
    connectionLayout->addWidget(connectButton);

    // --- File Browsers ---
    splitter = new QSplitter;
    
    // Local
    localModel = new QFileSystemModel;
    localModel->setRootPath(QDir::currentPath());
    localTreeView = new QTreeView(splitter);
    localTreeView->setModel(localModel);
    localTreeView->setRootIndex(localModel->index(QDir::currentPath()));

    // Remote
    remoteListWidget = new QListWidget(splitter);

    splitter->addWidget(localTreeView);
    splitter->addWidget(remoteListWidget);

    // --- Main Layout ---
    QWidget *centralWidget = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addWidget(connectionWidget);
    mainLayout->addWidget(splitter);

    setCentralWidget(centralWidget);

    // --- Connections ---
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectOrDisconnect);
    //connect(remoteListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::processItem);
}

void MainWindow::connectOrDisconnect()
{
    if (ftp) { // If we are connected, disconnect
        ftp->deleteLater();
        ftp = nullptr;
        remoteListWidget->clear();
        connectButton->setText("Connect");
        hostLineEdit->setEnabled(true);
        usernameLineEdit->setEnabled(true);
        passwordLineEdit->setEnabled(true);
        return;
    }

    // If we are not connected, connect
    ftp = new QNetworkAccessManager(this);
    connect(ftp, &QNetworkAccessManager::finished, this, &MainWindow::onFtpReply);

    QUrl url;
    url.setScheme("ftp");
    url.setHost(hostLineEdit->text());
    url.setUserName(usernameLineEdit->text());
    url.setPassword(passwordLineEdit->text());
    
    currentPath = "/"; // Start at root
    url.setPath(currentPath);

    ftp->get(QNetworkRequest(url));

    connectButton->setText("Connecting...");
    hostLineEdit->setEnabled(false);
    usernameLineEdit->setEnabled(false);
    passwordLineEdit->setEnabled(false);
}

void MainWindow::onFtpReply(QNetworkReply *reply)
{
    if (reply->error()) {
        QMessageBox::critical(this, "Error", reply->errorString());
        connectOrDisconnect(); // Trigger disconnect logic to reset UI
        reply->deleteLater();
        return;
    }

    connectButton->setText("Disconnect");

    QByteArray data = reply->readAll();
    
    remoteListWidget->clear();
    isDirectory.clear();

    QStringList lines = QString(data).split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        // This is a very basic UNIX `ls -l` parser, not robust!
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 9)
            continue;

        QString name = parts.sliced(8).join(' ');
        bool isDir = parts[0].startsWith('d');
        isDirectory[name] = isDir;

        QListWidgetItem *item = new QListWidgetItem(name);
        if (isDir) {
            item->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
        } else {
            item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
        }
        remoteListWidget->addItem(item);
    }

    reply->deleteLater();
}

void MainWindow::processItem(QListWidgetItem *item)
{
    // To be implemented
}

void MainWindow::uploadFile()
{
    // To be implemented
}

void MainWindow::downloadFile(const QString &fileName)
{
    // To be implemented
}
