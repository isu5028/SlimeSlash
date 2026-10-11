#include "SceneManager.h"

#include "AsepriteSpriteComponent.h"
#include "ColoredPlaneComponent.h"
#include "ImageSpriteComponent.h"
#include "PlayerControllerComponent.h"
#include "Renderer.h"
#include "SoundPlayer.h"
#include "WorldTextSpriteComponent.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    constexpr wchar_t PlayerJumpSoundPath[] = L"Project/Assets/Sounds/Jump.wav";
    constexpr float BgmBarDuration = 5.850f;
    constexpr char OptionFontPath[] = "Project/Assets/Fonts/Shikakufuto_Free_ver20251228.ttf";
    constexpr char OptionTriangleImagePath[] = "Assets/Images/png/triangle.png";
    constexpr char OptionSquareImagePath[] = "Assets/Images/png/square.png";
    constexpr char OptionCursorImagePath[] = "Assets/Images/png/cursor.aseprite";
    constexpr float OptionFadeDuration = 0.18f;
    constexpr float OptionMenuItemTextHeight = 0.62f;
    constexpr float OptionMenuVerticalOffset = -0.7f;
    constexpr float OptionVolumeLabelX = -1.35f;
    constexpr float OptionVolumeLabelYOffset = 0.12f;
    constexpr float OptionVolumeIconsCenterX = 1.35f;
    constexpr float OptionIconSpacing = 0.56f;
    constexpr float OptionVolumeRowY[] = { 1.25f, 0.35f, -0.55f };
    constexpr float OptionArrowOffset = 1.85f;
    constexpr float OptionTrianglePulseDistance = 0.1f;
    constexpr float OptionTrianglePulseDecay = 8.0f;
    constexpr float OptionCursorWorldHeight = 0.72f;
    constexpr float OptionCursorBaseX = -4.30f;
    constexpr float OptionCursorMoveEaseSpeed = 14.0f;
    constexpr float OptionIconWorldHeight = 0.92f;
    constexpr float OptionOverlayCameraDistance = 15.0f;
    constexpr float OptionDimmerStrength = 0.48f;
    constexpr float TitleLogoY = 1.55f;
    constexpr float TitleStartY = 0.02f;
    constexpr float TitleOptionY = -0.70f;
    constexpr float TitleQuitY = -1.42f;
    constexpr float TitleCursorBaseX = -1.95f;
    constexpr float TitleCursorWorldHeight = 0.72f;
    constexpr float TitleMenuBeatOffset = BgmBarDuration / 4.0f;
    constexpr float AnimatedLetterJumpHeight = 0.11f;
    constexpr float AnimatedLetterJumpDuration = 0.42f;
    constexpr float TutorialHintTextHeight = 0.58f;
    constexpr float TutorialHintSecondaryY = 1.50f;
    constexpr float TutorialHintPrimaryY = 2.20f;
    constexpr float TutorialMoveHintY = 2.58f;
    constexpr float TutorialAttackHintY = 1.82f;
    constexpr float TutorialHintFadeSpeed = 4.8f;
    constexpr float ContinuePromptY = TutorialHintPrimaryY;
    constexpr float ContinueChoiceYesY = -0.97f;
    constexpr float ContinueChoiceNoY = -1.77f;
    constexpr float ContinueCursorBaseX = -1.55f;
    constexpr float ContinueCursorWorldHeight = 0.72f;
    constexpr float ClearTitleY = ContinuePromptY + 0.48f;
    constexpr float ClearSummaryY = ContinuePromptY - 0.22f;
    constexpr float ClearBackToTitleY = (ContinueChoiceYesY + ContinueChoiceNoY) * 0.5f;
    constexpr float OptionCursorBlinkDelay = 2.0f;
    constexpr float OptionCursorBlinkPeriod = BgmBarDuration / 4.0f;
    constexpr float ContinueCursorBlinkDelay = 2.0f;
    constexpr float ContinueCursorBlinkPeriod = BgmBarDuration / 4.0f;
    constexpr float ClearBackToTitleAppearDelay = 2.0f;
    constexpr float ClearBackToTitleInputDelay = 0.5f;

    int VolumeToStep(float volume)
    {
        return static_cast<int>(std::round(std::max(0.0f, std::min(volume, 1.0f)) * 5.0f));
    }

    float StepToVolume(int step)
    {
        return std::max(0, std::min(step, 5)) / 5.0f;
    }
}

void SceneManager::InitializeTitleScreen(Renderer& renderer)
{
    animatedTextSystem_.CreateLine(
        titleLogoAnimatedText_,
        renderer,
        L"SLIME SLASH SLAYER",
        { 0.0f, TitleLogoY, 0.0f },
        0.9f,
        { 1.0f, 1.0f, 1.0f, 1.0f },
        AnimatedLetterJumpHeight,
        AnimatedLetterJumpDuration);
    animatedTextSystem_.CreateLine(
        titleStartAnimatedText_,
        renderer,
        L"START",
        { 0.0f, TitleStartY, 0.0f },
        0.62f,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        AnimatedLetterJumpHeight * 0.82f,
        AnimatedLetterJumpDuration,
        BgmBarDuration * 0.5f);
    animatedTextSystem_.CreateLine(
        titleOptionAnimatedText_,
        renderer,
        L"OPTION",
        { 0.0f, TitleOptionY, 0.0f },
        0.62f,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        AnimatedLetterJumpHeight * 0.82f,
        AnimatedLetterJumpDuration,
        BgmBarDuration * 0.5f + TitleMenuBeatOffset * 3);
    animatedTextSystem_.CreateLine(
        titleQuitAnimatedText_,
        renderer,
        L"QUIT",
        { 0.0f, TitleQuitY, 0.0f },
        0.62f,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        AnimatedLetterJumpHeight * 0.82f,
        AnimatedLetterJumpDuration,
        0.0f);

    titleCursor_.Initialize(
        renderer, OptionCursorImagePath, TitleCursorWorldHeight,
        { TitleCursorBaseX, TitleStartY, 0.0f },
        { 0.86f, 0.76f, 0.48f, 1.0f }, 1.0f);
    titleCursorCurrentY_ = TitleStartY;
    RefreshTitleMenuText();
}

