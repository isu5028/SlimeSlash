#include "Renderer.h"

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
}

bool Renderer::Initialize(HWND hWindow)
{
    // Windowに合わせてスクリーンサイズ初期化
    RECT rc;
    GetClientRect(hWindow, &rc);
    screenWidth_ = rc.right - rc.left;
    screenHeight_ = rc.bottom - rc.top;

    if (!initDeviceAndSwapChain(hWindow)) return false;
    if (!initBackBuffer()) return false;
    if (!CompileShader(L"VertexShader.hlsl", L"PixelShader.hlsl", defaultShader_)) return false;
    if (!sampleTriangle_.CreateVertexBuffer(*this)) return false;
    if (!renderParam_.Initialize(*this)) return false;

    return true;
}

bool Renderer::initDeviceAndSwapChain(HWND hWindow)
{
    // デバイスとスワップチェイン作成
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = backBufferNum_; // バックバッファの数
    sd.BufferDesc.Width = screenWidth_; // バックバッファの幅
    sd.BufferDesc.Height = screenHeight_; // バックバッファの高さ
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // フォーマット
    sd.BufferDesc.RefreshRate.Numerator = 60; // リフレッシュレートの分母
    sd.BufferDesc.RefreshRate.Denominator = 1; // リフレッシュレートの分子
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_CENTERED;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // バックバッファの使用法
    sd.OutputWindow = hWindow; // 関連付けるウィンドウ
    sd.SampleDesc.Count = 1; // マルチサンプル（アンチエイリアス）の数
    sd.SampleDesc.Quality = 0; // マルチサンプル（アンチエイリアス）のクオリティ
    sd.Windowed = TRUE; // ウィンドウモード
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // モード自動切換え

#if defined(DEBUG) || defined(_DEBUG)
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#else
    UINT createDeviceFlags = 0;
#endif

    const D3D_DRIVER_TYPE DriverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };

    HRESULT hr;
    for(auto type : DriverTypes){
        // ハードウェアデバイスを作成
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, type, nullptr, createDeviceFlags,
            pFeatureLevels_, FEATURE_LEVELS_NUM, D3D11_SDK_VERSION, &sd,
            &pSwapChain_, &pD3DDevice_, &featureLevelsSupported_, &pImmediateContext_);
        if(SUCCEEDED(hr)){
            break;
        }
    }
    if(FAILED(hr)) return false;

    return true;
}

bool Renderer::initBackBuffer()
{
    HRESULT hr;

    // スワップチェインから最初のバックバッファを取得する
    ID3D11Texture2D *pBackBuffer; // バッファのアクセスに使うインターフェース
    hr = pSwapChain_->GetBuffer(
        0,
        _uuidof(ID3D11Texture2D),
        (LPVOID*)&pBackBuffer);
    if(FAILED(hr)){
        //TRACE("InitBackBuffer g_pSwapChain->GetBuffer(%0x08x)n", hr); // 失敗
        return false;
    }

    // バックバッファの描画ターゲットビューをつくる
    D3D11_TEXTURE2D_DESC descBackBuffer;
    pBackBuffer->GetDesc(&descBackBuffer);

    // バックバッファの描画ターゲットビューをつくる
    hr = pD3DDevice_->CreateRenderTargetView(
        pBackBuffer, // ビューでアクセスするリソース
        nullptr, // 描画ターゲットビューの定義
        &pRenderTargetView_); // 描画ターゲットビューを受け取る変数
    DX_SAFE_RELEASE(pBackBuffer); // 移行、バックバッファは直接使わないので解法
    if(FAILED(hr)){
        //TRACE("InitBackBuffer g_pD3DDevice->CreateRenderTargetView(%0x08x)n", hr); // 失敗
        return false;
    }

    // ビューポートの設定
    viewPort_[0].TopLeftX = 0.0f; // ビューポート領域の左上X座標
    viewPort_[0].TopLeftY = 0.0f; // ビューポート領域の左上Y座標
    viewPort_[0].Width = static_cast<float>(screenWidth_); // ビューポート領域の幅
    viewPort_[0].Height = static_cast<float>(screenHeight_); // ビューポート領域の高さ
    viewPort_[0].MinDepth = 0.0f; // ビューポート領域の深度地の最小値
    viewPort_[0].MaxDepth = 1.0f; // ビューポート領域の深度値の最大値
    pImmediateContext_->RSSetViewports(1, &viewPort_[0]);

    return true;
}

void Renderer::Draw()
{
    if(!pImmediateContext_ || !pRenderTargetView_) return;

    pImmediateContext_->OMSetRenderTargets(1, &pRenderTargetView_, nullptr);

    float color[] = {0.f, 0.f, 0.f, 0.f};
    pImmediateContext_->ClearRenderTargetView(pRenderTargetView_, color);

    pImmediateContext_->IASetInputLayout(defaultShader_.pInputLayout);
    pImmediateContext_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pImmediateContext_->VSSetShader(defaultShader_.pVertexShader, nullptr, 0);
    pImmediateContext_->PSSetShader(defaultShader_.pPixelShader, nullptr, 0);

    sampleTriangle_.Draw(*this);
}

void Renderer::Swap()
{
    // バックバッファの表示（画面をすぐに更新）
    HRESULT hr = pSwapChain_->Present(0, 0);
    if(FAILED(hr)) {
        //TRACE("Swap failed(%0x08x)n", hr);
        return;
    }
}

void Renderer::Terminate()
{
    // デバイスステートのクリア
    if(pImmediateContext_) pImmediateContext_->ClearState();

    // スワップチェインをウィンドウモードにする
    if(pSwapChain_) pSwapChain_->SetFullscreenState(FALSE, nullptr);

    // 取得したインターフェイスの開放
    DX_SAFE_RELEASE(pRenderTargetView_);
    DX_SAFE_RELEASE(pSwapChain_);

    DX_SAFE_RELEASE(pImmediateContext_);
    DX_SAFE_RELEASE(pD3DDevice_);
}

bool Renderer::CompileShader(const WCHAR* vsPath, const WCHAR* psPath, Shader& outShader)
{
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* errBlob = nullptr;
    auto pDevice = GetDevice();

    // シェーダーコンパイル
    auto hr = D3DCompileFromFile(
        vsPath,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "vs_4_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &vsBlob,
        &errBlob
    );
    if (FAILED(hr)) return false;

    // 頂点シェーダ作成(シェーダオブジェクト作成)
    ID3D11VertexShader* pVertexShader = nullptr;
    hr = pDevice->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        &pVertexShader
    );
    if (FAILED(hr)) return false;

    // 入力レイアウトオブジェクト作成
    ID3D11InputLayout* pInputLayout = nullptr;
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = pDevice->CreateInputLayout(
        layout,
        _countof(layout),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &pInputLayout
    );
    if (FAILED(hr)) return false;

    // ピクセルシェーダー作成
    ID3DBlob* psBlob = nullptr;
    hr = D3DCompileFromFile(
        psPath,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "ps_4_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &psBlob,
        &errBlob
    );

    ID3D11PixelShader* pPixelShader = nullptr;
    hr = pDevice->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &pPixelShader
    );
    if (FAILED(hr)) return false;

    outShader.pVertexShader = pVertexShader;
    outShader.pPixelShader = pPixelShader;
    outShader.pInputLayout = pInputLayout;

    return true;
}
