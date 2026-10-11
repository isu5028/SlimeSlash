#include "AnimatedTextSystem.h"

#include "Renderer.h"
#include "InputManager.h"
#include "WorldTextSpriteComponent.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    constexpr float BgmBarDuration = 5.850f;
    constexpr float BgmLoopDuration = 175.52f;
    constexpr char OptionFontPath[] = "Project/Assets/Fonts/Shikakufuto_Free_ver20251228.ttf";
    constexpr float AnimatedLetterJumpDuration = 0.42f;
    constexpr float AnimatedLetterStaggerInterval = 0.04f;
    constexpr float AnimatedLetterTracking = 0.04f;
    constexpr float AnimatedWordSpacing = 0.16f;
    constexpr float AnimatedLetterSettleDepthRatio = 0.25f;
    constexpr float AnimatedLetterSettleDurationRatio = 0.4f;
    constexpr float TutorialKeyPulseDuration = 0.18f;
    constexpr float TutorialKeyPulseScale = 0.24f;

}

void AnimatedTextSystem::CreateLine(
    Line& line,
    Renderer& renderer,
    const std::wstring& text,
    const DirectX::XMFLOAT3& centerPosition,
    float worldHeight,
    const DirectX::XMFLOAT4& color,
    float jumpHeight,
    float jumpDuration,
    float startOffset) const
{
    line.characters.clear();
    line.color = color;
    line.jumpHeight = jumpHeight;
    line.jumpDuration = jumpDuration;
    line.startOffset = startOffset;
    line.wordWaveInterval = BgmBarDuration * 0.25f;

    std::vector<float> widths;
    std::vector<bool> widthHasCharacter;
    widths.reserve(text.size());
    widthHasCharacter.reserve(text.size());
    float totalWidth = 0.0f;
    int currentWordIndex = 0;
    int currentIndexInWord = 0;

    int sourceIndex = 0;
    for (const wchar_t character : text)
    {
        if (character == L' ')
        {
            widths.push_back(worldHeight * AnimatedWordSpacing);
            widthHasCharacter.push_back(false);
            totalWidth += widths.back();
            ++currentWordIndex;
            currentIndexInWord = 0;
            ++sourceIndex;
            continue;
        }

        Character animatedCharacter;
        animatedCharacter.object = std::make_unique<GameObject>();
        auto& rendererComponent =
            animatedCharacter.object->AddComponent<WorldTextSpriteComponent>();
        const std::wstring characterText(1, character);
        if (!rendererComponent.Setup(
            renderer, OptionFontPath,
            characterText,
            worldHeight, color))
        {
            continue;
        }

        animatedCharacter.renderer = &rendererComponent;
        animatedCharacter.character = character;
        animatedCharacter.sourceIndex = sourceIndex++;
        animatedCharacter.animatable = true;
        animatedCharacter.wordIndex = currentWordIndex;
        animatedCharacter.indexInWord = currentIndexInWord++;
        const float width = rendererComponent.GetContentWorldWidth();
        widths.push_back(width);
        widthHasCharacter.push_back(true);
        totalWidth += width;
        line.characters.push_back(std::move(animatedCharacter));
    }

    float cursorX = centerPosition.x - totalWidth * 0.5f;
    std::size_t characterIndex = 0;
    for (std::size_t widthIndex = 0; widthIndex < widths.size(); ++widthIndex)
    {
        const float width = widths[widthIndex];
        if (widthHasCharacter[widthIndex] && characterIndex < line.characters.size())
        {
            auto& animatedCharacter = line.characters[characterIndex];
            animatedCharacter.basePosition = {
                cursorX + width * 0.5f -
                    animatedCharacter.renderer->GetContentCenterOffsetWorldX(),
                centerPosition.y,
                centerPosition.z
            };
            animatedCharacter.object->GetTransform().SetPosition(animatedCharacter.basePosition);
            ++characterIndex;
        }
        if (widthHasCharacter[widthIndex] && widthIndex + 1 < widths.size() &&
            widthHasCharacter[widthIndex + 1])
        {
            cursorX += worldHeight * AnimatedLetterTracking;
        }
        cursorX += width;
    }
}

