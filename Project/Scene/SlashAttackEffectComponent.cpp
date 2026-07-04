#include "SlashAttackEffectComponent.h"

#include "AttackSettings.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace
{
    constexpr float AttackDuration = 0.28f;
    constexpr float AttackOffset = 0.68f;
    constexpr float AttackHeight = 0.35f;
    constexpr float Pi = 3.1415926535f;

    float Clamp01(float value)
    {
        return std::max(0.0f, std::min(value, 1.0f));
    }
}

SlashAttackEffectComponent::SlashAttackEffectComponent(GameObject& owner, GameObject& target)
    : Component(owner)
    , target_(target)
    , previousUpdate_(std::chrono::steady_clock::now())
{
}

SlashAttackEffectComponent::~SlashAttackEffectComponent()
{
    DX_SAFE_RELEASE(debugIndexBuffer_);
    DX_SAFE_RELEASE(indexBuffer_);
    DX_SAFE_RELEASE(vertexBuffer_);
}

bool SlashAttackEffectComponent::Setup(Renderer& renderer)
{
    D3D11_BUFFER_DESC vertexDescription = {};
    vertexDescription.ByteWidth = sizeof(Vertex) * VertexCount;
    vertexDescription.Usage = D3D11_USAGE_DYNAMIC;
    vertexDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(renderer.GetDevice()->CreateBuffer(&vertexDescription, nullptr, &vertexBuffer_)))
    {
        return false;
    }

    std::array<std::uint16_t, IndexCount> indices = {};
    for (int segment = 0; segment < SegmentCount; ++segment)
    {
        const std::uint16_t base = static_cast<std::uint16_t>(segment * 2);
        const int index = segment * 6;
        indices[index + 0] = base;
        indices[index + 1] = static_cast<std::uint16_t>(base + 1);
        indices[index + 2] = static_cast<std::uint16_t>(base + 2);
        indices[index + 3] = static_cast<std::uint16_t>(base + 1);
        indices[index + 4] = static_cast<std::uint16_t>(base + 3);
        indices[index + 5] = static_cast<std::uint16_t>(base + 2);
    }

    D3D11_BUFFER_DESC indexDescription = {};
    indexDescription.ByteWidth = sizeof(std::uint16_t) * IndexCount;
    indexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices.data();
    if (FAILED(renderer.GetDevice()->CreateBuffer(&indexDescription, &indexData, &indexBuffer_)))
    {
        return false;
    }

    std::array<std::uint16_t, DebugIndexCount> debugIndices = {};
    for (int segment = 0; segment < SegmentCount; ++segment)
    {
        const int index = segment * 3;
        debugIndices[index + 0] = 0;
        debugIndices[index + 1] = static_cast<std::uint16_t>(segment + 1);
        debugIndices[index + 2] = static_cast<std::uint16_t>(segment + 2);
    }
    indexDescription.ByteWidth = sizeof(std::uint16_t) * DebugIndexCount;
    indexData.pSysMem = debugIndices.data();
    if (FAILED(renderer.GetDevice()->CreateBuffer(
        &indexDescription, &indexData, &debugIndexBuffer_)))
    {
        return false;
    }

    return true;
}

void SlashAttackEffectComponent::Trigger(const DirectX::XMFLOAT2& direction, int comboStage)
{
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= 0.0001f)
    {
        return;
    }

    EffectInstance effect;
    effect.comboStage = comboStage;
    effect.direction.x = direction.x / length;
    effect.direction.y = direction.y / length;
    effect.reverseArcAnimation = effect.direction.x < 0.0f;
    if (comboStage == 2)
    {
        effect.reverseArcAnimation = !effect.reverseArcAnimation;
        effect.color = { 0.84705883f, 0.84705883f, 0.84705883f, 1.0f };
        effect.scale = 1.15f;
    }
    else if (comboStage == 3)
    {
        effect.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        effect.scale = 1.32f;
    }
    else
    {
        effect.color = { 0.68235296f, 0.68235296f, 0.68235296f, 1.0f };
        effect.scale = 1.0f;
    }
    const auto targetPosition = target_.GetTransform().GetPosition();
    effect.attackOrigin = targetPosition;
    effect.position = targetPosition;
    effect.position.x += effect.direction.x * AttackOffset;
    effect.position.y += AttackHeight;
    effect.position.z += effect.direction.y * AttackOffset;
    effects_.push_back(effect);
}

