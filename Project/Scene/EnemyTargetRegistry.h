#pragma once

#include "EnemyTarget.h"

#include <cstddef>
#include <vector>

class EnemyTargetRegistry
{
public:
    std::size_t Acquire(const EnemyTarget& target)
    {
        // 以前解放されたスロットを優先的に再利用して、登録配列の増加を抑える
        if (!freeIndices_.empty())
        {
            const std::size_t targetIndex = freeIndices_.back();
            freeIndices_.pop_back();
            targets_[targetIndex] = target;
            return targetIndex;
        }

        targets_.push_back(target);
        return targets_.size() - 1;
    }

    void Release(std::size_t targetIndex)
    {
        if (targetIndex >= targets_.size())
        {
            return;
        }

        // ポインタを空にしてからスロットを再利用可能にする
        targets_[targetIndex] = {};
        freeIndices_.push_back(targetIndex);
    }

    void Reserve(std::size_t capacity)
    {
        targets_.reserve(capacity);
    }

    void Clear()
    {
        targets_.clear();
        freeIndices_.clear();
    }

    std::vector<EnemyTarget>& Targets()
    {
        return targets_;
    }

    const std::vector<EnemyTarget>& Targets() const
    {
        return targets_;
    }

private:
    std::vector<EnemyTarget> targets_;
    std::vector<std::size_t> freeIndices_;
};
