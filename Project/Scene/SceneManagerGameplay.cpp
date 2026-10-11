#include "SceneManager.h"

#include "AsepriteSpriteComponent.h"
#include "ChargeParticleComponent.h"
#include "ColoredDiskComponent.h"
#include "ColoredPlaneComponent.h"
#include "HealthBarComponent.h"
#include "ImageSpriteComponent.h"
#include "ImpactParticleComponent.h"
#include "PlayerControllerComponent.h"
#include "SlashAttackEffectComponent.h"
#include "Slime2ControllerComponent.h"
#include "Slime3ControllerComponent.h"
#include "Slime4ControllerComponent.h"
#include "Slime5ControllerComponent.h"
#include "SlimeControllerComponent.h"
#include "SoundPlayer.h"
#include "UnitHealthComponent.h"
#include "UnitHitboxComponent.h"
#include "UnitVisualComponent.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <string>

namespace
{
    constexpr float EnemyShadowWidth = 1.8f;
    constexpr float EnemyShadowDepth = 1.2f;
    constexpr float Slime5ShadowOffsetZ = 0.6f;
    constexpr wchar_t PlayerJumpSoundPath[] = L"Project/Assets/Sounds/Jump.wav";
    constexpr float TitleStartY = 0.02f;
    constexpr DirectX::XMFLOAT3 TutorialPlayerIntroStart = { 0.0f, -1.25f, -8.0f };
    constexpr DirectX::XMFLOAT3 TutorialPlayerIntroLanding = { 0.0f, -1.25f, -1.0f };
    constexpr float TutorialPlayerIntroDuration = 0.92f;
    constexpr float TutorialPlayerIntroArcHeight = 4.0f;
    constexpr float GameplayFadeOutDuration = 0.28f;
    constexpr float GameplayFadeOutHoldDuration = 0.5f;
    constexpr float TutorialHintFadeSpeed = 4.8f;
    constexpr float TutorialAdvanceDelay = 0.20f;
}

void SceneManager::CreateWave(int waveNumber)
{
    // Wave番号から敵の種類と数を決める
    // 生成されたEnemyInstanceはSceneManager::enemies_が所有し、各敵Controllerは個体のAIだけを担当する
    std::vector<EnemyType> enemyTypes;
    switch (waveNumber)
    {
    case 1: enemyTypes = { EnemyType::Slime }; break;
    case 2: enemyTypes = { EnemyType::Slime2 }; break;
    case 3: enemyTypes = { EnemyType::Slime3 }; break;
    case 4: enemyTypes = { EnemyType::Slime, EnemyType::Slime3 }; break;
    case 5:
        enemyTypes = { EnemyType::Slime2, EnemyType::Slime2, EnemyType::Slime2 };
        break;
    case 6:
        enemyTypes.assign(4, EnemyType::Slime4);
        break;
    case 7: enemyTypes.assign(8, EnemyType::Slime); break;
    case 8: enemyTypes.assign(8, EnemyType::Slime2); break;
    case 9:
        enemyTypes = {
            EnemyType::Slime, EnemyType::Slime2,
            EnemyType::Slime3, EnemyType::Slime4,
            EnemyType::Slime, EnemyType::Slime2,
            EnemyType::Slime3, EnemyType::Slime4 };
        break;
    case 10: enemyTypes = { EnemyType::Slime5 }; break;
    default: return;
    }

    ClearEnemies();
    enemies_.reserve(enemyTypes.size());
    enemyTargetRegistry_.Reserve(enemyTypes.size());
    for (int index = 0; index < static_cast<int>(enemyTypes.size()); ++index)
    {
        // 生成順をspawnIndexとして渡し、出現位置や出現タイミングを個体ごとにずらす
        CreateEnemyOfType(index, enemyTypes[index]);
        const int spawnLocationIndex =
            waveNumber == 9 && index >= 4 ? (index - 4 + 2) % 4 : index;
        enemies_[index].controller->StartSpawn(index, spawnLocationIndex);
    }
}

void SceneManager::CreateEnemy(int spawnIndex)
{
    CreateEnemyOfType(spawnIndex, EnemyType::Auto);
}

