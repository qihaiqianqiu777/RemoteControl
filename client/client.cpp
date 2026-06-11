#include "socket_utils.hpp"
#include "file_utils.hpp"
int main()
{
    if (!SocketUtils::init())
        return 1;

    SOCKET serverSock = SocketUtils::createTcpSocket();

    if (serverSock == INVALID_SOCKET)
    {
        SocketUtils::cleanup();
        return 1;
    }

    if (!SocketUtils::connectToServer(serverSock, "192.168.10.86", 9999))
    {
        SocketUtils::closeSocket(serverSock);
        SocketUtils::cleanup();
        return 1;
    }

    long long frameCount = 0;

    while (true)
    {
        Packet packet;

        if (!SocketUtils::recvPacket(serverSock, packet))
        {
            std::cout << "recv packet failed" << std::endl;
            break;
        }

        if (packet.header.cmd == CMD_SCREEN_FRAME)
        {
            frameCount++;

            std::cout << "recv frame " << frameCount
                      << ", size: " << packet.body.size()
                      << " bytes" << std::endl;
            if (!FileUtils::writeBinaryFile("recv.bmp", packet.body))
            {
                std::cout << "write recv_screen.bmp failed" << std::endl;
                break;
            }
        }
        else
        {
            std::cout << "unknown cmd: " << packet.header.cmd << std::endl;
        }
    }

    SocketUtils::closeSocket(serverSock);
    SocketUtils::cleanup();
    return 0;
}