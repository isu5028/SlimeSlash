#pragma once

#include "HitReceiver.h"

class EnemyController : public HitReceiver
{
public:
    ~EnemyController() override = default;
    virtual void StartSpawn(int spawnIndex) = 0;
    virtual bool IsDefeated() const = 0;
};
