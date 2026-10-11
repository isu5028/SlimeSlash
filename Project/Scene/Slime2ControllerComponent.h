#pragma once

#include "Component.h"
#include "EnemyController.h"

#include <chrono>

class Camera;
class GameObject;
class PlayerControllerComponent;
class UnitVisualComponent;
class UnitHealthComponent;
class ChargeParticleComponent;
class ColoredPlaneComponent;

class Slime2ControllerComponent : public Component, public EnemyController
{
public:
    Slime2ControllerComponent(
        GameObject& owner,
        GameObject& target,
        PlayerControllerComponent& targetController,
        UnitVisualComponent& visual,
        UnitHealthComponent& health,
        ChargeParticleComponent& chargeParticles,
        GameObject& telegraphArea,
        ColoredPlaneComponent& telegraphRenderer);

    void Update() override;
    void ReceiveHit(const DirectX::XMFLOAT2& direction, float knockbackSpeed) override;
    void StartSpawn(int spawnIndex, int spawnLocationIndex = -1) override;
    bool IsDefeated() const override { return state_ == State::Hidden; }

private:
    // 出現後は溜めと予告を経て突進する
    // Charging → TelegraphPause → Dashing → Charging
    // Dashing中の被弾はInterruptedを経由してChargingへ戻る
    // DyingとEscapingは通常行動から離脱する終端側の状態
    enum class State
    {
        // 突進方向を決めながら予告する
        Charging,
        // 突進中
        Dashing,
        // 死亡演出中
        Dying,
        // 画面外へ退場中
        Escaping,
        // 退場完了
        Hidden,
        // 攻撃を被弾して一時中断中
        Interrupted,
        // 出現演出中
        Spawning,
        // 突進直前の短い停止
        TelegraphPause,
    };

    void BeginDying();
    void BeginEscape();
    void EmitChargeBurst(int count, float speedScale, float sizeScale);
    DirectX::XMFLOAT2 GetDirectionAwayFromPlayer();
    void HideTelegraph();
    void SnapAimToPlayer(const DirectX::XMFLOAT3& position);
    void UpdateTelegraph(
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT2& direction,
        float lengthScale);

    GameObject& target_;
    PlayerControllerComponent& targetController_;
    UnitVisualComponent& visual_;
    UnitHealthComponent& health_;
    ChargeParticleComponent& chargeParticles_;
    GameObject& telegraphArea_;
    ColoredPlaneComponent& telegraphRenderer_;
    State state_ = State::Charging;
    float stateElapsed_ = 0.0f;
    float hitFlashTimeRemaining_ = 0.0f;
    float knockbackTimeRemaining_ = 0.0f;
    float dashDuration_ = 0.0f;
    float telegraphPauseTime_ = 0.0f;
    float aimAngle_ = 0.0f;
    DirectX::XMFLOAT2 knockbackVelocity_ = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 dashDirection_ = { 1.0f, 0.0f };
    DirectX::XMFLOAT3 dashStart_ = {};
    DirectX::XMFLOAT3 telegraphAnchorPosition_ = {};
    DirectX::XMFLOAT3 spawnStart_ = {};
    DirectX::XMFLOAT3 spawnEnd_ = {};
    DirectX::XMFLOAT3 escapeStart_ = {};
    DirectX::XMFLOAT3 escapeEnd_ = {};
    float spawnDelayRemaining_ = 0.0f;
    float deathDuration_ = 0.0f;
    std::chrono::steady_clock::time_point previousUpdate_;
};
