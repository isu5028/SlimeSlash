#pragma once

#include "CbView.h"

struct CbViewSet
{
	CbView Data;
	ID3D11Buffer* pBuffer = nullptr;
};