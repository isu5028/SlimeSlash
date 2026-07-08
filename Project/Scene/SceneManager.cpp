#include "SceneManager.h"
#include "RotateComponent.h"
#include "Renderer.h"
#include "AsepriteSpriteComponent.h"
#include "SoundPlayer.h"
#include "ChargeParticleComponent.h"
#include "ColoredBoxComponent.h"
#include "ColoredDiskComponent.h"
#include "ColoredPlaneComponent.h"
#include "DamageFloorComponent.h"
#include "ImageSpriteComponent.h"
#include "ImpactParticleComponent.h"
#include "HealthBarComponent.h"
#include "PlayerControllerComponent.h"
#include "SlashAttackEffectComponent.h"
#include "SlimeControllerComponent.h"
#include "Slime2ControllerComponent.h"
#include "Slime3ControllerComponent.h"
#include "Slime4ControllerComponent.h"
#include "Slime5ControllerComponent.h"
#include "Slime5JumpShardComponent.h"
#include "UnitVisualComponent.h"
#include "UnitHitboxComponent.h"
#include "UnitHealthComponent.h"
#include "WorldTextSpriteComponent.h"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr wchar_t BgmSoundPath[] = L"Project/Assets/Sounds/BGM.wav";
    constexpr float BgmStartDelay = 2.0f;
    constexpr float BgmBarDuration = 5.8576f;
    constexpr float BgmLoopDuration = 175.52f;
    constexpr float EnemyShadowWidth = 1.8f;
    constexpr float EnemyShadowDepth = 1.2f;
    constexpr float Slime5ShadowOffsetZ = 0.6f;
    constexpr float GroundHalfSize = 5.0f;
    constexpr float DamageFloorY = -0.98f;
    constexpr float DamageFloorTileWidth = EnemyShadowWidth * 0.5f;
    constexpr float DamageFloorTileDepth = EnemyShadowDepth * 0.5f;
    constexpr float DamageFloorHitboxHeight = 0.35f;
    constexpr float DamageFloorClusterPlacementLimitX =
        GroundHalfSize - DamageFloorTileWidth * 1.5f;
    constexpr float DamageFloorClusterPlacementLimitZ =
        GroundHalfSize - DamageFloorTileDepth * 1.5f;
    constexpr float DamageFloorTilePlacementLimitX =
        GroundHalfSize - DamageFloorTileWidth * 0.5f;
    constexpr float DamageFloorTilePlacementLimitZ =
        GroundHalfSize - DamageFloorTileDepth * 0.5f;
    constexpr float Slime5JumpShardShadowY = -0.99f;
    constexpr float Slime5JumpShardSpriteHeight = 0.7f;
    constexpr float Slime5JumpShardShadowSize = 0.34f;
    constexpr float Slime5JumpShardHitboxRadius = 0.24f;
    constexpr float Slime5JumpShardHitboxHeight = 0.42f;
    constexpr int Slime5JumpShardHealth = 1000000;
    constexpr char OptionFontPath[] = "Project/Assets/Fonts/Shikakufuto_Free_ver20251228.ttf";
    constexpr char OptionTriangleImagePath[] = "Assets/Images/png/triangle.png";
    constexpr char OptionSquareImagePath[] = "Assets/Images/png/square.png";
    constexpr wchar_t PlayerJumpSoundPath[] = L"Project/Assets/Sounds/Jump.wav";
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
    constexpr float OptionIconWorldHeight = 0.92f;
    constexpr float OptionOverlayCameraDistance = 15.0f;
    constexpr float OptionDimmerStrength = 0.48f;
    constexpr float TitleLogoY = 1.55f;
    constexpr float TitleStartY = -0.75f;
    constexpr float AnimatedLetterJumpHeight = 0.22f;
    constexpr float AnimatedLetterJumpDuration = 0.42f;
    constexpr float AnimatedLetterTracking = 0.04f;
    constexpr float AnimatedWordSpacing = 0.22f;

    bool IsKeyDown(int virtualKey)
    {
        return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
    }

    int VolumeToStep(float volume)
    {
        return static_cast<int>(std::round(std::max(0.0f, std::min(volume, 1.0f)) * 5.0f));
    }

    float StepToVolume(int step)
    {
        return std::max(0, std::min(step, 5)) / 5.0f;
    }

}

SceneManager::SceneManager()
{

}

SceneManager::~SceneManager()
{

}

