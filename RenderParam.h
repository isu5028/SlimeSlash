#pragma once

#include "CbTransformSet.h"
#include "CbProjectionSet.h"
#include "CbViewSet.h"

struct RenderParam
{
	CbTransformSet CbTransformSet;
	CbProjectionSet CbProjectionSet;
	CbViewSet CbViewSet;

	bool Initialize(Renderer& renderer);
	void Terminate(Renderer& renderer);

private:
	bool initConstantBuffer(Renderer& renderer);
};