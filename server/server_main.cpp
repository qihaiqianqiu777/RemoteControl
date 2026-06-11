#include "socket_utils.hpp"
#include "dxgi_capture.hpp"
#include "h264_encoder.hpp"
#include <fstream>
extern "C"
{
#include <libavcodec/avcodec.h>
}

#include <iostream>

int main()
{
    if (!SocketUtils::init())
    {
        return 1;
    }

    SOCKET listenSock = SocketUtils::createTcpSocket();

    if (listenSock == INVALID_SOCKET)
    {
        SocketUtils::cleanup();
        return 1;
    }

    if (!SocketUtils::bindAndListen(listenSock, 9999))
    {
        SocketUtils::closeSocket(listenSock);
        SocketUtils::cleanup();
        return 1;
    }

    std::cout << "wait connect..." << std::endl;

    SOCKET clientSock = SocketUtils::acceptClient(listenSock);

    if (clientSock == INVALID_SOCKET)
    {
        SocketUtils::closeSocket(listenSock);
        SocketUtils::cleanup();
        return 1;
    }

    DxgiCapture capture;

    if (!capture.initByCursorMonitor())
    {
        std::cout << "initByCursorMonitor failed" << std::endl;
        SocketUtils::closeSocket(clientSock);
        SocketUtils::closeSocket(listenSock);
        SocketUtils::cleanup();

        return 1;
    }

    int srcWidth = capture.getWidth();
    int srcHeight = capture.getHeight();

    std::cout << "capture size: "
              << srcWidth << "x" << srcHeight
              << std::endl;

    int fps = 10;
    int bitrate = 3000 * 1000;

    H264Encoder encoder;

    if (!encoder.init(srcWidth, srcHeight, fps, bitrate))
    {
        std::cout << "encoder init failed" << std::endl;
        SocketUtils::closeSocket(clientSock);
        SocketUtils::closeSocket(listenSock);
        SocketUtils::cleanup();
        return 1;
    }

    long long frameCount = 0;

    while (true)
    {
        std::vector<unsigned char> pixels;

        if (!capture.captureFrame(pixels))
        {
            Sleep(10);
            continue;
        }

        if (pixels.empty())
        {
            std::cout << "pixels empty" << std::endl;
            Sleep(10);
            continue;
        }

        if (capture.isBlackFrame(pixels))
        {
            std::cout << "black frame, skip" << std::endl;
            Sleep(10);
            continue;
        }

        std::vector<std::vector<char>> h264Packets;

        if (!encoder.encodeBgraFrame(pixels, h264Packets))
        {
            std::cout << "encode failed" << std::endl;
            break;
        }

        for (const auto &pkt : h264Packets)
        {
            if (!SocketUtils::sendPacket(clientSock, CMD_VIDEO_PACKET, pkt))
            {
                std::cout << "send h264 packet failed" << std::endl;
                SocketUtils::closeSocket(clientSock);
                SocketUtils::closeSocket(listenSock);
                SocketUtils::cleanup();
                return 1;
            }

            std::cout << "write h264 packet, size: "
                      << pkt.size()
                      << " bytes" << std::endl;
        }

        frameCount++;

        Sleep(1000 / fps);
    }

    std::cout << "save test.h264 done" << std::endl;

    SocketUtils::closeSocket(clientSock);
    SocketUtils::closeSocket(listenSock);
    SocketUtils::cleanup();

    return 0;
}
