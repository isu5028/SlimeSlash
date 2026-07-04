#pragma once

#include <chrono>

class GameTime
{
public:
    static void StartHitStop(float durationSeconds);
    static bool IsHitStopped();

private:
    static std::chrono::steady_clock::time_point hitStopEnd_;
};
