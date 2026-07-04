#include "AsepriteLoader.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

#include "zlib.h"

namespace
{
    class Reader
    {
    public:
        Reader(const std::vector<std::uint8_t>& bytes, std::size_t begin, std::size_t end)
            : bytes_(bytes), position_(begin), end_(end)
        {
            if (begin > end || end > bytes.size()) throw std::runtime_error("Invalid data range");
        }

        std::uint8_t U8() { Require(1); return bytes_[position_++]; }
        std::uint16_t U16()
        {
            Require(2);
            const auto value = static_cast<std::uint16_t>(bytes_[position_]) |
                (static_cast<std::uint16_t>(bytes_[position_ + 1]) << 8);
            position_ += 2;
            return value;
        }
        std::int16_t S16() { return static_cast<std::int16_t>(U16()); }
        std::uint32_t U32()
        {
            Require(4);
            const auto value = static_cast<std::uint32_t>(bytes_[position_]) |
                (static_cast<std::uint32_t>(bytes_[position_ + 1]) << 8) |
                (static_cast<std::uint32_t>(bytes_[position_ + 2]) << 16) |
                (static_cast<std::uint32_t>(bytes_[position_ + 3]) << 24);
            position_ += 4;
            return value;
        }
        void Skip(std::size_t count) { Require(count); position_ += count; }
        std::string String()
        {
            const auto length = U16();
            Require(length);
            std::string value(reinterpret_cast<const char*>(bytes_.data() + position_), length);
            position_ += length;
            return value;
        }
        const std::uint8_t* Current() const { return bytes_.data() + position_; }
        std::size_t Remaining() const { return end_ - position_; }

    private:
        void Require(std::size_t count) const
        {
            if (count > end_ - position_) throw std::runtime_error("Unexpected end of file");
        }
        const std::vector<std::uint8_t>& bytes_;
        std::size_t position_;
        std::size_t end_;
    };

    struct Layer
    {
        bool visible = true;
        std::uint8_t opacity = 255;
        std::uint16_t blendMode = 0;
    };

    struct Cel
    {
        std::uint16_t layer = 0;
        std::int16_t x = 0;
        std::int16_t y = 0;
        std::uint8_t opacity = 255;
        std::uint16_t width = 0;
        std::uint16_t height = 0;
        std::vector<std::uint8_t> pixels;
    };

    void Composite(AsepriteFrame& frame, const Cel& cel, const Layer& layer,
        std::uint16_t canvasWidth, std::uint16_t canvasHeight)
    {
        if (!layer.visible) return;
        for (std::uint16_t sy = 0; sy < cel.height; ++sy)
        {
            const int dy = cel.y + sy;
            if (dy < 0 || dy >= canvasHeight) continue;
            for (std::uint16_t sx = 0; sx < cel.width; ++sx)
            {
                const int dx = cel.x + sx;
                if (dx < 0 || dx >= canvasWidth) continue;
                const auto sourceIndex = (static_cast<std::size_t>(sy) * cel.width + sx) * 4;
                const auto targetIndex = (static_cast<std::size_t>(dy) * canvasWidth + dx) * 4;
                const unsigned alpha = static_cast<unsigned>(cel.pixels[sourceIndex + 3]) *
                    cel.opacity * layer.opacity / (255u * 255u);
                const unsigned inverse = 255u - alpha;
                const unsigned oldAlpha = frame.rgba[targetIndex + 3];
                const unsigned outAlpha = alpha + oldAlpha * inverse / 255u;
                if (outAlpha == 0) continue;
                for (int channel = 0; channel < 3; ++channel)
                {
                    const unsigned premultiplied = cel.pixels[sourceIndex + channel] * alpha +
                        frame.rgba[targetIndex + channel] * oldAlpha * inverse / 255u;
                    frame.rgba[targetIndex + channel] = static_cast<std::uint8_t>(premultiplied / outAlpha);
                }
                frame.rgba[targetIndex + 3] = static_cast<std::uint8_t>(outAlpha);
            }
        }
    }
}