void SceneManager::InitializeContinuePrompt(Renderer& renderer)
{
    animatedTextSystem_.CreateLine(
        continuePromptAnimatedText_,
        renderer,
        L"CONTINUE?",
        { 0.0f, ContinuePromptY, 0.0f },
        TutorialHintTextHeight * 0.95f,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        0.0f,
        AnimatedLetterJumpDuration);
    animatedTextSystem_.CreateLine(
        continueYesAnimatedText_,
        renderer,
        L"YES",
        { 0.0f, ContinueChoiceYesY, 0.0f },
        0.68f,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        0.0f,
        AnimatedLetterJumpDuration);
    animatedTextSystem_.CreateLine(
        continueNoAnimatedText_,
        renderer,
        L"NO",
        { 0.0f, ContinueChoiceNoY, 0.0f },
        0.68f,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        0.0f,
        AnimatedLetterJumpDuration);
    animatedTextSystem_.CreateLine(
        clearTitleAnimatedText_,
        renderer,
        L"SLIMES SLAIN!",
        { 0.0f, ClearTitleY, 0.0f },
        TutorialHintTextHeight * 1.22f,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        0.0f,
        AnimatedLetterJumpDuration);
    animatedTextSystem_.CreateLine(
        clearBackToTitleAnimatedText_,
        renderer,
        L"ESC TO TITLE",
        { 0.0f, ClearBackToTitleY, 0.0f },
        0.64f,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        0.0f,
        AnimatedLetterJumpDuration);

    continueCursor_.Initialize(
        renderer, OptionCursorImagePath, ContinueCursorWorldHeight,
        { ContinueCursorBaseX, ContinueChoiceYesY, 0.0f },
        { 0.86f, 0.76f, 0.48f, 1.0f }, 0.0f);
}

void SceneManager::InitializeTutorialText(Renderer& renderer)
{
    animatedTextSystem_.CreateLine(
        tutorialMoveHintAnimatedText_,
        renderer,
        L"WASD : MOVE",
        { 0.0f, TutorialMoveHintY, 0.0f },
        TutorialHintTextHeight,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        0.0f,
        AnimatedLetterJumpDuration);
    animatedTextSystem_.BindRangeToKey(tutorialMoveHintAnimatedText_, 0, 0, 'W', true);
    animatedTextSystem_.BindRangeToKey(tutorialMoveHintAnimatedText_, 1, 1, 'A', true);
    animatedTextSystem_.BindRangeToKey(tutorialMoveHintAnimatedText_, 2, 2, 'S', true);
    animatedTextSystem_.BindRangeToKey(tutorialMoveHintAnimatedText_, 3, 3, 'D', true);
    animatedTextSystem_.ShiftLineToSourceIndexX(tutorialMoveHintAnimatedText_, 5, 0.0f);

    animatedTextSystem_.CreateLine(
        tutorialAttackHintAnimatedText_,
        renderer,
        L"J : ATTACK",
        { 0.0f, TutorialAttackHintY, 0.0f },
        TutorialHintTextHeight * 0.95f,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        0.0f,
        AnimatedLetterJumpDuration);
    animatedTextSystem_.BindRangeToKey(tutorialAttackHintAnimatedText_, 0, 0, 'J');
    animatedTextSystem_.ShiftLineToSourceIndexX(tutorialAttackHintAnimatedText_, 2, 0.0f);

    animatedTextSystem_.CreateLine(
        tutorialDodgeHintAnimatedText_,
        renderer,
        L"K : DODGE",
        { 0.0f, TutorialHintPrimaryY, 0.0f },
        TutorialHintTextHeight,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        0.0f,
        AnimatedLetterJumpDuration);
    animatedTextSystem_.BindRangeToKey(tutorialDodgeHintAnimatedText_, 0, 0, 'K');

    animatedTextSystem_.CreateLine(
        tutorialJumpHintAnimatedText_,
        renderer,
        L"SPACE : JUMP",
        { 0.0f, TutorialHintPrimaryY, 0.0f },
        TutorialHintTextHeight,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        0.0f,
        AnimatedLetterJumpDuration);
    animatedTextSystem_.BindRangeToKey(
        tutorialJumpHintAnimatedText_, 0, 4, VK_SPACE, false, true);
}

