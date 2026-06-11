#pragma once
#include "common\protocol.hpp"

class InputControl
{
public:
    int HandleCommand(Packet *packet)
    {
        switch (packet->header.cmd)
        {
        case CMD_SCREEN:
            SendSCREEN(packet);
            break;

        case CMD_MOUSE:
            HandleMouse(packet);
            break;
        case CMD_KEYBOARD:
            HandleKeyBoard(packet);
            break;
        default:
            break;
        }
    }

    int SendSCREEN(Packet *packet)
    {
    }

    int HandleMouse(Packet *packet)
    {
    }

    int HandleKeyBoard(Packet *packet)
    {
    }
};