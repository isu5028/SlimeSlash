#pragma once

class BgmPlaybackController
{
public:
    void Reset(float startDelay);
    void Update(float deltaTime);

    bool HasStarted() const { return started_; }
    float GetLoopElapsed() const { return loopElapsed_; }
    float GetAnimationTime() const { return started_ ? loopElapsed_ : -1.0f; }

private:
    float startDelayRemaining_ = 0.0f;
    float loopElapsed_ = 0.0f;
    bool started_ = false;
};
