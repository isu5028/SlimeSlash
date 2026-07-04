#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct AsepriteFrame
{
    std::uint32_t durationMs = 0;
    std::vector<std::uint8_t> rgba;
};

enum class AsepriteLoopDirection
{
    Forward,
    Reverse,
    PingPong,
    PingPongReverse
};

struct AsepriteTag
{
    std::string name;
    std::uint16_t firstFrame = 0;
    std::uint16_t lastFrame = 0;
    AsepriteLoopDirection direction = AsepriteLoopDirection::Forward;
    std::uint16_t repeatCount = 0; // 0 means infinite.
};

struct AsepriteData
{
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<AsepriteFrame> frames;
    std::vector<AsepriteTag> tags;
};

class AsepriteLoader
{
public:
    // Loads an RGBA .aseprite file and composites its visible normal-blend layers.
    // On failure, returns false and puts a readable reason in errorMessage.
    static bool Load(
        const std::string& filePath,
        AsepriteData& output,
        std::string& errorMessage);
};