void SceneManager::UpdateTitleScreen(float deltaTime)
{
    const float animationTime = bgmPlayback_.GetAnimationTime();
    if (sceneState_ == SceneState::Title && titleFadeAlpha_ > 0.0f)
    {
        titleFadeAlpha_ = std::max(0.0f, titleFadeAlpha_ - deltaTime / OptionFadeDuration);
    }

    const float titleAlpha = 1.0f - titleFadeAlpha_;
    animatedTextSystem_.UpdateLine(titleLogoAnimatedText_, animationTime, titleAlpha, deltaTime);
    animatedTextSystem_.UpdateLine(titleStartAnimatedText_, animationTime, titleAlpha, deltaTime);
    animatedTextSystem_.UpdateLine(titleOptionAnimatedText_, animationTime, titleAlpha, deltaTime);
    animatedTextSystem_.UpdateLine(titleQuitAnimatedText_, animationTime, titleAlpha, deltaTime);
    titleCursorIdleTime_ += deltaTime;
    const float cursorTargetY = GetTitleCursorTargetY();
    const float cursorEase = 1.0f - std::exp(-OptionCursorMoveEaseSpeed * deltaTime);
    titleCursorCurrentY_ += (cursorTargetY - titleCursorCurrentY_) * cursorEase;
    RefreshTitleMenuText();
    UpdateTitleCursor();

    const auto& input = gameInput_.Menu();
    const bool upPressed = input.upPressed;
    const bool downPressed = input.downPressed;
    const bool confirmPressed = input.confirmPressed;
    if (sceneState_ == SceneState::Title && (upPressed || downPressed))
    {
        titleCursorIdleTime_ = 0.0f;
        if (upPressed)
        {
            titleSelection_ =
                titleSelection_ == TitleMenuItem::Start ?
                TitleMenuItem::Quit :
                titleSelection_ == TitleMenuItem::Option ?
                TitleMenuItem::Start :
                TitleMenuItem::Option;
        }
        else
        {
            titleSelection_ =
                titleSelection_ == TitleMenuItem::Start ?
                TitleMenuItem::Option :
                titleSelection_ == TitleMenuItem::Option ?
                TitleMenuItem::Quit :
                TitleMenuItem::Start;
        }
        RefreshTitleMenuText();
    }
    if (sceneState_ == SceneState::Title && confirmPressed)
    {
        switch (titleSelection_)
        {
        case TitleMenuItem::Start:
            sceneState_ = SceneState::TitleFadingOut;
            titleFadeAlpha_ = 0.0f;
            break;
        case TitleMenuItem::Option:
            OpenOptionMenu();
            break;
        case TitleMenuItem::Quit:
            PostQuitMessage(0);
            break;
        }
    }

    if (sceneState_ != SceneState::TitleFadingOut)
    {
        return;
    }

    titleFadeAlpha_ = std::min(1.0f, titleFadeAlpha_ + deltaTime / OptionFadeDuration);

    if (titleFadeAlpha_ >= 1.0f)
    {
        StartGameplay(true, 1);
    }
}

void SceneManager::InitializeOptionMenu(Renderer& renderer)
{
    optionDimmer_ = std::make_unique<GameObject>();
    optionDimmer_->GetTransform().SetPosition({ 0.0f, -20.0f, 2.0f });
    optionDimmerRenderer_ = &optionDimmer_->AddComponent<ImageSpriteComponent>();
    optionDimmerRenderer_->SetupSolidColor(renderer, 40.0f);
    optionDimmerRenderer_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });

    optionPanel_ = std::make_unique<GameObject>();
    optionPanel_->GetTransform().SetPosition({ 0.0f, 0.1f + OptionMenuVerticalOffset, 1.0f });
    optionPanel_->GetTransform().SetRotation({ DirectX::XM_PIDIV2, 0.0f, 0.0f });
    optionPanelRenderer_ = &optionPanel_->AddComponent<ColoredPlaneComponent>();
    optionPanelRenderer_->Setup(renderer, 10.2f, 8.6f, { 0.17f, 0.10f, 0.09f, 0.92f });
    optionPanelRenderer_->SetOpacity(0.0f);

    optionTitle_ = std::make_unique<GameObject>();
    optionTitle_->GetTransform().SetPosition({ 0.0f, 3.0f + OptionMenuVerticalOffset, 0.0f });
    optionTitleRenderer_ = &optionTitle_->AddComponent<WorldTextSpriteComponent>();
    optionTitleRenderer_->Setup(renderer, OptionFontPath, L"OPTION", 1.0f, { 1, 1, 1, 1 });
    optionTitleRenderer_->SetVisible(false);
    animatedTextSystem_.CreateLine(
        optionTitleAnimatedText_,
        renderer,
        L"OPTION",
        { 0.0f, 3.0f + OptionMenuVerticalOffset, 0.0f },
        1.0f,
        { 1.0f, 1.0f, 1.0f, 1.0f },
        AnimatedLetterJumpHeight,
        AnimatedLetterJumpDuration);

    optionCursor_.Initialize(
        renderer, OptionCursorImagePath, OptionCursorWorldHeight,
        { OptionCursorBaseX, OptionVolumeRowY[0] + OptionMenuVerticalOffset, 0.0f },
        { 0.67843139f, 0.41960785f, 0.41568628f, 1.0f }, 0.0f);

    optionMasterText_ = std::make_unique<GameObject>();
    optionMasterText_->GetTransform().SetPosition(
        { OptionVolumeLabelX, OptionVolumeRowY[0] + OptionMenuVerticalOffset + OptionVolumeLabelYOffset, 0.0f });
    optionMasterRenderer_ = &optionMasterText_->AddComponent<WorldTextSpriteComponent>();
    optionMasterRenderer_->SetHorizontalAlignment(
        WorldTextSpriteComponent::HorizontalAlignment::Right);

    optionBgmText_ = std::make_unique<GameObject>();
    optionBgmText_->GetTransform().SetPosition(
        { OptionVolumeLabelX, OptionVolumeRowY[1] + OptionMenuVerticalOffset + OptionVolumeLabelYOffset, 0.0f });
    optionBgmRenderer_ = &optionBgmText_->AddComponent<WorldTextSpriteComponent>();
    optionBgmRenderer_->SetHorizontalAlignment(
        WorldTextSpriteComponent::HorizontalAlignment::Right);

    optionSeText_ = std::make_unique<GameObject>();
    optionSeText_->GetTransform().SetPosition(
        { OptionVolumeLabelX, OptionVolumeRowY[2] + OptionMenuVerticalOffset + OptionVolumeLabelYOffset, 0.0f });
    optionSeRenderer_ = &optionSeText_->AddComponent<WorldTextSpriteComponent>();
    optionSeRenderer_->SetHorizontalAlignment(
        WorldTextSpriteComponent::HorizontalAlignment::Right);

    optionReturnText_ = std::make_unique<GameObject>();
    optionReturnText_->GetTransform().SetPosition({ 0.0f, -1.6f + OptionMenuVerticalOffset, 0.0f });
    optionReturnRenderer_ = &optionReturnText_->AddComponent<WorldTextSpriteComponent>();

    optionResumeText_ = std::make_unique<GameObject>();
    optionResumeText_->GetTransform().SetPosition({ 0.0f, -2.5f + OptionMenuVerticalOffset, 0.0f });
    optionResumeRenderer_ = &optionResumeText_->AddComponent<WorldTextSpriteComponent>();

    optionMasterRenderer_->Setup(
        renderer, OptionFontPath, L"MASTER", OptionMenuItemTextHeight,
        { 1.0f, 1.0f, 1.0f, 1.0f });
    optionBgmRenderer_->Setup(
        renderer, OptionFontPath, L"BGM", OptionMenuItemTextHeight,
        { 1.0f, 1.0f, 1.0f, 1.0f });
    optionSeRenderer_->Setup(
        renderer, OptionFontPath, L"SE", OptionMenuItemTextHeight,
        { 1.0f, 1.0f, 1.0f, 1.0f });
    optionReturnRenderer_->Setup(
        renderer, OptionFontPath, L"RETURN TO TITLE", OptionMenuItemTextHeight,
        { 1.0f, 1.0f, 1.0f, 1.0f });
    optionResumeRenderer_->Setup(
        renderer, OptionFontPath, L"RESUME", OptionMenuItemTextHeight,
        { 1.0f, 1.0f, 1.0f, 1.0f });

    for (int row = 0; row < 3; ++row)
    {
        optionLeftTriangles_[row] = std::make_unique<GameObject>();
        optionLeftTriangles_[row]->GetTransform().SetPosition(
            {
                OptionVolumeIconsCenterX - OptionArrowOffset,
                OptionVolumeRowY[row] + OptionMenuVerticalOffset,
                0.0f
            });
        auto& leftTriangle = optionLeftTriangles_[row]->AddComponent<ImageSpriteComponent>();
        leftTriangle.Setup(renderer, OptionTriangleImagePath, OptionIconWorldHeight);
        optionLeftTriangleRenderers_[row] = &leftTriangle;

        optionRightTriangles_[row] = std::make_unique<GameObject>();
        optionRightTriangles_[row]->GetTransform().SetPosition(
            {
                OptionVolumeIconsCenterX + OptionArrowOffset,
                OptionVolumeRowY[row] + OptionMenuVerticalOffset,
                0.0f
            });
        optionRightTriangles_[row]->GetTransform().SetRotation(
            { 0.0f, 0.0f, DirectX::XM_PI });
        auto& rightTriangle = optionRightTriangles_[row]->AddComponent<ImageSpriteComponent>();
        rightTriangle.Setup(renderer, OptionTriangleImagePath, OptionIconWorldHeight);
        optionRightTriangleRenderers_[row] = &rightTriangle;

        for (int column = 0; column < 5; ++column)
        {
            const int squareIndex = row * 5 + column;
            optionVolumeSquares_[squareIndex] = std::make_unique<GameObject>();
            optionVolumeSquares_[squareIndex]->GetTransform().SetPosition(
                {
                    OptionVolumeIconsCenterX + (column - 2) * OptionIconSpacing,
                    OptionVolumeRowY[row] + OptionMenuVerticalOffset,
                    0.0f
                });
            auto& square = optionVolumeSquares_[squareIndex]->AddComponent<ImageSpriteComponent>();
            square.Setup(renderer, OptionSquareImagePath, OptionIconWorldHeight);
            optionVolumeSquareRenderers_[squareIndex] = &square;
        }
    }

    RefreshOptionMenuText();
    UpdateOptionTrianglePositions();
}