void SlashAttackEffectComponent::Update()
{
    const auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - previousUpdate_).count();
    previousUpdate_ = now;
    if (GameTime::IsHitStopped()) return;
    deltaTime = std::min(deltaTime, 0.05f);

    if (effects_.empty())
    {
        return;
    }

    for (auto& effect : effects_)
    {
        effect.elapsed += deltaTime;
    }
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
        [](const EffectInstance& effect) { return effect.elapsed >= AttackDuration; }),
        effects_.end());
}

void SlashAttackEffectComponent::Draw(Renderer& renderer)
{
    if (effects_.empty() || !vertexBuffer_ || !indexBuffer_)
    {
        return;
    }

    renderer.ApplyDefaultPipeline();
    auto* context = renderer.GetDeviceContext();
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R16_UINT, 0);

    for (const auto& effect : effects_)
    {
        if (hitboxDebugVisible_)
        {
            UpdateDebugVertices(effect);
            SetupWorldTransform(renderer, effect.attackOrigin);
            D3D11_MAPPED_SUBRESOURCE debugMapped = {};
            if (SUCCEEDED(context->Map(
                vertexBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &debugMapped)))
            {
                CopyMemory(debugMapped.pData, debugVertices_.data(),
                    sizeof(Vertex) * DebugVertexCount);
                context->Unmap(vertexBuffer_, 0);
                context->IASetIndexBuffer(debugIndexBuffer_, DXGI_FORMAT_R16_UINT, 0);
                context->DrawIndexed(DebugIndexCount, 0, 0);
            }
        }

        UpdateVertices(effect);
        SetupWorldTransform(renderer, effect.position);

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(context->Map(vertexBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            continue;
        }
        CopyMemory(mapped.pData, vertices_.data(), sizeof(Vertex) * VertexCount);
        context->Unmap(vertexBuffer_, 0);
        context->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R16_UINT, 0);
        context->DrawIndexed(IndexCount, 0, 0);
    }
}

void SlashAttackEffectComponent::UpdateDebugVertices(const EffectInstance& effect)
{
    const DirectX::XMFLOAT4 debugColor = { 1.0f, 0.18f, 0.08f, 0.32f };
    debugVertices_[0].Position = { 0.0f, 0.27f, 0.0f };
    debugVertices_[0].Color = debugColor;

    const float centerAngle = std::atan2(effect.direction.x, effect.direction.y);
    const float halfAngle = DirectX::XMConvertToRadians(
        AttackSettings::GetHitHalfAngleDegrees(effect.comboStage));
    const float hitRange = AttackSettings::GetHitRange(effect.comboStage);
    for (int segment = 0; segment <= SegmentCount; ++segment)
    {
        const float t = static_cast<float>(segment) / static_cast<float>(SegmentCount);
        const float angle = centerAngle - halfAngle + halfAngle * 2.0f * t;
        auto& vertex = debugVertices_[segment + 1];
        vertex.Position = {
            std::sin(angle) * hitRange,
            0.27f,
            std::cos(angle) * hitRange
        };
        vertex.Color = debugColor;
    }
}

