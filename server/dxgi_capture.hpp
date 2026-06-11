#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl\client.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

template <typename T>
void SafeRelease(T *&p)
{
    if (p != nullptr)
    {
        p->Release();
        p = nullptr;
    }
}

class DxgiCapture
{
public:
    DxgiCapture() = default;

    ~DxgiCapture()
    {
        cleanup();
    }

    bool initByCursorMonitor()
    {
        POINT pt{};

        if (!GetCursorPos(&pt))
        {
            std::cout << "getCursorPos failed" << std::endl;
            return false;
        }
        HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        if (!monitor)
        {
            std::cout << "MonitorFromPoint failed" << std::endl;
            return false;
        }
        return initByMonitor(monitor);
    }

    bool initByMonitor(HMONITOR targetMonitor)
    {
        cleanup();

        if (!targetMonitor)
        {
            std::cout << "targetMonitor is null" << std::endl;
            return false;
        }

        // creat D3D11 device
        HRESULT hr = S_OK;

        IDXGIFactory1 *factory = nullptr;

        hr = CreateDXGIFactory1(
            __uuidof(IDXGIFactory1),
            reinterpret_cast<void **>(&factory));

        if (FAILED(hr))
        {
            std::cout << "CreateDXGIFactory1 failed: 0x"
                      << std::hex << hr << std::dec << std::endl;
            return false;
        }

        bool found = false;
        // 枚举显卡
        for (UINT adapterIndex = 0;; adapterIndex++)
        {
            IDXGIAdapter *tempAdapter = nullptr;

            hr = factory->EnumAdapters(adapterIndex, &tempAdapter);
            if (hr == DXGI_ERROR_NOT_FOUND)
            {
                break;
            }

            if (FAILED(hr))
            {
                std::cout << "EnumAdapters failed: 0x"
                          << std::hex << hr << std::dec << std::endl;
                break;
            }
            DXGI_ADAPTER_DESC adapterDesc{};
            tempAdapter->GetDesc(&adapterDesc);

            std::wcout << L"Adapter " << adapterIndex
                       << L": " << adapterDesc.Description
                       << std::endl;

            // 枚举当前显卡下的显示器
            for (UINT outputIndex = 0;; outputIndex++)
            {
                IDXGIOutput *tempOutput = nullptr;

                hr = tempAdapter->EnumOutputs(outputIndex, &tempOutput);

                if (hr == DXGI_ERROR_NOT_FOUND)
                {
                    break;
                }

                if (FAILED(hr))
                {
                    std::cout << "EnumOutputs failed: 0x"
                              << std::hex << hr << std::dec << std::endl;
                    break;
                }
                DXGI_OUTPUT_DESC outputDesc{};

                hr = tempOutput->GetDesc(&outputDesc);

                if (FAILED(hr))
                {
                    SafeRelease(tempOutput);
                    continue;
                }

                std::wcout << L"  Output " << outputIndex
                           << L": " << outputDesc.DeviceName
                           << std::endl;

                std::cout << "    AttachedToDesktop: "
                          << outputDesc.AttachedToDesktop << std::endl;

                std::cout << "    Rect: "
                          << outputDesc.DesktopCoordinates.left << ", "
                          << outputDesc.DesktopCoordinates.top << ", "
                          << outputDesc.DesktopCoordinates.right << ", "
                          << outputDesc.DesktopCoordinates.bottom
                          << std::endl;

                if (outputDesc.Monitor == targetMonitor)
                {
                    std::cout << "found target monitor" << std::endl;
                    adapter = tempAdapter;
                    output = tempOutput;

                    // 防止后面被 SafeRelease 释放掉
                    tempAdapter = nullptr;
                    tempOutput = nullptr;

                    found = true;
                    break;
                }
                SafeRelease(tempOutput);
            }

            if (found)
            {
                break;
            }

            SafeRelease(tempAdapter);
        }

        SafeRelease(factory);
        if (!found)
        {
            std::cout << "target monitor not found in DXGI outputs" << std::endl;
            return false;
        }

        //  获取显示器信息
        DXGI_OUTPUT_DESC selectedDesc{};
        hr = output->GetDesc(&selectedDesc);

        if (FAILED(hr))
        {
            std::cout << "GetDesc failed: 0x"
                      << std::hex << hr << std::dec << std::endl;
            return false;
        }

        width = selectedDesc.DesktopCoordinates.right - selectedDesc.DesktopCoordinates.left;
        height = selectedDesc.DesktopCoordinates.bottom - selectedDesc.DesktopCoordinates.top;

        monitorLeft = selectedDesc.DesktopCoordinates.left;
        monitorTop = selectedDesc.DesktopCoordinates.top;

        std::wcout << L"Selected Output: "
                   << selectedDesc.DeviceName
                   << std::endl;

        std::cout << "Selected size: "
                  << width << " x " << height << std::endl;

        //  创建 D3D11 设备
        // DXGI 截屏需要通过 Direct3D 11 拿到桌面纹理。

        hr = D3D11CreateDevice(
            adapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            device.GetAddressOf(),
            nullptr,
            context.GetAddressOf()

        );

        if (FAILED(hr))
        {
            std::cout << "D3D11CreateDevice failed: 0x"
                      << std::hex << hr << std::dec << std::endl;
            return false;
        }

        // ===============================
        // IDXGIOutput -> IDXGIOutput1
        // ===============================
        // DuplicateOutput 是 IDXGIOutput1 的函数。
        hr = output.As(&output1);

        if (FAILED(hr))
        {
            std::cout << "QueryInterface IDXGIOutput1 failed: 0x"
                      << std::hex << hr << std::dec << std::endl;
            return false;
        }
        // ===============================
        //  创建桌面复制对象
        // ===============================
        // duplication 后面用来 AcquireNextFrame。
        hr = output1->DuplicateOutput(
            device.Get(),
            duplication.GetAddressOf());

        if (FAILED(hr))
        {
            std::cout << "DuplicateOutput failed: 0x"
                      << std::hex << hr << std::dec << std::endl;
            return false;
        }

        std::cout << "DXGI init by monitor success" << std::endl;

        return true;
    }