void SceneManager::RefreshOptionMenuText()
{
    if (!pRenderer_ ||
        !optionMasterRenderer_ ||
        !optionBgmRenderer_ ||
        !optionSeRenderer_ ||
        !optionReturnRenderer_ ||
        !optionResumeRenderer_)
    {
        return;
    }

    const auto selectedColor = DirectX::XMFLOAT4{ 0.67843139f, 0.41960785f, 0.41568628f, optionFadeAlpha_ };
    const auto normalColor = DirectX::XMFLOAT4{ 0.92f, 0.88f, 0.84f, optionFadeAlpha_ };
    const auto inactiveColor = DirectX::XMFLOAT4{ 0.72f, 0.66f, 0.62f, optionFadeAlpha_ };
    const auto colorFor = [&](OptionItem item, bool inactive = false)
    {
        if (optionSelection_ == item) return selectedColor;
        return inactive ? inactiveColor : normalColor;
    };

    if (optionTitleRenderer_) optionTitleRenderer_->SetColor({ 1.0f, 1.0f, 1.0f, optionFadeAlpha_ });
    optionMasterRenderer_->SetColor(colorFor(OptionItem::MasterVolume));
    optionBgmRenderer_->SetColor(colorFor(OptionItem::BgmVolume));
    optionSeRenderer_->SetColor(colorFor(OptionItem::SeVolume));
    optionReturnRenderer_->SetColor(colorFor(OptionItem::ReturnToTitle));
    optionResumeRenderer_->SetColor(colorFor(OptionItem::Resume));

    const int volumeSteps[] = {
        VolumeToStep(SoundPlayer::GetMasterVolume()),
        VolumeToStep(SoundPlayer::GetBgmVolume()),
        VolumeToStep(SoundPlayer::GetSeVolume())
    };
    for (int row = 0; row < 3; ++row)
    {
        const bool selected =
            optionSelection_ == static_cast<OptionItem>(static_cast<int>(OptionItem::MasterVolume) + row);
        const DirectX::XMFLOAT4 selectedIconColor = {
            0.67843139f, 0.41960785f, 0.41568628f, optionFadeAlpha_
        };
        const DirectX::XMFLOAT4 normalIconColor = {
            0.92f, 0.88f, 0.84f, optionFadeAlpha_
        };
        const DirectX::XMFLOAT4 inactiveSquareColor = {
            0.45f, 0.38f, 0.35f, optionFadeAlpha_
        };

        if (optionLeftTriangleRenderers_[row])
        {
            optionLeftTriangleRenderers_[row]->SetColor(selected ? selectedIconColor : normalIconColor);
        }
        if (optionRightTriangleRenderers_[row])
        {
            optionRightTriangleRenderers_[row]->SetColor(selected ? selectedIconColor : normalIconColor);
        }

        for (int column = 0; column < 5; ++column)
        {
            auto* square = optionVolumeSquareRenderers_[row * 5 + column];
            if (!square) continue;
            square->SetColor(
                column < volumeSteps[row] ?
                (selected ? selectedIconColor : normalIconColor) :
                inactiveSquareColor);
        }
    }
}

