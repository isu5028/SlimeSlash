#include "UiSelectionCursor.h"

#include "AsepriteSpriteComponent.h"
#include "Renderer.h"

#include <cmath>

bool UiSelectionCursor::Initialize(
    Renderer& renderer,
    const char* imagePath,
    float worldHeight,
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT4& color,
    float opacity)
{
    object_ = std::make_unique<GameObject>();
    object_->GetTransform().SetPosition(position);
    renderer_ = &object_->AddComponent<AsepriteSpriteComponent>();
    if (!renderer_->Setup(renderer, imagePath, worldHeight)) return false;
    renderer_->SetOpacity(opacity);
    renderer_->SetFlash(color, 1.0f);
    return true;
}

void UiSelectionCursor::Update(
    float x,
    float y,
    float opacity,
    float idleTime,
    float animationTime,
    float blinkDelay,
    float blinkPeriod)
{
    if (!object_ || !renderer_) return;
    if (idleTime >= blinkDelay && blinkPeriod > 0.0f)
    {
        const float blinkTime = std::fmod(animationTime, blinkPeriod) / blinkPeriod;
        opacity *= blinkTime < 0.5f ? 1.0f : 0.0f;
    }
    object_->GetTransform().SetPosition({ x, y, 0.0f });
    renderer_->SetOpacity(opacity);
    object_->Update();
}

void UiSelectionCursor::Draw(Renderer& renderer) const
{
    if (object_) object_->Draw(renderer);
}

void UiSelectionCursor::SetOpacity(float opacity)
{
    if (renderer_) renderer_->SetOpacity(opacity);
}

void UiSelectionCursor::Reset()
{
    object_.reset();
    renderer_ = nullptr;
}
