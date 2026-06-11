#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include <string>
#include <cstdint>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
}

// ==============================
// FFmpeg 错误码转字符串
// ==============================
inline std::string ffmpegErrorToString(int err)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(err, buffer, sizeof(buffer));
    return std::string(buffer);
}

// ==============================
// 自定义释放器
// ==============================

struct AVCodecContextDeleter
{
    void operator()(AVCodecContext *p) const
    {
        if (p)
        {
            avcodec_free_context(&p);
        }
    }
};

struct AVFrameDeleter
{
    void operator()(AVFrame *p) const
    {
        if (p)
        {
            av_frame_free(&p);
        }
    }
};

struct AVPacketDeleter
{
    void operator()(AVPacket *p) const
    {
        if (p)
        {
            av_packet_free(&p);
        }
    }
};

struct SwsContextDeleter
{
    void operator()(SwsContext *p) const
    {
        if (p)
        {
            sws_freeContext(p);
        }
    }
};

class H264Encoder
{
public:
    H264Encoder() = default;

    ~H264Encoder()
    {
        cleanup();
    }

    H264Encoder(const H264Encoder &) = delete;
    H264Encoder &operator=(const H264Encoder &) = delete;

    // 初始化 H.264 编码器
    //
    // width   : 宽度，例如 960
    // height  : 高度，例如 540
    // fps     : 帧率，例如 10
    // bitrate : 码率，例如 1500 * 1000
    bool init(int width, int height, int fps, int bitrate)
    {
        cleanup();

        if (width <= 0 || height <= 0 || fps <= 0 || bitrate <= 0)
        {
            std::cout << "invalid encoder params" << std::endl;
            return false;
        }

        this->width = width;
        this->height = height;
        this->fps = fps;
        this->pts = 0;

        // 1. 查找 H.264 编码器，优先使用 libx264
        const AVCodec *codec = avcodec_find_encoder_by_name("libx264");

        if (!codec)
        {
            codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        }

        if (!codec)
        {
            std::cout << "H264 encoder not found" << std::endl;
            return false;
        }

        std::cout << "use encoder name: " << codec->name << std::endl;

        // 2. 创建编码器上下文
        codecCtx.reset(avcodec_alloc_context3(codec));

        if (!codecCtx)
        {
            std::cout << "avcodec_alloc_context3 failed" << std::endl;
            return false;
        }

        // 3. 设置编码参数
        codecCtx->width = width;
        codecCtx->height = height;

        // H.264 常用输入格式是 YUV420P
        codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

        // 时间基：每一帧的时间单位
        codecCtx->time_base = AVRational{1, fps};

        // 帧率
        codecCtx->framerate = AVRational{fps, 1};

        // 码率，单位 bit/s
        codecCtx->bit_rate = bitrate;

        // GOP 大小，这里设置成 fps，表示大约 1 秒一个关键帧
        codecCtx->gop_size = fps;

        // 不使用 B 帧，降低延迟
        codecCtx->max_b_frames = 0;

        // libx264 低延迟参数
        if (std::string(codec->name) == "libx264")
        {
            av_opt_set(codecCtx->priv_data, "preset", "ultrafast", 0);
            av_opt_set(codecCtx->priv_data, "tune", "zerolatency", 0);
            av_opt_set(codecCtx->priv_data, "profile", "baseline", 0);
        }

        // 4. 打开编码器
        int ret = avcodec_open2(codecCtx.get(), codec, nullptr);

        if (ret < 0)
        {
            std::cout << "avcodec_open2 failed: "
                      << ffmpegErrorToString(ret) << std::endl;
            cleanup();
            return false;
        }

        // 5. 创建 AVFrame
        frame.reset(av_frame_alloc());

        if (!frame)
        {
            std::cout << "av_frame_alloc failed" << std::endl;
            cleanup();
            return false;
        }

        frame->format = codecCtx->pix_fmt;
        frame->width = width;
        frame->height = height;

        ret = av_frame_get_buffer(frame.get(), 32);

        if (ret < 0)
        {
            std::cout << "av_frame_get_buffer failed: "
                      << ffmpegErrorToString(ret) << std::endl;
            cleanup();
            return false;
        }

        // 6. 创建 AVPacket
        packet.reset(av_packet_alloc());

        if (!packet)
        {
            std::cout << "av_packet_alloc failed" << std::endl;
            cleanup();
            return false;
        }

        // 7. 创建像素格式转换器：BGRA -> YUV420P
        swsCtx.reset(
            sws_getContext(
                width,
                height,
                AV_PIX_FMT_BGRA,

                width,
                height,
                AV_PIX_FMT_YUV420P,

                SWS_FAST_BILINEAR,
                nullptr,
                nullptr,
                nullptr));

        if (!swsCtx)
        {
            std::cout << "sws_getContext failed" << std::endl;
            cleanup();
            return false;
        }

        initialized = true;

        std::cout << "H264Encoder init success: "
                  << width << "x" << height
                  << ", fps=" << fps
                  << ", bitrate=" << bitrate
                  << std::endl;

        return true;
    }