void SceneManager::RefreshContinuePromptText()
{
    const float activeAlpha = continuePromptActive_ ? continuePromptAlpha_ : 0.0f;
    const DirectX::XMFLOAT4 normalColor = { 0.92f, 0.88f, 0.84f, activeAlpha };
    const DirectX::XMFLOAT4 selectedColor = { 0.86f, 0.76f, 0.48f, activeAlpha };
    continuePromptAnimatedText_.color = normalColor;
    continueYesAnimatedText_.color =
        continueSelection_ == ContinueChoice::Yes ? selectedColor : normalColor;
    continueNoAnimatedText_.color =
        continueSelection_ == ContinueChoice::No ? selectedColor : normalColor;

    const float clearAlpha = clearPromptActive_ ? clearPromptAlpha_ : 0.0f;
    const DirectX::XMFLOAT4 clearNormalColor = { 0.92f, 0.88f, 0.84f, clearAlpha };
    const DirectX::XMFLOAT4 clearActionColor = {
        0.92f, 0.88f, 0.84f, clearBackToTitleAlpha_
    };
    clearTitleAnimatedText_.color = clearNormalColor;
    clearSummaryAnimatedText_.color = clearNormalColor;
    clearBackToTitleAnimatedText_.color = clearActionColor;
}

void SceneManager::OpenClearPrompt()
{
    if (!pRenderer_) return;

    clearPromptActive_ = true;
    clearPromptAlpha_ = 0.0f;
    clearBackToTitleAlpha_ = 0.0f;
    clearBackToTitleDelayRemaining_ = ClearBackToTitleAppearDelay;
    clearBackToTitleInputDelayRemaining_ = ClearBackToTitleInputDelay;
    continueCursor_.SetOpacity(0.0f);

    clearSummaryAnimatedText_.characters.clear();
    const std::wstring summaryText =
        continueCountThisRun_ == 0 ?
        L"NO CONTINUE!" :
        (L"CONTINUE : " + std::to_wstring(continueCountThisRun_));
    animatedTextSystem_.CreateLine(
        clearSummaryAnimatedText_,
        *pRenderer_,
        summaryText,
        { 0.0f, ClearSummaryY, 0.0f },
        TutorialHintTextHeight * 0.62f,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        0.0f,
        AnimatedLetterJumpDuration);

    tutorialHint_ = TutorialHint::None;
    tutorialHintFadeAlpha_ = 0.0f;
    tutorialHintTargetAlpha_ = 0.0f;
    RefreshContinuePromptText();
    UpdateContinueCursor();
    gameInput_.SuppressConfirm();
}

void SceneManager::RefreshTitleMenuText()
{
    const float alpha = 1.0f - titleFadeAlpha_;
    const DirectX::XMFLOAT4 normalColor = { 0.92f, 0.88f, 0.84f, alpha };
    const DirectX::XMFLOAT4 selectedColor = { 0.86f, 0.76f, 0.48f, alpha };
    titleStartAnimatedText_.color =
        titleSelection_ == TitleMenuItem::Start ? selectedColor : normalColor;
    titleOptionAnimatedText_.color =
        titleSelection_ == TitleMenuItem::Option ? selectedColor : normalColor;
    titleQuitAnimatedText_.color =
        titleSelection_ == TitleMenuItem::Quit ? selectedColor : normalColor;
}

void SceneManager::UpdateOptionTrianglePositions()
{
    for (int row = 0; row < 3; ++row)
    {
        if (optionLeftTriangles_[row])
        {
            optionLeftTriangles_[row]->GetTransform().SetPosition(
                {
                    OptionVolumeIconsCenterX - OptionArrowOffset -
                        optionLeftTrianglePulse_[row] * OptionTrianglePulseDistance,
                    OptionVolumeRowY[row] + OptionMenuVerticalOffset,
                    0.0f
                });
        }
        if (optionRightTriangles_[row])
        {
            optionRightTriangles_[row]->GetTransform().SetPosition(
                {
                    OptionVolumeIconsCenterX + OptionArrowOffset +
                        optionRightTrianglePulse_[row] * OptionTrianglePulseDistance,
                    OptionVolumeRowY[row] + OptionMenuVerticalOffset,
                    0.0f
                });
        }
    }
}

float SceneManager::GetOptionCursorTargetY() const
{
    switch (optionSelection_)
    {
    case OptionItem::MasterVolume:
        return OptionVolumeRowY[0] + OptionMenuVerticalOffset;
    case OptionItem::BgmVolume:
        return OptionVolumeRowY[1] + OptionMenuVerticalOffset;
    case OptionItem::SeVolume:
        return OptionVolumeRowY[2] + OptionMenuVerticalOffset;
    case OptionItem::ReturnToTitle:
        return -1.6f + OptionMenuVerticalOffset;
    case OptionItem::Resume:
        return -2.5f + OptionMenuVerticalOffset;
    }

    return OptionVolumeRowY[0] + OptionMenuVerticalOffset;
}

