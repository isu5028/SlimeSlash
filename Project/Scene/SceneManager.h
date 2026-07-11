#pragma once

#include <memory>
#include <array>
#include <vector>
#include "Camera.h"
#include "AnimatedTextSystem.h"
#include "BgmPlaybackController.h"
#include "InputManager.h"
#include "GameInput.h"
#include "EnemyTargetRegistry.h"
#include "EnemyController.h"
#include "GameObject.h"
#include "SceneHazardSystem.h"
#include "UiSelectionCursor.h"

#include <chrono>

class PlayerControllerComponent;
class AsepriteSpriteComponent;
class ColoredDiskComponent;
class ColoredPlaneComponent;
class DamageFloorComponent;
class ImpactParticleComponent;
class ImageSpriteComponent;
class Slime5JumpShardComponent;
class UnitHitboxComponent;
class UnitHealthComponent;
class UnitVisualComponent;
class WorldTextSpriteComponent;
class HealthBarComponent;

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
	struct EnemyInstance
	{
		std::unique_ptr<GameObject> shadow;
		std::unique_ptr<GameObject> telegraphArea;
        std::unique_ptr<GameObject> radialTelegraphArea;
        std::unique_ptr<GameObject> object;
        std::unique_ptr<GameObject> healthBar;
        UnitVisualComponent* visual = nullptr;
        ColoredPlaneComponent* telegraphRenderer = nullptr;
        ColoredDiskComponent* radialTelegraphRenderer = nullptr;
        HealthBarComponent* healthBarComponent = nullptr;
        EnemyController* controller = nullptr;
    };

    using AnimatedTextLine = AnimatedTextSystem::Line;

    enum class TutorialStep : int;

	void CreateWave(int waveNumber);
	void CreateEnemy(int spawnIndex);
    enum class EnemyType
    {
        Auto,
        Slime,
        Slime2,
        Slime3,
        Slime4,
        Slime5
    };
    void CreateEnemyOfType(int spawnIndex, EnemyType enemyType);
    void InitializeOptionMenu(Renderer& renderer);
    void InitializeContinuePrompt(Renderer& renderer);
    void RefreshOptionMenuText();
    void RefreshContinuePromptText();
    void OpenClearPrompt();
    void RefreshTitleMenuText();
    void UpdateOptionTrianglePositions();
    float GetOptionCursorTargetY() const;
    float GetContinueCursorTargetY() const;
    float GetTitleCursorTargetY() const;
    void UpdateOptionCursor();
    void UpdateContinuePrompt(float deltaTime);
    void UpdateContinueCursor();
    void UpdateTitleCursor();
    void DrawContinuePrompt();
    void DrawClearPrompt();
    void OpenContinuePrompt();
    void UpdateOptionMenu(float deltaTime);
    void DrawOptionMenu();
    void InitializeTitleScreen(Renderer& renderer);
    void StartGameplay(
        bool startTutorial = true,
        int startingWave = 1,
        TutorialStep tutorialRestartStep = TutorialStep::PlayerIntro);
    void BeginReturnToTitle();
    void StartReturnToTitleTransition();
    void ReturnToTitle();
    void UpdateGameplayFadeOut(float deltaTime);
    void ApplyGameplayFadeOut(float fadeAmount);
    void UpdateTitleScreen(float deltaTime);
    void DrawTitleScreen();
    void InitializeTutorialText(Renderer& renderer);
    void UpdateTutorial(float deltaTime);
    void UpdateTutorialHintAnimations(float deltaTime);
    void SpawnTutorialEnemy(EnemyType enemyType);
    void ClearEnemies();
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

    enum class ContinueChoice
    {
        Yes,
        No
    };

    enum class TitleMenuItem
    {
        Start,
        Option,
        Quit
    };

    enum class SceneState
    {
        Title,
        TitleFadingOut,
        GameplayFadingToTitle,
        Gameplay
    };

    enum class TutorialStep : int
    {
        Inactive,
        PlayerIntro,
        MoveAttack,
        MoveAttackFadeOut,
        DodgeDelay,
        Dodge,
        DodgeFadeOut,
        JumpDelay,
        Jump,
        JumpFadeOut,
        CompleteDelay
    };

    enum class TutorialHint
    {
        None,
        MoveAndAttack,
        Dodge,
        Jump
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
    HealthBarComponent* playerHealthBarComponent_ = nullptr;
    std::unique_ptr<GameObject> optionDimmer_;
    std::unique_ptr<GameObject> optionPanel_;
    std::unique_ptr<GameObject> optionTitle_;
    UiSelectionCursor optionCursor_;
    UiSelectionCursor continueCursor_;
    UiSelectionCursor titleCursor_;
    std::unique_ptr<GameObject> optionMasterText_;
    std::unique_ptr<GameObject> optionBgmText_;
    std::unique_ptr<GameObject> optionSeText_;
    std::unique_ptr<GameObject> optionReturnText_;
    std::unique_ptr<GameObject> optionResumeText_;
    std::array<std::unique_ptr<GameObject>, 3> optionLeftTriangles_;
    std::array<std::unique_ptr<GameObject>, 3> optionRightTriangles_;
    std::array<std::unique_ptr<GameObject>, 15> optionVolumeSquares_;
	PlayerControllerComponent* playerController_ = nullptr;
    UnitVisualComponent* playerVisual_ = nullptr;
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
    EnemyTargetRegistry enemyTargetRegistry_;
    SceneHazardSystem hazardSystem_;
    InputManager input_;
    GameInput gameInput_;
    AnimatedTextSystem animatedTextSystem_;
    AnimatedTextLine titleLogoAnimatedText_;
    AnimatedTextLine titleStartAnimatedText_;
    AnimatedTextLine titleOptionAnimatedText_;
    AnimatedTextLine titleQuitAnimatedText_;
    AnimatedTextLine optionTitleAnimatedText_;
    AnimatedTextLine tutorialMoveHintAnimatedText_;
    AnimatedTextLine tutorialAttackHintAnimatedText_;
    AnimatedTextLine tutorialDodgeHintAnimatedText_;
    AnimatedTextLine tutorialJumpHintAnimatedText_;
    AnimatedTextLine continuePromptAnimatedText_;
    AnimatedTextLine continueYesAnimatedText_;
    AnimatedTextLine continueNoAnimatedText_;
    AnimatedTextLine clearTitleAnimatedText_;
    AnimatedTextLine clearSummaryAnimatedText_;
    AnimatedTextLine clearBackToTitleAnimatedText_;
	int waveEnemyCount_ = 1;
    BgmPlaybackController bgmPlayback_;
    SceneState sceneState_ = SceneState::Title;
    OptionMenuState optionMenuState_ = OptionMenuState::Closed;
    OptionItem optionSelection_ = OptionItem::MasterVolume;
    ContinueChoice continueSelection_ = ContinueChoice::Yes;
    TitleMenuItem titleSelection_ = TitleMenuItem::Start;
    float optionFadeAlpha_ = 0.0f;
    float optionCursorIdleTime_ = 0.0f;
    float optionCursorCurrentY_ = 0.0f;
    float continueCursorIdleTime_ = 0.0f;
    float continueCursorCurrentY_ = 0.0f;
    float continuePromptAlpha_ = 0.0f;
    float clearPromptAlpha_ = 0.0f;
    float clearBackToTitleAlpha_ = 0.0f;
    float titleCursorIdleTime_ = 0.0f;
    float titleCursorCurrentY_ = 0.0f;
    float titleFadeAlpha_ = 0.0f;
    float gameplayFadeOutAlpha_ = 0.0f;
    float gameplayFadeOutHoldRemaining_ = 0.0f;
    float tutorialHintFadeAlpha_ = 0.0f;
    float tutorialHintTargetAlpha_ = 0.0f;
    float tutorialTransitionDelayRemaining_ = 0.0f;
    float clearBackToTitleDelayRemaining_ = 0.0f;
    float clearBackToTitleInputDelayRemaining_ = 0.0f;
    std::array<float, 3> optionLeftTrianglePulse_ = {};
    std::array<float, 3> optionRightTrianglePulse_ = {};
    bool pendingReturnToTitle_ = false;
    bool pendingWaveSpawnAfterIntro_ = false;
    bool continuePromptActive_ = false;
    bool clearPromptActive_ = false;
    bool continueRestartTutorial_ = false;
    bool tutorialActive_ = false;
    TutorialStep tutorialStep_ = TutorialStep::Inactive;
    TutorialStep continueRestartTutorialStep_ = TutorialStep::PlayerIntro;
    TutorialHint tutorialHint_ = TutorialHint::None;
    int continueCountThisRun_ = 0;
    int continueRestartWaveCount_ = 1;
    UnitHealthComponent* playerHealth_ = nullptr;
    std::chrono::steady_clock::time_point previousUpdate_ =
        std::chrono::steady_clock::now();
};
