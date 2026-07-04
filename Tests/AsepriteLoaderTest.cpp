#include "AsepriteLoader.h"

#include <iostream>

int main()
{
    AsepriteData data;
    std::string error;
    if (!AsepriteLoader::Load("Project/Assets/Images/aseprite/Player.aseprite", data, error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << data.width << 'x' << data.height << " frames=" << data.frames.size()
        << " tags=" << data.tags.size() << '\n';
    for (const auto& tag : data.tags)
        std::cout << tag.name << ' ' << tag.firstFrame << '-' << tag.lastFrame << '\n';
    std::size_t visiblePixels = 0;
    for (std::size_t index = 3; index < data.frames[0].rgba.size(); index += 4)
        if (data.frames[0].rgba[index] != 0) ++visiblePixels;
    std::cout << "frame0 visiblePixels=" << visiblePixels << '\n';
    return data.width == 48 && data.height == 32 && data.frames.size() == 10 && visiblePixels > 0 ? 0 : 2;
}
