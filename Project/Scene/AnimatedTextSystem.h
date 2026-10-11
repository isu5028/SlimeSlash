#pragma once

#include "GameObject.h"

#include <memory>
#include <string>
#include <vector>

class Renderer;
class WorldTextSpriteComponent;
class InputManager;

class AnimatedTextSystem
{
public:
    explicit AnimatedTextSystem(InputManager& input) : input_(input) {}
    struct Character
    {
        std::unique_ptr<GameObject> object;
        WorldTextSpriteComponent* renderer = nullptr;
        DirectX::XMFLOAT3 basePosition = {};
        wchar_t character = L'\0';
        int sourceIndex = -1;
        int reactiveVirtualKey = 0;
        bool reactiveScaleWhileHeld = false;
        bool previousReactiveKeyDown = false;
        float reactivePulseRemaining = 0.0f;
        float reactiveGroupCenterX = 0.0f;
        bool reactiveUseGroupCenter = false;
        bool animatable = false;
        int wordIndex = 0;
        int indexInWord = 0;
    };

    struct Line
    {
        std::vector<Character> characters;
        DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float jumpHeight = 0.0f;
        float jumpDuration = 0.0f;
        float startOffset = 0.0f;
        float wordWaveInterval = 0.0f;
    };

    void CreateLine(
        Line& line,
        Renderer& renderer,
        const std::wstring& text,
        const DirectX::XMFLOAT3& centerPosition,
        float worldHeight,
        const DirectX::XMFLOAT4& color,
        float jumpHeight,
        float jumpDuration,
        float startOffset = 0.0f) const;
    void UpdateLine(
        Line& line,
        float loopTime,
        float fadeAlpha = 1.0f,
        float deltaTime = 0.0f) const;
    void DrawLine(const Line& line, Renderer& renderer) const;
    void BindRangeToKey(
        Line& line,
        int startSourceIndex,
        int endSourceIndex,
        int virtualKey,
        bool scaleWhileHeld = false,
        bool useGroupCenter = false) const;
    void ShiftLineToSourceIndexX(Line& line, int sourceIndex, float targetX) const;

private:
    InputManager& input_;
};
