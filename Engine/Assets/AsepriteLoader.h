#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Asepriteの1フレーム分の完成画像
struct AsepriteFrame
{
    // このフレームを表示する時間（ミリ秒）Aseprite側で設定した値を保持する
    std::uint32_t durationMs = 0;
    // 幅×高さ×4バイトのRGBA画像。レイヤー合成まで完了した状態
    std::vector<std::uint8_t> rgba;
};

// Asepriteのアニメーションタグに設定できる再生方向
enum class AsepriteLoopDirection
{
    Forward,
    Reverse,
    PingPong,
    PingPongReverse
};

// 「Idle」「Walk」など、Asepriteで付けたアニメーションタグ
struct AsepriteTag
{
    std::string name;
    // タグに含まれる先頭・末尾フレーム 両端を含む
    std::uint16_t firstFrame = 0;
    std::uint16_t lastFrame = 0;
    AsepriteLoopDirection direction = AsepriteLoopDirection::Forward;
    // 0は無限ループを表す
    std::uint16_t repeatCount = 0;
};

// 1つの.asepriteファイルをゲームで扱いやすい形に変換したデータ
struct AsepriteData
{
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<AsepriteFrame> frames;
    std::vector<AsepriteTag> tags;
};

class AsepriteLoader
{
public:
    // RGBA形式の.asepriteファイルを読み込み、表示レイヤーをフレームごとに合成する
    // 成功時はtrue、失敗時はfalseを返し、errorMessageへ原因を格納する
    static bool Load(
        const std::string& filePath,
        AsepriteData& output,
        std::string& errorMessage);
    static bool Load(
        const std::wstring& filePath,
        AsepriteData& output,
        std::string& errorMessage);
};
