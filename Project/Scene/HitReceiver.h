#pragma once

class HitReceiver
{
public:
    virtual ~HitReceiver() = default;
    virtual void ReceiveHit(const DirectX::XMFLOAT2& direction, float knockbackSpeed) = 0;
};
