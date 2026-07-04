#include "Model.h"
#include "Mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

Model::Model()
{

}

Model::~Model()
{
	SAFE_DELETE_ARRAY(meshes_);
}

bool Model::Setup(Renderer& renderer, const char* filePath)
{
	// load処理
	Assimp::Importer importer;
	u_int flag = aiProcess_Triangulate;
	auto pScene = importer.ReadFile(filePath, flag);

	if (pScene == nullptr) return false;

	meshNum_ = pScene->mNumMeshes;
	if (meshNum_ > 0)
	{
		meshes_ = new Mesh[meshNum_];
		for (u_int meshIdx = 0; meshIdx < pScene->mNumMeshes; ++meshIdx)
		{
			auto pMeshData = pScene->mMeshes[meshIdx];
			if (meshes_[meshIdx].Setup(renderer, pMeshData) == false) {
				return false;
			}
		}

	}

	return true;
}

void Model::Terminate()
{
	for (u_int meshIdx = 0; meshIdx < meshNum_; ++meshIdx)
	{
		meshes_[meshIdx].Terminate();
	}
	SAFE_DELETE_ARRAY(meshes_);
	meshNum_ = 0;
}

void Model::Draw(Renderer& renderer)
{
	for (u_int meshIdx = 0; meshIdx < meshNum_; ++meshIdx)
	{
		meshes_[meshIdx].Draw(renderer);
	}
}
