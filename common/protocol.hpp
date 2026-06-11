#pragma once

#include <stdio.h>
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

#pragma pack(push, 1)
struct PacketHeader
{
    int magic;

    int cmd;

    int body_len;
};
#pragma pack(pop)

struct Packet
{
    std::vector<char> body;
    PacketHeader header{};
    const std::string getBodyString() const
    {
        return std::string(body.begin(), body.end());
    }
};

enum Cmd
{

    CMD_SCREEN_FRAME = 200, // Bmp 图片帧
    CMD_VIDEO_PACKET = 210, // H.264视频包
    CMD_DISCONNECT = 600

};

class Protocol
{
public:
    static constexpr int MAGIC = 0x55AA77CC;

    static std::vector<char> makePacket(int cmd, const std::vector<char> &body)
    {
        std::vector<char> packet(sizeof(PacketHeader) + body.size());

        PacketHeader *header = reinterpret_cast<PacketHeader *>(packet.data());
        header->magic = 0x55AA77CC;
        header->cmd = cmd;
        header->body_len = static_cast<int>(body.size());

        std::memcpy(packet.data() + sizeof(PacketHeader), body.data(), body.size());
        return packet;
    }
};