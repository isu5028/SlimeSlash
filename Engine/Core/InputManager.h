#pragma once

#include <array>

class InputManager
{
public:
    // 現在のキーボード状態を取得する
    void Sample();
    // 現在の状態を次のフレームの比較対象として確定する
    void Commit();
    // メニュー処理などで押下イベントを消費済みにする
    void ConsumePress(int virtualKey);

    // 現在押されているかを返す
    bool IsDown(int virtualKey) const;
    // 前フレームでは押されておらず、今フレームで押されたかを返す
    bool WasPressed(int virtualKey) const;
    // 前フレームでは押されていて、今フレームで離されたかを返す
    bool WasReleased(int virtualKey) const;
    bool IsAnyDown(int firstVirtualKey, int secondVirtualKey) const;
    bool WasAnyPressed(int firstVirtualKey, int secondVirtualKey) const;

private:
    static constexpr std::size_t KeyCount = 256;
    bool IsPreviousDown(int virtualKey) const;
    std::array<bool, KeyCount> current_ = {};
    std::array<bool, KeyCount> previous_ = {};
};
