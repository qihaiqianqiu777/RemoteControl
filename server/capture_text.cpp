#include "dxgi_capture.hpp"
#include "../common/file_utils.hpp"

int main()
{
    DxgiCapture capture;

    // 鼠标放在哪个屏幕，就抓哪个屏幕
    if (!capture.initByCursorMonitor())
    {
        std::cout << "initByCursorMonitor failed" << std::endl;
        return 1;
    }

    std::vector<unsigned char> pixels;

    bool gotFrame = false;

    for (int i = 0; i < 50; i++)
    {
        if (capture.captureFrame(pixels))
        {
            if (!capture.isBlackFrame(pixels))
            {
                gotFrame = true;
                break;
            }

            std::cout << "frame is black, retry..." << std::endl;
        }
        else
        {
            std::cout << "no valid frame, retry..." << std::endl;
        }

        Sleep(100);
    }

    if (!gotFrame)
    {
        std::cout << "failed to capture visible frame" << std::endl;
        return 1;
    }
    if (pixels.empty())
    {
        std::cout << "pixels empty" << std::endl;
        return 1;
    }

    if (capture.isBlackFrame(pixels))
    {
        std::cout << "warning: captured frame is black" << std::endl;
    }
    else
    {
        std::cout << "captured frame has visible content" << std::endl;
    }

    std::vector<char> bmpData;

    if (!capture.buildBmpData(pixels, bmpData))
    {
        std::cout << "buildBmpData failed" << std::endl;
        return 1;
    }

    if (!FileUtils::writeBinaryFile("dxgi_monitor.bmp", bmpData))
    {
        std::cout << "write bmp failed" << std::endl;
        return 1;
    }

    std::cout << "save dxgi_monitor.bmp success" << std::endl;

    return 0;
}