void SceneManager::CreateEnemyOfType(int spawnIndex, EnemyType enemyType)
{
    EnemyInstance enemy;
    enemy.shadow = std::make_unique<GameObject>();
    enemy.shadow->GetTransform().SetPosition({ 0.0f, -0.99f, 0.0f });
    auto& shadowRenderer = enemy.shadow->AddComponent<ColoredPlaneComponent>();
    shadowRenderer.Setup(
        *pRenderer_, EnemyShadowWidth, EnemyShadowDepth, { 0.05f, 0.05f, 0.05f, 0.50f });

    enemy.telegraphArea = std::make_unique<GameObject>();
    enemy.telegraphArea->GetTransform().SetPosition({ 0.0f, -1.23f, 0.0f });
    auto& telegraphRenderer = enemy.telegraphArea->AddComponent<ColoredPlaneComponent>();
    telegraphRenderer.Setup(
        *pRenderer_, 1.0f, 1.0f, { 0.32941177f, 0.18431373f, 0.15686275f, 0.3f });
    telegraphRenderer.SetVisible(false);
    telegraphRenderer.SetDissolvePixelSize(0.0625f, 0.0625f);

    enemy.radialTelegraphArea = std::make_unique<GameObject>();
    enemy.radialTelegraphArea->GetTransform().SetPosition({ 0.0f, -1.23f, 0.0f });
    auto& radialTelegraphRenderer =
        enemy.radialTelegraphArea->AddComponent<ColoredDiskComponent>();
    radialTelegraphRenderer.Setup(
        *pRenderer_, 1.0f, { 0.32941177f, 0.18431373f, 0.15686275f, 0.3f });
    radialTelegraphRenderer.SetVisible(false);

    // 敵本体・影・予告表示・体力バーを1つのEnemyInstanceとしてまとめて所有する
    enemy.object = std::make_unique<GameObject>();
    auto& sprite = enemy.object->AddComponent<AsepriteSpriteComponent>();
    const bool useSlime5 = enemyType == EnemyType::Slime5 ||
        (enemyType == EnemyType::Auto && waveEnemyCount_ == 1);
    const bool useSlime4 = enemyType == EnemyType::Slime4 ||
        (enemyType == EnemyType::Auto &&
            !useSlime5 && waveEnemyCount_ >= 4 && (spawnIndex % 4 == 3));
    const bool useSlime3 = enemyType == EnemyType::Slime3 ||
        (enemyType == EnemyType::Auto &&
            !useSlime5 && !useSlime4 && waveEnemyCount_ >= 3 && (spawnIndex % 3 == 2));
    const bool useSlime2 = enemyType == EnemyType::Slime2 ||
        (enemyType == EnemyType::Auto &&
            !useSlime5 && !useSlime4 && !useSlime3 &&
            waveEnemyCount_ >= 2 && (spawnIndex % 2 == 1));
    if (sprite.Setup(*pRenderer_,
        useSlime5 ? "Assets/Images/aseprite/Slime5.aseprite" :
        useSlime4 ? "Assets/Images/aseprite/Slime4.aseprite" :
        useSlime3 ? "Assets/Images/aseprite/Slime3.aseprite" :
        useSlime2 ? "Assets/Images/aseprite/Slime2.aseprite" :
        "Assets/Images/aseprite/Slime.aseprite",
        useSlime5 ? 4.0f : 2.0f))
    {
        sprite.Play("Idle", true);
    }
    if (useSlime5)
    {
        enemy.shadow->GetTransform().SetScale({ 2.0f, 1.0f, 2.0f });
    }
    auto& visual = enemy.object->AddComponent<UnitVisualComponent>(
        *enemy.shadow, shadowRenderer, sprite,
        useSlime5 ? 1.5f : 0.75f,
        useSlime5 ? 1.1f : 0.55f);
    if (useSlime5)
    {
        visual.SetShadowOffset(0.0f, Slime5ShadowOffsetZ);
    }
    auto& chargeParticles = enemy.object->AddComponent<ChargeParticleComponent>(camera_);
    chargeParticles.Setup(*pRenderer_);
    auto& hitbox = enemy.object->AddComponent<UnitHitboxComponent>();
    hitbox.Setup(*pRenderer_, useSlime5 ? 1.6f : 0.80f, useSlime5 ? 1.6f : 0.80f,
        { 1.0f, 0.25f, 0.15f, 0.25f }, ShowUnitHitboxes);
    visual.SetHitbox(hitbox);
    UnitHitboxComponent* contactHitbox = &hitbox;
    if (useSlime5)
    {
        auto& smallerContactHitbox = enemy.object->AddComponent<UnitHitboxComponent>();
        smallerContactHitbox.Setup(
            *pRenderer_, 0.92f, 1.25f,
            { 0.95f, 0.95f, 0.2f, 0.2f }, false);
        contactHitbox = &smallerContactHitbox;
    }
    auto& health = enemy.object->AddComponent<UnitHealthComponent>(useSlime5 ? 120 : 10);
    if (useSlime5)
    {
        auto& controller = enemy.object->AddComponent<Slime5ControllerComponent>(
            *player_, *playerController_, visual, health, chargeParticles,
            *enemy.telegraphArea, telegraphRenderer,
            *enemy.radialTelegraphArea, radialTelegraphRenderer,
            [this](const DirectX::XMFLOAT3& position, float delay)
            {
                // 敵は床の所有者にならず、HazardSystemへ生成だけを依頼する
                hazardSystem_.QueueDamageFloorSpawn(position, delay);
            },
            [this](const DirectX::XMFLOAT3& position, float angle)
            {
                hazardSystem_.SpawnSlime5JumpShards(*pRenderer_, position, angle);
            },
            [this](const DirectX::XMFLOAT3& position)
            {
                static std::mt19937 randomEngine(
                    static_cast<std::mt19937::result_type>(
                        std::chrono::steady_clock::now().time_since_epoch().count()));
                std::uniform_real_distribution<float> angleDistribution(
                    -DirectX::XM_PI, DirectX::XM_PI);
                const float angle = angleDistribution(randomEngine);
                if (!impactParticlesComponent_) return;
                impactParticlesComponent_->Emit(
                    position,
                    { std::cos(angle), std::sin(angle) },
                    true,
                    { 1.0f, 1.0f, 1.0f, 1.0f },
                    1.45f,
                    9);
                camera_.TriggerShake({ std::cos(angle), std::sin(angle) }, 0.24f, 0.12f, true);
            },
            [this]()
            {
                camera_.TriggerShake({ 0.0f, 1.0f }, 0.54f, 0.34f, true);
            });
        enemy.controller = &controller;
        enemyTargetRegistry_.Acquire({
            &*enemy.object, &hitbox, contactHitbox, &health, &controller,
            true, true, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.2f, -1 });
    }
    else if (useSlime2)
    {
        auto& controller = enemy.object->AddComponent<Slime2ControllerComponent>(
            *player_, *playerController_, visual, health, chargeParticles,
            *enemy.telegraphArea, telegraphRenderer);
        enemy.controller = &controller;
        enemyTargetRegistry_.Acquire({
            &*enemy.object, &hitbox, contactHitbox, &health, &controller,
            true, true, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, -1 });
    }
    else if (useSlime4)
    {
        auto& controller = enemy.object->AddComponent<Slime4ControllerComponent>(
            *player_, *playerController_, visual, health,
            [this](const DirectX::XMFLOAT3& position)
            {
                // Slime4が消えても床を残すため、実体の管理はHazardSystemに委譲する
                hazardSystem_.SpawnDamageFloor(*pRenderer_, position);
            });
        enemy.controller = &controller;
        enemyTargetRegistry_.Acquire({
            &*enemy.object, &hitbox, contactHitbox, &health, &controller,
            true, true, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, -1 });
    }
    else if (useSlime3)
    {
        auto& controller = enemy.object->AddComponent<Slime3ControllerComponent>(
            *player_, *playerController_, visual, health);
        enemy.controller = &controller;
        enemyTargetRegistry_.Acquire({
            &*enemy.object, &hitbox, contactHitbox, &health, &controller,
            true, true, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, -1 });
    }
    else
    {
        auto& controller = enemy.object->AddComponent<SlimeControllerComponent>(
            *player_, *playerController_, visual, health);
        enemy.controller = &controller;
        enemyTargetRegistry_.Acquire({
            &*enemy.object, &hitbox, contactHitbox, &health, &controller,
            true, true, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, -1 });
    }

    enemy.healthBar = std::make_unique<GameObject>();
    auto& enemyHealthBar = enemy.healthBar->AddComponent<HealthBarComponent>(
        *enemy.object, health);
    enemyHealthBar.Setup(
        *pRenderer_, "Assets/Images/aseprite/HealthBar.aseprite",
        useSlime5 ? 1.6f : 1.0f, useSlime5 ? 2.0f : 1.25f);
    enemy.visual = &visual;
    enemy.telegraphRenderer = &telegraphRenderer;
    enemy.radialTelegraphRenderer = &radialTelegraphRenderer;
    enemy.healthBarComponent = &enemyHealthBar;
    enemies_.push_back(std::move(enemy));
}

