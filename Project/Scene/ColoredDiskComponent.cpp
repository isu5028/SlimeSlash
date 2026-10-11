#include "ColoredDiskComponent.h"

#include "GameObject.h"
#include "Renderer.h"
#include "Vertex.h"

#include <array>
#include <cmath>
#include <cstdint>

ColoredDiskComponent::ColoredDiskComponent(GameObject& owner)
    : Component(owner)
{
}

ColoredDiskComponent::~ColoredDiskComponent()
{
    DX_SAFE_RELEASE(indexBuffer_);
    DX_SAFE_RELEASE(vertexBuffer_);
}

bool ColoredDiskComponent::Setup(
    Renderer& renderer, float radius, const DirectX::XMFLOAT4& color)
{
    if (radius <= 0.0f) return false;

    std::array<Vertex, VertexCount> vertices = {};
    vertices[0].Position = { 0.0f, 0.0f, 0.0f };
    vertices[0].Color = color;
    for (int segment = 0; segment <= SegmentCount; ++segment)
    {
        const float angle = DirectX::XM_2PI * static_cast<float>(segment) /
            static_cast<float>(SegmentCount);
        vertices[segment + 1].Position = {
            std::sin(angle) * radius,
            0.0f,
            std::cos(angle) * radius
        };
        vertices[segment + 1].Color = color;
    }

    D3D11_BUFFER_DESC vertexDescription = {};
    vertexDescription.ByteWidth = sizeof(vertices);
    vertexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    vertexDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = vertices.data();
    if (FAILED(renderer.GetDevice()->CreateBuffer(
        &vertexDescription, &vertexData, &vertexBuffer_))) return false;

    std::array<std::uint16_t, IndexCount> indices = {};
    for (int segment = 0; segment < SegmentCount; ++segment)
    {
        const int index = segment * 3;
        indices[index + 0] = 0;
        indices[index + 1] = static_cast<std::uint16_t>(segment + 1);
        indices[index + 2] = static_cast<std::uint16_t>(segment + 2);
    }

    D3D11_BUFFER_DESC indexDescription = {};
    indexDescription.ByteWidth = sizeof(indices);
    indexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices.data();
    return SUCCEEDED(renderer.GetDevice()->CreateBuffer(
        &indexDescription, &indexData, &indexBuffer_));
}

void ColoredDiskComponent::SetupWorldTransform(Renderer& renderer)
{
    auto& constantBuffer = renderer.GetRenderParam().CbTransformSet;
    DirectX::XMStoreFloat4x4(&constantBuffer.Data.Transform,
        DirectX::XMMatrixTranspose(GetOwner().GetTransform().GetWorldMatrix()));

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    auto* context = renderer.GetDeviceContext();
    if (SUCCEEDED(context->Map(
        constantBuffer.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        CopyMemory(mapped.pData, &constantBuffer.Data, sizeof(constantBuffer.Data));
        context->Unmap(constantBuffer.pBuffer, 0);
        context->VSSetConstantBuffers(0, 1, &constantBuffer.pBuffer);
    }
}

void ColoredDiskComponent::Draw(Renderer& renderer)
{
    if (!visible_ || !vertexBuffer_ || !indexBuffer_) return;
    renderer.ApplyDefaultPipeline();
    SetupWorldTransform(renderer);

    auto* context = renderer.GetDeviceContext();
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R16_UINT, 0);
    context->DrawIndexed(IndexCount, 0, 0);
}
