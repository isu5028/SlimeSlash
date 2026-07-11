#pragma once

#include <cstdint>

enum class Slime5AttackPattern
{
    Chase,
    Dash,
    RadialFloor,
    Strafe,
    JumpSlam,
};

class Slime5PatternSelector
{
public:
    void Reset();
    Slime5AttackPattern Choose(float healthRatio);

private:
    Slime5AttackPattern previous_ = Slime5AttackPattern::Chase;
    bool hasPrevious_ = false;
    bool radialFloorUnlocked_ = false;
    bool radialFloorGuaranteedPending_ = false;
    bool radialFloorRecoveryPending_ = false;
    std::uint32_t randomState_ = 0x5f3759dfu;
};
