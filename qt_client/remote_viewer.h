#pragma once

#include <QWidget>
#include <QLabel>
#include <QImage>
#include <QPixmap>
#include <QVBoxLayout>
#include <QByteArray>
#include <QMetaObject>
#include <QElapsedTimer>
#include <QResizeEvent>

#include <thread>
#include <atomic>
#include <iostream>

#include "h264_decoder.hpp"
#include "socket_utils.hpp"

QT_BEGIN_INCLUDE_NAMESPACE
namespace Ui
{
    class RemoteViewer;
}
QT_END_NAMESPACE

class RemoteViewer : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteViewer(QWidget *parent = nullptr);
    ~RemoteViewer();

    void RecvThread();

    bool connectToServer(const char *ip, unsigned short port);

private slots:
    void updateFrame(const QByteArray &imageData);

    void updateVideoFrame(const QByteArray &bgraData, int width, int height);

private:
    // 更新右上角信息显示
    void updateInfoLabel();

    // 窗口大小变化时，重新调整右上角信息框位置
    void resizeEvent(QResizeEvent *event) override;

private:
    std::unique_ptr<Ui ::RemoteViewer> ui;
    SOCKET serverSock = INVALID_SOCKET;

    std::thread recvThread;
    std::atomic<bool> running = false;
    H264Decoder decoder;

    // FPS 统计
    QElapsedTimer fpsTimer;
    int fpsFrameCount = 0;
    double currentFps = 0.0;

    // 显示信息
    long long totalFrameCount = 0;
    int lastFrameSize = 0;
    int imageWidth = 0;
    int imageHeight = 0;
};
