#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include "Shader.h"
#include "Triangle.h"
#include "RenderParam.h"

class Renderer
{
public:
    Renderer();
    ~Renderer();

    bool Initialize(HWND hWindow);
    void Terminate();
    void Draw();
    void Swap();
    ID3D11Device* GetDevice() {return pD3DDevice_;}
    ID3D11DeviceContext* GetDeviceContext() { return pImmediateContext_;}
    RenderParam& GetRenderParam() { return renderParam_; }

    bool CompileShader(const WCHAR*, const WCHAR*, Shader&);
    bool SetupViewTransform(const DirectX::XMMATRIX& viewMat);
    void ApplyDefaultPipeline();

private:
    bool initDeviceAndSwapChain(HWND hWindow);
    bool initBackBuffer();
    bool setupProjectionTransform();

    //! 機能レベルの配列
    static const UINT FEATURE_LEVELS_NUM = 4;
    D3D_FEATURE_LEVEL pFeatureLevels_[FEATURE_LEVELS_NUM] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    //! デバイス作成時に返される機能レベル
    D3D_FEATURE_LEVEL featureLevelsSupported_;

    //! デバイス
    ID3D11Device* pD3DDevice_ = nullptr;
    //! デバイスコンテキスト
    ID3D11DeviceContext* pImmediateContext_ = nullptr;
    RenderParam renderParam_;
    //! スワップチェイン
    IDXGISwapChain* pSwapChain_ = nullptr;
    //! 描画ターゲットビュー
    ID3D11RenderTargetView* pRenderTargetView_ = nullptr;
    //! ブレンドステート
    ID3D11BlendState* pBlendState_ = nullptr;

    //! ビューポート
    D3D11_VIEWPORT viewPort_[1];

    Shader defaultShader_;
    Triangle sampleTriangle_;

    UINT backBufferNum_ = 3;
    UINT screenWidth_ = 0;
    UINT screenHeight_ = 0;

    float nearClipDist_ = 0.f;
    float farClipDist_ = 0.f;
    float fov_ = 0.f;

};
