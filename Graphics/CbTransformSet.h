#pragma once

#include "CbTransform.h"

struct CbTransformSet
{
	CbTransform Data;
	ID3D11Buffer* pBuffer = nullptr;
};