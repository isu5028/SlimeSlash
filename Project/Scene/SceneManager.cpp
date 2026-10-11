#include "SceneManager.h"

#include "ColoredBoxComponent.h"
#include "PlayerControllerComponent.h"
#include "Renderer.h"
#include "SoundPlayer.h"
#include "UnitHealthComponent.h"
#include "WorldTextSpriteComponent.h"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace
{
    constexpr float BgmStartDelay = 2.0f;

}

SceneManager::SceneManager()
    : hazardSystem_(enemyTargetRegistry_)
    , gameInput_(input_)
    , animatedTextSystem_(input_)
{
}

SceneManager::~SceneManager()
{
}

void SceneManager::Initialize(Renderer& renderer)
{
    pRenderer_ = &renderer;
    bgmPlayback_.Reset(BgmStartDelay);
    sceneState_ = SceneState::Title;
    waveEnemyCount_ = 1;
    optionMenuState_ = OptionMenuState::Closed;
    optionFadeAlpha_ = 0.0f;
    continuePromptActive_ = false;
    continueSelection_ = ContinueChoice::Yes;
    titleSelection_ = TitleMenuItem::Start;
    continueCursorIdleTime_ = 0.0f;
    continuePromptAlpha_ = 0.0f;
    clearPromptAlpha_ = 0.0f;
    clearBackToTitleAlpha_ = 0.0f;
    titleCursorIdleTime_ = 0.0f;
    pendingWaveSpawnAfterIntro_ = false;
    clearPromptActive_ = false;
    continueRestartTutorial_ = false;
    continueCountThisRun_ = 0;
    continueRestartWaveCount_ = 1;
    clearBackToTitleDelayRemaining_ = 0.0f;
    clearBackToTitleInputDelayRemaining_ = 0.0f;
    titleFadeAlpha_ = 0.0f;
    gameplayFadeOutAlpha_ = 0.0f;
    gameplayFadeOutHoldRemaining_ = 0.0f;
    pendingReturnToTitle_ = false;
    camera_.SetEyePosition({ 0.0f, 8.0f, -20.0f });
    camera_.SetFocusPosition({ 0.0f, -0.3f, -1.0f });

    ground_ = std::make_unique<GameObject>();
    ground_->GetTransform().SetPosition({ 0.0f, -1.0f, 0.0f });
    ground_->AddComponent<ColoredBoxComponent>().Setup(
        renderer, 10.0f, 1.0f, 10.0f,
        // Keep the arena in the same rose palette as the animated background,
        // with a little transparency so its surface does not feel detached.
        { 0.60f, 0.36f, 0.42f, 0.90f });

    stageTitle_ = std::make_unique<GameObject>();
    stageTitle_->GetTransform().SetPosition({ 0.0f, -1.85f, -5.02f });
    stageTitle_->AddComponent<WorldTextSpriteComponent>().Setup(
        renderer,
        "Project/Assets/Fonts/Shikakufuto_Free_ver20251228.ttf",
        L"",
        0.7f,
        { 1.0f, 1.0f, 1.0f, 1.0f });

    InitializeOptionMenu(renderer);
    InitializeTitleScreen(renderer);
    InitializeTutorialText(renderer);
    InitializeContinuePrompt(renderer);
}