void SceneManager::Initialize(Renderer& renderer)
{
	pRenderer_ = &renderer;
    SoundPlayer::StopBgm();
    bgmStartDelayRemaining_ = BgmStartDelay;
    bgmLoopElapsed_ = 0.0f;
    bgmStarted_ = false;
    sceneState_ = SceneState::Title;
    waveEnemyCount_ = 1;
    optionMenuState_ = OptionMenuState::Closed;
    optionFadeAlpha_ = 0.0f;
    titleFadeAlpha_ = 0.0f;
    titleStartPulseTime_ = 0.0f;
	camera_.SetEyePosition({ 0.0f, 8.0f, -20.0f });
	camera_.SetFocusPosition({ 0.0f, -0.3f, -1.0f });

	// キャラクターが立つXZ平面の地面。
	ground_ = std::make_unique<GameObject>();
	ground_->GetTransform().SetPosition({ 0.0f, -1.0f, 0.0f });
	ground_->AddComponent<ColoredBoxComponent>().Setup(
		renderer, 10.0f, 1.0f, 10.0f,
		{ 0.67843139f, 0.41960785f, 0.41568628f, 1.0f });

	stageTitle_ = std::make_unique<GameObject>();
	stageTitle_->GetTransform().SetPosition({ 0.0f, -1.85f, -5.02f });
	stageTitle_->AddComponent<WorldTextSpriteComponent>().Setup(
		renderer,
		"Project/Assets/Fonts/Shikakufuto_Free_ver20251228.ttf",
		L"",
		0.7f,
		{ 1.0f, 1.0f, 1.0f, 1.0f });

	// 地面より少し上に半透明の長方形を置き、足元の簡易影にする。
    InitializeOptionMenu(renderer);
    InitializeTitleScreen(renderer);

	// 元画像の下端に約4pxの透明余白があるため、その分だけ下げて見た目の足を地面へ合わせる。
}

void SceneManager::CreateWave(int enemyCount)
{
	if (static_cast<int>(enemies_.size()) < enemyCount)
	{
		enemies_.reserve(enemyCount);
		enemyTargets_.reserve(enemyCount);
		while (static_cast<int>(enemies_.size()) < enemyCount)
		{
			CreateEnemy(static_cast<int>(enemies_.size()));
		}
	}

	for (int index = 0; index < enemyCount; ++index)
	{
		enemies_[index].controller->StartSpawn(index);
	}
}

std::size_t SceneManager::AcquireEnemyTargetSlot(const EnemyTarget& target)
{
    if (!freeEnemyTargetIndices_.empty())
    {
        const std::size_t targetIndex = freeEnemyTargetIndices_.back();
        freeEnemyTargetIndices_.pop_back();
        enemyTargets_[targetIndex] = target;
        return targetIndex;
    }

    enemyTargets_.push_back(target);
    return enemyTargets_.size() - 1;
}

void SceneManager::ReleaseEnemyTargetSlot(std::size_t targetIndex)
{
    if (targetIndex >= enemyTargets_.size()) return;
    enemyTargets_[targetIndex] = {};
    freeEnemyTargetIndices_.push_back(targetIndex);
}

void SceneManager::CreateEnemy(int spawnIndex)
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

	enemy.object = std::make_unique<GameObject>();
	auto& sprite = enemy.object->AddComponent<AsepriteSpriteComponent>();
	const bool useSlime5 = waveEnemyCount_ == 1 && spawnIndex == 0;
	const bool useSlime4 = !useSlime5 && waveEnemyCount_ >= 4 && (spawnIndex % 4 == 3);
	const bool useSlime3 = !useSlime5 && !useSlime4 && waveEnemyCount_ >= 3 && (spawnIndex % 3 == 2);
	const bool useSlime2 = !useSlime5 && !useSlime4 && !useSlime3 && waveEnemyCount_ >= 2 && (spawnIndex % 2 == 1);
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
				QueueDamageFloorSpawn(position, delay);
			},
			[this](const DirectX::XMFLOAT3& position, float angle)
			{
				SpawnSlime5JumpShards(position, angle);
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
		AcquireEnemyTargetSlot({
			&*enemy.object, &hitbox, contactHitbox, &health, &controller,
			true, true, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.2f, -1 });
	}
	else
	if (useSlime2)
	{
		auto& controller = enemy.object->AddComponent<Slime2ControllerComponent>(
			*player_, *playerController_, visual, health, chargeParticles,
			*enemy.telegraphArea, telegraphRenderer);
		enemy.controller = &controller;
		AcquireEnemyTargetSlot({
			&*enemy.object, &hitbox, contactHitbox, &health, &controller,
			true, true, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, -1 });
	}
	else if (useSlime4)
	{
		auto& controller = enemy.object->AddComponent<Slime4ControllerComponent>(
			*player_, *playerController_, visual, health,
            [this](const DirectX::XMFLOAT3& position) { SpawnDamageFloor(position); });
		enemy.controller = &controller;
		AcquireEnemyTargetSlot({
			&*enemy.object, &hitbox, contactHitbox, &health, &controller,
			true, true, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, -1 });
	}
	else if (useSlime3)
	{
		auto& controller = enemy.object->AddComponent<Slime3ControllerComponent>(
			*player_, *playerController_, visual, health);
		enemy.controller = &controller;
		AcquireEnemyTargetSlot({
			&*enemy.object, &hitbox, contactHitbox, &health, &controller,
			true, true, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, -1 });
	}
	else
	{
		auto& controller = enemy.object->AddComponent<SlimeControllerComponent>(
			*player_, *playerController_, visual, health);
		enemy.controller = &controller;
		AcquireEnemyTargetSlot({
			&*enemy.object, &hitbox, contactHitbox, &health, &controller,
			true, true, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, -1 });
	}

	enemy.healthBar = std::make_unique<GameObject>();
	enemy.healthBar->AddComponent<HealthBarComponent>(
		*enemy.object, health).Setup(
			*pRenderer_, "Assets/Images/aseprite/HealthBar.aseprite",
			useSlime5 ? 1.6f : 1.0f, useSlime5 ? 2.0f : 1.25f);
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
    optionDimmer_.reset();
    optionPanel_.reset();
    optionTitle_.reset();
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
	damageFloors_.clear();
	slime5JumpShards_.clear();
	queuedDamageFloorSpawns_.clear();
	enemyTargets_.clear();
    freeEnemyTargetIndices_.clear();
    titleLogoAnimatedText_.characters.clear();
    titleStartAnimatedText_.characters.clear();
    optionTitleAnimatedText_.characters.clear();
	enemies_.clear();
	player_.reset();
	shadow_.reset();
}

