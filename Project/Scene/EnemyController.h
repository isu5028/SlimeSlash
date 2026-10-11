#pragma once

#include "HitReceiver.h"

class EnemyController : public HitReceiver
{
public:
    ~EnemyController() override = default;
    virtual void StartSpawn(int spawnIndex, int spawnLocationIndex = -1) = 0;
    virtual bool IsDefeated() const = 0;
};
