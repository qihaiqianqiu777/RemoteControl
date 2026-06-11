#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include <string>
#include <cstdint>
#include <cstring>

// FFmpeg 是 C 语言库。
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
}

inline std::string ffmpegDecodeErrorToString(int err)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(err, buffer, sizeof(buffer));
    return std::string(buffer);
}

struct DecoderContextDeleter
{
    void operator()(AVCodecContext *p) const
    {
        if (p)
        {
            avcodec_free_context(&p);
        }
    }
};

struct DecoderFrameDeleter
{
    void operator()(AVFrame *p) const
    {
        if (p)
        {
            av_frame_free(&p);
        }
    }
};

struct DecoderPacketDeleter
{
    void operator()(AVPacket *p) const
    {
        if (p)
        {
            av_packet_free(&p);
        }
    }
};

struct DecoderSwsContextDeleter
{
    void operator()(SwsContext *p) const
    {
        if (p)
        {
            sws_freeContext(p);
        }
    }
};

struct DecodedFrame
{

    std::vector<unsigned char> bgraPixels;

    int height = 0;
    int width = 0;
};

// ==============================
// H264Decoder
// ==============================
//
// 作用：
//   输入：H.264 压缩数据 packet
//   输出：BGRA 像素帧
//
// 客户端流程：
//   SocketUtils::recvPacket()
//        ↓
//   packet.body 是 H.264 数据
//        ↓
//   decoder.decodePacket(packet.body, decodedFrames)
//        ↓
//   得到 BGRA 像素
//        ↓
//   QImage 显示
// ==============================

class H264Decoder
{
public:
    H264Decoder() = default;

    ~H264Decoder()
    {
        cleanup();
    }

    bool init()
    {
        cleanup();

        const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);

        if (!codec)
        {
            std::cout << "H264 decoder not found" << std::endl;
            return false;
        }
        std::cout << "use decoder: " << codec->name << std::endl;

        codecCtx.reset(avcodec_alloc_context3(codec));

        if (!codecCtx)
        {
            std::cout << "avcodec_alloc_context3 failed" << std::endl;
            return false;
        }

        int ret = avcodec_open2(codecCtx.get(), codec, nullptr);

        if (ret < 0)
        {
            std::cout << "avcodec_open2 decoder failed: "
                      << ffmpegDecodeErrorToString(ret)
                      << std::endl;

            cleanup();
            return false;
        }
        frame.reset(av_frame_alloc());

        if (!frame)
        {
            std::cout << "av_frame_alloc failed" << std::endl;
            cleanup();
            return false;
        }
        packet.reset(av_packet_alloc());

        if (!packet)
        {
            std::cout << "av_packet_alloc failed" << std::endl;
            cleanup();
            return false;
        }

        initialized = true;

        std::cout << "H264Decoder init success" << std::endl;

        return true;
    }

    // 解码一个 H.264 packet
    //
    // h264Data:
    //   从服务端收到的一包 H.264 数据。
    //
    // outFrames:
    //   输出解码后的 BGRA 图像帧。

    bool decodePacket(
        const std::vector<char> &h264Data,
        std::vector<DecodedFrame> &outFrames)
    {
        outFrames.clear();

        if (!initialized)
        {
            std::cout << "decoder not initialized" << std::endl;
            return false;
        }

        if (h264Data.empty())
        {
            return true;
        }

        av_packet_unref(packet.get());

        int ret = av_new_packet(
            packet.get(),
            static_cast<int>(h264Data.size()));

        if (ret < 0)
        {
            std::cout << "av_new_packet failed: "
                      << ffmpegDecodeErrorToString(ret)
                      << std::endl;
            return false;
        }

        std::memcpy(
            packet->data,
            h264Data.data(),
            h264Data.size());

        ret = avcodec_send_packet(codecCtx.get(), packet.get());

        av_packet_unref(packet.get());

        if (ret < 0)
        {
            std::cout << "avcodec_send_packet failed: "
                      << ffmpegDecodeErrorToString(ret)
                      << std::endl;
            return false;
        }

        // 从解码器取出解码后的 AVFrame
        //
        // FFmpeg 新版 API 是 send / receive 模型：
        //
        // avcodec_send_packet()
        //       ↓
        // avcodec_receive_frame()
        //
        // receive_frame 可能一次取出 0 帧、1 帧或多帧。
        while (true)
        {
            ret = avcodec_receive_frame(codecCtx.get(), frame.get());

            // EAGAIN:
            //   当前没有更多解码帧了，需要继续送新的 packet。
            //
            // AVERROR_EOF:
            //   解码器结束。
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }

            if (ret < 0)
            {
                std::cout << "avcodec_receive_frame failed: "
                          << ffmpegDecodeErrorToString(ret)
                          << std::endl;
                return false;
            }

            DecodedFrame decoded;

            if (!convertFrameToBgra(decoded))
            {

                return false;
            }

            outFrames.push_back(std::move(decoded));

            av_frame_unref(frame.get());
        }
        return true;
    }
    void cleanup()
    {

        swsCtx.reset();

        packet.reset();
        frame.reset();
        codecCtx.reset();

        cachedWidth = 0;
        cachedHeight = 0;
        cachedFormat = AV_PIX_FMT_NONE;

        initialized = false;
    }

