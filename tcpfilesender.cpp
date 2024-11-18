#include "tcpfilesender.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QHostAddress>
#include <QFileDialog>
#include <QDebug>
#include <QTimer>

TcpFileSender::TcpFileSender(QWidget *parent)
    : QDialog(parent)
{
    loadSize = 1024 * 64;  // 更大的數據塊
    totalBytes = 0;
    bytesWritten = 0;
    bytesToWrite = 0;

    // 初始化 UI 元件
    clientProgressBar = new QProgressBar;
    clientStatusLabel = new QLabel(QStringLiteral("客戶端就緒"));
    startButton = new QPushButton(QStringLiteral("開始"));
    quitButton = new QPushButton(QStringLiteral("退出"));
    openButton = new QPushButton(QStringLiteral("開檔"));

    ipLabel = new QLabel(QStringLiteral("目標 IP 地址："));
    ipLineEdit = new QLineEdit;
    ipLineEdit->setPlaceholderText("例如：127.0.0.1");

    startButton->setEnabled(false);
    buttonBox = new QDialogButtonBox;
    buttonBox->addButton(startButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(openButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(quitButton, QDialogButtonBox::RejectRole);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ipLabel);
    mainLayout->addWidget(ipLineEdit);
    mainLayout->addWidget(clientProgressBar);
    mainLayout->addWidget(clientStatusLabel);
    mainLayout->addStretch(1);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    setWindowTitle(QStringLiteral("檔案傳送"));

    // 設置非阻塞寫入
    tcpClient.setSocketOption(QAbstractSocket::LowDelayOption, 1);

    // 設置信號和槽
    connect(openButton, SIGNAL(clicked()), this, SLOT(openFile()));
    connect(startButton, SIGNAL(clicked()), this, SLOT(start()));
    connect(&tcpClient, SIGNAL(connected()), this, SLOT(startTransfer()));
    connect(&tcpClient, SIGNAL(bytesWritten(qint64)), this, SLOT(updateClientProgress(qint64)));
    connect(quitButton, SIGNAL(clicked()), this, SLOT(close()));

    // 使用 QTimer 定時更新進度
    progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &TcpFileSender::updateProgress);
    progressTimer->start(100);  // 每100ms更新一次進度
}

void TcpFileSender::openFile()
{
    fileName = QFileDialog::getOpenFileName(this);
    if (!fileName.isEmpty()) startButton->setEnabled(true);
}

void TcpFileSender::start()
{
    startButton->setEnabled(false);
    bytesWritten = 0;
    clientStatusLabel->setText(QStringLiteral("連接中..."));

    QString ipAddress = ipLineEdit->text().trimmed();
    if (ipAddress.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("錯誤"), QStringLiteral("請輸入有效的 IP 地址"));
        return;
    }

    tcpClient.connectToHost(QHostAddress(ipAddress), 16998);
}

void TcpFileSender::startTransfer()
{
    localFile = new QFile(fileName);
    if (!localFile->open(QFile::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("應用程式"),
                             QStringLiteral("無法讀取 %1:\n%2.").arg(fileName)
                                 .arg(localFile->errorString()));
        return;
    }

    totalBytes = localFile->size();
    QDataStream sendOut(&outBlock, QIODevice::WriteOnly);
    sendOut.setVersion(QDataStream::Qt_4_6);
    QString currentFile = fileName.right(fileName.size() - fileName.lastIndexOf("/") - 1);
    sendOut << qint64(0) << qint64(0) << currentFile;
    totalBytes += outBlock.size();

    sendOut.device()->seek(0);
    sendOut << totalBytes << qint64((outBlock.size() - sizeof(qint64) * 2));
    bytesToWrite = totalBytes - tcpClient.write(outBlock);
    clientStatusLabel->setText(QStringLiteral("已連接"));
    outBlock.resize(0);
}

void TcpFileSender::updateClientProgress(qint64 numBytes)
{
    bytesWritten += (int) numBytes;
    if (bytesToWrite > 0) {
        outBlock = localFile->read(qMin(bytesToWrite, loadSize));
        bytesToWrite -= (int) tcpClient.write(outBlock);
        outBlock.resize(0);
    } else {
        localFile->close();
    }
}

void TcpFileSender::updateProgress()
{
    clientProgressBar->setMaximum(totalBytes);
    clientProgressBar->setValue(bytesWritten);
    clientStatusLabel->setText(QStringLiteral("已傳送 %1 Bytes").arg(bytesWritten));
}

TcpFileSender::~TcpFileSender()
{
    if (localFile) {
        delete localFile;
    }
}