void SlashAttackEffectComponent::SetupWorldTransform(
    Renderer& renderer, const DirectX::XMFLOAT3& position)
{
    auto& constantBuffer = renderer.GetRenderParam().CbTransformSet;
    DirectX::XMStoreFloat4x4(&constantBuffer.Data.Transform,
        DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(
            position.x, position.y, position.z)));

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    auto* context = renderer.GetDeviceContext();
    if (SUCCEEDED(context->Map(constantBuffer.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        CopyMemory(mapped.pData, &constantBuffer.Data, sizeof(constantBuffer.Data));
        context->Unmap(constantBuffer.pBuffer, 0);
        context->VSSetConstantBuffers(0, 1, &constantBuffer.pBuffer);
    }
}

void SlashAttackEffectComponent::UpdateVertices(const EffectInstance& effect)
{
    const float normalizedTime = Clamp01(effect.elapsed / AttackDuration);
    const float appear = EaseOutCubic(std::min(normalizedTime / 0.35f, 1.0f));
    const float disappear = EaseInCubic(std::max((normalizedTime - 0.50f) / 0.50f, 0.0f));
    const float lifeScale = Clamp01(appear * (1.0f - disappear));
    const float sweep = DirectX::XMConvertToRadians(120.0f);
    const float baseRotationOffset =
        DirectX::XMConvertToRadians(-28.0f + 34.0f * EaseInOutQuad(normalizedTime));
    const float rotationOffset = effect.reverseArcAnimation ? -baseRotationOffset : baseRotationOffset;
    const float centerRadius = (1.08f + 0.10f * lifeScale) * effect.scale;
    const float maxThickness = (0.12f + 0.20f * lifeScale) * effect.scale;
    const float cameraDepthAlignment = Clamp01(-effect.direction.y);
    const float centerAngle = std::atan2(effect.direction.x, effect.direction.y);
    const float startAngle = centerAngle - sweep * 0.5f + rotationOffset;
    const float endAngle = centerAngle + sweep * 0.5f + rotationOffset;
    const float forwardStartT = 0.82f * disappear;
    const float forwardEndT = std::max(forwardStartT + 0.001f, appear);
    const float visibleStartT = effect.reverseArcAnimation ? 1.0f - forwardEndT : forwardStartT;
    const float visibleEndT = effect.reverseArcAnimation ? 1.0f - forwardStartT : forwardEndT;
    const float visibleLength = std::max(visibleEndT - visibleStartT, 0.001f);

    for (int segment = 0; segment <= SegmentCount; ++segment)
    {
        const float segmentT = static_cast<float>(segment) / static_cast<float>(SegmentCount);
        const float clampedT = std::max(visibleStartT, std::min(segmentT, visibleEndT));
        const float localT = Clamp01((clampedT - visibleStartT) / visibleLength);
        const float angle = startAngle + (endAngle - startAngle) * clampedT;
        const float directionX = std::sin(angle);
        const float directionZ = std::cos(angle);
        const int vertexIndex = segment * 2;
        const float thicknessProfile = std::sin(localT * Pi);
        const float halfThickness = std::max(0.012f, maxThickness * thicknessProfile * 0.5f);
        const float innerRadius = std::max(0.02f, centerRadius - halfThickness);
        const float outerRadius = centerRadius + halfThickness;
        const float verticalThicknessCompensation =
            cameraDepthAlignment * maxThickness * thicknessProfile * 0.28f;

        vertices_[vertexIndex + 0].Position = {
            directionX * innerRadius,
            0.05f + 0.02f * thicknessProfile + verticalThicknessCompensation,
            directionZ * innerRadius
        };
        vertices_[vertexIndex + 1].Position = {
            directionX * outerRadius,
            0.13f + 0.08f * thicknessProfile - verticalThicknessCompensation,
            directionZ * outerRadius
        };

        vertices_[vertexIndex + 0].Color = effect.color;
        vertices_[vertexIndex + 1].Color = effect.color;
    }
}

float SlashAttackEffectComponent::EaseOutCubic(float t) const
{
    t = Clamp01(t);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float SlashAttackEffectComponent::EaseInCubic(float t) const
{
    t = Clamp01(t);
    return t * t * t;
}

float SlashAttackEffectComponent::EaseInOutQuad(float t) const
{
    t = Clamp01(t);
    if (t < 0.5f)
    {
        return 2.0f * t * t;
    }

    const float inv = -2.0f * t + 2.0f;
    return 1.0f - (inv * inv) * 0.5f;
}