void SceneManager::CreateAnimatedTextLine(
    AnimatedTextLine& line,
    Renderer& renderer,
    const std::wstring& text,
    const DirectX::XMFLOAT3& centerPosition,
    float worldHeight,
    const DirectX::XMFLOAT4& color,
    float jumpHeight,
    float jumpDuration,
    float startOffset)
{
    line.characters.clear();
    line.jumpHeight = jumpHeight;
    line.jumpDuration = jumpDuration;
    line.startOffset = startOffset;

    std::vector<float> widths;
    std::vector<bool> widthHasCharacter;
    widths.reserve(text.size());
    widthHasCharacter.reserve(text.size());
    float totalWidth = 0.0f;

    for (const wchar_t character : text)
    {
        if (character == L' ')
        {
            widths.push_back(worldHeight * AnimatedWordSpacing);
            widthHasCharacter.push_back(false);
            totalWidth += widths.back();
            continue;
        }

        AnimatedTextCharacter animatedCharacter;
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
        animatedCharacter.animatable = true;
        const float width = rendererComponent.GetWorldWidth() + worldHeight * AnimatedLetterTracking;
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
                cursorX + width * 0.5f,
                centerPosition.y,
                centerPosition.z
            };
            animatedCharacter.object->GetTransform().SetPosition(animatedCharacter.basePosition);
            ++characterIndex;
        }
        cursorX += width;
    }
}

void SceneManager::UpdateAnimatedTextLine(
    AnimatedTextLine& line,
    float loopTime,
    float fadeAlpha)
{
    if (line.characters.empty()) return;

    bool animate = false;
    float wrappedTime = 0.0f;
    int activeStep = -1;
    float stepTime = 0.0f;
    if (loopTime >= 0.0f)
    {
        animate = true;
        wrappedTime = std::fmod(
            loopTime + line.startOffset + BgmLoopDuration, BgmLoopDuration);
        activeStep = std::max(0, static_cast<int>(std::floor(wrappedTime / BgmBarDuration)));
        stepTime = wrappedTime - activeStep * BgmBarDuration;
    }

    int animatableIndex = 0;
    for (auto& animatedCharacter : line.characters)
    {
        float jumpOffset = 0.0f;
        if (animatedCharacter.animatable)
        {
            if (animate &&
                animatableIndex == activeStep &&
                stepTime < line.jumpDuration)
            {
                const float t = std::max(0.0f, std::min(stepTime / line.jumpDuration, 1.0f));
                jumpOffset = std::sin(t * DirectX::XM_PI) * line.jumpHeight;
            }
            ++animatableIndex;
        }

        auto position = animatedCharacter.basePosition;
        position.y += jumpOffset;
        animatedCharacter.object->GetTransform().SetPosition(position);
        if (animatedCharacter.renderer)
        {
            const auto baseColor = animatedCharacter.animatable ?
                DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, fadeAlpha } :
                DirectX::XMFLOAT4{ 0.92f, 0.88f, 0.84f, fadeAlpha };
            animatedCharacter.renderer->SetColor(baseColor);
        }
        animatedCharacter.object->Update();
    }
}

void SceneManager::DrawAnimatedTextLine(const AnimatedTextLine& line)
{
    if (!pRenderer_) return;
    for (const auto& animatedCharacter : line.characters)
    {
        if (animatedCharacter.object)
        {
            animatedCharacter.object->Draw(*pRenderer_);
        }
    }
}

