#include "AsepriteSpriteComponent.h"

#include "AssetPack.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Renderer.h"
#include "SpriteVertex.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    Shader sharedSpriteShader;
    ID3D11Device* sharedSpriteShaderDevice = nullptr;

    void ShareShader(const Shader& source, Shader& destination)
    {
        destination.pVertexShader = source.pVertexShader;
        destination.pPixelShader = source.pPixelShader;
        destination.pInputLayout = source.pInputLayout;
        destination.pVertexShader->AddRef();
        destination.pPixelShader->AddRef();
        destination.pInputLayout->AddRef();
    }

    // スプライト専用の頂点・ピクセルシェーダーをHLSLからコンパイルし、
    // Direct3Dで使用できるShaderオブジェクトへ変換する
    bool CompileSpriteShader(Renderer& renderer, Shader& output, std::string& error)
    {
        if (sharedSpriteShaderDevice == renderer.GetDevice() &&
            sharedSpriteShader.pVertexShader)
        {
            ShareShader(sharedSpriteShader, output);
            return true;
        }

        ID3DBlob* vertexBlob = nullptr;
        ID3DBlob* pixelBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;
        const UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

        // 頂点シェーダーをコンパイルする。失敗時はHLSLコンパイラの内容をそのまま返す
        const std::wstring vertexShaderPath = AssetPack::ResolvePath(L"Engine/Shaders/SpriteVertexShader.hlsl");
        const std::wstring pixelShaderPath = AssetPack::ResolvePath(L"Engine/Shaders/SpritePixelShader.hlsl");

        HRESULT result = D3DCompileFromFile(vertexShaderPath.c_str(), nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_4_0", flags, 0, &vertexBlob, &errorBlob);
        if (FAILED(result))
        {
            if (errorBlob) error.assign(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
            DX_SAFE_RELEASE(errorBlob);
            return false;
        }
        DX_SAFE_RELEASE(errorBlob);

        // コンパイル済みバイトコードからGPU用の頂点シェーダーを生成する
        result = renderer.GetDevice()->CreateVertexShader(vertexBlob->GetBufferPointer(),
            vertexBlob->GetBufferSize(), nullptr, &output.pVertexShader);
        if (FAILED(result)) { error = "Could not create sprite vertex shader"; DX_SAFE_RELEASE(vertexBlob); return false; }

        // SpriteVertexのメモリ配置と、HLSL側のPOSITION/TEXCOORDを対応付ける
        const D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        result = renderer.GetDevice()->CreateInputLayout(layout, _countof(layout),
            vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), &output.pInputLayout);
        DX_SAFE_RELEASE(vertexBlob);
        if (FAILED(result)) { error = "Could not create sprite input layout"; return false; }

        // テクスチャを読み取って色を出力するピクセルシェーダーも同様に生成する
        result = D3DCompileFromFile(pixelShaderPath.c_str(), nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_4_0", flags, 0, &pixelBlob, &errorBlob);
        if (FAILED(result))
        {
            if (errorBlob) error.assign(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
            DX_SAFE_RELEASE(errorBlob);
            return false;
        }
        DX_SAFE_RELEASE(errorBlob);
        result = renderer.GetDevice()->CreatePixelShader(pixelBlob->GetBufferPointer(),
            pixelBlob->GetBufferSize(), nullptr, &output.pPixelShader);
        DX_SAFE_RELEASE(pixelBlob);
        if (FAILED(result)) { error = "Could not create sprite pixel shader"; return false; }
        sharedSpriteShaderDevice = renderer.GetDevice();
        ShareShader(output, sharedSpriteShader);
        return true;
    }
}

AsepriteSpriteComponent::AsepriteSpriteComponent(GameObject& owner)
    : Component(owner)
{
}

AsepriteSpriteComponent::~AsepriteSpriteComponent()
{
    // COMオブジェクトは自動解放されないため、コンポーネント破棄時に明示的に解放する
    ReleaseResources();
}

bool AsepriteSpriteComponent::Setup(Renderer& renderer, const std::string& filePath, float worldHeight)
{
    // 再Setupにも対応できるよう、古いGPUリソースと読み込み結果を先に破棄する
    ReleaseResources();
    data_ = {};
    lastError_.clear();
    worldHeight_ = worldHeight;
    if (worldHeight <= 0.0f) { lastError_ = "Sprite world height must be positive"; return false; }
    // .asepriteをCPU上のフレームRGBAとタグ情報へ変換する
    if (!AsepriteLoader::Load(AssetPack::ResolvePath(filePath), data_, lastError_)) return false;

    // 全フレームを横に並べるため、GPUが扱える最大テクスチャ幅を超えないか確認する
    const auto sheetWidth = static_cast<std::uint64_t>(data_.width) * data_.frames.size();
    if (sheetWidth > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    {
        lastError_ = "Sprite sheet is wider than the Direct3D 11 texture limit";
        return false;
    }
    // 描画に必要なリソースを順に作る。1つでも失敗したら作成済みのものも解放する
    if (!CreateTexture(renderer) || !CreateGeometry(renderer) ||
        !CreateShaders(renderer) || !CreateSampler(renderer) || !CreateColorBuffer(renderer))
    {
        ReleaseResources();
        return false;
    }
    // 初期アニメーションは最初のタグ。タグがなければ全フレームを対象にする
    activeTag_ = data_.tags.empty() ? nullptr : &data_.tags.front();
    currentFrame_ = activeTag_ ? activeTag_->firstFrame : 0;
    playbackStep_ = activeTag_ && (activeTag_->direction == AsepriteLoopDirection::Reverse ||
        activeTag_->direction == AsepriteLoopDirection::PingPongReverse) ? -1 : 1;
    if (playbackStep_ < 0 && activeTag_) currentFrame_ = activeTag_->lastFrame;
    previousUpdate_ = std::chrono::steady_clock::now();
    frameElapsed_ = std::chrono::milliseconds(0);
    verticesDirty_ = true;
    manualFrame_ = false;
    playbackCompleted_ = false;
    horizontalStart_ = 0.0f;
    horizontalEnd_ = 1.0f;
    initialized_ = true;
    return true;
}

bool AsepriteSpriteComponent::CreateTexture(Renderer& renderer)
{
    // 各フレームを横方向へ並べた、1枚のスプライトシートをCPUメモリ上に作る
    // 例: 48px幅×10フレームなら、480px幅の画像になる
    const UINT sheetWidth = static_cast<UINT>(data_.width * data_.frames.size());
    const UINT sheetHeight = data_.height;
    std::vector<std::uint8_t> sheet(static_cast<std::size_t>(sheetWidth) * sheetHeight * 4);
    for (std::size_t frame = 0; frame < data_.frames.size(); ++frame)
    {
        for (UINT y = 0; y < sheetHeight; ++y)
        {
            const auto source = data_.frames[frame].rgba.data() + static_cast<std::size_t>(y) * data_.width * 4;
            const auto destination = sheet.data() + (static_cast<std::size_t>(y) * sheetWidth + frame * data_.width) * 4;
            std::memcpy(destination, source, static_cast<std::size_t>(data_.width) * 4);
        }
    }
    // CPU上のスプライトシートと同じRGBA8形式で、変更しないGPUテクスチャを作る
    D3D11_TEXTURE2D_DESC description = {};
    description.Width = sheetWidth;
    description.Height = sheetHeight;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initialData = {};
    initialData.pSysMem = sheet.data();
    initialData.SysMemPitch = sheetWidth * 4;
    HRESULT result = renderer.GetDevice()->CreateTexture2D(&description, &initialData, &texture_);
    if (FAILED(result)) { lastError_ = "Could not create sprite texture"; return false; }
    // SRVは、ピクセルシェーダーからTexture2Dを読み取るための「見え方」を定義する
    result = renderer.GetDevice()->CreateShaderResourceView(texture_, nullptr, &textureView_);
    if (FAILED(result)) { lastError_ = "Could not create sprite shader resource view"; return false; }
    return true;
}

bool AsepriteSpriteComponent::CreateGeometry(Renderer& renderer)
{
    // UVはアニメーション中に変化するため、CPUから書き換え可能な頂点バッファにする
    D3D11_BUFFER_DESC vertexDescription = {};
    vertexDescription.ByteWidth = sizeof(SpriteVertex) * 4;
    vertexDescription.Usage = D3D11_USAGE_DYNAMIC;
    vertexDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(renderer.GetDevice()->CreateBuffer(&vertexDescription, nullptr, &vertexBuffer_)))
    { lastError_ = "Could not create sprite vertex buffer"; return false; }

    // 4頂点の四角形を、2枚の三角形として描くための頂点番号
    const std::uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };
    D3D11_BUFFER_DESC indexDescription = {};
    indexDescription.ByteWidth = sizeof(indices);
    indexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices;
    if (FAILED(renderer.GetDevice()->CreateBuffer(&indexDescription, &indexData, &indexBuffer_)))
    { lastError_ = "Could not create sprite index buffer"; return false; }
    return true;
}

bool AsepriteSpriteComponent::CreateShaders(Renderer& renderer)
{
    return CompileSpriteShader(renderer, shader_, lastError_);
}

bool AsepriteSpriteComponent::CreateSampler(Renderer& renderer)
{
    // POINTフィルターは隣のピクセルと色を混ぜないため、ドット絵がぼやけない
    // CLAMPはスプライトの端で隣のフレームが混入することを防ぐ
    D3D11_SAMPLER_DESC description = {};
    description.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    description.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    description.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    description.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    description.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(renderer.GetDevice()->CreateSamplerState(&description, &sampler_)))
    { lastError_ = "Could not create sprite point sampler"; return false; }
    return true;
}