void SceneManager::Terminate()
{
    SoundPlayer::StopBgm();
    cube_.reset();
    ground_.reset();
    stageTitle_.reset();
    attackEffect_.reset();
    impactParticles_.reset();
    impactParticlesComponent_ = nullptr;
    playerHealthBar_.reset();
    playerHealthBarComponent_ = nullptr;
    optionDimmer_.reset();
    optionPanel_.reset();
    optionTitle_.reset();
    optionCursor_.Reset();
    continueCursor_.Reset();
    titleCursor_.Reset();
    optionMasterText_.reset();
    optionBgmText_.reset();
    optionSeText_.reset();
    optionReturnText_.reset();
    optionResumeText_.reset();
    for (auto& triangle : optionLeftTriangles_) triangle.reset();
    for (auto& triangle : optionRightTriangles_) triangle.reset();
    for (auto& square : optionVolumeSquares_) square.reset();
    optionDimmerRenderer_ = nullptr;
    optionPanelRenderer_ = nullptr;
    optionTitleRenderer_ = nullptr;
    optionMasterRenderer_ = nullptr;
    optionBgmRenderer_ = nullptr;
    optionSeRenderer_ = nullptr;
    optionReturnRenderer_ = nullptr;
    optionResumeRenderer_ = nullptr;
    optionLeftTriangleRenderers_.fill(nullptr);
    optionRightTriangleRenderers_.fill(nullptr);
    optionVolumeSquareRenderers_.fill(nullptr);
    hazardSystem_.Reset();
    enemyTargetRegistry_.Clear();
    titleLogoAnimatedText_.characters.clear();
    titleStartAnimatedText_.characters.clear();
    titleOptionAnimatedText_.characters.clear();
    titleQuitAnimatedText_.characters.clear();
    optionTitleAnimatedText_.characters.clear();
    tutorialMoveHintAnimatedText_.characters.clear();
    tutorialAttackHintAnimatedText_.characters.clear();
    tutorialDodgeHintAnimatedText_.characters.clear();
    tutorialJumpHintAnimatedText_.characters.clear();
    continuePromptAnimatedText_.characters.clear();
    continueYesAnimatedText_.characters.clear();
    continueNoAnimatedText_.characters.clear();
    clearTitleAnimatedText_.characters.clear();
    clearSummaryAnimatedText_.characters.clear();
    clearBackToTitleAnimatedText_.characters.clear();
    enemies_.clear();
    player_.reset();
    shadow_.reset();
    playerVisual_ = nullptr;
    playerHealth_ = nullptr;
}