void SceneManager::InitializeTitleScreen(Renderer& renderer)
{
    CreateAnimatedTextLine(
        titleLogoAnimatedText_,
        renderer,
        L"SLIME SLASH SLAYER",
        { 0.0f, TitleLogoY, 0.0f },
        0.9f,
        { 1.0f, 1.0f, 1.0f, 1.0f },
        AnimatedLetterJumpHeight,
        AnimatedLetterJumpDuration);
    CreateAnimatedTextLine(
        titleStartAnimatedText_,
        renderer,
        L"SPACE TO START",
        { 0.0f, TitleStartY, 0.0f },
        0.62f,
        { 0.92f, 0.88f, 0.84f, 1.0f },
        AnimatedLetterJumpHeight * 0.82f,
        AnimatedLetterJumpDuration,
        BgmBarDuration * 0.5f);
}

void SceneManager::StartGameplay()
{
    if (!pRenderer_ || sceneState_ == SceneState::Gameplay)
    {
        return;
    }

    sceneState_ = SceneState::Gameplay;
    titleFadeAlpha_ = 0.0f;
    optionMenuState_ = OptionMenuState::Closed;
    optionFadeAlpha_ = 0.0f;
    waveEnemyCount_ = 1;
    damageFloors_.clear();
    slime5JumpShards_.clear();
    queuedDamageFloorSpawns_.clear();
    enemyTargets_.clear();
    freeEnemyTargetIndices_.clear();
    enemies_.clear();
    playerController_ = nullptr;
    playerHitbox_ = nullptr;

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
    auto& playerHitbox = player_->AddComponent<UnitHitboxComponent>();
    playerHitbox.Setup(
        *pRenderer_, 0.42f, 1.55f, { 0.15f, 0.55f, 1.0f, 0.25f }, ShowUnitHitboxes);
    playerHitbox_ = &playerHitbox;
    playerVisual.SetHitbox(playerHitbox);
    auto& playerHealth = player_->AddComponent<UnitHealthComponent>(10);

    playerController_ = &player_->AddComponent<PlayerControllerComponent>(
        camera_, impactParticles, sprite, playerVisual, playerHitbox, playerHealth,
        attackEffect, enemyTargets_, -1.25f);

    playerHealthBar_ = std::make_unique<GameObject>();
    playerHealthBar_->AddComponent<HealthBarComponent>(*player_, playerHealth).Setup(
        *pRenderer_, "Assets/Images/aseprite/HealthBar.aseprite", 1.0f, 1.75f);
    playerVisual.SyncShadow();
    CreateWave(waveEnemyCount_);
}

void SceneManager::ReturnToTitle()
{
    sceneState_ = SceneState::Title;
    titleFadeAlpha_ = 0.0f;
    optionMenuState_ = OptionMenuState::Closed;
    optionFadeAlpha_ = 0.0f;
    titleStartPulseTime_ = 0.0f;
    waveEnemyCount_ = 1;
    attackEffect_.reset();
    impactParticles_.reset();
    impactParticlesComponent_ = nullptr;
    playerHealthBar_.reset();
    damageFloors_.clear();
    slime5JumpShards_.clear();
    queuedDamageFloorSpawns_.clear();
    enemyTargets_.clear();
    freeEnemyTargetIndices_.clear();
    enemies_.clear();
    player_.reset();
    shadow_.reset();
    playerController_ = nullptr;
    playerHitbox_ = nullptr;
    previousConfirmKey_ = true;
    if (optionDimmerRenderer_)
    {
        optionDimmerRenderer_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
    }
}