void AnimatedTextSystem::UpdateLine(
    Line& line,
    float loopTime,
    float fadeAlpha,
    float deltaTime) const
{
    if (line.characters.empty()) return;

    bool animate = false;
    float stepTime = 0.0f;
    if (loopTime >= 0.0f)
    {
        animate = true;
        const float wrappedTime = std::fmod(
            loopTime + line.startOffset + BgmLoopDuration, BgmLoopDuration);
        stepTime = std::fmod(wrappedTime, BgmBarDuration);
    }

    for (auto& animatedCharacter : line.characters)
    {
        float jumpOffset = 0.0f;
        float pulseScale = 1.0f;
        if (animatedCharacter.reactiveVirtualKey != 0)
        {
            const bool keyDown = input_.IsDown(animatedCharacter.reactiveVirtualKey);
            if (keyDown && !animatedCharacter.previousReactiveKeyDown)
            {
                animatedCharacter.reactivePulseRemaining = TutorialKeyPulseDuration;
            }
            animatedCharacter.previousReactiveKeyDown = keyDown;
            if (animatedCharacter.reactiveScaleWhileHeld)
            {
                pulseScale = keyDown ? (1.0f + TutorialKeyPulseScale) : 1.0f;
            }
            else
            {
                animatedCharacter.reactivePulseRemaining = std::max(
                    0.0f, animatedCharacter.reactivePulseRemaining - deltaTime);
                if (animatedCharacter.reactivePulseRemaining > 0.0f)
                {
                    const float t = 1.0f -
                        animatedCharacter.reactivePulseRemaining / TutorialKeyPulseDuration;
                    pulseScale += std::sin(t * DirectX::XM_PI) * TutorialKeyPulseScale;
                }
            }
        }

        if (animatedCharacter.animatable)
        {
            const float characterStartTime =
                line.wordWaveInterval * static_cast<float>(animatedCharacter.wordIndex) +
                AnimatedLetterStaggerInterval * static_cast<float>(animatedCharacter.indexInWord);
            const float settleDuration =
                line.jumpDuration * AnimatedLetterSettleDurationRatio;
            if (animate &&
                stepTime >= characterStartTime &&
                stepTime < characterStartTime + line.jumpDuration + settleDuration)
            {
                const float elapsed = stepTime - characterStartTime;
                if (elapsed < line.jumpDuration)
                {
                    const float t = std::max(
                        0.0f,
                        std::min(elapsed / line.jumpDuration, 1.0f));
                    jumpOffset = std::sin(t * DirectX::XM_PI) * line.jumpHeight;
                }
                else
                {
                    const float t = std::max(
                        0.0f,
                        std::min((elapsed - line.jumpDuration) / settleDuration, 1.0f));
                    jumpOffset =
                        -std::sin(t * DirectX::XM_PI) *
                        (line.jumpHeight * AnimatedLetterSettleDepthRatio);
                }
            }
        }

        auto position = animatedCharacter.basePosition;
        if (animatedCharacter.reactiveUseGroupCenter && std::abs(pulseScale - 1.0f) > 0.0001f)
        {
            position.x = animatedCharacter.reactiveGroupCenterX +
                (animatedCharacter.basePosition.x - animatedCharacter.reactiveGroupCenterX) *
                pulseScale;
        }
        position.y += jumpOffset;
        animatedCharacter.object->GetTransform().SetPosition(position);
        animatedCharacter.object->GetTransform().SetScale({ pulseScale, pulseScale, 1.0f });
        if (animatedCharacter.renderer)
        {
            const DirectX::XMFLOAT4 baseColor = {
                line.color.x,
                line.color.y,
                line.color.z,
                line.color.w * fadeAlpha
            };
            animatedCharacter.renderer->SetColor(baseColor);
        }
        animatedCharacter.object->Update();
    }
}

void AnimatedTextSystem::DrawLine(const Line& line, Renderer& renderer) const
{

    for (const auto& animatedCharacter : line.characters)
    {
        if (animatedCharacter.object)
        {
            animatedCharacter.object->Draw(renderer);
        }
    }
}

void AnimatedTextSystem::BindRangeToKey(
    Line& line,
    int startSourceIndex,
    int endSourceIndex,
    int virtualKey,
    bool scaleWhileHeld,
    bool useGroupCenter) const
{
    float minX = 0.0f;
    float maxX = 0.0f;
    bool hasBounds = false;
    for (const auto& character : line.characters)
    {
        if (!character.renderer) continue;
        if (character.sourceIndex < startSourceIndex ||
            character.sourceIndex > endSourceIndex)
        {
            continue;
        }

        const float halfWidth = character.renderer->GetContentWorldWidth() * 0.5f;
        const float left = character.basePosition.x - halfWidth;
        const float right = character.basePosition.x + halfWidth;
        if (!hasBounds)
        {
            minX = left;
            maxX = right;
            hasBounds = true;
        }
        else
        {
            minX = std::min(minX, left);
            maxX = std::max(maxX, right);
        }
    }
    const float groupCenterX = hasBounds ? (minX + maxX) * 0.5f : 0.0f;

    for (auto& character : line.characters)
    {
        if (character.sourceIndex >= startSourceIndex &&
            character.sourceIndex <= endSourceIndex)
        {
            character.reactiveVirtualKey = virtualKey;
            character.reactiveScaleWhileHeld = scaleWhileHeld;
            character.reactiveUseGroupCenter = useGroupCenter;
            character.reactiveGroupCenterX = groupCenterX;
        }
    }
}

void AnimatedTextSystem::ShiftLineToSourceIndexX(
    Line& line,
    int sourceIndex,
    float targetX) const
{
    for (auto& character : line.characters)
    {
        if (character.sourceIndex != sourceIndex) continue;
        const float deltaX = targetX - character.basePosition.x;
        for (auto& shiftedCharacter : line.characters)
        {
            shiftedCharacter.basePosition.x += deltaX;
            if (shiftedCharacter.object)
            {
                shiftedCharacter.object->GetTransform().SetPosition(
                    shiftedCharacter.basePosition);
            }
        }
        break;
    }
}