    // 抓取一帧屏幕画面
    //
    // 参数：
    // pixels：输出参数，用来保存屏幕像素数据
    //
    // 返回：
    // true  ：成功抓到一帧
    // false ：抓取失败 / 超时 / 出错
    bool captureFrame(std::vector<unsigned char> &pixels)
    {
        pixels.clear();

        // 判断DXGI复制对象是否有效
        if (!duplication)
        {
            std::cout << "duplication is null" << std::endl;
            return false;
        }
        HRESULT hr = S_OK;

        // 保存当前帧信息，如鼠标状态，更新时间
        DXGI_OUTDUPL_FRAME_INFO frameInfo{};

        // AcqyureNextFrame 返回 通用 DXGI资源
        //  需要后续转化为 ID3D11Texture2D
        IDXGIResource *desktopResource = nullptr;

        // ===============================
        // 1. 获取下一帧
        // ===============================
        // 参数 1000 表示最多等待 1000ms。
        hr = duplication->AcquireNextFrame(
            1000,
            &frameInfo,
            &desktopResource);

        // 超时不一定是严重错误
        if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        {
            return false;
        }

        if (FAILED(hr))
        {
            std::cout << "AcquireNextFrame failed: 0x"
                      << std::hex << hr << std::dec << std::endl;
            return false;
        }

        updatePointerInfo(frameInfo);

        std::cout
            << "AccumulatedFrames: " << frameInfo.AccumulatedFrames << std::endl;
        std::cout << "LastPresentTime: " << frameInfo.LastPresentTime.QuadPart << std::endl;

        // 保存桌面纹理
        ID3D11Texture2D *desktopTexture = nullptr;

        // 把 IDXGIResource 转为 ID3D11Texture2D
        hr = desktopResource->QueryInterface(
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void **>(&desktopTexture));

        SafeRelease(desktopResource);

        if (FAILED(hr))
        {
            std::cout << "QueryInterface ID3D11Texture2D failed: 0x"
                      << std::hex << hr << std::dec << std::endl;

            duplication->ReleaseFrame();
            return false;
        }

        bool ok = copyTextureToPixels(desktopTexture, pixels);

        SafeRelease(desktopTexture);

        duplication->ReleaseFrame();

        return ok;
    }

