#pragma once

class Mesh;

class Model
{
public:
	Model();
	~Model();

	bool Setup(Renderer& renderer, const char* filePath);
	void Terminate();

	void Draw(Renderer& renderer);

private:
	void setupTransform(Renderer& renderer);

	Mesh* meshes_ = nullptr;
	u_int meshNum_ = 0;

	float translateX_ = 0.f;
	float translateY_ = 0.f;
	float angle_ = 0.f;
	float scale_ = 1.f;
};