bool AsepriteLoader::Load(const std::string& filePath, AsepriteData& output, std::string& errorMessage)
{
    try
    {
        output = {};
        errorMessage.clear();
        std::ifstream stream(filePath, std::ios::binary | std::ios::ate);
        if (!stream) throw std::runtime_error("Could not open: " + filePath);
        const auto fileSize = stream.tellg();
        if (fileSize < 128) throw std::runtime_error("File is too small to be an Aseprite file");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
        stream.seekg(0);
        if (!stream.read(reinterpret_cast<char*>(bytes.data()), fileSize))
            throw std::runtime_error("Could not read the complete file");

        Reader header(bytes, 0, 128);
        const auto declaredSize = header.U32();
        if (header.U16() != 0xA5E0) throw std::runtime_error("Invalid Aseprite file magic");
        const auto frameCount = header.U16();
        output.width = header.U16();
        output.height = header.U16();
        const auto colorDepth = header.U16();
        if (declaredSize != bytes.size()) throw std::runtime_error("Aseprite file size header is invalid");
        if (colorDepth != 32) throw std::runtime_error("Only 32-bit RGBA Aseprite files are supported");
        if (output.width == 0 || output.height == 0 || frameCount == 0)
            throw std::runtime_error("Aseprite dimensions or frame count are zero");

        output.frames.resize(frameCount);
        std::vector<Layer> layers;
        std::vector<std::vector<Cel>> frameCels(frameCount);
        std::size_t framePosition = 128;

        for (std::uint16_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
        {
            Reader frameHeader(bytes, framePosition, bytes.size());
            const auto frameBytes = frameHeader.U32();
            if (frameBytes < 16 || framePosition + frameBytes > bytes.size())
                throw std::runtime_error("Invalid frame size");
            if (frameHeader.U16() != 0xF1FA) throw std::runtime_error("Invalid frame magic");
            const auto oldChunkCount = frameHeader.U16();
            output.frames[frameIndex].durationMs = frameHeader.U16();
            frameHeader.Skip(2);
            const auto newChunkCount = frameHeader.U32();
            const auto chunkCount = newChunkCount == 0 ? oldChunkCount : newChunkCount;
            std::size_t chunkPosition = framePosition + 16;

            for (std::uint32_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
            {
                Reader chunkHeader(bytes, chunkPosition, framePosition + frameBytes);
                const auto chunkBytes = chunkHeader.U32();
                const auto chunkType = chunkHeader.U16();
                if (chunkBytes < 6 || chunkPosition + chunkBytes > framePosition + frameBytes)
                    throw std::runtime_error("Invalid chunk size");
                Reader chunk(bytes, chunkPosition + 6, chunkPosition + chunkBytes);

                if (chunkType == 0x2004)
                {
                    const auto flags = chunk.U16();
                    const auto layerType = chunk.U16();
                    chunk.Skip(6); // Child level and deprecated default width/height.
                    const auto blendMode = chunk.U16();
                    const auto opacity = chunk.U8();
                    chunk.Skip(3);
                    chunk.String();
                    if (layerType != 0) throw std::runtime_error("Tilemap layers are not supported");
                    if (blendMode != 0) throw std::runtime_error("Only normal layer blending is supported");
                    layers.push_back({ (flags & 1u) != 0, opacity, blendMode });
                }
                else if (chunkType == 0x2005)
                {
                    Cel cel;
                    cel.layer = chunk.U16();
                    cel.x = chunk.S16();
                    cel.y = chunk.S16();
                    cel.opacity = chunk.U8();
                    const auto celType = chunk.U16();
                    chunk.Skip(7);
                    if (cel.layer >= layers.size()) throw std::runtime_error("Cel refers to an unknown layer");
                    if (celType == 1)
                    {
                        const auto linkedFrame = chunk.U16();
                        if (linkedFrame >= frameIndex) throw std::runtime_error("Invalid linked cel frame");
                        const auto& linkedCels = frameCels[linkedFrame];
                        const auto linked = std::find_if(linkedCels.begin(), linkedCels.end(),
                            [&cel](const Cel& candidate) { return candidate.layer == cel.layer; });
                        if (linked == linkedCels.end()) throw std::runtime_error("Linked cel was not found");
                        cel.width = linked->width;
                        cel.height = linked->height;
                        cel.pixels = linked->pixels;
                    }
                    else if (celType == 0 || celType == 2)
                    {
                        cel.width = chunk.U16();
                        cel.height = chunk.U16();
                        const std::size_t pixelBytes = static_cast<std::size_t>(cel.width) * cel.height * 4;
                        cel.pixels.resize(pixelBytes);
                        if (celType == 0)
                        {
                            if (chunk.Remaining() != pixelBytes) throw std::runtime_error("Raw cel size does not match dimensions");
                            std::memcpy(cel.pixels.data(), chunk.Current(), pixelBytes);
                        }
                        else
                        {
                            uLongf resultBytes = static_cast<uLongf>(pixelBytes);
                            const int result = uncompress(cel.pixels.data(), &resultBytes,
                                chunk.Current(), static_cast<uLong>(chunk.Remaining()));
                            if (result != Z_OK || resultBytes != pixelBytes)
                                throw std::runtime_error("Could not decompress a cel image");
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Unsupported cel type");
                    }
                    frameCels[frameIndex].push_back(std::move(cel));
                }
                else if (chunkType == 0x2018)
                {
                    const auto tagCount = chunk.U16();
                    chunk.Skip(8);
                    for (std::uint16_t tagIndex = 0; tagIndex < tagCount; ++tagIndex)
                    {
                        AsepriteTag tag;
                        tag.firstFrame = chunk.U16();
                        tag.lastFrame = chunk.U16();
                        const auto direction = chunk.U8();
                        tag.direction = static_cast<AsepriteLoopDirection>(direction);
                        tag.repeatCount = chunk.U16();
                        chunk.Skip(6);
                        chunk.Skip(3); // Deprecated tag color.
                        chunk.Skip(1);
                        tag.name = chunk.String();
                        if (direction > 3 || tag.firstFrame > tag.lastFrame || tag.lastFrame >= frameCount)
                            throw std::runtime_error("Invalid animation tag");
                        output.tags.push_back(std::move(tag));
                    }
                }
                chunkPosition += chunkBytes;
            }
            framePosition += frameBytes;
        }

        const auto canvasBytes = static_cast<std::size_t>(output.width) * output.height * 4;
        for (std::uint16_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
        {
            output.frames[frameIndex].rgba.assign(canvasBytes, 0);
            for (const auto& cel : frameCels[frameIndex])
                Composite(output.frames[frameIndex], cel, layers[cel.layer], output.width, output.height);
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        output = {};
        errorMessage = exception.what();
        return false;
    }
}
