#pragma once

#include <memory>
#include <array>
#include <vector>
#include "Camera.h"
#include "EnemyController.h"
#include "EnemyTarget.h"
#include "GameObject.h"

#include <chrono>

class PlayerControllerComponent;
class ColoredPlaneComponent;
class DamageFloorComponent;
class ImpactParticleComponent;
class ImageSpriteComponent;
class Slime5JumpShardComponent;
class UnitHitboxComponent;
class UnitHealthComponent;
class WorldTextSpriteComponent;

class SceneManager
{
public:
	static constexpr bool ShowAttackHitboxes = false;
	static constexpr bool ShowUnitHitboxes = false;

	SceneManager();
	~SceneManager();

	void Initialize(Renderer& renderer);
	void Terminate();

	void Update();
	void Draw();
private:
    struct QueuedDamageFloorSpawn
    {
        DirectX::XMFLOAT3 position = {};
        float delay = 0.0f;
    };

    struct DamageFloorInstance
    {
        std::unique_ptr<GameObject> object;
        DamageFloorComponent* controller = nullptr;
        UnitHitboxComponent* hitbox = nullptr;
        UnitHealthComponent* health = nullptr;
        DirectX::XMFLOAT3 center = {};
        float halfWidth = 0.0f;
        float halfDepth = 0.0f;
        size_t targetIndex = 0;
    };

    struct Slime5JumpShardInstance
    {
        std::unique_ptr<GameObject> shadow;
        std::unique_ptr<GameObject> object;
        Slime5JumpShardComponent* controller = nullptr;
        UnitHitboxComponent* hitbox = nullptr;
        UnitHealthComponent* health = nullptr;
        size_t targetIndex = 0;
    };

	struct EnemyInstance
	{
		std::unique_ptr<GameObject> shadow;
		std::unique_ptr<GameObject> telegraphArea;
		std::unique_ptr<GameObject> radialTelegraphArea;
		std::unique_ptr<GameObject> object;
        std::unique_ptr<GameObject> healthBar;
        EnemyController* controller = nullptr;
    };

    struct AnimatedTextCharacter
    {
        std::unique_ptr<GameObject> object;
        WorldTextSpriteComponent* renderer = nullptr;
        DirectX::XMFLOAT3 basePosition = {};
        bool animatable = false;
    };

    struct AnimatedTextLine
    {
        std::vector<AnimatedTextCharacter> characters;
        float jumpHeight = 0.0f;
        float jumpDuration = 0.0f;
        float startOffset = 0.0f;
    };

	void CreateWave(int enemyCount);
	void CreateEnemy(int spawnIndex);
	void SpawnDamageFloor(const DirectX::XMFLOAT3& position);
	void SpawnDamageFloorTile(const DirectX::XMFLOAT3& position);
	void SpawnSlime5JumpShards(const DirectX::XMFLOAT3& position, float angle);
	void QueueDamageFloorSpawn(const DirectX::XMFLOAT3& position, float delay);
	void UpdateDamageFloors(float deltaTime);
	void UpdateSlime5JumpShards();
	void TryHitPlayerWithDamageFloor();
    std::size_t AcquireEnemyTargetSlot(const EnemyTarget& target);
    void ReleaseEnemyTargetSlot(std::size_t targetIndex);
	void InitializeOptionMenu(Renderer& renderer);
    void RefreshOptionMenuText();
    void UpdateOptionTrianglePositions();
    void UpdateOptionMenu(float deltaTime);
    void DrawOptionMenu();
    void InitializeTitleScreen(Renderer& renderer);
    void StartGameplay();
    void ReturnToTitle();
    void UpdateTitleScreen(float deltaTime);
    void DrawTitleScreen();
    void CreateAnimatedTextLine(
        AnimatedTextLine& line,
        Renderer& renderer,
        const std::wstring& text,
        const DirectX::XMFLOAT3& centerPosition,
        float worldHeight,
        const DirectX::XMFLOAT4& color,
        float jumpHeight,
        float jumpDuration,
        float startOffset = 0.0f);
    void UpdateAnimatedTextLine(
        AnimatedTextLine& line,
        float loopTime,
        float fadeAlpha = 1.0f);
    void DrawAnimatedTextLine(const AnimatedTextLine& line);
    void OpenOptionMenu();
    void BeginCloseOptionMenu();
    bool IsOptionMenuActive() const;