void SceneManager::StartGameplay(
    bool startTutorial,
    int startingWave,
    TutorialStep tutorialRestartStep)
{
    if (!pRenderer_)
    {
        return;
    }

    sceneState_ = SceneState::Gameplay;
    titleFadeAlpha_ = 0.0f;
    gameplayFadeOutAlpha_ = 0.0f;
    gameplayFadeOutHoldRemaining_ = 0.0f;
    pendingReturnToTitle_ = false;
    pendingWaveSpawnAfterIntro_ = !startTutorial;
    tutorialActive_ = startTutorial;
    tutorialStep_ =
        startTutorial ? TutorialStep::PlayerIntro : TutorialStep::Inactive;
    continueRestartTutorialStep_ =
        startTutorial ? tutorialRestartStep : TutorialStep::PlayerIntro;
    tutorialHint_ = TutorialHint::None;
    tutorialHintFadeAlpha_ = 0.0f;
    tutorialHintTargetAlpha_ = 0.0f;
    tutorialTransitionDelayRemaining_ = 0.0f;
    optionMenuState_ = OptionMenuState::Closed;
    optionFadeAlpha_ = 0.0f;
    continuePromptActive_ = false;
    continuePromptAlpha_ = 0.0f;
    clearPromptActive_ = false;
    clearPromptAlpha_ = 0.0f;
    continueRestartTutorial_ = false;
    continueSelection_ = ContinueChoice::Yes;
    titleSelection_ = TitleMenuItem::Start;
    continueCursorIdleTime_ = 0.0f;
    titleCursorIdleTime_ = 0.0f;
    titleCursorCurrentY_ = TitleStartY;
    if (startTutorial && startingWave <= 1)
    {
        continueCountThisRun_ = 0;
    }
    waveEnemyCount_ = std::max(1, startingWave);
    hazardSystem_.Reset();
    enemyTargetRegistry_.Clear();
    enemies_.clear();
    playerController_ = nullptr;
    playerVisual_ = nullptr;
    playerHitbox_ = nullptr;
    playerHealth_ = nullptr;

    shadow_ = std::make_unique<GameObject>();
    shadow_->GetTransform().SetPosition({ 0.0f, -0.99f, -1.0f });
    auto& playerShadowRenderer = shadow_->AddComponent<ColoredPlaneComponent>();
    playerShadowRenderer.Setup(
        *pRenderer_, 1.0f, 1.0f, { 0.05f, 0.05f, 0.05f, 0.50f });

    player_ = std::make_unique<GameObject>();
    player_->GetTransform().SetPosition({ 0.0f, -1.25f, -1.0f });
    auto& sprite = player_->AddComponent<AsepriteSpriteComponent>();
    if (sprite.Setup(*pRenderer_, "Assets/Images/aseprite/Player.aseprite", 2.0f))
    {
        sprite.Play("Idle", true);
    }

    attackEffect_ = std::make_unique<GameObject>();
    auto& attackEffect = attackEffect_->AddComponent<SlashAttackEffectComponent>(*player_);
    attackEffect.Setup(*pRenderer_);
    attackEffect.SetHitboxDebugVisible(ShowAttackHitboxes);

    impactParticles_ = std::make_unique<GameObject>();
    auto& impactParticles = impactParticles_->AddComponent<ImpactParticleComponent>(camera_);
    impactParticles.Setup(*pRenderer_);
    impactParticlesComponent_ = &impactParticles;

    auto& playerVisual = player_->AddComponent<UnitVisualComponent>(
        *shadow_, playerShadowRenderer, sprite, 0.8f, 0.6f);
    playerVisual_ = &playerVisual;
    auto& playerHitbox = player_->AddComponent<UnitHitboxComponent>();
    playerHitbox.Setup(
        *pRenderer_, 0.42f, 1.55f, { 0.15f, 0.55f, 1.0f, 0.25f }, ShowUnitHitboxes);
    playerHitbox_ = &playerHitbox;
    playerVisual.SetHitbox(playerHitbox);
    auto& playerHealth = player_->AddComponent<UnitHealthComponent>(10);
    playerHealth_ = &playerHealth;

    playerController_ = &player_->AddComponent<PlayerControllerComponent>(
        input_, camera_, impactParticles, sprite, playerVisual, playerHitbox, playerHealth,
        attackEffect, enemyTargetRegistry_.Targets(), -1.25f);
    playerController_->SetInputEnabled(false);
    SoundPlayer::Play(PlayerJumpSoundPath);
    playerController_->BeginStageIntro(
        TutorialPlayerIntroStart,
        TutorialPlayerIntroLanding,
        TutorialPlayerIntroDuration,
        TutorialPlayerIntroArcHeight);

    playerHealthBar_ = std::make_unique<GameObject>();
    auto& playerHealthBar = playerHealthBar_->AddComponent<HealthBarComponent>(
        *player_, playerHealth);
    playerHealthBar.Setup(
        *pRenderer_, "Assets/Images/aseprite/HealthBar.aseprite", 1.0f, 1.75f);
    playerHealthBarComponent_ = &playerHealthBar;
    playerVisual.SyncShadow();
}