private:
    bool ensureSwsContext()
    {
        int frameWidth = frame->width;
        int frameHeight = frame->height;

        AVPixelFormat frameFormat = static_cast<AVPixelFormat>(frame->format);

        if (frameWidth <= 0 || frameHeight <= 0)
        {
            std::cout << "invalid decoded frame size" << std::endl;
            return false;
        }

        if (swsCtx &&
            cachedWidth == frameWidth &&
            cachedHeight == frameHeight &&
            cachedFormat == frameFormat)
        {
            return true;
        }

        swsCtx.reset();

        swsCtx.reset(
            sws_getContext(
                frameWidth,
                frameHeight,
                frameFormat,

                frameWidth,
                frameHeight,
                AV_PIX_FMT_BGRA,

                SWS_FAST_BILINEAR,
                nullptr,
                nullptr,
                nullptr

                )

        );
        if (!swsCtx)
        {
            std::cout << "sws_getContext decoder failed" << std::endl;
            return false;
        }

        cachedWidth = frameWidth;
        cachedHeight = frameHeight;
        cachedFormat = frameFormat;

        std::cout << "decoder sws context created: "
                  << frameWidth << "x" << frameHeight
                  << std::endl;

        return true;
    }

    bool convertFrameToBgra(DecodedFrame &decoded)
    {
        if (!ensureSwsContext())
        {
            return false;
        }

        decoded.width = frame->width;

        decoded.height = frame->height;

        const int bytesPerPixel = 4;

        const int dstStride = decoded.width * bytesPerPixel;

        decoded.bgraPixels.resize(
            static_cast<size_t>(decoded.width) *
            static_cast<size_t>(decoded.height) *
            bytesPerPixel);

        uint8_t *dstData[1] = {
            decoded.bgraPixels.data()};
        int dstLinesize[1] = {
            dstStride};

        sws_scale(
            swsCtx.get(),
            frame->data,
            frame->linesize,
            0,
            frame->height,
            dstData,
            dstLinesize);
        return true;
    }

    // 解码器上下文
    std::unique_ptr<AVCodecContext, DecoderContextDeleter>
        codecCtx;

    // 解码后的原始帧
    std::unique_ptr<AVFrame, DecoderFrameDeleter> frame;

    // 输入的 H.264 packet
    std::unique_ptr<AVPacket, DecoderPacketDeleter> packet;

    // 像素格式转换器：YUV420P -> BGRA
    std::unique_ptr<SwsContext, DecoderSwsContextDeleter> swsCtx;

    // 缓存 swsCtx 当前对应的尺寸和格式
    // 如果下一帧尺寸和格式没变，
    // 就复用 swsCtx，减少重复创建。
    int cachedWidth = 0;
    int cachedHeight = 0;
    AVPixelFormat cachedFormat = AV_PIX_FMT_NONE;

    bool initialized = false;
};