    bool isBlackFrame(const std::vector<unsigned char> &pixels)
    {
        if (pixels.empty())
        {
            return true;
        }
        long long sum = 0;

        for (size_t i = 0; i + 3 < pixels.size(); i += 4)
        {
            sum += pixels[i];     // B
            sum += pixels[i + 1]; // G
            sum += pixels[i + 2]; // R
        }
        return sum < 1000;
    }

    // 创建 CPU 可读的 staging texture
    //
    // 参数：
    // sourceTexture: 原始桌面纹理
    // stagingTexture: 输出参数，保存创建的CPU可读纹理
    //
    bool createStagingTexture(
        ID3D11Texture2D *sourceTexture,
        ID3D11Texture2D **stagingTexture)
    {
        if (!sourceTexture || !stagingTexture)
            return false;

        // 获取纹理的描述信息

        D3D11_TEXTURE2D_DESC desc{};
        sourceTexture->GetDesc(&desc);

        // 修改纹理描述，
        //
        // D3D11_USAGE_STAGING:
        // 表示纹理应用于CPU与GPU间传输数据
        //
        // D3D11+CPU_ACCESS_READ:
        // 表示CPU可读取该纹理

        desc.Usage = D3D11_USAGE_STAGING;

        // staging texture 无需绑定渲染管线
        desc.BindFlags = 0;

        // CPU可读
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        // 无需额外标志
        desc.MiscFlags = 0;

        // 创建纹理
        HRESULT hr = device->CreateTexture2D(
            &desc,
            nullptr,
            stagingTexture);

        if (FAILED(hr))
        {
            std::cout << "Create staging texture failed: 0x"
                      << std::hex << hr << std::dec
                      << std::endl;
            return false;
        }
        return true;
    }

    // 把桌面纹理复制到 CPU 到 内存 pixels
    //
    // 参数：
    // desktopTexture: DXGI 抓到的桌面纹理，数据在CPU中
    // pixels :输出参数，保存最终的像素数据
    bool copyTextureToPixels(
        ID3D11Texture2D *desktopTexture,
        std::vector<unsigned char> &pixels)
    {
        if (desktopTexture == nullptr)
        {
            std::cout << "desltopTexture is null" << std::endl;
            return false;
        }

        D3D11_TEXTURE2D_DESC desc{};
        desktopTexture->GetDesc(&desc);

        width = static_cast<int>(desc.Width);
        height = static_cast<int>(desc.Height);

        std::cout << "Texture Width: " << width << std::endl;
        std::cout << "Texture Height: " << height << std::endl;
        std::cout << "Texture Format: " << desc.Format << std::endl;

        pixels.clear();

        if (!desktopTexture)
            return false;

        ID3D11Texture2D *stagingTexture = nullptr;

        if (!createStagingTexture(desktopTexture, &stagingTexture))
        {
            return false;
        }

        context->CopyResource(stagingTexture, desktopTexture);

        // Map: 把staging texture 映射到 CPU可访问内存
        D3D11_MAPPED_SUBRESOURCE mapped{};

        HRESULT hr = context->Map(
            stagingTexture,
            0,
            D3D11_MAP_READ,
            0,
            &mapped);

        if (FAILED(hr))
        {
            std::cout << "MAP failed: 0x"
                      << std::hex << hr << std::dec << std::endl;

            SafeRelease(stagingTexture);
            return false;
        }

        // 返回 BGRA
        const int bytesPerPixel = 4;

        const int rowSize = width * bytesPerPixel;

        // 大小= 宽*高*4

        pixels.resize(width * height * bytesPerPixel);

        // 按行复制数据

        for (int y = 0; y < height; y++)
        {
            unsigned char *srcRow = static_cast<unsigned char *>(mapped.pData) + y * mapped.RowPitch;

            unsigned char *dstRow = pixels.data() + y * rowSize;

            std::memcpy(dstRow, srcRow, rowSize);
        }

        context->Unmap(stagingTexture, 0);

        SafeRelease(stagingTexture);

        return true;
    }