void SceneManager::ReturnToTitle()
{
    sceneState_ = SceneState::Title;
    titleFadeAlpha_ = 1.0f;
    gameplayFadeOutAlpha_ = 0.0f;
    gameplayFadeOutHoldRemaining_ = 0.0f;
    pendingReturnToTitle_ = false;
    tutorialActive_ = false;
    tutorialStep_ = TutorialStep::Inactive;
    tutorialHint_ = TutorialHint::None;
    tutorialHintFadeAlpha_ = 0.0f;
    tutorialHintTargetAlpha_ = 0.0f;
    tutorialTransitionDelayRemaining_ = 0.0f;
    optionMenuState_ = OptionMenuState::Closed;
    optionFadeAlpha_ = 0.0f;
    continuePromptActive_ = false;
    continueSelection_ = ContinueChoice::Yes;
    continueCursorIdleTime_ = 0.0f;
    waveEnemyCount_ = 1;
    clearPromptActive_ = false;
    clearPromptAlpha_ = 0.0f;
    continueRestartTutorial_ = false;
    continueCountThisRun_ = 0;
    attackEffect_.reset();
    impactParticles_.reset();
    impactParticlesComponent_ = nullptr;
    playerHealthBar_.reset();
    playerHealthBarComponent_ = nullptr;
    hazardSystem_.Reset();
    enemyTargetRegistry_.Clear();
    enemies_.clear();
    player_.reset();
    shadow_.reset();
    playerController_ = nullptr;
    playerVisual_ = nullptr;
    playerHitbox_ = nullptr;
    playerHealth_ = nullptr;
    continuePromptActive_ = false;
    continueSelection_ = ContinueChoice::Yes;
    continueCursorIdleTime_ = 0.0f;
    continuePromptAlpha_ = 0.0f;
    pendingWaveSpawnAfterIntro_ = false;
    clearPromptActive_ = false;
    titleSelection_ = TitleMenuItem::Start;
    titleCursorIdleTime_ = 0.0f;
    titleCursorCurrentY_ = TitleStartY;
    gameInput_.SuppressConfirm();
    if (optionDimmerRenderer_)
    {
        optionDimmerRenderer_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
    }
}

