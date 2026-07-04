#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#define DX_SAFE_RELEASE(x) {if(x){(x)->Release();(x)=nullptr;}}
#define SAFE_DELETE(p) { if (p) { delete p; p = nullptr; } }

#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] p; p = nullptr; } }

#include "Vertex.h"
#include "Triangle.h"
#include "Renderer.h"
#include "CbTransform.h"
#include "CbTransformSet.h"
#include "CbProjection.h"
#include "CbProjectionSet.h"
#include "CbView.h"
#include "CbViewSet.h"

using namespace DirectX;

class Renderer;
