#include "WorldTextSpriteComponent.h"

#include "AssetPack.h"
#include "GameObject.h"
#include "Renderer.h"
#include "SpriteVertex.h"

#include <Windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
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
        const std::wstring vertexShaderPath =
            AssetPack::ResolvePath(L"Engine/Shaders/SpriteVertexShader.hlsl");
        const std::wstring pixelShaderPath =
            AssetPack::ResolvePath(L"Engine/Shaders/SpritePixelShader.hlsl");

        HRESULT result = D3DCompileFromFile(
            vertexShaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "main", "vs_4_0", flags, 0, &vertexBlob, &errorBlob);
        if (FAILED(result))
        {
            if (errorBlob)
            {
                error.assign(
                    static_cast<const char*>(errorBlob->GetBufferPointer()),
                    errorBlob->GetBufferSize());
            }
            DX_SAFE_RELEASE(errorBlob);
            return false;
        }
        DX_SAFE_RELEASE(errorBlob);

        result = renderer.GetDevice()->CreateVertexShader(
            vertexBlob->GetBufferPointer(),
            vertexBlob->GetBufferSize(),
            nullptr,
            &output.pVertexShader);
        if (FAILED(result))
        {
            error = "Could not create text sprite vertex shader";
            DX_SAFE_RELEASE(vertexBlob);
            return false;
        }

        const D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        result = renderer.GetDevice()->CreateInputLayout(
            layout, _countof(layout),
            vertexBlob->GetBufferPointer(),
            vertexBlob->GetBufferSize(),
            &output.pInputLayout);
        DX_SAFE_RELEASE(vertexBlob);
        if (FAILED(result))
        {
            error = "Could not create text sprite input layout";
            return false;
        }

        result = D3DCompileFromFile(
            pixelShaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "main", "ps_4_0", flags, 0, &pixelBlob, &errorBlob);
        if (FAILED(result))
        {
            if (errorBlob)
            {
                error.assign(
                    static_cast<const char*>(errorBlob->GetBufferPointer()),
                    errorBlob->GetBufferSize());
            }
            DX_SAFE_RELEASE(errorBlob);
            return false;
        }
        DX_SAFE_RELEASE(errorBlob);

        result = renderer.GetDevice()->CreatePixelShader(
            pixelBlob->GetBufferPointer(),
            pixelBlob->GetBufferSize(),
            nullptr,
            &output.pPixelShader);
        DX_SAFE_RELEASE(pixelBlob);
        if (FAILED(result))
        {
            error = "Could not create text sprite pixel shader";
            return false;
        }

        sharedSpriteShaderDevice = renderer.GetDevice();
        ShareShader(output, sharedSpriteShader);
        return true;
    }

    class GdiplusSession
    {
    public:
        GdiplusSession()
        {
            Gdiplus::GdiplusStartupInput startupInput;
            Gdiplus::GdiplusStartup(&token_, &startupInput, nullptr);
        }

        ~GdiplusSession()
        {
            if (token_ != 0) Gdiplus::GdiplusShutdown(token_);
        }

    private:
        ULONG_PTR token_ = 0;
    };

    GdiplusSession& GetGdiplusSession()
    {
        static GdiplusSession session;
        return session;
    }

    BYTE ToByte(float value)
    {
        const float scaled = std::max(0.0f, std::min(value, 1.0f)) * 255.0f;
        return static_cast<BYTE>(scaled + 0.5f);
    }
}

WorldTextSpriteComponent::WorldTextSpriteComponent(GameObject& owner)
    : Component(owner)
{
}

WorldTextSpriteComponent::~WorldTextSpriteComponent()
{
    ReleaseResources();
}

