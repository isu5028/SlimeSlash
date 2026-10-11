#include "GameObject.h"
#include "Renderer.h"

GameObject::GameObject()
	: transform_(*this)
{

}

GameObject::~GameObject()
{
}

void GameObject::Update()
{
	for (auto& component : components_)
	{
		component->Update();
	}
}

void GameObject::Draw(Renderer& renderer)
{
	for (auto& component : components_)
	{
		component->Draw(renderer);
	}
}