#include "ColoredBoxComponent.h"

#include "GameObject.h"
#include "Renderer.h"
#include "Vertex.h"

#include <cstdint>

ColoredBoxComponent::ColoredBoxComponent(GameObject& owner)
    : Component(owner)
{
}

ColoredBoxComponent::~ColoredBoxComponent()
{
    DX_SAFE_RELEASE(indexBuffer_);
    DX_SAFE_RELEASE(vertexBuffer_);
}

bool ColoredBoxComponent::Setup(
    Renderer& renderer,
    float width,
    float height,
    float depth,
    const DirectX::XMFLOAT4& topColor)
{
    if (width <= 0.0f || height <= 0.0f || depth <= 0.0f) return false;

    const float x = width * 0.5f;
    const float z = depth * 0.5f;
    const float bottom = -height;
    const DirectX::XMFLOAT4 sideColor = {
        topColor.x * 0.72f, topColor.y * 0.72f, topColor.z * 0.72f, topColor.w
    };
    const DirectX::XMFLOAT4 bottomColor = {
        topColor.x * 0.55f, topColor.y * 0.55f, topColor.z * 0.55f, topColor.w
    };

    Vertex vertices[] = {
        { -x, 0.0f, -z }, { x, 0.0f, -z }, { x, 0.0f, z }, { -x, 0.0f, z },
        { -x, 0.0f, -z }, { x, 0.0f, -z }, { x, bottom, -z }, { -x, bottom, -z },
        { -x, 0.0f, z }, { -x, bottom, z }, { x, bottom, z }, { x, 0.0f, z },
        { -x, 0.0f, -z }, { -x, bottom, -z }, { -x, bottom, z }, { -x, 0.0f, z },
        { x, 0.0f, -z }, { x, 0.0f, z }, { x, bottom, z }, { x, bottom, -z },
        { -x, bottom, -z }, { x, bottom, -z }, { x, bottom, z }, { -x, bottom, z },
    };
    for (int index = 0; index < 4; ++index) vertices[index].Color = topColor;
    for (int index = 4; index < 20; ++index) vertices[index].Color = sideColor;
    for (int index = 20; index < 24; ++index) vertices[index].Color = bottomColor;

    const std::uint16_t indices[] = {
        0, 2, 1, 0, 3, 2,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
    };

    D3D11_BUFFER_DESC vertexDescription = {};
    vertexDescription.ByteWidth = sizeof(vertices);
    vertexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    vertexDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = vertices;
    if (FAILED(renderer.GetDevice()->CreateBuffer(
        &vertexDescription, &vertexData, &vertexBuffer_))) return false;

    D3D11_BUFFER_DESC indexDescription = {};
    indexDescription.ByteWidth = sizeof(indices);
    indexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices;
    if (FAILED(renderer.GetDevice()->CreateBuffer(
        &indexDescription, &indexData, &indexBuffer_))) return false;

    return true;
}

void ColoredBoxComponent::SetupWorldTransform(Renderer& renderer)
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

void ColoredBoxComponent::Draw(Renderer& renderer)
{
    if (!vertexBuffer_ || !indexBuffer_) return;
    renderer.ApplyDefaultPipeline();
    SetupWorldTransform(renderer);

    auto* context = renderer.GetDeviceContext();
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R16_UINT, 0);
    context->DrawIndexed(36, 0, 0);
}
