#include "GameTime.h"

#include <algorithm>

std::chrono::steady_clock::time_point GameTime::hitStopEnd_ = {};

void GameTime::StartHitStop(float durationSeconds)
{
    if (durationSeconds <= 0.0f) return;
    const auto requestedEnd = std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<float>(durationSeconds));
    hitStopEnd_ = std::max(hitStopEnd_, requestedEnd);
}

bool GameTime::IsHitStopped()
{
    return std::chrono::steady_clock::now() < hitStopEnd_;
}
