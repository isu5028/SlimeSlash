#include "BgmPlaybackController.h"

#include "SoundPlayer.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr wchar_t BgmSoundPath[] = L"Project/Assets/Sounds/BGM.wav";
    constexpr float BgmLoopDuration = 175.52f;
}

void BgmPlaybackController::Reset(float startDelay)
{
    SoundPlayer::StopBgm();
    startDelayRemaining_ = startDelay;
    loopElapsed_ = 0.0f;
    started_ = false;
}

void BgmPlaybackController::Update(float deltaTime)
{
    if (!started_)
    {
        startDelayRemaining_ = std::max(0.0f, startDelayRemaining_ - deltaTime);
        if (startDelayRemaining_ <= 0.0f)
        {
            started_ = SoundPlayer::PlayBgm(BgmSoundPath, true);
        }
        return;
    }

    loopElapsed_ = std::fmod(loopElapsed_ + deltaTime, BgmLoopDuration);
}