float SceneManager::GetContinueCursorTargetY() const
{
    switch (continueSelection_)
    {
    case ContinueChoice::Yes:
        return ContinueChoiceYesY;
    case ContinueChoice::No:
        return ContinueChoiceNoY;
    }

    return ContinueChoiceYesY;
}

float SceneManager::GetTitleCursorTargetY() const
{
    switch (titleSelection_)
    {
    case TitleMenuItem::Start:
        return TitleStartY;
    case TitleMenuItem::Option:
        return TitleOptionY;
    case TitleMenuItem::Quit:
        return TitleQuitY;
    }

    return TitleStartY;
}

void SceneManager::UpdateOptionCursor()
{
    optionCursor_.Update(
        OptionCursorBaseX, optionCursorCurrentY_, optionFadeAlpha_,
        optionCursorIdleTime_, bgmPlayback_.GetLoopElapsed(),
        OptionCursorBlinkDelay, OptionCursorBlinkPeriod);
}

void SceneManager::UpdateContinueCursor()
{
    const bool overlayActive = continuePromptActive_;
    const float baseX = ContinueCursorBaseX;
    const float cursorOpacity = continuePromptActive_ ? continuePromptAlpha_ : 0.0f;
    continueCursor_.Update(
        baseX, continueCursorCurrentY_, cursorOpacity,
        overlayActive ? continueCursorIdleTime_ : 0.0f,
        bgmPlayback_.GetLoopElapsed(), ContinueCursorBlinkDelay, ContinueCursorBlinkPeriod);
}

void SceneManager::UpdateTitleCursor()
{
    titleCursor_.Update(
        TitleCursorBaseX, titleCursorCurrentY_, 1.0f - titleFadeAlpha_,
        titleCursorIdleTime_, bgmPlayback_.GetLoopElapsed(),
        ContinueCursorBlinkDelay, ContinueCursorBlinkPeriod);
}

void SceneManager::OpenContinuePrompt()
{
    continuePromptActive_ = true;
    continueSelection_ = ContinueChoice::Yes;
    continueCursorIdleTime_ = 0.0f;
    continueCursorCurrentY_ = GetContinueCursorTargetY();
    continuePromptAlpha_ = 0.0f;
    continueRestartWaveCount_ = waveEnemyCount_;
    continueRestartTutorial_ = tutorialActive_;
    continueRestartTutorialStep_ = TutorialStep::PlayerIntro;
    if (tutorialActive_)
    {
        switch (tutorialStep_)
        {
        case TutorialStep::Dodge:
        case TutorialStep::DodgeDelay:
        case TutorialStep::DodgeFadeOut:
            continueRestartTutorialStep_ = TutorialStep::Dodge;
            break;

        case TutorialStep::Jump:
        case TutorialStep::JumpDelay:
        case TutorialStep::JumpFadeOut:
            continueRestartTutorialStep_ = TutorialStep::Jump;
            break;

        case TutorialStep::MoveAttack:
        case TutorialStep::MoveAttackFadeOut:
        case TutorialStep::PlayerIntro:
        case TutorialStep::CompleteDelay:
        case TutorialStep::Inactive:
        default:
            continueRestartTutorialStep_ = TutorialStep::PlayerIntro;
            break;
        }
    }
    tutorialHint_ = TutorialHint::None;
    tutorialHintFadeAlpha_ = 0.0f;
    tutorialHintTargetAlpha_ = 0.0f;
    RefreshContinuePromptText();
    UpdateContinueCursor();
    input_.Commit();
}

void SceneManager::UpdateContinuePrompt(float deltaTime)
{
    if (!continuePromptActive_ && !clearPromptActive_)
    {
        continueCursor_.SetOpacity(0.0f);
        return;
    }

    if (continuePromptActive_ &&
        playerController_ && playerController_->IsDeathDissolving())
    {
        continuePromptAlpha_ = std::min(
            1.0f, continuePromptAlpha_ + deltaTime * TutorialHintFadeSpeed);
    }
    if (clearPromptActive_)
    {
        clearPromptAlpha_ = std::min(
            1.0f, clearPromptAlpha_ + deltaTime * TutorialHintFadeSpeed);
        if (clearBackToTitleDelayRemaining_ > 0.0f)
        {
            const float previousDelay = clearBackToTitleDelayRemaining_;
            clearBackToTitleDelayRemaining_ = std::max(
                0.0f, clearBackToTitleDelayRemaining_ - deltaTime);
            if (previousDelay > 0.0f && clearBackToTitleDelayRemaining_ <= 0.0f)
            {
                continueCursorIdleTime_ = 0.0f;
            }
        }
        else
        {
            const float previousAlpha = clearBackToTitleAlpha_;
            clearBackToTitleAlpha_ = std::min(
                1.0f, clearBackToTitleAlpha_ + deltaTime * TutorialHintFadeSpeed);
            if (previousAlpha < 1.0f && clearBackToTitleAlpha_ >= 1.0f)
            {
                clearBackToTitleInputDelayRemaining_ = ClearBackToTitleInputDelay;
            }
        }
        if (clearBackToTitleAlpha_ >= 1.0f && clearBackToTitleInputDelayRemaining_ > 0.0f)
        {
            clearBackToTitleInputDelayRemaining_ = std::max(
                0.0f, clearBackToTitleInputDelayRemaining_ - deltaTime);
        }
    }

    continueCursorIdleTime_ += deltaTime;
    const float cursorTargetY = GetContinueCursorTargetY();
    const float cursorEase = 1.0f - std::exp(-OptionCursorMoveEaseSpeed * deltaTime);
    continueCursorCurrentY_ +=
        (cursorTargetY - continueCursorCurrentY_) * cursorEase;

    const auto& input = gameInput_.Menu();
    const bool upPressed = input.upPressed;
    const bool downPressed = input.downPressed;
    const bool confirmPressed = input.confirmPressed;

    if (continuePromptActive_ && (upPressed || downPressed))
    {
        continueSelection_ =
            continueSelection_ == ContinueChoice::Yes ?
            ContinueChoice::No :
            ContinueChoice::Yes;
        continueCursorIdleTime_ = 0.0f;
    }

    RefreshContinuePromptText();
    animatedTextSystem_.UpdateLine(continuePromptAnimatedText_, -1.0f, 1.0f, deltaTime);
    animatedTextSystem_.UpdateLine(continueYesAnimatedText_, -1.0f, 1.0f, deltaTime);
    animatedTextSystem_.UpdateLine(continueNoAnimatedText_, -1.0f, 1.0f, deltaTime);
    animatedTextSystem_.UpdateLine(clearTitleAnimatedText_, -1.0f, 1.0f, deltaTime);
    animatedTextSystem_.UpdateLine(clearSummaryAnimatedText_, -1.0f, 1.0f, deltaTime);
    animatedTextSystem_.UpdateLine(clearBackToTitleAnimatedText_, -1.0f, 1.0f, deltaTime);
    UpdateContinueCursor();

    const float overlayAlpha =
        continuePromptActive_ ? continuePromptAlpha_ :
        clearPromptActive_ ? clearPromptAlpha_ :
        0.0f;
    if (clearPromptActive_)
    {
        if (overlayAlpha <= 0.0f || !input.escapePressed) return;
        if (clearBackToTitleAlpha_ < 1.0f || clearBackToTitleInputDelayRemaining_ > 0.0f)
        {
            return;
        }
        clearPromptActive_ = false;
        StartReturnToTitleTransition();
    }
    else
    {
        if (overlayAlpha <= 0.0f || !confirmPressed) return;
        continuePromptActive_ = false;
        if (continueSelection_ == ContinueChoice::Yes)
        {
            if (continueRestartTutorial_)
            {
                StartGameplay(true, 1, continueRestartTutorialStep_);
            }
            else
            {
                StartGameplay(false, continueRestartWaveCount_);
            }
            ++continueCountThisRun_;
        }
        else
        {
            ReturnToTitle();
        }
    }
}