    // 编码一帧 BGRA 图像
    //
    // bgraPixels:
    //   输入图像，格式 BGRA，每个像素 4 字节
    //
    // outPackets:
    //   输出编码后的 H.264 数据包
    bool encodeBgraFrame(
        const std::vector<unsigned char> &bgraPixels,
        std::vector<std::vector<char>> &outPackets)
    {
        outPackets.clear();

        if (!initialized)
        {
            std::cout << "encoder not initialized" << std::endl;
            return false;
        }

        const int bytesPerPixel = 4;
        const int srcStride = width * bytesPerPixel;
        const int srcSize = srcStride * height;

        if (bgraPixels.size() < static_cast<size_t>(srcSize))
        {
            std::cout << "bgraPixels size invalid" << std::endl;
            return false;
        }

        // 1. 确保 frame 可写
        int ret = av_frame_make_writable(frame.get());

        if (ret < 0)
        {
            std::cout << "av_frame_make_writable failed: "
                      << ffmpegErrorToString(ret) << std::endl;
            return false;
        }

        // 2. 准备 BGRA 输入数据
        const uint8_t *srcData[1] =
            {
                reinterpret_cast<const uint8_t *>(bgraPixels.data())};

        int srcLinesize[1] =
            {
                srcStride};

        // 3. BGRA -> YUV420P
        sws_scale(
            swsCtx.get(),
            srcData,
            srcLinesize,
            0,
            height,
            frame->data,
            frame->linesize);

        // 4. 设置帧序号
        frame->pts = pts++;

        // 5. 把原始帧送进编码器
        ret = avcodec_send_frame(codecCtx.get(), frame.get());

        if (ret < 0)
        {
            std::cout << "avcodec_send_frame failed: "
                      << ffmpegErrorToString(ret) << std::endl;
            return false;
        }

        // 6. 从编码器取出 H.264 packet
        while (true)
        {
            ret = avcodec_receive_packet(codecCtx.get(), packet.get());

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }

            if (ret < 0)
            {
                std::cout << "avcodec_receive_packet failed: "
                          << ffmpegErrorToString(ret) << std::endl;
                return false;
            }

            std::vector<char> data(
                reinterpret_cast<char *>(packet->data),
                reinterpret_cast<char *>(packet->data) + packet->size);

            outPackets.push_back(std::move(data));

            av_packet_unref(packet.get());
        }

        return true;
    }

    void cleanup()
    {
        // unique_ptr.reset() 会自动调用对应删除器
        swsCtx.reset();
        packet.reset();
        frame.reset();
        codecCtx.reset();

        width = 0;
        height = 0;
        fps = 0;
        pts = 0;
        initialized = false;
    }

    bool isInitialized() const
    {
        return initialized;
    }

private:
    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> codecCtx;
    std::unique_ptr<AVFrame, AVFrameDeleter> frame;
    std::unique_ptr<AVPacket, AVPacketDeleter> packet;
    std::unique_ptr<SwsContext, SwsContextDeleter> swsCtx;

    int width = 0;
    int height = 0;
    int fps = 0;

    int64_t pts = 0;

    bool initialized = false;
};