bool WorldTextSpriteComponent::Setup(
    Renderer& renderer,
    const std::string& fontPath,
    const std::wstring& text,
    float worldHeight,
    const DirectX::XMFLOAT4& color)
{
    ReleaseResources();
    worldHeight_ = worldHeight;
    color_ = color;
    visible_ = true;
    textContentLeftPixels_ = 0.0f;
    textContentWidthPixels_ = 0.0f;
    if (worldHeight <= 0.0f || text.empty()) return false;

    const std::wstring resolvedFontPath = AssetPack::ResolvePath(fontPath);
    if (!CreateTextTexture(renderer, resolvedFontPath, text, color) ||
        !CreateGeometry(renderer) ||
        !CreateShaders(renderer) ||
        !CreateSampler(renderer) ||
        !CreateColorBuffer(renderer))
    {
        ReleaseResources();
        return false;
    }

    UpdateVertices(renderer);
    initialized_ = true;
    return true;
}

bool WorldTextSpriteComponent::CreateTextTexture(
    Renderer& renderer,
    const std::wstring& fontFilePath,
    const std::wstring& text,
    const DirectX::XMFLOAT4& color)
{
    GetGdiplusSession();

    Gdiplus::PrivateFontCollection fontCollection;
    if (fontCollection.AddFontFile(fontFilePath.c_str()) != Gdiplus::Ok)
    {
        lastError_ = "Could not load font file";
        return false;
    }
    if (fontCollection.GetFamilyCount() == 0)
    {
        lastError_ = "Font file did not contain a usable family";
        return false;
    }

    Gdiplus::FontFamily family;
    INT familyCount = 0;
    fontCollection.GetFamilies(1, &family, &familyCount);
    if (familyCount <= 0)
    {
        lastError_ = "Font family could not be resolved";
        return false;
    }

    const Gdiplus::REAL fontPixelSize = 140.0f;
    Gdiplus::Font font(&family, fontPixelSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::StringFormat stringFormat(Gdiplus::StringFormat::GenericTypographic());
    stringFormat.SetFormatFlags(
        stringFormat.GetFormatFlags() |
        Gdiplus::StringFormatFlagsNoClip |
        Gdiplus::StringFormatFlagsNoFitBlackBox);
    stringFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
    stringFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    Gdiplus::Bitmap measureBitmap(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics measureGraphics(&measureBitmap);
    measureGraphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    Gdiplus::RectF measureBounds = {};
    measureGraphics.MeasureString(
        text.c_str(), -1, &font,
        Gdiplus::PointF(0.0f, 0.0f), &stringFormat, &measureBounds);

    const UINT paddingX = 32;
    const UINT paddingY = 24;
    textureWidth_ = std::max(1u, static_cast<UINT>(std::ceil(measureBounds.Width)) + paddingX * 2);
    textureHeight_ = std::max(1u, static_cast<UINT>(std::ceil(measureBounds.Height)) + paddingY * 2);

    Gdiplus::Bitmap bitmap(textureWidth_, textureHeight_, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(&bitmap);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

    const Gdiplus::Color textColor(
        ToByte(color.w), ToByte(color.x), ToByte(color.y), ToByte(color.z));
    Gdiplus::SolidBrush brush(textColor);
    Gdiplus::RectF layoutRect(
        0.0f, 0.0f,
        static_cast<Gdiplus::REAL>(textureWidth_),
        static_cast<Gdiplus::REAL>(textureHeight_));
    graphics.DrawString(text.c_str(), -1, &font, layoutRect, &stringFormat, &brush);

    Gdiplus::Rect lockRect(0, 0, static_cast<INT>(textureWidth_), static_cast<INT>(textureHeight_));
    Gdiplus::BitmapData bitmapData = {};
    if (bitmap.LockBits(
        &lockRect,
        Gdiplus::ImageLockModeRead,
        PixelFormat32bppARGB,
        &bitmapData) != Gdiplus::Ok)
    {
        lastError_ = "Could not access rendered text bitmap";
        return false;
    }

    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(textureWidth_) * textureHeight_ * 4);
    for (UINT y = 0; y < textureHeight_; ++y)
    {
        const auto* sourceRow = static_cast<const std::uint8_t*>(bitmapData.Scan0) +
            static_cast<std::size_t>(y) * bitmapData.Stride;
        auto* destinationRow = pixels.data() +
            static_cast<std::size_t>(y) * textureWidth_ * 4;
        std::memcpy(destinationRow, sourceRow, static_cast<std::size_t>(textureWidth_) * 4);
    }
    bitmap.UnlockBits(&bitmapData);

    UINT minContentX = textureWidth_;
    UINT maxContentX = 0;
    bool hasVisiblePixel = false;
    for (UINT y = 0; y < textureHeight_; ++y)
    {
        const auto* row = pixels.data() + static_cast<std::size_t>(y) * textureWidth_ * 4;
        for (UINT x = 0; x < textureWidth_; ++x)
        {
            const std::uint8_t alpha = row[static_cast<std::size_t>(x) * 4 + 3];
            if (alpha == 0) continue;
            minContentX = std::min(minContentX, x);
            maxContentX = std::max(maxContentX, x);
            hasVisiblePixel = true;
        }
    }

    if (hasVisiblePixel)
    {
        textContentLeftPixels_ = static_cast<float>(minContentX);
        textContentWidthPixels_ = static_cast<float>(maxContentX - minContentX + 1);
    }
    else
    {
        textContentLeftPixels_ = 0.0f;
        textContentWidthPixels_ = std::max(1.0f, measureBounds.Width);
    }

    D3D11_TEXTURE2D_DESC description = {};
    description.Width = textureWidth_;
    description.Height = textureHeight_;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initialData = {};
    initialData.pSysMem = pixels.data();
    initialData.SysMemPitch = textureWidth_ * 4;
    HRESULT result = renderer.GetDevice()->CreateTexture2D(
        &description, &initialData, &texture_);
    if (FAILED(result))
    {
        lastError_ = "Could not create text texture";
        return false;
    }

    result = renderer.GetDevice()->CreateShaderResourceView(
        texture_, nullptr, &textureView_);
    if (FAILED(result))
    {
        lastError_ = "Could not create text texture view";
        return false;
    }

    return true;
}

bool WorldTextSpriteComponent::CreateGeometry(Renderer& renderer)
{
    D3D11_BUFFER_DESC vertexDescription = {};
    vertexDescription.ByteWidth = sizeof(SpriteVertex) * 4;
    vertexDescription.Usage = D3D11_USAGE_DYNAMIC;
    vertexDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(renderer.GetDevice()->CreateBuffer(
        &vertexDescription, nullptr, &vertexBuffer_)))
    {
        lastError_ = "Could not create text vertex buffer";
        return false;
    }

    const std::uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };
    D3D11_BUFFER_DESC indexDescription = {};
    indexDescription.ByteWidth = sizeof(indices);
    indexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices;
    if (FAILED(renderer.GetDevice()->CreateBuffer(
        &indexDescription, &indexData, &indexBuffer_)))
    {
        lastError_ = "Could not create text index buffer";
        return false;
    }

    return true;
}

