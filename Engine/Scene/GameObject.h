#pragma once

#include <utility>
#include <memory>
#include <type_traits>
#include <vector>
#include "Component.h"
#include "TransformComponent.h"

class GameObject
{
public:
	GameObject();
	~GameObject();

	void Update();
	void Draw(Renderer& renderer);

	// 可変長テンプレート
	template<class T, class... Args>
	T& AddComponent(Args&&... args)
	{
		// TがComponentを継承しているか確認
		static_assert(
			std::is_base_of<Component, T>::value,
			"T must inherit from Component"
		);

		// Tを生成し、そのコンストラクタの第一引数に所有者のGameObjectを渡す
		// それ以降にはAddComponentが受け取った引数を転送
		auto component = std::make_unique<T>(
			*this,
			std::forward<Args>(args)...
		);

		//unique_ptrが指す実体への参照を取得
		T& result = *component;
		// componentが持つTの所有権をcomponents_に移す
		components_.push_back(std::move(component));
		// components_が所有するTの実体への参照を返す
		return result;
	}

	TransformComponent& GetTransform()
	{
		return transform_;
	}

private:
	TransformComponent transform_;
	std::vector<std::unique_ptr<Component>> components_;
};