    // 把 DXGI Pointer Shape 绘制到 pixels 上
    //
    // pixels:
    //   当前屏幕图像，格式 BGRA，每个像素 4 字节
    //
    // 注意：
    //   这个函数是把鼠标“画进图片里”。
    //   所以客户端不需要额外处理鼠标，Qt 还是正常显示 BMP 就行。
    bool drawPointerShapeOnPixels(std::vector<unsigned char> &pixels)
    {
        if (!pointerVisible)
        {
            // 鼠标隐藏，不需要画
            return true;
        }

        if (pixels.empty())
        {
            return false;
        }

        if (pointerShapeBuffer.empty())
        {
            // 还没有拿到鼠标形状，暂时没法画
            return true;
        }

        if (width <= 0 || height <= 0)
        {
            return false;
        }

        const int bytesPerPixel = 4;

        // 当前鼠标在截图内部的坐标
        //
        // pointerPosition 是整个桌面的全局坐标。
        // pixels 是当前显示器内部坐标。
        // 所以要减掉 monitorLeft / monitorTop。
        int cursorX = pointerPosition.x - monitorLeft;
        int cursorY = pointerPosition.y - monitorTop;

        // HotSpot 是真正点击的位置
        // Draw 时需要换算成鼠标图案左上角
        int drawX = cursorX - static_cast<int>(pointerShapeInfo.HotSpot.x);
        int drawY = cursorY - static_cast<int>(pointerShapeInfo.HotSpot.y);

        int cursorWidth = static_cast<int>(pointerShapeInfo.Width);
        int cursorHeight = static_cast<int>(pointerShapeInfo.Height);
        int cursorPitch = static_cast<int>(pointerShapeInfo.Pitch);

        if (cursorWidth <= 0 || cursorHeight <= 0 || cursorPitch <= 0)
        {
            return true;
        }

        // 目前先支持彩色鼠标
        if (pointerShapeInfo.Type != DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR &&
            pointerShapeInfo.Type != DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR)
        {
            std::cout << "unsupported pointer shape type: "
                      << pointerShapeInfo.Type << std::endl;
            return true;
        }

        // 遍历鼠标图案每一个像素
        for (int y = 0; y < cursorHeight; y++)
        {
            int dstY = drawY + y;

            // 超出屏幕范围就跳过
            if (dstY < 0 || dstY >= height)
            {
                continue;
            }

            for (int x = 0; x < cursorWidth; x++)
            {
                int dstX = drawX + x;

                if (dstX < 0 || dstX >= width)
                {
                    continue;
                }

                // 鼠标 shape 源像素
                //
                // Pointer Shape 的 COLOR / MASKED_COLOR 通常也是 BGRA
                const unsigned char *src =
                    pointerShapeBuffer.data() + y * cursorPitch + x * bytesPerPixel;

                unsigned char b = src[0];
                unsigned char g = src[1];
                unsigned char r = src[2];
                unsigned char a = src[3];

                // alpha 为 0，说明完全透明，跳过
                if (a == 0)
                {
                    continue;
                }

                // 目标屏幕像素
                unsigned char *dst =
                    pixels.data() + (dstY * width + dstX) * bytesPerPixel;

                // 如果 alpha 是 255，直接覆盖
                if (a == 255)
                {
                    dst[0] = b;
                    dst[1] = g;
                    dst[2] = r;
                    dst[3] = 255;
                }
                else
                {
                    // 半透明混合
                    //
                    // 新颜色 = 鼠标颜色 * alpha + 原图颜色 * (1 - alpha)
                    float alpha = a / 255.0f;

                    dst[0] = static_cast<unsigned char>(b * alpha + dst[0] * (1.0f - alpha));
                    dst[1] = static_cast<unsigned char>(g * alpha + dst[1] * (1.0f - alpha));
                    dst[2] = static_cast<unsigned char>(r * alpha + dst[2] * (1.0f - alpha));
                    dst[3] = 255;
                }
            }
        }

        return true;
    }

