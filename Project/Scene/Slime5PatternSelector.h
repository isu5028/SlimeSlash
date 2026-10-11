#pragma once

#include <cstdint>

enum class Slime5AttackPattern
{
    // 通常追跡
    Chase,
    // 予告後に直線突進
    Dash,
    // 周囲にダメージ床を生成
    RadialFloor,
    // プレイヤーの周囲を移動
    Strafe,
    // 跳び上がって着地点を攻撃
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