bool WorldTextSpriteComponent::CreateShaders(Renderer& renderer)
{
    return CompileSpriteShader(renderer, shader_, lastError_);
}

bool WorldTextSpriteComponent::CreateSampler(Renderer& renderer)
{
    D3D11_SAMPLER_DESC description = {};
    description.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    description.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    description.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    description.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    description.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(renderer.GetDevice()->CreateSamplerState(&description, &sampler_)))
    {
        lastError_ = "Could not create text sampler";
        return false;
    }
    return true;
}

bool WorldTextSpriteComponent::CreateColorBuffer(Renderer& renderer)
{
    D3D11_BUFFER_DESC description = {};
    description.ByteWidth = 48;
    description.Usage = D3D11_USAGE_DYNAMIC;
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(renderer.GetDevice()->CreateBuffer(&description, nullptr, &colorBuffer_)))
    {
        lastError_ = "Could not create text color buffer";
        return false;
    }
    return true;
}

void WorldTextSpriteComponent::UpdateVertices(Renderer& renderer)
{
    const float aspect = static_cast<float>(textureWidth_) /
        static_cast<float>(std::max(textureHeight_, 1u));
    const float width = worldHeight_ * aspect;
    float left = 0.0f;
    float right = 0.0f;
    switch (horizontalAlignment_)
    {
    case HorizontalAlignment::Right:
        left = -width;
        right = 0.0f;
        break;
    case HorizontalAlignment::Center:
    default:
        left = -width * 0.5f;
        right = width * 0.5f;
        break;
    }

    const SpriteVertex vertices[] = {
        { { left,  worldHeight_, 0.0f }, { 0.0f, 0.0f } },
        { { right, worldHeight_, 0.0f }, { 1.0f, 0.0f } },
        { { right, 0.0f,        0.0f }, { 1.0f, 1.0f } },
        { { left,  0.0f,        0.0f }, { 0.0f, 1.0f } }
    };

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(renderer.GetDeviceContext()->Map(
        vertexBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, vertices, sizeof(vertices));
        renderer.GetDeviceContext()->Unmap(vertexBuffer_, 0);
    }
}