bool AsepriteSpriteComponent::CreateColorBuffer(Renderer& renderer)
{
    D3D11_BUFFER_DESC description = {};
    description.ByteWidth = 48;
    description.Usage = D3D11_USAGE_DYNAMIC;
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(renderer.GetDevice()->CreateBuffer(&description, nullptr, &colorBuffer_)))
    {
        lastError_ = "Could not create sprite color buffer";
        return false;
    }
    return true;
}

bool AsepriteSpriteComponent::Play(const std::string& tagName, bool restart)
{
    // 名前が一致するAsepriteタグを検索する
    const auto found = std::find_if(data_.tags.begin(), data_.tags.end(),
        [&tagName](const AsepriteTag& tag) { return tag.name == tagName; });
    if (found == data_.tags.end()) { lastError_ = "Animation tag was not found: " + tagName; return false; }
    if (!restart && activeTag_ == &*found) return true;
    activeTag_ = &*found;
    manualFrame_ = false;
    // 逆再生系のタグは末尾から開始し、それ以外は先頭から開始する
    playbackStep_ = (found->direction == AsepriteLoopDirection::Reverse ||
        found->direction == AsepriteLoopDirection::PingPongReverse) ? -1 : 1;
    currentFrame_ = playbackStep_ > 0 ? found->firstFrame : found->lastFrame;
    frameElapsed_ = std::chrono::milliseconds(0);
    playbackCompleted_ = false;
    previousUpdate_ = std::chrono::steady_clock::now();
    verticesDirty_ = true;
    return true;
}

