#include "InputManager.h"

#include <Windows.h>

void InputManager::Sample()
{
    // Windowsの仮想キーコードを毎フレーム読み取り、押下状態だけを保持する
    for (std::size_t key = 0; key < KeyCount; ++key)
    {
        current_[key] = (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
    }
}

void InputManager::Commit()
{
    // WasPressedとWasReleasedはcurrent_とprevious_の差分で判定する
    previous_ = current_;
}

void InputManager::ConsumePress(int virtualKey)
{
    if (virtualKey >= 0 && virtualKey < static_cast<int>(KeyCount))
    {
        previous_[static_cast<std::size_t>(virtualKey)] = current_[static_cast<std::size_t>(virtualKey)];
    }
}

bool InputManager::IsDown(int virtualKey) const
{
    return virtualKey >= 0 && virtualKey < static_cast<int>(KeyCount) &&
        current_[static_cast<std::size_t>(virtualKey)];
}

bool InputManager::IsPreviousDown(int virtualKey) const
{
    return virtualKey >= 0 && virtualKey < static_cast<int>(KeyCount) &&
        previous_[static_cast<std::size_t>(virtualKey)];
}

bool InputManager::WasPressed(int virtualKey) const
{
    return IsDown(virtualKey) && !IsPreviousDown(virtualKey);
}

bool InputManager::WasReleased(int virtualKey) const
{
    return !IsDown(virtualKey) && IsPreviousDown(virtualKey);
}

bool InputManager::IsAnyDown(int firstVirtualKey, int secondVirtualKey) const
{
    return IsDown(firstVirtualKey) || IsDown(secondVirtualKey);
}

bool InputManager::WasAnyPressed(int firstVirtualKey, int secondVirtualKey) const
{
    return IsAnyDown(firstVirtualKey, secondVirtualKey) &&
        !IsPreviousDown(firstVirtualKey) && !IsPreviousDown(secondVirtualKey);
}
