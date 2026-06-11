#include "remote_viewer.h"
#include "ui_qt_client.h"
RemoteViewer::RemoteViewer(QWidget *parent)
    : QWidget(parent), ui(new Ui::RemoteViewer)
{
    ui->setupUi(this);

    setWindowTitle("Remote Desktop Qt Viewer");
    resize(960, 540);

    // imageLabel 是 Designer 里拖出来的
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setStyleSheet("background-color: black;");

    // infoLabel 也是 Designer 里拖出来的
    ui->infoLabel->setText(
        "FPS: 0.0\n"
        "Frame: 0\n"
        "Size: 0 KB\n"
        "Res: 0 x 0");

    ui->infoLabel->setStyleSheet(
        "QLabel {"
        "background-color: rgba(0, 0, 0, 160);"
        "color: white;"
        "font-size: 14px;"
        "padding: 8px;"
        "border-radius: 6px;"
        "}");

    ui->infoLabel->resize(180, 90);

    // 初始放到右上角
    ui->infoLabel->move(
        width() - ui->infoLabel->width() - 12,
        12);

    ui->infoLabel->raise();

    fpsTimer.start();

    if (!decoder.init())
    {
        std::cout << "H264 decoder init failed" << std::endl;
    }
}

RemoteViewer::~RemoteViewer()
{
    running = false;

    SocketUtils::closeSocket(serverSock);

    if (recvThread.joinable())
    {
        recvThread.join();
    }
}

bool RemoteViewer::connectToServer(const char *ip, unsigned short port)
{
    serverSock = SocketUtils::createTcpSocket();

    if (serverSock == INVALID_SOCKET)
    {
        return false;
    }

    if (!SocketUtils::connectToServer(serverSock, ip, port))
    {
        serverSock = INVALID_SOCKET;
        return false;
    }

    return true;
}

void RemoteViewer::RecvThread()
{
    running = true;

    recvThread = std::thread([this]()
                             {
                                 while (running)
                                 {
                                     Packet packet;

                                     if (!SocketUtils::recvPacket(serverSock, packet))
                                     {
                                         std::cout << "recv packet failed" << std::endl;
                                         break;
                                     }

                                     if (packet.header.cmd == CMD_SCREEN_FRAME)
                                     {
                                         QByteArray imageData(
                                             packet.body.data(),
                                             static_cast<int>(packet.body.size())
                                             );

                                         QMetaObject::invokeMethod(
                                             this,
                                             "updateFrame",
                                             Qt::QueuedConnection,
                                             Q_ARG(QByteArray, imageData)
                                             );
                                     }

                                     if(packet.header.cmd==CMD_VIDEO_PACKET){
                                         std::vector<DecodedFrame> decodedFrames;

    // 把 H.264 数据送进解码器
    if (!decoder.decodePacket(packet.body, decodedFrames))
    {
        std::cout << "decode h264 packet failed" << std::endl;
        continue;
    }

    // 一个 packet 可能解出 0 帧或多帧
    for (const auto& frame : decodedFrames)
    {
        QByteArray bgraData(
            reinterpret_cast<const char*>(frame.bgraPixels.data()),
            static_cast<int>(frame.bgraPixels.size())
        );

        // 切回 Qt 主线程更新 UI
        QMetaObject::invokeMethod(
            this,
            "updateVideoFrame",
            Qt::QueuedConnection,
            Q_ARG(QByteArray, bgraData),
            Q_ARG(int, frame.width),
            Q_ARG(int, frame.height)
        );
    }
                                     }
                                 }

                                 running = false; });
}

void RemoteViewer::updateFrame(const QByteArray &imageData)
{
    QImage image = QImage::fromData(imageData);

    if (image.isNull())
    {
        std::cout << "QImage load failed" << std::endl;
        return;
    }

    totalFrameCount++;
    lastFrameSize = imageData.size();
    imageWidth = image.width();
    imageHeight = image.height();

    fpsFrameCount++;

    qint64 elapsed = fpsTimer.elapsed();

    if (elapsed >= 1000)
    {
        currentFps = fpsFrameCount * 1000.0 / elapsed;
        fpsFrameCount = 0;
        fpsTimer.restart();
    }

    QPixmap pixmap = QPixmap::fromImage(image);

    ui->imageLabel->setPixmap(
        pixmap.scaled(
            ui->imageLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));

    updateInfoLabel();
}

void RemoteViewer::updateInfoLabel()
{
    QString text = QString(
                       "FPS: %1\n"
                       "Frame: %2\n"
                       "Size: %3 KB\n"
                       "Res: %4 x %5")
                       .arg(currentFps, 0, 'f', 1)
                       .arg(totalFrameCount)
                       .arg(lastFrameSize / 1024)
                       .arg(imageWidth)
                       .arg(imageHeight);

    ui->infoLabel->setText(text);

    ui->infoLabel->raise();
}

void RemoteViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (ui && ui->infoLabel)
    {
        ui->infoLabel->move(
            width() - ui->infoLabel->width() - 12,
            12);

        ui->infoLabel->raise();
    }
}

void RemoteViewer::updateVideoFrame(
    const QByteArray &bgraData,
    int width,
    int height)
{
    if (bgraData.isEmpty() || width <= 0 || height <= 0)
    {
        return;
    }

    // 用 BGRA 数据创建 QImage
    //
    // 注意：
    // QImage 这里默认不复制 bgraData 的内存，
    // 所以后面转 QPixmap 前用 image.copy() 更安全。
    QImage image(
        reinterpret_cast<const uchar *>(bgraData.constData()),
        width,
        height,
        width * 4,
        QImage::Format_ARGB32
        );

    if (image.isNull())
    {
        std::cout << "QImage from BGRA failed" << std::endl;
        return;
    }

    // image.copy() 会复制一份图像数据，
    // 避免 bgraData 生命周期结束后 QPixmap 引用无效内存。
    QPixmap pixmap = QPixmap::fromImage(image.copy());

    ui->imageLabel->setPixmap(
        pixmap.scaled(
            ui->imageLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));

    imageWidth = width;
    imageHeight = height;
    lastFrameSize = bgraData.size();
    totalFrameCount++;

    fpsFrameCount++;

    qint64 elapsed = fpsTimer.elapsed();

    if (elapsed >= 1000)
    {
        currentFps = fpsFrameCount * 1000.0 / elapsed;
        fpsFrameCount = 0;
        fpsTimer.restart();
    }

    updateInfoLabel();
}