    // 把 pixels转为 BMP文件
    // return: 成功 true, 失败 false
    bool buildBmpData(
        const std::vector<unsigned char> &pixels,
        std::vector<char> &bmpData)
    {
        bmpData.clear();

        if (pixels.empty())
        {
            std::cout << "pixels is null" << std::endl;
            return false;
        }

        // 一个像素4字节 B G R A
        const int bytesPerPixel = 4;

        const int imageSize = width * height * bytesPerPixel;

        if (pixels.size() < static_cast<size_t>(imageSize))
        {
            std::cout << "pixels size invalid" << std::endl;
            return false;
        }

        // BMP文件头
        BITMAPFILEHEADER fileHeader{};

        // BMP 信息头
        BITMAPINFOHEADER infoHeader{};

        // BMP 标识
        fileHeader.bfType = 0x4D42;

        // 像素位置起点
        // BMP文件结构：
        //[BITMAPFILEHEADER][BITMAPINFOHEADER][像素数据]
        fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

        // 整个文件大小
        fileHeader.bfSize = fileHeader.bfOffBits + imageSize;

        // 信息头大小
        infoHeader.biSize = sizeof(BITMAPINFOHEADER);

        // 图片宽度
        infoHeader.biWidth = width;

        // 图片高度
        // 负数表示从上到下存储
        infoHeader.biHeight = -height;

        infoHeader.biPlanes = 1;

        // 每个像素32位
        infoHeader.biBitCount = 32;

        // 暂时不压缩
        infoHeader.biCompression = BI_RGB;

        infoHeader.biSizeImage = imageSize;

        bmpData.resize(fileHeader.bfSize);

        char *dst = bmpData.data();

        std::memcpy(dst, &fileHeader, sizeof(fileHeader));
        dst += sizeof(fileHeader);

        std::memcpy(dst, &infoHeader, sizeof(infoHeader));
        dst += sizeof(infoHeader);

        std::memcpy(dst, pixels.data(), imageSize);
        unsigned char *bmpPixels = reinterpret_cast<unsigned char *>(dst);
        for (int i = 3; i < imageSize; i += bytesPerPixel)
        {
            bmpPixels[i] = 0xFF;
        }

        return true;
    }

    bool updatePointerInfo(const DXGI_OUTDUPL_FRAME_INFO &frameInfo)
    {
        // 更新鼠标是否可显示状态
        pointerVisible = frameInfo.PointerPosition.Visible;

        // 更新鼠标位置
        pointerPosition = frameInfo.PointerPosition.Position;

        // frameinfo.PointerShapeBufferSize == 0 ==> 鼠标没变化
        if (frameInfo.PointerShapeBufferSize == 0)
        {
            return true;
        }

        UINT requiredSize = 0;

        pointerShapeBuffer.resize(frameInfo.PointerShapeBufferSize);

        HRESULT hr = duplication->GetFramePointerShape(
            frameInfo.PointerShapeBufferSize,
            pointerShapeBuffer.data(),
            &requiredSize,
            &pointerShapeInfo);

        // if 缓冲区不足
        if (hr == DXGI_ERROR_MORE_DATA)
        {
            pointerShapeBuffer.resize(requiredSize);

            HRESULT hr = duplication->GetFramePointerShape(
                frameInfo.PointerShapeBufferSize,
                pointerShapeBuffer.data(),
                &requiredSize,
                &pointerShapeInfo);
        }

        if (FAILED(hr))
        {
            std::cout << "GetFramePointerShape failed: 0x"
                      << std::hex << hr << std::dec << std::endl;
            return false;
        }

        std::cout << "Pointer shape updated: "
                  << "type=" << pointerShapeInfo.Type
                  << ", width=" << pointerShapeInfo.Width
                  << ", height=" << pointerShapeInfo.Height
                  << ", pitch=" << pointerShapeInfo.Pitch
                  << std::endl;

        return true;
    }

    int getWidth() const
    {
        return width;
    }

    int getHeight() const
    {
        return height;
    }

    void cleanup()
    {

        width = 0;
        height = 0;
        monitorLeft = 0;
        monitorTop = 0;
        pointerVisible = false;
        pointerPosition = {};
        pointerShapeInfo = {};
        pointerShapeBuffer.clear();
    }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    Microsoft::WRL::ComPtr<IDXGIOutput1> output1;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication;

    int monitorLeft = 0;
    int monitorTop = 0;

    // 是否绘制鼠标
    bool pointerVisible;

    POINT pointerPosition{};

    // 鼠标图案信息
    DXGI_OUTDUPL_POINTER_SHAPE_INFO pointerShapeInfo{};

    std::vector<unsigned char> pointerShapeBuffer;

    int width = 0;
    int height = 0;
};