void SceneManager::Update()
{
    // 1フレーム分の入力を確定してから、現在のSceneStateに応じた更新へ進む
    const auto now = std::chrono::steady_clock::now();
    const float deltaTime = std::min(
        std::chrono::duration<float>(now - previousUpdate_).count(), 0.05f);
    previousUpdate_ = now;

    input_.Sample();
    gameInput_.Update();
    const auto& menuInput = gameInput_.Menu();
    const bool escPressed = menuInput.escapePressed;
    if ((sceneState_ == SceneState::Gameplay || sceneState_ == SceneState::Title) &&
        escPressed && !IsOptionMenuActive() &&
        !continuePromptActive_ && !clearPromptActive_)
    {
        OpenOptionMenu();
    }

    UpdateOptionMenu(deltaTime);

    bgmPlayback_.Update(deltaTime);

    if (IsOptionMenuActive())
    {
        if (sceneState_ == SceneState::GameplayFadingToTitle)
        {
            UpdateGameplayFadeOut(deltaTime);
        }
        input_.Commit();
        return;
    }

    if (sceneState_ == SceneState::Title ||
        sceneState_ == SceneState::TitleFadingOut)
    {
        if (ground_) ground_->Update();
        if (stageTitle_) stageTitle_->Update();
        UpdateTitleScreen(deltaTime);
        input_.Commit();
        return;
    }

    if (sceneState_ == SceneState::GameplayFadingToTitle)
    {
        if (ground_) ground_->Update();
        if (stageTitle_) stageTitle_->Update();
        UpdateGameplayFadeOut(deltaTime);
        input_.Commit();
        return;
    }

    camera_.Update();
    // Gameplayではプレイヤーと危険物を更新した後、危険物の接触判定を行う
    // その後に敵やエフェクトを更新して、各Objectの描画状態を整える
    if (cube_) cube_->Update();
    if (ground_) ground_->Update();
    if (stageTitle_) stageTitle_->Update();
    if (shadow_) shadow_->Update();
    if (player_) player_->Update();

    hazardSystem_.Update(*pRenderer_, deltaTime);
    if (player_ && playerController_ && playerHitbox_)
    {
        hazardSystem_.TryHitPlayer(*playerController_, *player_, *playerHitbox_);
    }

    for (auto& enemy : enemies_)
    {
        enemy.shadow->Update();
        if (enemy.telegraphArea) enemy.telegraphArea->Update();
        if (enemy.radialTelegraphArea) enemy.radialTelegraphArea->Update();
        enemy.object->Update();
        enemy.healthBar->Update();
    }
    if (attackEffect_) attackEffect_->Update();
    if (impactParticles_) impactParticles_->Update();
    if (playerHealthBar_) playerHealthBar_->Update();

    if (!continuePromptActive_ && !clearPromptActive_ &&
        sceneState_ == SceneState::Gameplay &&
        playerHealth_ && !playerHealth_->IsAlive())
    {
        OpenContinuePrompt();
    }

    UpdateTutorial(deltaTime);

    if (pendingWaveSpawnAfterIntro_ &&
        playerController_ && !playerController_->IsStageIntroActive())
    {
        playerController_->SetInputEnabled(true);
        CreateWave(waveEnemyCount_);
        pendingWaveSpawnAfterIntro_ = false;
    }

    if (!continuePromptActive_ && !clearPromptActive_ && !tutorialActive_)
    {
        // 各敵Controllerの死亡状態を集約して、Wave全体のクリアを判定する
        // 敵個体のAIや死亡演出はController側、次のWaveへの遷移はSceneManager側の責務
        const bool waveDefeated = !enemies_.empty() && std::all_of(
            enemies_.begin(), enemies_.end(), [](const EnemyInstance& enemy)
            {
                return enemy.controller && enemy.controller->IsDefeated();
            });
        if (waveDefeated)
        {
            if (waveEnemyCount_ >= 10)
            {
                OpenClearPrompt();
            }
            else
            {
                ++waveEnemyCount_;
                CreateWave(waveEnemyCount_);
            }
        }
    }

    UpdateContinuePrompt(deltaTime);

    input_.Commit();
}

void SceneManager::Draw()
{
    auto viewMatrix = camera_.GetViewMatrix();
    pRenderer_->SetupViewTransform(viewMatrix);

    if (cube_) cube_->Draw(*pRenderer_);
    if (ground_) ground_->Draw(*pRenderer_);
    if (stageTitle_) stageTitle_->Draw(*pRenderer_);
    DrawClearPrompt();
    if (shadow_) shadow_->Draw(*pRenderer_);
    for (auto& enemy : enemies_)
    {
        enemy.shadow->Draw(*pRenderer_);
        if (enemy.telegraphArea) enemy.telegraphArea->Draw(*pRenderer_);
        if (enemy.radialTelegraphArea) enemy.radialTelegraphArea->Draw(*pRenderer_);
    }
    hazardSystem_.Draw(*pRenderer_);

    const bool shouldDrawTutorialHints =
        (sceneState_ == SceneState::Gameplay ||
            sceneState_ == SceneState::GameplayFadingToTitle) &&
        !continuePromptActive_ && !clearPromptActive_ &&
        (tutorialHint_ != TutorialHint::None || tutorialHintFadeAlpha_ > 0.0f);
    if (shouldDrawTutorialHints)
    {
        animatedTextSystem_.DrawLine(tutorialMoveHintAnimatedText_, *pRenderer_);
        animatedTextSystem_.DrawLine(tutorialAttackHintAnimatedText_, *pRenderer_);
        animatedTextSystem_.DrawLine(tutorialDodgeHintAnimatedText_, *pRenderer_);
        animatedTextSystem_.DrawLine(tutorialJumpHintAnimatedText_, *pRenderer_);
    }
    if (attackEffect_) attackEffect_->Draw(*pRenderer_);

    std::vector<GameObject*> units;
    units.reserve(enemies_.size() + 8);
    units.push_back(player_.get());
    for (auto& enemy : enemies_) units.push_back(enemy.object.get());
    hazardSystem_.AppendUnitObjects(units);
    std::sort(units.begin(), units.end(), [&viewMatrix](GameObject* left, GameObject* right)
    {
        if (!left) return false;
        if (!right) return true;

        const auto leftPosition = DirectX::XMLoadFloat3(&left->GetTransform().GetPosition());
        const auto rightPosition = DirectX::XMLoadFloat3(&right->GetTransform().GetPosition());
        const float leftDepth = DirectX::XMVectorGetZ(
            DirectX::XMVector3TransformCoord(leftPosition, viewMatrix));
        const float rightDepth = DirectX::XMVectorGetZ(
            DirectX::XMVector3TransformCoord(rightPosition, viewMatrix));
        return leftDepth > rightDepth;
    });

    for (auto* unit : units)
    {
        if (unit) unit->Draw(*pRenderer_);
    }
    if (impactParticles_) impactParticles_->Draw(*pRenderer_);
    if (playerHealthBar_) playerHealthBar_->Draw(*pRenderer_);
    for (auto& enemy : enemies_) enemy.healthBar->Draw(*pRenderer_);
    DrawContinuePrompt();
    DrawTitleScreen();
    DrawOptionMenu();
}
