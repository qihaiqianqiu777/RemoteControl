#pragma once
#include "protocol.hpp"

class SocketUtils
{
public:
    // 初始化 Winsock
    // 所有 socket 操作之前必须调用一次
    static bool init()
    {
        WSADATA wsaData;

        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            std::cout << "WSAStartup failed: " << WSAGetLastError() << std::endl;
            return false;
        }

        return true;
    }

    // 清理 Winsock
    // 程序结束前调用
    static void cleanup()
    {
        WSACleanup();
    }

    // 创建 TCP socket
    static SOCKET createTcpSocket()
    {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

        if (sock == INVALID_SOCKET)
        {
            std::cout << "socket failed: " << WSAGetLastError() << std::endl;
        }

        return sock;
    }

    // 关闭 socket
    static void closeSocket(SOCKET sock)
    {
        if (sock != INVALID_SOCKET)
        {
            closesocket(sock);
        }
    }

    // 客户端连接服务器
    static bool connectToServer(SOCKET sock_server, const char *ip, unsigned short port)
    {

        SOCKADDR_IN server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = inet_addr(ip);

        if (connect(sock_server, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR)
        {
            std::cout << "connect failed: " << WSAGetLastError() << std::endl;
            SocketUtils::closeSocket(sock_server);
            sock_server = INVALID_SOCKET;
            return false;
        }
        std::cout << "connect success" << std::endl;
        return true;
    }

    // 服务端绑定端口并监听
    static bool bindAndListen(SOCKET listenSock, unsigned short port)
    {
        SOCKADDR_IN addr{};
        addr.sin_family = AF_INET;         // IPv4
        addr.sin_port = htons(port);       // 绑定端口，转为网络字节序
        addr.sin_addr.s_addr = INADDR_ANY; // 监听ip

        // 绑定IP+端口
        if (bind(listenSock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == SOCKET_ERROR)
        {
            std::cout << "bind failed: " << WSAGetLastError() << std::endl;
            return false;
        }

        // listen(SOCKET s, int backlog)
        //  监听 s,  等待队列长度 backlog
        if (listen(listenSock, 1) == SOCKET_ERROR)
        {
            std::cout << "listen failed: " << WSAGetLastError() << std::endl;
            return false;
        }
        return true;
    }

    static SOCKET acceptClient(SOCKET listenSock)
    {
        SOCKADDR_IN client_addr{};
        int client_addr_len = sizeof(client_addr);

        SOCKET clientSock = accept(
            listenSock,
            reinterpret_cast<sockaddr *>(&client_addr),
            &client_addr_len);

        if (clientSock == INVALID_SOCKET)
        {
            std::cout << "accept failed: " << WSAGetLastError() << std::endl;
        }
        else
        {
            std::cout << "client connected" << std::endl;
        }

        return clientSock;
    }

    // ===============================
    // recvAll
    // ===============================
    // recv() 不保证一次收到完整数据。
    // 所以这里循环接收，直到 len 个字节全部收到。
    static bool recvAll(SOCKET sock, char *buffer, int len)
    {
        int received = 0;
        while (received < len)
        {
            int ret = recv(sock, buffer + received, len - received, 0);
            if (ret == 0)
            {
                std::cout << "client disconnected" << std::endl;
                return false;
            }

            if (ret == SOCKET_ERROR)
            {
                std::cout << "client failed:" << WSAGetLastError() << std::endl;
                return false;
            }
            received += ret;
        }
        return true;
    }

    // 接收一个协议包
    // return : 成功 true, 失败 false;
    static bool recvPacket(SOCKET socket_client, Packet &packet)
    {
        PacketHeader header{};
        if (!recvAll(socket_client, reinterpret_cast<char *>(&header), sizeof(PacketHeader)))
        {
            return false;
        }

        if (header.magic != 0x55AA77CC)
        {
            std::cout << "invalid packet" << std::endl;
            return false;
        }
        if (header.body_len < 0 || header.body_len > 1024 * 1024 * 40)
        {
            std::cout << "invalid body_len: " << header.body_len << std::endl;
            return false;
        }

        packet.header = header;
        packet.body.resize(header.body_len);

        if (!recvAll(socket_client, packet.body.data(), header.body_len))
        {
            return false;
        }

        return true;
    }

    // ===============================
    // sendAll
    // ===============================
    // send() 不保证一次把所有数据发出去。
    // 所以这里循环发送，直到 len 个字节全部发完。
    static bool sendAll(SOCKET sock, const char *data, int len)
    {
        int sent = 0;

        while (sent < len)
        {
            int ret = send(sock, data + sent, len - sent, 0);

            if (ret == SOCKET_ERROR)
            {
                std::cout << "send failed: " << WSAGetLastError() << std::endl;
                return false;
            }

            sent += ret;
        }

        return true;
    }

    // 发送一个协议包
    // return: 成功 true, 失败 false
    static bool sendPacket(SOCKET sock, int cmd, const std::vector<char> &body)
    {
        std::vector<char> packet = Protocol::makePacket(cmd, body);

        return sendAll(
            sock,
            packet.data(),
            static_cast<int>(packet.size()));
    }
};