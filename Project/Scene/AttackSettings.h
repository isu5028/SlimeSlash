#pragma once

namespace AttackSettings
{
    constexpr float HitBottomOffset = 0.15f;
    constexpr float HitTopOffset = 1.35f;

    constexpr float GetHitRange(int comboStage)
    {
        return comboStage == 1 ? 2.4f : comboStage == 2 ? 2.7f : 3.0f;
    }

    constexpr float GetHitHalfAngleDegrees(int comboStage)
    {
        return comboStage == 1 ? 50.0f : comboStage == 2 ? 55.0f : 60.0f;
    }

    constexpr float GetHitMinimumDot(int comboStage)
    {
        return comboStage == 1 ? 0.529919f : comboStage == 2 ? 0.469472f : 0.406737f;
    }
}
