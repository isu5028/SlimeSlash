#pragma once

#include "CbProjection.h"

struct CbProjectionSet
{
	CbProjection Data;
	ID3D11Buffer* pBuffer = nullptr;
};