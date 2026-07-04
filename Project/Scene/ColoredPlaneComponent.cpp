#include "ColoredPlaneComponent.h"

#include "AssetPack.h"
#include "GameObject.h"
#include "Renderer.h"
#include "Vertex.h"

#include <algorithm>
#include <cstdint>

namespace
{
    Shader sharedDitherShader;
    ID3D11Device* sharedDitherShaderDevice = nullptr;

    void ShareShader(const Shader& source, Shader& destination)
    {
        destination.pVertexShader = source.pVertexShader;
        destination.pPixelShader = source.pPixelShader;
        destination.pInputLayout = source.pInputLayout;
        destination.pVertexShader->AddRef();
        destination.pPixelShader->AddRef();
        destination.pInputLayout->AddRef();
    }
}

ColoredPlaneComponent::ColoredPlaneComponent(GameObject& owner)
    : Component(owner)
{
}

ColoredPlaneComponent::~ColoredPlaneComponent()
{
    DX_SAFE_RELEASE(ditherBuffer_);
    DX_SAFE_RELEASE(indexBuffer_);
    DX_SAFE_RELEASE(vertexBuffer_);
}

bool ColoredPlaneComponent::Setup(
    Renderer& renderer, float width, float depth, const DirectX::XMFLOAT4& color)
{
    if (width <= 0.0f || depth <= 0.0f) return false;

    const float halfWidth = width * 0.5f;
    const float halfDepth = depth * 0.5f;
    halfWidth_ = halfWidth;
    halfDepth_ = halfDepth;

    // 長方形の中心がGameObjectの原点になるよう、XZ平面へ4頂点を配置する。
    Vertex vertices[] = {
        { -halfWidth, 0.0f, -halfDepth },
        {  halfWidth, 0.0f, -halfDepth },
        {  halfWidth, 0.0f,  halfDepth },
        { -halfWidth, 0.0f,  halfDepth }
    };
    for (auto& vertex : vertices) vertex.Color = color;

    D3D11_BUFFER_DESC vertexDescription = {};
    vertexDescription.ByteWidth = sizeof(vertices);
    vertexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    vertexDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = vertices;
    if (FAILED(renderer.GetDevice()->CreateBuffer(&vertexDescription, &vertexData, &vertexBuffer_)))
        return false;

    // 上側から見たときに表面になる頂点順で、2枚の三角形を作る。
    const std::uint16_t indices[] = { 0, 2, 1, 0, 3, 2 };
    D3D11_BUFFER_DESC indexDescription = {};
    indexDescription.ByteWidth = sizeof(indices);
    indexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices;
    if (FAILED(renderer.GetDevice()->CreateBuffer(&indexDescription, &indexData, &indexBuffer_)))
        return false;

    return CreateDitherResources(renderer);
}

bool ColoredPlaneComponent::CreateDitherResources(Renderer& renderer)
{
    if (sharedDitherShaderDevice == renderer.GetDevice() &&
        sharedDitherShader.pVertexShader)
    {
        ShareShader(sharedDitherShader, ditherShader_);
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = 32;
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(renderer.GetDevice()->CreateBuffer(
            &description, nullptr, &ditherBuffer_));
    }

    ID3DBlob* vertexBlob = nullptr;
    ID3DBlob* pixelBlob = nullptr;
    const UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    const std::wstring shaderPath = AssetPack::ResolvePath(L"Engine/Shaders/ColoredPlaneDitherShader.hlsl");
    if (FAILED(D3DCompileFromFile(
        shaderPath.c_str(), nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_4_0", flags, 0,
        &vertexBlob, nullptr))) return false;
    if (FAILED(D3DCompileFromFile(
        shaderPath.c_str(), nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_4_0", flags, 0,
        &pixelBlob, nullptr)))
    {
        DX_SAFE_RELEASE(vertexBlob);
        return false;
    }

    auto* device = renderer.GetDevice();
    HRESULT result = device->CreateVertexShader(
        vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
        nullptr, &ditherShader_.pVertexShader);
    if (SUCCEEDED(result)) result = device->CreatePixelShader(
        pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(),
        nullptr, &ditherShader_.pPixelShader);
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
            D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    if (SUCCEEDED(result)) result = device->CreateInputLayout(
        layout, _countof(layout), vertexBlob->GetBufferPointer(),
        vertexBlob->GetBufferSize(), &ditherShader_.pInputLayout);
    DX_SAFE_RELEASE(vertexBlob);
    DX_SAFE_RELEASE(pixelBlob);
    if (FAILED(result)) return false;

    sharedDitherShaderDevice = renderer.GetDevice();
    ShareShader(ditherShader_, sharedDitherShader);

    D3D11_BUFFER_DESC description = {};
    description.ByteWidth = 32;
    description.Usage = D3D11_USAGE_DYNAMIC;
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device->CreateBuffer(&description, nullptr, &ditherBuffer_));
}

void ColoredPlaneComponent::SetDissolveAmount(float amount)
{
    dissolveAmount_ = std::max(0.0f, std::min(amount, 1.0f));
}

void ColoredPlaneComponent::SetDissolvePixelSize(float width, float depth)
{
    dissolvePixelWidth_ = std::max(width, 0.0001f);
    dissolvePixelDepth_ = std::max(depth, 0.0001f);
}

void ColoredPlaneComponent::SetupWorldTransform(Renderer& renderer)
{
    auto& constantBuffer = renderer.GetRenderParam().CbTransformSet;
    DirectX::XMStoreFloat4x4(&constantBuffer.Data.Transform,
        DirectX::XMMatrixTranspose(GetOwner().GetTransform().GetWorldMatrix()));

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    auto* context = renderer.GetDeviceContext();
    if (SUCCEEDED(context->Map(constantBuffer.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        CopyMemory(mapped.pData, &constantBuffer.Data, sizeof(constantBuffer.Data));
        context->Unmap(constantBuffer.pBuffer, 0);
        context->VSSetConstantBuffers(0, 1, &constantBuffer.pBuffer);
    }
}

void ColoredPlaneComponent::Draw(Renderer& renderer)
{
    if (!visible_ || !vertexBuffer_ || !indexBuffer_) return;
    renderer.ApplyDefaultPipeline();
    SetupWorldTransform(renderer);

    auto* context = renderer.GetDeviceContext();
    context->IASetInputLayout(ditherShader_.pInputLayout);
    context->VSSetShader(ditherShader_.pVertexShader, nullptr, 0);
    context->PSSetShader(ditherShader_.pPixelShader, nullptr, 0);
    D3D11_MAPPED_SUBRESOURCE ditherMapped = {};
    if (SUCCEEDED(context->Map(
        ditherBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &ditherMapped)))
    {
        const float data[8] = {
            dissolveAmount_, halfWidth_, halfDepth_, 0.0f,
            dissolvePixelWidth_, dissolvePixelDepth_, 0.0f, 0.0f
        };
        CopyMemory(ditherMapped.pData, data, sizeof(data));
        context->Unmap(ditherBuffer_, 0);
        context->PSSetConstantBuffers(0, 1, &ditherBuffer_);
    }
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R16_UINT, 0);
    context->DrawIndexed(6, 0, 0);
}