void SceneManager::OpenOptionMenu()
{
    optionMenuState_ = OptionMenuState::FadingIn;
    optionFadeAlpha_ = 0.0f;
    optionCursorIdleTime_ = 0.0f;
    optionSelection_ = OptionItem::MasterVolume;
    optionCursorCurrentY_ = GetOptionCursorTargetY();
    UpdateOptionCursor();
    RefreshOptionMenuText();
}

void SceneManager::BeginCloseOptionMenu()
{
    if (!IsOptionMenuActive()) return;
    optionMenuState_ = OptionMenuState::FadingOut;
}

bool SceneManager::IsOptionMenuActive() const
{
    return optionMenuState_ != OptionMenuState::Closed;
}

void SceneManager::UpdateOptionMenu(float deltaTime)
{
    if (!IsOptionMenuActive())
    {
        optionCursor_.SetOpacity(0.0f);
        return;
    }
    optionCursorIdleTime_ += deltaTime;
    const float cursorTargetY = GetOptionCursorTargetY();
    const float cursorEase = 1.0f - std::exp(-OptionCursorMoveEaseSpeed * deltaTime);
    optionCursorCurrentY_ += (cursorTargetY - optionCursorCurrentY_) * cursorEase;

    if (optionMenuState_ == OptionMenuState::FadingIn)
    {
        optionFadeAlpha_ = std::min(1.0f, optionFadeAlpha_ + deltaTime / OptionFadeDuration);
        if (optionFadeAlpha_ >= 1.0f) optionMenuState_ = OptionMenuState::Open;
    }
    else if (optionMenuState_ == OptionMenuState::FadingOut)
    {
        optionFadeAlpha_ = std::max(0.0f, optionFadeAlpha_ - deltaTime / OptionFadeDuration);
        if (optionFadeAlpha_ <= 0.0f)
        {
            optionMenuState_ = OptionMenuState::Closed;
            if (pendingReturnToTitle_)
            {
                StartReturnToTitleTransition();
            }
        }
    }

    for (int row = 0; row < 3; ++row)
    {
        optionLeftTrianglePulse_[row] = std::max(
            0.0f, optionLeftTrianglePulse_[row] - deltaTime * OptionTrianglePulseDecay);
        optionRightTrianglePulse_[row] = std::max(
            0.0f, optionRightTrianglePulse_[row] - deltaTime * OptionTrianglePulseDecay);
    }
    UpdateOptionTrianglePositions();

    if (optionDimmerRenderer_)
    {
        optionDimmerRenderer_->SetColor(
            { 0.0f, 0.0f, 0.0f, optionFadeAlpha_ * OptionDimmerStrength });
    }
    if (optionPanelRenderer_)
    {
        optionPanelRenderer_->SetOpacity(optionFadeAlpha_);
    }
    animatedTextSystem_.UpdateLine(
        optionTitleAnimatedText_,
        bgmPlayback_.GetAnimationTime(),
        optionFadeAlpha_,
        deltaTime);
    UpdateOptionCursor();
    RefreshOptionMenuText();

    if (optionMenuState_ != OptionMenuState::Open)
    {
        return;
    }

    const auto& input = gameInput_.Menu();
    const bool upPressed = input.upPressed;
    const bool downPressed = input.downPressed;
    const bool leftPressed = input.leftPressed;
    const bool rightPressed = input.rightPressed;
    const bool escPressed = input.escapePressed;
    const bool confirmPressed = input.confirmPressed;

    bool refreshText = false;
    int selectionIndex = static_cast<int>(optionSelection_);
    const int firstOptionIndex = static_cast<int>(OptionItem::MasterVolume);
    const int lastOptionIndex = static_cast<int>(OptionItem::Resume);
    if (upPressed)
    {
        selectionIndex =
            (selectionIndex <= firstOptionIndex) ? lastOptionIndex : (selectionIndex - 1);
        optionSelection_ = static_cast<OptionItem>(selectionIndex);
        optionCursorIdleTime_ = 0.0f;
        refreshText = true;
    }
    else if (downPressed)
    {
        selectionIndex =
            (selectionIndex >= lastOptionIndex) ? firstOptionIndex : (selectionIndex + 1);
        optionSelection_ = static_cast<OptionItem>(selectionIndex);
        optionCursorIdleTime_ = 0.0f;
        refreshText = true;
    }

    auto adjustVolume = [&](float currentVolume, bool increase)
    {
        int step = VolumeToStep(currentVolume);
        step += increase ? 1 : -1;
        return StepToVolume(step);
    };

    if (leftPressed || rightPressed)
    {
        optionCursorIdleTime_ = 0.0f;
        const bool increase = rightPressed;
        const int selectedRow = static_cast<int>(optionSelection_);
        switch (optionSelection_)
        {
        case OptionItem::MasterVolume:
            SoundPlayer::SetMasterVolume(
                adjustVolume(SoundPlayer::GetMasterVolume(), increase));
            if (increase) optionRightTrianglePulse_[selectedRow] = 1.0f;
            else optionLeftTrianglePulse_[selectedRow] = 1.0f;
            refreshText = true;
            break;
        case OptionItem::BgmVolume:
            SoundPlayer::SetBgmVolume(
                adjustVolume(SoundPlayer::GetBgmVolume(), increase));
            if (increase) optionRightTrianglePulse_[selectedRow] = 1.0f;
            else optionLeftTrianglePulse_[selectedRow] = 1.0f;
            refreshText = true;
            break;
        case OptionItem::SeVolume:
            SoundPlayer::SetSeVolume(
                adjustVolume(SoundPlayer::GetSeVolume(), increase));
            SoundPlayer::Play(PlayerJumpSoundPath);
            if (increase) optionRightTrianglePulse_[selectedRow] = 1.0f;
            else optionLeftTrianglePulse_[selectedRow] = 1.0f;
            refreshText = true;
            break;
        default:
            break;
        }
    }

    if (escPressed ||
        (confirmPressed && optionSelection_ == OptionItem::Resume))
    {
        pendingReturnToTitle_ = false;
        BeginCloseOptionMenu();
    }
    else if (confirmPressed && optionSelection_ == OptionItem::ReturnToTitle)
    {
        BeginReturnToTitle();
    }

    if (refreshText)
    {
        RefreshOptionMenuText();
    }
}

