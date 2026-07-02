#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#define DX_SAFE_RELEASE(x) {if(x){(x)->Release();(x)=nullptr;}}

#include "Vertex.h"
#include "CbTransform.h"
#include "CbTransformSet.h"

using namespace DirectX;

class Renderer;