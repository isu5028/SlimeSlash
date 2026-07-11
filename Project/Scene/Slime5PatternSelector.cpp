#include "Slime5PatternSelector.h"

#include <array>
#include <cstddef>

void Slime5PatternSelector::Reset()
{
    previous_ = Slime5AttackPattern::Chase;
    hasPrevious_ = false;
    radialFloorUnlocked_ = false;
    radialFloorGuaranteedPending_ = false;
    radialFloorRecoveryPending_ = false;
    randomState_ = 0x5f3759dfu;
}

Slime5AttackPattern Slime5PatternSelector::Choose(float healthRatio)
{
    using Pattern = Slime5AttackPattern;
    if (!radialFloorUnlocked_ && healthRatio <= 0.6f)
    {
        radialFloorUnlocked_ = true;
        radialFloorGuaranteedPending_ = true;
    }
    if (radialFloorGuaranteedPending_)
    {
        previous_ = Pattern::RadialFloor;
        hasPrevious_ = true;
        radialFloorGuaranteedPending_ = false;
        radialFloorRecoveryPending_ = true;
        return Pattern::RadialFloor;
    }

    std::array<Pattern, 5> candidates = {
        Pattern::Chase, Pattern::Dash, Pattern::RadialFloor,
        Pattern::Strafe, Pattern::JumpSlam
    };
    std::size_t candidateCount = 0;
    for (const Pattern candidate : candidates)
    {
        if (candidate == Pattern::RadialFloor &&
            (!radialFloorUnlocked_ || radialFloorRecoveryPending_)) continue;
        if (hasPrevious_)
        {
            const bool previousWasWalking =
                previous_ == Pattern::Chase || previous_ == Pattern::Strafe;
            const bool candidateIsWalking =
                candidate == Pattern::Chase || candidate == Pattern::Strafe;
            if (previousWasWalking && candidateIsWalking) continue;
            if ((candidate == Pattern::Chase && previous_ == Pattern::Chase) ||
                (candidate == Pattern::RadialFloor && previous_ == Pattern::RadialFloor)) continue;
        }
        candidates[candidateCount++] = candidate;
    }
    if (candidateCount == 0)
    {
        candidates[0] = Pattern::Dash;
        candidateCount = 1;
    }

    randomState_ = randomState_ * 1664525u + 1013904223u;
    const Pattern next = candidates[(randomState_ >> 16) % candidateCount];
    if (next == Pattern::RadialFloor) radialFloorRecoveryPending_ = true;
    else if (next == Pattern::Dash || next == Pattern::JumpSlam)
        radialFloorRecoveryPending_ = false;
    previous_ = next;
    hasPrevious_ = true;
    return next;
}
