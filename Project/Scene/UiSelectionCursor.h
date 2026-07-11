#pragma once

#include "GameObject.h"

#include <memory>

class AsepriteSpriteComponent;
class Renderer;

class UiSelectionCursor
{
public:
    bool Initialize(
        Renderer& renderer,
        const char* imagePath,
        float worldHeight,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT4& color,
        float opacity);
    void Update(
        float x,
        float y,
        float opacity,
        float idleTime,
        float animationTime,
        float blinkDelay,
        float blinkPeriod);
    void Draw(Renderer& renderer) const;
    void SetOpacity(float opacity);
    void Reset();

private:
    std::unique_ptr<GameObject> object_;
    AsepriteSpriteComponent* renderer_ = nullptr;
};
