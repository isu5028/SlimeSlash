#include "AsepriteSpriteComponent.h"

#include "GameObject.h"
#include "Renderer.h"
#include "SpriteVertex.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    bool CompileSpriteShader(Renderer& renderer, Shader& output, std::string& error)
    {
        ID3DBlob* vertexBlob = nullptr;
        ID3DBlob* pixelBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;
        const UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

        HRESULT result = D3DCompileFromFile(L"Engine/Shaders/SpriteVertexShader.hlsl", nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_4_0", flags, 0, &vertexBlob, &errorBlob);
        if (FAILED(result))
        {
            if (errorBlob) error.assign(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
            DX_SAFE_RELEASE(errorBlob);
            return false;
        }
        DX_SAFE_RELEASE(errorBlob);

        result = renderer.GetDevice()->CreateVertexShader(vertexBlob->GetBufferPointer(),
            vertexBlob->GetBufferSize(), nullptr, &output.pVertexShader);
        if (FAILED(result)) { error = "Could not create sprite vertex shader"; DX_SAFE_RELEASE(vertexBlob); return false; }

        const D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        result = renderer.GetDevice()->CreateInputLayout(layout, _countof(layout),
            vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), &output.pInputLayout);
        DX_SAFE_RELEASE(vertexBlob);
        if (FAILED(result)) { error = "Could not create sprite input layout"; return false; }

        result = D3DCompileFromFile(L"Engine/Shaders/SpritePixelShader.hlsl", nullptr,
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
        return true;
    }
}

AsepriteSpriteComponent::AsepriteSpriteComponent(GameObject& owner)
    : Component(owner)
{
}

AsepriteSpriteComponent::~AsepriteSpriteComponent()
{
    ReleaseResources();
}

bool AsepriteSpriteComponent::Setup(Renderer& renderer, const std::string& filePath, float worldHeight)
{
    ReleaseResources();
    data_ = {};
    lastError_.clear();
    worldHeight_ = worldHeight;
    if (worldHeight <= 0.0f) { lastError_ = "Sprite world height must be positive"; return false; }
    if (!AsepriteLoader::Load(filePath, data_, lastError_)) return false;
    const auto sheetWidth = static_cast<std::uint64_t>(data_.width) * data_.frames.size();
    if (sheetWidth > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    {
        lastError_ = "Sprite sheet is wider than the Direct3D 11 texture limit";
        return false;
    }
    if (!CreateTexture(renderer) || !CreateGeometry(renderer) ||
        !CreateShaders(renderer) || !CreateSampler(renderer))
    {
        ReleaseResources();
        return false;
    }
    activeTag_ = data_.tags.empty() ? nullptr : &data_.tags.front();
    currentFrame_ = activeTag_ ? activeTag_->firstFrame : 0;
    playbackStep_ = activeTag_ && (activeTag_->direction == AsepriteLoopDirection::Reverse ||
        activeTag_->direction == AsepriteLoopDirection::PingPongReverse) ? -1 : 1;
    if (playbackStep_ < 0 && activeTag_) currentFrame_ = activeTag_->lastFrame;
    previousUpdate_ = std::chrono::steady_clock::now();
    frameElapsed_ = std::chrono::milliseconds(0);
    verticesDirty_ = true;
    initialized_ = true;
    return true;
}

bool AsepriteSpriteComponent::CreateTexture(Renderer& renderer)
{
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
    result = renderer.GetDevice()->CreateShaderResourceView(texture_, nullptr, &textureView_);
    if (FAILED(result)) { lastError_ = "Could not create sprite shader resource view"; return false; }
    return true;
}

bool AsepriteSpriteComponent::CreateGeometry(Renderer& renderer)
{
    D3D11_BUFFER_DESC vertexDescription = {};
    vertexDescription.ByteWidth = sizeof(SpriteVertex) * 4;
    vertexDescription.Usage = D3D11_USAGE_DYNAMIC;
    vertexDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(renderer.GetDevice()->CreateBuffer(&vertexDescription, nullptr, &vertexBuffer_)))
    { lastError_ = "Could not create sprite vertex buffer"; return false; }

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

bool AsepriteSpriteComponent::Play(const std::string& tagName, bool restart)
{
    const auto found = std::find_if(data_.tags.begin(), data_.tags.end(),
        [&tagName](const AsepriteTag& tag) { return tag.name == tagName; });
    if (found == data_.tags.end()) { lastError_ = "Animation tag was not found: " + tagName; return false; }
    if (!restart && activeTag_ == &*found) return true;
    activeTag_ = &*found;
    playbackStep_ = (found->direction == AsepriteLoopDirection::Reverse ||
        found->direction == AsepriteLoopDirection::PingPongReverse) ? -1 : 1;
    currentFrame_ = playbackStep_ > 0 ? found->firstFrame : found->lastFrame;
    frameElapsed_ = std::chrono::milliseconds(0);
    previousUpdate_ = std::chrono::steady_clock::now();
    verticesDirty_ = true;
    return true;
}

void AsepriteSpriteComponent::Update()
{
    if (!initialized_ || data_.frames.empty()) return;
    const auto now = std::chrono::steady_clock::now();
    frameElapsed_ += std::chrono::duration_cast<std::chrono::milliseconds>(now - previousUpdate_);
    previousUpdate_ = now;
    while (frameElapsed_.count() >= std::max<std::uint32_t>(1, data_.frames[currentFrame_].durationMs))
    {
        const auto duration = std::max<std::uint32_t>(1, data_.frames[currentFrame_].durationMs);
        frameElapsed_ -= std::chrono::milliseconds(duration);
        AdvanceFrame();
    }
}

void AsepriteSpriteComponent::AdvanceFrame()
{
    const std::size_t first = activeTag_ ? activeTag_->firstFrame : 0;
    const std::size_t last = activeTag_ ? activeTag_->lastFrame : data_.frames.size() - 1;
    const auto direction = activeTag_ ? activeTag_->direction : AsepriteLoopDirection::Forward;
    if (direction == AsepriteLoopDirection::PingPong || direction == AsepriteLoopDirection::PingPongReverse)
    {
        if (playbackStep_ > 0 && currentFrame_ >= last) playbackStep_ = -1;
        else if (playbackStep_ < 0 && currentFrame_ <= first) playbackStep_ = 1;
        currentFrame_ = static_cast<std::size_t>(static_cast<int>(currentFrame_) + playbackStep_);
    }
    else if (playbackStep_ > 0)
        currentFrame_ = currentFrame_ >= last ? first : currentFrame_ + 1;
    else
        currentFrame_ = currentFrame_ <= first ? last : currentFrame_ - 1;
    verticesDirty_ = true;
}

void AsepriteSpriteComponent::UpdateVertices(Renderer& renderer)
{
    const float aspect = static_cast<float>(data_.width) / data_.height;
    const float halfWidth = worldHeight_ * aspect * 0.5f;
    const float u0 = static_cast<float>(currentFrame_) / data_.frames.size();
    const float u1 = static_cast<float>(currentFrame_ + 1) / data_.frames.size();
    const SpriteVertex vertices[] = {
        { { -halfWidth, worldHeight_, 0.0f }, { u0, 0.0f } },
        { {  halfWidth, worldHeight_, 0.0f }, { u1, 0.0f } },
        { {  halfWidth, 0.0f, 0.0f }, { u1, 1.0f } },
        { { -halfWidth, 0.0f, 0.0f }, { u0, 1.0f } }
    };
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
    auto& constantBuffer = renderer.GetRenderParam().CbTransformSet;
    DirectX::XMStoreFloat4x4(&constantBuffer.Data.Transform,
        DirectX::XMMatrixTranspose(GetOwner().GetTransform().GetWorldMatrix()));
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    auto* context = renderer.GetDeviceContext();
    if (SUCCEEDED(context->Map(constantBuffer.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, &constantBuffer.Data, sizeof(constantBuffer.Data));
        context->Unmap(constantBuffer.pBuffer, 0);
        context->VSSetConstantBuffers(0, 1, &constantBuffer.pBuffer);
    }
}

void AsepriteSpriteComponent::Draw(Renderer& renderer)
{
    if (!initialized_) return;
    if (verticesDirty_) UpdateVertices(renderer);
    SetupWorldTransform(renderer);
    auto* context = renderer.GetDeviceContext();
    const UINT stride = sizeof(SpriteVertex);
    const UINT offset = 0;
    context->IASetInputLayout(shader_.pInputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R16_UINT, 0);
    context->VSSetShader(shader_.pVertexShader, nullptr, 0);
    context->PSSetShader(shader_.pPixelShader, nullptr, 0);
    context->PSSetShaderResources(0, 1, &textureView_);
    context->PSSetSamplers(0, 1, &sampler_);
    context->DrawIndexed(6, 0, 0);
    ID3D11ShaderResourceView* emptyView = nullptr;
    context->PSSetShaderResources(0, 1, &emptyView);
}

void AsepriteSpriteComponent::ReleaseResources()
{
    initialized_ = false;
    DX_SAFE_RELEASE(sampler_);
    DX_SAFE_RELEASE(textureView_);
    DX_SAFE_RELEASE(texture_);
    DX_SAFE_RELEASE(indexBuffer_);
    DX_SAFE_RELEASE(vertexBuffer_);
}