	Renderer* pRenderer_ = nullptr;
	Camera camera_;

    enum class OptionItem
    {
        MasterVolume,
        BgmVolume,
        SeVolume,
        ReturnToTitle,
        Resume
    };

    enum class OptionMenuState
    {
        Closed,
        FadingIn,
        Open,
        FadingOut
    };

    enum class SceneState
    {
        Title,
        TitleFadingOut,
        Gameplay
    };
	
	std::unique_ptr<GameObject> cube_;
	std::unique_ptr<GameObject> ground_;
	std::unique_ptr<GameObject> stageTitle_;
	std::unique_ptr<GameObject> shadow_;
	std::unique_ptr<GameObject> player_;
	std::unique_ptr<GameObject> attackEffect_;
	std::unique_ptr<GameObject> impactParticles_;
    ImpactParticleComponent* impactParticlesComponent_ = nullptr;
	std::unique_ptr<GameObject> playerHealthBar_;
    std::unique_ptr<GameObject> optionDimmer_;
    std::unique_ptr<GameObject> optionPanel_;
    std::unique_ptr<GameObject> optionTitle_;
    std::unique_ptr<GameObject> optionMasterText_;
    std::unique_ptr<GameObject> optionBgmText_;
    std::unique_ptr<GameObject> optionSeText_;
    std::unique_ptr<GameObject> optionReturnText_;
    std::unique_ptr<GameObject> optionResumeText_;
    std::array<std::unique_ptr<GameObject>, 3> optionLeftTriangles_;
    std::array<std::unique_ptr<GameObject>, 3> optionRightTriangles_;
    std::array<std::unique_ptr<GameObject>, 15> optionVolumeSquares_;
	PlayerControllerComponent* playerController_ = nullptr;
    UnitHitboxComponent* playerHitbox_ = nullptr;
    ImageSpriteComponent* optionDimmerRenderer_ = nullptr;
    ColoredPlaneComponent* optionPanelRenderer_ = nullptr;
    WorldTextSpriteComponent* optionTitleRenderer_ = nullptr;
    WorldTextSpriteComponent* optionMasterRenderer_ = nullptr;
    WorldTextSpriteComponent* optionBgmRenderer_ = nullptr;
    WorldTextSpriteComponent* optionSeRenderer_ = nullptr;
    WorldTextSpriteComponent* optionReturnRenderer_ = nullptr;
    WorldTextSpriteComponent* optionResumeRenderer_ = nullptr;
    std::array<ImageSpriteComponent*, 3> optionLeftTriangleRenderers_ = {};
    std::array<ImageSpriteComponent*, 3> optionRightTriangleRenderers_ = {};
    std::array<ImageSpriteComponent*, 15> optionVolumeSquareRenderers_ = {};
	std::vector<EnemyInstance> enemies_;
    std::vector<DamageFloorInstance> damageFloors_;
    std::vector<Slime5JumpShardInstance> slime5JumpShards_;
    std::vector<QueuedDamageFloorSpawn> queuedDamageFloorSpawns_;
	std::vector<EnemyTarget> enemyTargets_;
    std::vector<std::size_t> freeEnemyTargetIndices_;
    AnimatedTextLine titleLogoAnimatedText_;
    AnimatedTextLine titleStartAnimatedText_;
    AnimatedTextLine optionTitleAnimatedText_;
	int waveEnemyCount_ = 1;
    float bgmStartDelayRemaining_ = 0.0f;
    float bgmLoopElapsed_ = 0.0f;
    bool bgmStarted_ = false;
    SceneState sceneState_ = SceneState::Title;
    OptionMenuState optionMenuState_ = OptionMenuState::Closed;
    OptionItem optionSelection_ = OptionItem::MasterVolume;
    float optionFadeAlpha_ = 0.0f;
    float titleFadeAlpha_ = 0.0f;
    std::array<float, 3> optionLeftTrianglePulse_ = {};
    std::array<float, 3> optionRightTrianglePulse_ = {};
    float titleStartPulseTime_ = 0.0f;
    bool previousEscKey_ = false;
    bool previousUpKey_ = false;
    bool previousDownKey_ = false;
    bool previousLeftKey_ = false;
    bool previousRightKey_ = false;
    bool previousConfirmKey_ = false;
    std::chrono::steady_clock::time_point previousUpdate_ =
        std::chrono::steady_clock::now();
};