void WorldTextSpriteComponent::SetupWorldTransform(Renderer& renderer)
{
    auto& constantBuffer = renderer.GetRenderParam().CbTransformSet;
    const auto& transform = GetOwner().GetTransform();
    const auto& scale = transform.GetScale();
    const auto& rotation = transform.GetRotation();
    const auto& position = transform.GetPosition();

    const auto scaleMatrix = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
    const auto rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(
        rotation.x, rotation.y, rotation.z);
    const auto pivotToOrigin =
        DirectX::XMMatrixTranslation(0.0f, -worldHeight_ * 0.5f, 0.0f);
    const auto originToPivot =
        DirectX::XMMatrixTranslation(0.0f, worldHeight_ * 0.5f, 0.0f);
    const auto translationMatrix = DirectX::XMMatrixTranslation(
        position.x, position.y, position.z);
    const auto worldMatrix =
        pivotToOrigin * scaleMatrix * rotationMatrix * originToPivot * translationMatrix;

    DirectX::XMStoreFloat4x4(
        &constantBuffer.Data.Transform,
        DirectX::XMMatrixTranspose(worldMatrix));
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    auto* context = renderer.GetDeviceContext();
    if (SUCCEEDED(context->Map(
        constantBuffer.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, &constantBuffer.Data, sizeof(constantBuffer.Data));
        context->Unmap(constantBuffer.pBuffer, 0);
        context->VSSetConstantBuffers(0, 1, &constantBuffer.pBuffer);
    }
}

void WorldTextSpriteComponent::Draw(Renderer& renderer)
{
    if (!visible_ || !initialized_ || !textureView_ || !vertexBuffer_ || !indexBuffer_) return;

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

    D3D11_MAPPED_SUBRESOURCE colorMapped = {};
    if (SUCCEEDED(context->Map(colorBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &colorMapped)))
    {
        const float colorData[12] = {
            color_.x, color_.y, color_.z, color_.w,
            -1.0f, 0.0f, 0.0f, 0.0f,
            static_cast<float>(textureWidth_),
            static_cast<float>(textureWidth_),
            static_cast<float>(textureHeight_), 1.0f
        };
        std::memcpy(colorMapped.pData, colorData, sizeof(colorData));
        context->Unmap(colorBuffer_, 0);
        context->PSSetConstantBuffers(0, 1, &colorBuffer_);
    }

    context->DrawIndexed(6, 0, 0);

    ID3D11ShaderResourceView* emptyView = nullptr;
    context->PSSetShaderResources(0, 1, &emptyView);
}

void WorldTextSpriteComponent::ReleaseResources()
{
    initialized_ = false;
    DX_SAFE_RELEASE(colorBuffer_);
    DX_SAFE_RELEASE(sampler_);
    DX_SAFE_RELEASE(indexBuffer_);
    DX_SAFE_RELEASE(vertexBuffer_);
    DX_SAFE_RELEASE(textureView_);
    DX_SAFE_RELEASE(texture_);
}