bool AsepriteSpriteComponent::SetFrame(std::size_t frameIndex)
{
    if (!initialized_ || frameIndex >= data_.frames.size()) return false;
    currentFrame_ = frameIndex;
    activeTag_ = nullptr;
    manualFrame_ = true;
    playbackCompleted_ = false;
    verticesDirty_ = true;
    return true;
}

bool AsepriteSpriteComponent::GetOpaqueHorizontalBounds(
    std::size_t frameIndex, float& start, float& end) const
{
    if (frameIndex >= data_.frames.size() || data_.width == 0 || data_.height == 0)
        return false;

    std::uint32_t minimumX = data_.width;
    std::uint32_t maximumX = 0;
    bool foundOpaquePixel = false;
    const auto& pixels = data_.frames[frameIndex].rgba;
    for (std::uint32_t y = 0; y < data_.height; ++y)
    {
        for (std::uint32_t x = 0; x < data_.width; ++x)
        {
            const std::size_t alphaIndex =
                (static_cast<std::size_t>(y) * data_.width + x) * 4 + 3;
            if (pixels[alphaIndex] == 0) continue;
            minimumX = std::min(minimumX, x);
            maximumX = std::max(maximumX, x);
            foundOpaquePixel = true;
        }
    }
    if (!foundOpaquePixel) return false;

    start = static_cast<float>(minimumX) / static_cast<float>(data_.width);
    end = static_cast<float>(maximumX + 1) / static_cast<float>(data_.width);
    return true;
}

