#pragma once

class HitReceiver
{
public:
    virtual ~HitReceiver() = default;
    // 命中判定そのものは行わず、命中後の敵固有の反応だけを受け持つ
    // 例: ノックバック、ダッシュ中断、死亡処理
    virtual void ReceiveHit(const DirectX::XMFLOAT2& direction, float knockbackSpeed) = 0;
};