void SceneManager::UpdateTitleScreen(float deltaTime)
{
    titleStartPulseTime_ += deltaTime;
    const float animationTime = bgmStarted_ ? bgmLoopElapsed_ : -1.0f;
    UpdateAnimatedTextLine(titleLogoAnimatedText_, animationTime);

    const float pulseAlpha =
        0.55f + (std::sin(titleStartPulseTime_ * 3.2f) * 0.5f + 0.5f) * 0.45f;
    UpdateAnimatedTextLine(titleStartAnimatedText_, animationTime, pulseAlpha);

    const bool confirmKey =
        IsKeyDown(VK_RETURN) || IsKeyDown(VK_SPACE) || IsKeyDown('J') || IsKeyDown('Z');
    const bool confirmPressed = confirmKey && !previousConfirmKey_;
    if (sceneState_ == SceneState::Title && confirmPressed)
    {
        sceneState_ = SceneState::TitleFadingOut;
        titleFadeAlpha_ = 0.0f;
    }

    if (sceneState_ != SceneState::TitleFadingOut)
    {
        if (optionDimmerRenderer_)
        {
            optionDimmerRenderer_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        }
        return;
    }

    titleFadeAlpha_ = std::min(1.0f, titleFadeAlpha_ + deltaTime / OptionFadeDuration);
    if (optionDimmerRenderer_)
    {
        optionDimmerRenderer_->SetColor({ 0.0f, 0.0f, 0.0f, titleFadeAlpha_ });
    }

    if (titleFadeAlpha_ >= 1.0f)
    {
        StartGameplay();
        if (optionDimmerRenderer_)
        {
            optionDimmerRenderer_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        }
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
    CreateAnimatedTextLine(
        optionTitleAnimatedText_,
        renderer,
        L"OPTION",
        { 0.0f, 3.0f + OptionMenuVerticalOffset, 0.0f },
        1.0f,
        { 1.0f, 1.0f, 1.0f, 1.0f },
        AnimatedLetterJumpHeight,
        AnimatedLetterJumpDuration);

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
    optionReturnRenderer_->Setup(
        *pRenderer_, OptionFontPath,
        L"RETURN TO TITLE", OptionMenuItemTextHeight, colorFor(OptionItem::ReturnToTitle, true));
    optionResumeRenderer_->Setup(
        *pRenderer_, OptionFontPath,
        L"RESUME", OptionMenuItemTextHeight, colorFor(OptionItem::Resume));

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

void SceneManager::OpenOptionMenu()
{
    optionMenuState_ = OptionMenuState::FadingIn;
    optionFadeAlpha_ = 0.0f;
    optionSelection_ = OptionItem::MasterVolume;
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
        return;
    }

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
    UpdateAnimatedTextLine(
        optionTitleAnimatedText_,
        bgmStarted_ ? bgmLoopElapsed_ : -1.0f,
        optionFadeAlpha_);
    RefreshOptionMenuText();

    if (optionMenuState_ != OptionMenuState::Open)
    {
        return;
    }

    const bool upKey = IsKeyDown('W') || IsKeyDown(VK_UP);
    const bool downKey = IsKeyDown('S') || IsKeyDown(VK_DOWN);
    const bool leftKey = IsKeyDown('A') || IsKeyDown(VK_LEFT);
    const bool rightKey = IsKeyDown('D') || IsKeyDown(VK_RIGHT);
    const bool escKey = IsKeyDown(VK_ESCAPE);
    const bool confirmKey =
        IsKeyDown(VK_RETURN) || IsKeyDown(VK_SPACE) || IsKeyDown('J') || IsKeyDown('Z');

    const bool upPressed = upKey && !previousUpKey_;
    const bool downPressed = downKey && !previousDownKey_;
    const bool leftPressed = leftKey && !previousLeftKey_;
    const bool rightPressed = rightKey && !previousRightKey_;
    const bool escPressed = escKey && !previousEscKey_;
    const bool confirmPressed = confirmKey && !previousConfirmKey_;

    bool refreshText = false;
    int selectionIndex = static_cast<int>(optionSelection_);
    const int firstOptionIndex = static_cast<int>(OptionItem::MasterVolume);
    const int lastOptionIndex = static_cast<int>(OptionItem::Resume);
    if (upPressed)
    {
        selectionIndex =
            (selectionIndex <= firstOptionIndex) ? lastOptionIndex : (selectionIndex - 1);
        optionSelection_ = static_cast<OptionItem>(selectionIndex);
        refreshText = true;
    }
    else if (downPressed)
    {
        selectionIndex =
            (selectionIndex >= lastOptionIndex) ? firstOptionIndex : (selectionIndex + 1);
        optionSelection_ = static_cast<OptionItem>(selectionIndex);
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
        BeginCloseOptionMenu();
    }
    else if (confirmPressed && optionSelection_ == OptionItem::ReturnToTitle)
    {
        ReturnToTitle();
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
    DrawAnimatedTextLine(optionTitleAnimatedText_);
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

    DrawAnimatedTextLine(titleLogoAnimatedText_);
    DrawAnimatedTextLine(titleStartAnimatedText_);
    if (sceneState_ == SceneState::TitleFadingOut && optionDimmer_)
    {
        optionDimmer_->Draw(*pRenderer_);
    }
}

void SceneManager::SpawnDamageFloor(const DirectX::XMFLOAT3& position)
{
	const DirectX::XMFLOAT3 clampedPosition = {
		std::max(
			-DamageFloorClusterPlacementLimitX,
			std::min(position.x, DamageFloorClusterPlacementLimitX)),
		position.y,
		std::max(
			-DamageFloorClusterPlacementLimitZ,
			std::min(position.z, DamageFloorClusterPlacementLimitZ))
	};
	const float offsetX = DamageFloorTileWidth * 0.5f;
	const float offsetZ = DamageFloorTileDepth * 0.5f;
	const DirectX::XMFLOAT2 offsets[] = {
		{ -offsetX, -offsetZ },
		{  offsetX, -offsetZ },
		{ -offsetX,  offsetZ },
		{  offsetX,  offsetZ },
	};

	for (const auto& offset : offsets)
	{
		SpawnDamageFloorTile({
			clampedPosition.x + offset.x,
			DamageFloorY,
			clampedPosition.z + offset.y
		});
	}
}

void SceneManager::SpawnDamageFloorTile(const DirectX::XMFLOAT3& position)
{
	DamageFloorInstance floor;
	floor.object = std::make_unique<GameObject>();
	floor.center = {
		std::max(
			-DamageFloorTilePlacementLimitX,
			std::min(position.x, DamageFloorTilePlacementLimitX)),
		DamageFloorY,
		std::max(
			-DamageFloorTilePlacementLimitZ,
			std::min(position.z, DamageFloorTilePlacementLimitZ))
	};
	floor.object->GetTransform().SetPosition(floor.center);
	auto& renderer = floor.object->AddComponent<ColoredPlaneComponent>();
	renderer.Setup(
		*pRenderer_, DamageFloorTileWidth, DamageFloorTileDepth,
		{ 0.0f, 0.0f, 0.0f, 1.0f });
	renderer.SetDissolvePixelSize(0.0625f, 0.0625f);
	renderer.SetUniformDissolve(true);
	auto& hitbox = floor.object->AddComponent<UnitHitboxComponent>();
	hitbox.Setup(
		*pRenderer_,
		std::min(DamageFloorTileWidth, DamageFloorTileDepth) * 0.42f,
		DamageFloorHitboxHeight,
		{ 0.0f, 0.0f, 0.0f, 0.0f },
		false);
	auto& health = floor.object->AddComponent<UnitHealthComponent>(1);
	auto& controller = floor.object->AddComponent<DamageFloorComponent>(renderer, health);
	floor.controller = &controller;
	floor.hitbox = &hitbox;
	floor.health = &health;
	floor.halfWidth = DamageFloorTileWidth * 0.5f;
	floor.halfDepth = DamageFloorTileDepth * 0.5f;
	floor.targetIndex = AcquireEnemyTargetSlot({
		floor.object.get(), &hitbox, &hitbox, &health, &controller,
		false, true, { 0.0f, 0.0f, 0.0f, 1.0f }, 0.6666667f, 1
	});
	damageFloors_.push_back(std::move(floor));
}

void SceneManager::SpawnSlime5JumpShards(const DirectX::XMFLOAT3& position, float angle)
{
	static std::mt19937 randomEngine(
		static_cast<std::mt19937::result_type>(
			std::chrono::steady_clock::now().time_since_epoch().count()));
	std::uniform_real_distribution<float> jitterDistribution(-0.75f, 0.75f);
	const float angles[] = {
		angle + jitterDistribution(randomEngine),
		angle + DirectX::XM_PI + jitterDistribution(randomEngine),
		angle + DirectX::XM_PIDIV2 + jitterDistribution(randomEngine),
		angle - DirectX::XM_PIDIV2 + jitterDistribution(randomEngine),
	};
	const DirectX::XMFLOAT2 directions[] = {
		{ std::cos(angles[0]), std::sin(angles[0]) },
		{ std::cos(angles[1]), std::sin(angles[1]) },
		{ std::cos(angles[2]), std::sin(angles[2]) },
		{ std::cos(angles[3]), std::sin(angles[3]) },
	};

	for (const auto& direction : directions)
	{
		Slime5JumpShardInstance shard;
		shard.shadow = std::make_unique<GameObject>();
		shard.shadow->GetTransform().SetPosition({
			position.x, Slime5JumpShardShadowY, position.z
		});
		auto& shadowRenderer = shard.shadow->AddComponent<ColoredPlaneComponent>();
		shadowRenderer.Setup(
			*pRenderer_,
			Slime5JumpShardShadowSize,
			Slime5JumpShardShadowSize,
			{ 0.05f, 0.05f, 0.05f, 0.50f });
		shadowRenderer.SetDissolvePixelSize(0.0625f, 0.0625f);

		shard.object = std::make_unique<GameObject>();
		shard.object->GetTransform().SetPosition(position);
		auto& sprite = shard.object->AddComponent<AsepriteSpriteComponent>();
		if (sprite.Setup(
			*pRenderer_, "Assets/Images/aseprite/Square.aseprite",
			Slime5JumpShardSpriteHeight))
		{
			sprite.Play("Idle", true);
		}

		auto& hitbox = shard.object->AddComponent<UnitHitboxComponent>();
		hitbox.Setup(
			*pRenderer_,
			Slime5JumpShardHitboxRadius,
			Slime5JumpShardHitboxHeight,
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			false);
		auto& health = shard.object->AddComponent<UnitHealthComponent>(Slime5JumpShardHealth);
		health.SetBarVisible(false);
		auto& controller = shard.object->AddComponent<Slime5JumpShardComponent>(
			*shard.shadow, shadowRenderer, sprite, health, direction);

		shard.controller = &controller;
		shard.hitbox = &hitbox;
		shard.health = &health;
		shard.targetIndex = AcquireEnemyTargetSlot({
			shard.object.get(), &hitbox, &hitbox, &health, &controller,
			true, false, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, -1
		});
		slime5JumpShards_.push_back(std::move(shard));
	}
}

void SceneManager::QueueDamageFloorSpawn(
	const DirectX::XMFLOAT3& position, float delay)
{
	queuedDamageFloorSpawns_.push_back({ position, std::max(delay, 0.0f) });
}

void SceneManager::UpdateDamageFloors(float deltaTime)
{
	for (auto& queuedSpawn : queuedDamageFloorSpawns_)
	{
		queuedSpawn.delay -= deltaTime;
	}
	for (size_t index = 0; index < queuedDamageFloorSpawns_.size();)
	{
		if (queuedDamageFloorSpawns_[index].delay > 0.0f)
		{
			++index;
			continue;
		}
		SpawnDamageFloor(queuedDamageFloorSpawns_[index].position);
		queuedDamageFloorSpawns_.erase(queuedDamageFloorSpawns_.begin() + index);
	}

	for (auto& floor : damageFloors_)
	{
		if (!floor.object || !floor.controller) continue;
		floor.object->Update();
	}

	damageFloors_.erase(std::remove_if(
		damageFloors_.begin(), damageFloors_.end(),
		[this](const DamageFloorInstance& floor)
		{
			if (!floor.controller || !floor.controller->IsExpired()) return false;
			ReleaseEnemyTargetSlot(floor.targetIndex);
			return true;
		}), damageFloors_.end());
}

void SceneManager::UpdateSlime5JumpShards()
{
	for (auto& shard : slime5JumpShards_)
	{
		if (shard.shadow) shard.shadow->Update();
		if (shard.object) shard.object->Update();
	}

	slime5JumpShards_.erase(std::remove_if(
		slime5JumpShards_.begin(), slime5JumpShards_.end(),
		[this](const Slime5JumpShardInstance& shard)
		{
			if (!shard.controller || !shard.controller->IsExpired()) return false;
			ReleaseEnemyTargetSlot(shard.targetIndex);
			return true;
		}), slime5JumpShards_.end());
}

void SceneManager::TryHitPlayerWithDamageFloor()
{
	if (!player_ || !playerController_ || !playerHitbox_) return;

	const auto& playerPosition = player_->GetTransform().GetPosition();
	if (playerPosition.y > DamageFloorY + 0.08f) return;

	for (const auto& floor : damageFloors_)
	{
		if (!floor.object || !floor.controller || !floor.controller->CanDamagePlayer()) continue;

		const float offsetX = playerPosition.x - floor.center.x;
		const float offsetZ = playerPosition.z - floor.center.z;
		if (std::abs(offsetX) > floor.halfWidth + playerHitbox_->GetRadius()) continue;
		if (std::abs(offsetZ) > floor.halfDepth + playerHitbox_->GetDepthRadius()) continue;

		float knockbackX = offsetX;
		float knockbackZ = offsetZ;
		const float length = std::sqrt(knockbackX * knockbackX + knockbackZ * knockbackZ);
		if (length > 0.0001f)
		{
			knockbackX /= length;
			knockbackZ /= length;
		}
		else
		{
			knockbackX = 0.0f;
			knockbackZ = 1.0f;
		}

		auto impactPosition = playerPosition;
		impactPosition.y += playerHitbox_->GetHeight() * 0.55f;
		if (playerController_->ReceiveHazardDamage(
			{ knockbackX, knockbackZ }, 2, impactPosition))
		{
			break;
		}
	}
}

void SceneManager::Update()
{
	const auto now = std::chrono::steady_clock::now();
	const float deltaTime = std::min(
		std::chrono::duration<float>(now - previousUpdate_).count(), 0.05f);
	previousUpdate_ = now;

    const bool escKey = IsKeyDown(VK_ESCAPE);
    const bool escPressed = escKey && !previousEscKey_;
    if (sceneState_ == SceneState::Gameplay && escPressed && !IsOptionMenuActive())
    {
        OpenOptionMenu();
    }

    UpdateOptionMenu(deltaTime);

    if (!bgmStarted_)
    {
        bgmStartDelayRemaining_ = std::max(0.0f, bgmStartDelayRemaining_ - deltaTime);
        if (bgmStartDelayRemaining_ <= 0.0f)
        {
            bgmStarted_ = SoundPlayer::PlayBgm(BgmSoundPath, true);
        }
    }

    if (IsOptionMenuActive())
    {
        previousEscKey_ = escKey;
        previousUpKey_ = IsKeyDown('W') || IsKeyDown(VK_UP);
        previousDownKey_ = IsKeyDown('S') || IsKeyDown(VK_DOWN);
        previousLeftKey_ = IsKeyDown('A') || IsKeyDown(VK_LEFT);
        previousRightKey_ = IsKeyDown('D') || IsKeyDown(VK_RIGHT);
        previousConfirmKey_ =
            IsKeyDown(VK_RETURN) || IsKeyDown(VK_SPACE) || IsKeyDown('J') || IsKeyDown('Z');
        return;
    }

    if (sceneState_ == SceneState::Title)
    {
        if (ground_) ground_->Update();
        if (stageTitle_) stageTitle_->Update();
        UpdateTitleScreen(deltaTime);
        previousEscKey_ = escKey;
        previousUpKey_ = IsKeyDown('W') || IsKeyDown(VK_UP);
        previousDownKey_ = IsKeyDown('S') || IsKeyDown(VK_DOWN);
        previousLeftKey_ = IsKeyDown('A') || IsKeyDown(VK_LEFT);
        previousRightKey_ = IsKeyDown('D') || IsKeyDown(VK_RIGHT);
        previousConfirmKey_ =
            IsKeyDown(VK_RETURN) || IsKeyDown(VK_SPACE) || IsKeyDown('J') || IsKeyDown('Z');
        return;
    }

	camera_.Update();
	if (cube_)
	{
		cube_->Update();
	}
	if (ground_)
	{
		ground_->Update();
	}
	if (stageTitle_)
	{
		stageTitle_->Update();
	}
	if (shadow_)
	{
		shadow_->Update();
	}
	if (player_)
	{
		player_->Update();
	}
	UpdateDamageFloors(deltaTime);
	UpdateSlime5JumpShards();
	TryHitPlayerWithDamageFloor();
	for (auto& enemy : enemies_)
	{
		enemy.shadow->Update();
		if (enemy.telegraphArea) enemy.telegraphArea->Update();
		if (enemy.radialTelegraphArea) enemy.radialTelegraphArea->Update();
		enemy.object->Update();
		enemy.healthBar->Update();
	}
	if (attackEffect_)
	{
		attackEffect_->Update();
	}
	if (impactParticles_)
	{
		impactParticles_->Update();
	}
	if (playerHealthBar_) playerHealthBar_->Update();

	const bool waveDefeated = !enemies_.empty() && std::all_of(
		enemies_.begin(), enemies_.end(), [](const EnemyInstance& enemy)
		{
			return enemy.controller && enemy.controller->IsDefeated();
		});
	if (waveDefeated)
	{
		++waveEnemyCount_;
		CreateWave(waveEnemyCount_);
	}

    previousEscKey_ = escKey;
    previousUpKey_ = IsKeyDown('W') || IsKeyDown(VK_UP);
    previousDownKey_ = IsKeyDown('S') || IsKeyDown(VK_DOWN);
    previousLeftKey_ = IsKeyDown('A') || IsKeyDown(VK_LEFT);
    previousRightKey_ = IsKeyDown('D') || IsKeyDown(VK_RIGHT);
    previousConfirmKey_ =
        IsKeyDown(VK_RETURN) || IsKeyDown(VK_SPACE) || IsKeyDown('J') || IsKeyDown('Z');
}

void SceneManager::Draw()
{
	auto viewMatrix = camera_.GetViewMatrix();
	pRenderer_->SetupViewTransform(viewMatrix);

	if (cube_)
	{
		cube_->Draw(*pRenderer_);
	}
	if (ground_)
	{
		ground_->Draw(*pRenderer_);
	}
	if (stageTitle_)
	{
		stageTitle_->Draw(*pRenderer_);
	}
	if (shadow_)
	{
		shadow_->Draw(*pRenderer_);
	}
	for (auto& enemy : enemies_)
	{
		enemy.shadow->Draw(*pRenderer_);
		if (enemy.telegraphArea) enemy.telegraphArea->Draw(*pRenderer_);
		if (enemy.radialTelegraphArea) enemy.radialTelegraphArea->Draw(*pRenderer_);
	}
	for (auto& floor : damageFloors_)
	{
		if (floor.object) floor.object->Draw(*pRenderer_);
	}
	for (auto& shard : slime5JumpShards_)
	{
		if (shard.shadow) shard.shadow->Draw(*pRenderer_);
	}
	if (attackEffect_)
	{
		attackEffect_->Draw(*pRenderer_);
	}
	std::vector<GameObject*> units;
	units.reserve(enemies_.size() + slime5JumpShards_.size() + 1);
	units.push_back(player_.get());
	for (auto& enemy : enemies_) units.push_back(enemy.object.get());
	for (auto& shard : slime5JumpShards_) units.push_back(shard.object.get());
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
	if (impactParticles_)
	{
		impactParticles_->Draw(*pRenderer_);
	}
	if (playerHealthBar_) playerHealthBar_->Draw(*pRenderer_);
	for (auto& enemy : enemies_) enemy.healthBar->Draw(*pRenderer_);
    DrawTitleScreen();
    DrawOptionMenu();
}