void AsepriteSpriteComponent::SetHorizontalRange(float start, float end)
{
    const float clampedStart = std::max(0.0f, std::min(start, 1.0f));
    const float clampedEnd = std::max(clampedStart, std::min(end, 1.0f));
    if (horizontalStart_ == clampedStart && horizontalEnd_ == clampedEnd) return;
    horizontalStart_ = clampedStart;
    horizontalEnd_ = clampedEnd;
    verticesDirty_ = true;
}

float AsepriteSpriteComponent::GetAnimationDurationSeconds(const std::string& tagName) const
{
    const auto found = std::find_if(data_.tags.begin(), data_.tags.end(),
        [&tagName](const AsepriteTag& tag) { return tag.name == tagName; });
    if (found == data_.tags.end()) return 0.0f;

    std::uint64_t durationMs = 0;
    for (std::size_t frame = found->firstFrame; frame <= found->lastFrame; ++frame)
    {
        durationMs += data_.frames[frame].durationMs;
    }
    return static_cast<float>(durationMs) / 1000.0f;
}

void AsepriteSpriteComponent::SetFlipX(bool flip)
{
    if (flipX_ == flip) return;

    flipX_ = flip;
    verticesDirty_ = true;
}

void AsepriteSpriteComponent::SetFlash(const DirectX::XMFLOAT4& color, float amount)
{
    flashColor_ = color;
    flashAmount_ = std::max(0.0f, std::min(amount, 1.0f));
}

void AsepriteSpriteComponent::SetDissolveAmount(float amount)
{
    dissolveAmount_ = std::max(0.0f, std::min(amount, 1.0f));
}

void AsepriteSpriteComponent::Update()
{
    if (!initialized_ || data_.frames.empty()) return;

    // 前回Updateからの実時間を加算する。ゲームのFPSが変化しても再生速度は変わらない
    const auto now = std::chrono::steady_clock::now();
    if (GameTime::IsHitStopped())
    {
        previousUpdate_ = now;
        return;
    }
    if (manualFrame_ || playbackCompleted_)
    {
        previousUpdate_ = now;
        return;
    }
    frameElapsed_ += std::chrono::duration_cast<std::chrono::milliseconds>(now - previousUpdate_);
    previousUpdate_ = now;
    // 処理落ちで複数フレーム分の時間が経っていた場合に備え、whileで追いつかせる
    // duration=0は無限ループ防止のため1msとして扱う
    while (frameElapsed_.count() >= std::max<std::uint32_t>(1, data_.frames[currentFrame_].durationMs))
    {
        const auto duration = std::max<std::uint32_t>(1, data_.frames[currentFrame_].durationMs);
        frameElapsed_ -= std::chrono::milliseconds(duration);
        AdvanceFrame();
    }
}