void SceneManager::BeginReturnToTitle()
{
    if (sceneState_ != SceneState::Gameplay)
    {
        StartReturnToTitleTransition();
        return;
    }

    pendingReturnToTitle_ = false;
    if (IsOptionMenuActive())
    {
        BeginCloseOptionMenu();
    }
    StartReturnToTitleTransition();
}

void SceneManager::StartReturnToTitleTransition()
{
    if (sceneState_ != SceneState::Gameplay)
    {
        if (sceneState_ != SceneState::GameplayFadingToTitle)
        {
            ReturnToTitle();
        }
        return;
    }

    sceneState_ = SceneState::GameplayFadingToTitle;
    gameplayFadeOutAlpha_ = 0.0f;
    gameplayFadeOutHoldRemaining_ = GameplayFadeOutHoldDuration;
    pendingReturnToTitle_ = false;
    hazardSystem_.ClearQueuedSpawns();
    attackEffect_.reset();
    impactParticles_.reset();
    impactParticlesComponent_ = nullptr;

    for (auto& enemy : enemies_)
    {
        if (enemy.telegraphRenderer) enemy.telegraphRenderer->SetVisible(false);
        if (enemy.radialTelegraphRenderer) enemy.radialTelegraphRenderer->SetVisible(false);
    }
}

void SceneManager::ApplyGameplayFadeOut(float fadeAmount)
{
    const float visibleAlpha = 1.0f - fadeAmount;
    if (playerVisual_)
    {
        playerVisual_->SetDissolveAmount(fadeAmount);
    }
    if (playerHealthBarComponent_)
    {
        playerHealthBarComponent_->SetFadeAlpha(visibleAlpha);
    }

    for (auto& enemy : enemies_)
    {
        if (enemy.visual)
        {
            enemy.visual->SetDissolveAmount(fadeAmount);
        }
        if (enemy.healthBarComponent)
        {
            enemy.healthBarComponent->SetFadeAlpha(visibleAlpha);
        }
        if (enemy.telegraphRenderer) enemy.telegraphRenderer->SetVisible(false);
        if (enemy.radialTelegraphRenderer) enemy.radialTelegraphRenderer->SetVisible(false);
    }

    hazardSystem_.ApplyFadeOut(fadeAmount);
}

void SceneManager::UpdateGameplayFadeOut(float deltaTime)
{
    gameplayFadeOutAlpha_ = std::min(
        1.0f, gameplayFadeOutAlpha_ + deltaTime / GameplayFadeOutDuration);
    ApplyGameplayFadeOut(gameplayFadeOutAlpha_);

    if (gameplayFadeOutAlpha_ >= 1.0f)
    {
        gameplayFadeOutHoldRemaining_ = std::max(
            0.0f, gameplayFadeOutHoldRemaining_ - deltaTime);
        if (gameplayFadeOutHoldRemaining_ <= 0.0f)
        {
            ReturnToTitle();
        }
    }
}

