#include "ModelComponent.h"

#include "GameObject.h"
#include "Renderer.h"

ModelComponent::ModelComponent(GameObject& owner)
	:Component(owner)
{

}

ModelComponent::~ModelComponent()
{

}

bool ModelComponent::Setup(
	Renderer& renderer, const char* filePath)
{
	return model_.Setup(renderer, filePath);
}

void ModelComponent::Draw(Renderer& renderer)
{
	SetupWorldTransform(renderer);
	model_.Draw(renderer);
}

void ModelComponent::SetupWorldTransform(Renderer& renderer)
{
	auto& cb = renderer.GetRenderParam().CbTransformSet;
	const auto worldMatrix = GetOwner().GetTransform().GetWorldMatrix();

	DirectX::XMStoreFloat4x4(
		&cb.Data.Transform, DirectX::XMMatrixTranspose(worldMatrix)
	);

	D3D11_MAPPED_SUBRESOURCE mappedResource = {};

	ID3D11DeviceContext* context = renderer.GetDeviceContext();

	const HRESULT result = context->Map(
		cb.pBuffer,
		0,
		D3D11_MAP_WRITE_DISCARD,
		0,
		&mappedResource
	);

	if (FAILED(result))
	{
		return;
	}

	CopyMemory(
		mappedResource.pData,
		&cb.Data,
		sizeof(cb.Data)
	);

	context->Unmap(cb.pBuffer, 0);
	context->VSSetConstantBuffers(0, 1, &cb.pBuffer);
}