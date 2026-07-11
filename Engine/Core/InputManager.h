#pragma once

#include <array>

class InputManager
{
public:
    void Sample();
    void Commit();
    void ConsumePress(int virtualKey);

    bool IsDown(int virtualKey) const;
    bool WasPressed(int virtualKey) const;
    bool WasReleased(int virtualKey) const;
    bool IsAnyDown(int firstVirtualKey, int secondVirtualKey) const;
    bool WasAnyPressed(int firstVirtualKey, int secondVirtualKey) const;

private:
    static constexpr std::size_t KeyCount = 256;
    bool IsPreviousDown(int virtualKey) const;
    std::array<bool, KeyCount> current_ = {};
    std::array<bool, KeyCount> previous_ = {};
};