void SceneManager::ClearEnemies()
{
    auto& targets = enemyTargetRegistry_.Targets();
    for (std::size_t targetIndex = 0; targetIndex < targets.size(); ++targetIndex)
    {
        const GameObject* targetObject = targets[targetIndex].object;
        if (!targetObject) continue;

        const bool belongsToEnemy = std::any_of(
            enemies_.begin(), enemies_.end(),
            [targetObject](const EnemyInstance& enemy)
            {
                return enemy.object.get() == targetObject;
            });
        if (belongsToEnemy)
        {
            enemyTargetRegistry_.Release(targetIndex);
        }
    }
    // Registryを先に解除してからunique_ptrを破棄する
    // これにより、破棄済みGameObjectを指すEnemyTargetを残さない
    enemies_.clear();
}

void SceneManager::SpawnTutorialEnemy(EnemyType enemyType)
{
    ClearEnemies();
    CreateEnemyOfType(0, enemyType);
    if (!enemies_.empty() && enemies_[0].controller)
    {
        enemies_[0].controller->StartSpawn(0);
    }
}

void SceneManager::UpdateTutorialHintAnimations(float deltaTime)
{
    const float animationTime = -1.0f;
    animatedTextSystem_.UpdateLine(
        tutorialMoveHintAnimatedText_,
        animationTime,
        tutorialHint_ == TutorialHint::MoveAndAttack ? tutorialHintFadeAlpha_ : 0.0f,
        deltaTime);
    animatedTextSystem_.UpdateLine(
        tutorialAttackHintAnimatedText_,
        animationTime,
        tutorialHint_ == TutorialHint::MoveAndAttack ? tutorialHintFadeAlpha_ : 0.0f,
        deltaTime);
    animatedTextSystem_.UpdateLine(
        tutorialDodgeHintAnimatedText_,
        animationTime,
        tutorialHint_ == TutorialHint::Dodge ? tutorialHintFadeAlpha_ : 0.0f,
        deltaTime);
    animatedTextSystem_.UpdateLine(
        tutorialJumpHintAnimatedText_,
        animationTime,
        tutorialHint_ == TutorialHint::Jump ? tutorialHintFadeAlpha_ : 0.0f,
        deltaTime);
}