void SceneManager::DrawOptionMenu()
{
    if (!IsOptionMenuActive() || optionFadeAlpha_ <= 0.0f || !pRenderer_)
    {
        return;
    }

    const auto overlayView = DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(0.0f, 0.0f, -OptionOverlayCameraDistance, 1.0f),
        DirectX::XMVectorZero(),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    pRenderer_->SetupViewTransform(overlayView);

    if (optionDimmer_) optionDimmer_->Draw(*pRenderer_);
    if (optionPanel_) optionPanel_->Draw(*pRenderer_);
    animatedTextSystem_.DrawLine(optionTitleAnimatedText_, *pRenderer_);
    optionCursor_.Draw(*pRenderer_);
    for (auto& triangle : optionLeftTriangles_)
    {
        if (triangle) triangle->Draw(*pRenderer_);
    }
    for (auto& square : optionVolumeSquares_)
    {
        if (square) square->Draw(*pRenderer_);
    }
    for (auto& triangle : optionRightTriangles_)
    {
        if (triangle) triangle->Draw(*pRenderer_);
    }
    if (optionMasterText_) optionMasterText_->Draw(*pRenderer_);
    if (optionBgmText_) optionBgmText_->Draw(*pRenderer_);
    if (optionSeText_) optionSeText_->Draw(*pRenderer_);
    if (optionReturnText_) optionReturnText_->Draw(*pRenderer_);
    if (optionResumeText_) optionResumeText_->Draw(*pRenderer_);
}

void SceneManager::DrawContinuePrompt()
{
    if (!continuePromptActive_ || !pRenderer_) return;

    animatedTextSystem_.DrawLine(continuePromptAnimatedText_, *pRenderer_);
    animatedTextSystem_.DrawLine(continueYesAnimatedText_, *pRenderer_);
    animatedTextSystem_.DrawLine(continueNoAnimatedText_, *pRenderer_);
    continueCursor_.Draw(*pRenderer_);
}

void SceneManager::DrawClearPrompt()
{
    if (!clearPromptActive_ || !pRenderer_) return;

    animatedTextSystem_.DrawLine(clearTitleAnimatedText_, *pRenderer_);
    animatedTextSystem_.DrawLine(clearSummaryAnimatedText_, *pRenderer_);
    animatedTextSystem_.DrawLine(clearBackToTitleAnimatedText_, *pRenderer_);
}

void SceneManager::DrawTitleScreen()
{
    if ((sceneState_ != SceneState::Title &&
        sceneState_ != SceneState::TitleFadingOut) || !pRenderer_)
    {
        return;
    }

    const auto overlayView = DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(0.0f, 0.0f, -OptionOverlayCameraDistance, 1.0f),
        DirectX::XMVectorZero(),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    pRenderer_->SetupViewTransform(overlayView);

    animatedTextSystem_.DrawLine(titleLogoAnimatedText_, *pRenderer_);
    animatedTextSystem_.DrawLine(titleStartAnimatedText_, *pRenderer_);
    animatedTextSystem_.DrawLine(titleOptionAnimatedText_, *pRenderer_);
    animatedTextSystem_.DrawLine(titleQuitAnimatedText_, *pRenderer_);
    titleCursor_.Draw(*pRenderer_);
}