void AsepriteSpriteComponent::AdvanceFrame()
{
    // タグがあればタグ範囲、なければファイル内の全フレームを再生範囲にする
    const std::size_t first = activeTag_ ? activeTag_->firstFrame : 0;
    const std::size_t last = activeTag_ ? activeTag_->lastFrame : data_.frames.size() - 1;
    const auto direction = activeTag_ ? activeTag_->direction : AsepriteLoopDirection::Forward;
    // PingPong系は端へ到達するたびに進行方向を反転する
    if (direction == AsepriteLoopDirection::PingPong || direction == AsepriteLoopDirection::PingPongReverse)
    {
        if (playbackStep_ > 0 && currentFrame_ >= last) playbackStep_ = -1;
        else if (playbackStep_ < 0 && currentFrame_ <= first) playbackStep_ = 1;
        currentFrame_ = static_cast<std::size_t>(static_cast<int>(currentFrame_) + playbackStep_);
    }
    // 通常・逆再生は範囲の端に到達したら反対側へ戻る
    else if (playbackStep_ > 0)
    {
        if (currentFrame_ >= last && activeTag_ && activeTag_->repeatCount != 0)
            playbackCompleted_ = true;
        else
            currentFrame_ = currentFrame_ >= last ? first : currentFrame_ + 1;
    }
    else
    {
        if (currentFrame_ <= first && activeTag_ && activeTag_->repeatCount != 0)
            playbackCompleted_ = true;
        else
            currentFrame_ = currentFrame_ <= first ? last : currentFrame_ - 1;
    }
    // 表示フレームが変わったため、次回DrawでUVを更新する
    verticesDirty_ = true;
}

void AsepriteSpriteComponent::UpdateVertices(Renderer& renderer)
{
    // 元画像の縦横比を保ったまま、指定されたworldHeightから板の幅を求める
    // 原点を足元中央にするため、Y=0から上方向へ板を作る
    const float aspect = static_cast<float>(data_.width) / data_.height;
    const float halfWidth = worldHeight_ * aspect * 0.5f;
    const float fullWidth = halfWidth * 2.0f;
    const float left = -halfWidth + fullWidth * horizontalStart_;
    const float right = -halfWidth + fullWidth * horizontalEnd_;
    // 横一列のスプライトシートから、現在フレームが占めるU座標範囲を計算する
    // V座標は全高を使用するので0～1のまま
    const float baseU0 = static_cast<float>(currentFrame_) / data_.frames.size();
    const float baseU1 = static_cast<float>(currentFrame_ + 1) / data_.frames.size();
    const float frameUWidth = baseU1 - baseU0;
    const float clippedU0 = baseU0 + frameUWidth * horizontalStart_;
    const float clippedU1 = baseU0 + frameUWidth * horizontalEnd_;
    const float u0 = flipX_ ? clippedU1 : clippedU0;
    const float u1 = flipX_ ? clippedU0 : clippedU1;
    const SpriteVertex vertices[] = {
        { { left, worldHeight_, 0.0f }, { u0, 0.0f } },
        { { right, worldHeight_, 0.0f }, { u1, 0.0f } },
        { { right, 0.0f, 0.0f }, { u1, 1.0f } },
        { { left, 0.0f, 0.0f }, { u0, 1.0f } }
    };
    // WRITE_DISCARDで古い頂点内容を捨て、新しい位置・UVをGPUバッファへ転送する
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(renderer.GetDeviceContext()->Map(vertexBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, vertices, sizeof(vertices));
        renderer.GetDeviceContext()->Unmap(vertexBuffer_, 0);
        verticesDirty_ = false;
    }
}