void SceneManager::UpdateTutorial(float deltaTime)
{
    if (!tutorialActive_)
    {
        tutorialHint_ = TutorialHint::None;
        tutorialHintFadeAlpha_ = 0.0f;
        tutorialHintTargetAlpha_ = 0.0f;
        UpdateTutorialHintAnimations(deltaTime);
        return;
    }

    if (tutorialHintFadeAlpha_ < tutorialHintTargetAlpha_)
    {
        tutorialHintFadeAlpha_ = std::min(
            tutorialHintTargetAlpha_,
            tutorialHintFadeAlpha_ + deltaTime * TutorialHintFadeSpeed);
    }
    else if (tutorialHintFadeAlpha_ > tutorialHintTargetAlpha_)
    {
        tutorialHintFadeAlpha_ = std::max(
            tutorialHintTargetAlpha_,
            tutorialHintFadeAlpha_ - deltaTime * TutorialHintFadeSpeed);
    }

    UpdateTutorialHintAnimations(deltaTime);

    const bool currentEnemyDefeated = !enemies_.empty() && std::all_of(
        enemies_.begin(), enemies_.end(), [](const EnemyInstance& enemy)
        {
            return enemy.controller && enemy.controller->IsDefeated();
        });

    switch (tutorialStep_)
    {
    case TutorialStep::PlayerIntro:
        if (playerController_ && !playerController_->IsStageIntroActive())
        {
            playerController_->SetInputEnabled(true);
            switch (continueRestartTutorialStep_)
            {
            case TutorialStep::Dodge:
            case TutorialStep::DodgeDelay:
            case TutorialStep::DodgeFadeOut:
                tutorialHint_ = TutorialHint::Dodge;
                tutorialHintTargetAlpha_ = 1.0f;
                SpawnTutorialEnemy(EnemyType::Slime2);
                tutorialStep_ = TutorialStep::Dodge;
                break;

            case TutorialStep::Jump:
            case TutorialStep::JumpDelay:
            case TutorialStep::JumpFadeOut:
                tutorialHint_ = TutorialHint::Jump;
                tutorialHintTargetAlpha_ = 1.0f;
                SpawnTutorialEnemy(EnemyType::Slime3);
                tutorialStep_ = TutorialStep::Jump;
                break;

            case TutorialStep::MoveAttack:
            case TutorialStep::MoveAttackFadeOut:
            case TutorialStep::PlayerIntro:
            case TutorialStep::Inactive:
            case TutorialStep::CompleteDelay:
            default:
                tutorialHint_ = TutorialHint::MoveAndAttack;
                tutorialHintTargetAlpha_ = 1.0f;
                SpawnTutorialEnemy(EnemyType::Slime);
                tutorialStep_ = TutorialStep::MoveAttack;
                break;
            }
        }
        break;

    case TutorialStep::MoveAttack:
        if (currentEnemyDefeated)
        {
            tutorialHintTargetAlpha_ = 0.0f;
            tutorialStep_ = TutorialStep::MoveAttackFadeOut;
        }
        break;

    case TutorialStep::MoveAttackFadeOut:
        if (tutorialHintFadeAlpha_ <= 0.0f)
        {
            tutorialTransitionDelayRemaining_ = TutorialAdvanceDelay;
            tutorialHint_ = TutorialHint::None;
            tutorialStep_ = TutorialStep::DodgeDelay;
        }
        break;

    case TutorialStep::DodgeDelay:
        tutorialTransitionDelayRemaining_ =
            std::max(0.0f, tutorialTransitionDelayRemaining_ - deltaTime);
        if (tutorialTransitionDelayRemaining_ <= 0.0f)
        {
            tutorialHint_ = TutorialHint::Dodge;
            tutorialHintTargetAlpha_ = 1.0f;
            SpawnTutorialEnemy(EnemyType::Slime2);
            tutorialStep_ = TutorialStep::Dodge;
        }
        break;

    case TutorialStep::Dodge:
        if (currentEnemyDefeated)
        {
            tutorialHintTargetAlpha_ = 0.0f;
            tutorialStep_ = TutorialStep::DodgeFadeOut;
        }
        break;

    case TutorialStep::DodgeFadeOut:
        if (tutorialHintFadeAlpha_ <= 0.0f)
        {
            tutorialTransitionDelayRemaining_ = TutorialAdvanceDelay;
            tutorialHint_ = TutorialHint::None;
            tutorialStep_ = TutorialStep::JumpDelay;
        }
        break;

    case TutorialStep::JumpDelay:
        tutorialTransitionDelayRemaining_ =
            std::max(0.0f, tutorialTransitionDelayRemaining_ - deltaTime);
        if (tutorialTransitionDelayRemaining_ <= 0.0f)
        {
            tutorialHint_ = TutorialHint::Jump;
            tutorialHintTargetAlpha_ = 1.0f;
            SpawnTutorialEnemy(EnemyType::Slime3);
            tutorialStep_ = TutorialStep::Jump;
        }
        break;

    case TutorialStep::Jump:
        if (currentEnemyDefeated)
        {
            tutorialHintTargetAlpha_ = 0.0f;
            tutorialStep_ = TutorialStep::JumpFadeOut;
        }
        break;

    case TutorialStep::JumpFadeOut:
        if (tutorialHintFadeAlpha_ <= 0.0f)
        {
            tutorialTransitionDelayRemaining_ = TutorialAdvanceDelay;
            tutorialHint_ = TutorialHint::None;
            tutorialStep_ = TutorialStep::CompleteDelay;
        }
        break;

    case TutorialStep::CompleteDelay:
        tutorialTransitionDelayRemaining_ =
            std::max(0.0f, tutorialTransitionDelayRemaining_ - deltaTime);
        if (tutorialTransitionDelayRemaining_ <= 0.0f)
        {
            tutorialActive_ = false;
            tutorialStep_ = TutorialStep::Inactive;
            tutorialHint_ = TutorialHint::None;
            tutorialHintFadeAlpha_ = 0.0f;
            tutorialHintTargetAlpha_ = 0.0f;
            ClearEnemies();
            waveEnemyCount_ = 4;
            CreateWave(waveEnemyCount_);
        }
        break;

    case TutorialStep::Inactive:
        break;
    }
}
