#include <QApplication>
#include <iostream>

#include "remote_viewer.h"

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);

    if (!SocketUtils::init())
    {
        return 1;
    }
    RemoteViewer viewer;

    if (!viewer.connectToServer("192.168.10.86", 9999))
    {
        std::cout << "connect server failed" << std::endl;
        SocketUtils::cleanup();
        return 1;
    }

    viewer.show();
    viewer.RecvThread();

    int ret = app.exec();

    SocketUtils::cleanup();

    return ret;
}