void AsepriteSpriteComponent::SetupWorldTransform(Renderer& renderer)
{
    // GameObjectの位置・回転・拡縮をワールド行列へまとめ、頂点シェーダーのb0へ渡す
    // HLSLの行列配置に合わせるため、転置してから格納する
    auto& constantBuffer = renderer.GetRenderParam().CbTransformSet;
    const auto& transform = GetOwner().GetTransform();
    const auto& scale = transform.GetScale();
    const auto& rotation = transform.GetRotation();
    const auto& position = transform.GetPosition();

    const auto scaleMatrix = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
    const auto rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(
        rotation.x, rotation.y, rotation.z);
    const auto pivotToOrigin = DirectX::XMMatrixTranslation(0.0f, -worldHeight_ * 0.5f, 0.0f);
    const auto originToPivot = DirectX::XMMatrixTranslation(0.0f, worldHeight_ * 0.5f, 0.0f);
    const auto translationMatrix = DirectX::XMMatrixTranslation(
        position.x, position.y, position.z);
    const auto worldMatrix =
        pivotToOrigin * scaleMatrix * rotationMatrix * originToPivot * translationMatrix;

    DirectX::XMStoreFloat4x4(&constantBuffer.Data.Transform,
        DirectX::XMMatrixTranspose(worldMatrix));
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    auto* context = renderer.GetDeviceContext();
    // CPUで作った行列を定数バッファへ書き込み、頂点シェーダーのスロット0へ接続する
    if (SUCCEEDED(context->Map(constantBuffer.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, &constantBuffer.Data, sizeof(constantBuffer.Data));
        context->Unmap(constantBuffer.pBuffer, 0);
        context->VSSetConstantBuffers(0, 1, &constantBuffer.pBuffer);
    }
}

void AsepriteSpriteComponent::Draw(Renderer& renderer)
{
    if (!initialized_ || !visible_ || horizontalEnd_ - horizontalStart_ <= 0.0001f) return;

    // アニメーションでフレームが変わったときだけ頂点UVを更新する
    if (verticesDirty_) UpdateVertices(renderer);
    SetupWorldTransform(renderer);
    auto* context = renderer.GetDeviceContext();
    const UINT stride = sizeof(SpriteVertex);
    const UINT offset = 0;
    // Input Assemblerへ頂点形式、三角形の描き方、頂点・インデックスバッファを設定する
    context->IASetInputLayout(shader_.pInputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R16_UINT, 0);
    // 使用するシェーダー、テクスチャ、ポイントサンプラーをパイプラインへ接続する
    context->VSSetShader(shader_.pVertexShader, nullptr, 0);
    context->PSSetShader(shader_.pPixelShader, nullptr, 0);
    context->PSSetShaderResources(0, 1, &textureView_);
    context->PSSetSamplers(0, 1, &sampler_);
    D3D11_MAPPED_SUBRESOURCE colorMapped = {};
    if (SUCCEEDED(context->Map(colorBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &colorMapped)))
    {
        float colorData[12] = {
            flashColor_.x, flashColor_.y, flashColor_.z, flashColor_.w,
            flashAmount_, dissolveAmount_, 0.0f, 0.0f,
            static_cast<float>(data_.width),
            static_cast<float>(data_.width * data_.frames.size()),
            static_cast<float>(data_.height), 0.0f
        };
        std::memcpy(colorMapped.pData, colorData, sizeof(colorData));
        context->Unmap(colorBuffer_, 0);
        context->PSSetConstantBuffers(0, 1, &colorBuffer_);
    }
    // 6個のインデックスを使い、板を構成する2枚の三角形を描画する
    context->DrawIndexed(6, 0, 0);

    // 次の描画やリソース更新と競合しないよう、SRVをスロットから外す
    ID3D11ShaderResourceView* emptyView = nullptr;
    context->PSSetShaderResources(0, 1, &emptyView);
}

void AsepriteSpriteComponent::ReleaseResources()
{
    // SAFE_RELEASEはRelease後にnullptrを代入するため、途中まで作成された場合にも安全
    initialized_ = false;
    DX_SAFE_RELEASE(colorBuffer_);
    DX_SAFE_RELEASE(sampler_);
    DX_SAFE_RELEASE(textureView_);
    DX_SAFE_RELEASE(texture_);
    DX_SAFE_RELEASE(indexBuffer_);
    DX_SAFE_RELEASE(vertexBuffer_);
}
