#include "Renderer.h"
#include "AssetPack.h"

Renderer::Renderer()
    : nearClipDist_(0.1f)
    , farClipDist_(1000.f)
    , fov_(XMConvertToRadians(30.f))
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
    if (!CompileShader(L"Engine/Shaders/VertexShader.hlsl", L"Engine/Shaders/PixelShader.hlsl", defaultShader_)) return false;
    if (!initAnimatedBackground()) return false;
    if (!renderParam_.Initialize(*this)) return false;
    if (!setupProjectionTransform()) return false;

    return true;
}

/*
 * 透視投影行列を設定
 */
bool Renderer::setupProjectionTransform()
{
    XMMATRIX mat = XMMatrixPerspectiveFovLH(
        fov_,
        static_cast<float>(screenWidth_) / static_cast<float>(screenHeight_),   // アスペクト比
        nearClipDist_,
        farClipDist_);
    mat = XMMatrixTranspose(mat);

    auto& cb = GetRenderParam().CbProjectionSet;
    XMStoreFloat4x4(&cb.Data.Projection, mat);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    auto pDeviceContext = GetDeviceContext();
    // CBufferにひもづくハードウェアリソースマップ取得（ロックして取得）
    HRESULT hr = pDeviceContext->Map(
        cb.pBuffer,
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mappedResource);
    if (FAILED(hr)) {
        return false;
    }
    CopyMemory(mappedResource.pData, &cb.Data, sizeof(cb.Data));
    // マップ解除
    pDeviceContext->Unmap(cb.pBuffer, 0);

    // VSにProjectionMatrixをセット(ここで1度セットして以後不変)
    pDeviceContext->VSSetConstantBuffers(2, 1, &cb.pBuffer);

    return true;
}

bool Renderer::SetupViewTransform(const XMMATRIX& viewMat)
{
    auto& cb = GetRenderParam().CbViewSet;
    if (!cb.pBuffer || !GetDeviceContext()) return false;
    XMStoreFloat4x4(&cb.Data.View, XMMatrixTranspose(viewMat));

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    auto pDeviceContext = GetDeviceContext();
    // CBufferにひもづくハードウェアリソースマップ取得（ロックして取得）
    HRESULT hr = pDeviceContext->Map(
        cb.pBuffer,
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mappedResource);
    if (FAILED(hr)) {
        //DXTRACE_ERR(L"DrawSceneGraph failed", hr);
        return false;
    }
    CopyMemory(mappedResource.pData, &cb.Data, sizeof(cb.Data));
    // マップ解除
    pDeviceContext->Unmap(cb.pBuffer, 0);

    // VSにViewMatrixをセット
    pDeviceContext->VSSetConstantBuffers(1, 1, &cb.pBuffer);

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
    sd.SampleDesc.Count = 1; // アンチエイリアス数
    sd.SampleDesc.Quality = 0; // アンチエイリアスのクオリティ
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

    // RenderTarget0へのAlphaブレンド描画設定
    D3D11_BLEND_DESC BlendState;
    ZeroMemory(&BlendState, sizeof(D3D11_BLEND_DESC));
    BlendState.AlphaToCoverageEnable = FALSE;
    BlendState.IndependentBlendEnable = FALSE;
    BlendState.RenderTarget[0].BlendEnable = TRUE;
    BlendState.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendState.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = pD3DDevice_->CreateBlendState(&BlendState, &pBlendState_);
    if (FAILED(hr)) {
        //TRACE(L"InitDirect3D g_pD3DDevice->CreateBlendState", hr);
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

    float color[] = { 0.45490196f, 0.27843139f, 0.23921569f, 1.0f };
    pImmediateContext_->ClearRenderTargetView(pRenderTargetView_, color);

    // OMにブレンドステートオブジェクトを設定
    FLOAT BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    pImmediateContext_->OMSetBlendState(pBlendState_, BlendFactor, 0xffffffff);

    drawAnimatedBackground();

    ApplyDefaultPipeline();
}

void Renderer::ApplyDefaultPipeline()
{
    if (!pImmediateContext_) return;

    pImmediateContext_->IASetInputLayout(defaultShader_.pInputLayout);
    pImmediateContext_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pImmediateContext_->VSSetShader(defaultShader_.pVertexShader, nullptr, 0);
    pImmediateContext_->PSSetShader(defaultShader_.pPixelShader, nullptr, 0);
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

    DX_SAFE_RELEASE(pBackgroundBuffer_);

    // 取得したインターフェイスの開放
    DX_SAFE_RELEASE(pRenderTargetView_);
    DX_SAFE_RELEASE(pSwapChain_);
    DX_SAFE_RELEASE(pBlendState_);

    DX_SAFE_RELEASE(pImmediateContext_);
    DX_SAFE_RELEASE(pD3DDevice_);
}

bool Renderer::initAnimatedBackground()
{
    ID3DBlob* vertexBlob = nullptr;
    ID3DBlob* pixelBlob = nullptr;
    const std::wstring path = AssetPack::ResolvePath(L"Engine/Shaders/AnimatedBackgroundShader.hlsl");
    const UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    HRESULT result = D3DCompileFromFile(path.c_str(), nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_4_0", flags, 0,
        &vertexBlob, nullptr);
    if (SUCCEEDED(result)) result = D3DCompileFromFile(path.c_str(), nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_4_0", flags, 0,
        &pixelBlob, nullptr);
    if (SUCCEEDED(result)) result = pD3DDevice_->CreateVertexShader(
        vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), nullptr,
        &backgroundShader_.pVertexShader);
    if (SUCCEEDED(result)) result = pD3DDevice_->CreatePixelShader(
        pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(), nullptr,
        &backgroundShader_.pPixelShader);
    DX_SAFE_RELEASE(vertexBlob);
    DX_SAFE_RELEASE(pixelBlob);
    if (FAILED(result)) return false;

    D3D11_BUFFER_DESC description = {};
    description.ByteWidth = 48;
    description.Usage = D3D11_USAGE_DYNAMIC;
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(pD3DDevice_->CreateBuffer(&description, nullptr, &pBackgroundBuffer_)))
        return false;
    backgroundStartTime_ = std::chrono::steady_clock::now();
    return true;
}

void Renderer::drawAnimatedBackground()
{
    if (!pBackgroundBuffer_ || !backgroundShader_.pVertexShader) return;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(pImmediateContext_->Map(
        pBackgroundBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;

    const float time = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - backgroundStartTime_).count();
    // Muted ink and warm coral: deliberately only these two output colors.
    const float parameters[12] = {
        time, static_cast<float>(screenWidth_), static_cast<float>(screenHeight_), 0.0f,
        0.32f, 0.22f, 0.29f, 1.0f,
        0.52f, 0.36f, 0.40f, 1.0f
    };
    CopyMemory(mapped.pData, parameters, sizeof(parameters));
    pImmediateContext_->Unmap(pBackgroundBuffer_, 0);

    pImmediateContext_->IASetInputLayout(nullptr);
    pImmediateContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pImmediateContext_->VSSetShader(backgroundShader_.pVertexShader, nullptr, 0);
    pImmediateContext_->PSSetShader(backgroundShader_.pPixelShader, nullptr, 0);
    pImmediateContext_->PSSetConstantBuffers(0, 1, &pBackgroundBuffer_);
    pImmediateContext_->Draw(3, 0);
}

bool Renderer::CompileShader(const WCHAR* vsPath, const WCHAR* psPath, Shader& outShader)
{
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errBlob = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    auto* device = GetDevice();

    const auto releaseTemporaryResources = [&]()
    {
        DX_SAFE_RELEASE(errBlob);
        DX_SAFE_RELEASE(psBlob);
        DX_SAFE_RELEASE(vsBlob);
        DX_SAFE_RELEASE(inputLayout);
        DX_SAFE_RELEASE(pixelShader);
        DX_SAFE_RELEASE(vertexShader);
    };

    if (!device || !vsPath || !psPath) return false;

    // シェーダーコンパイル
    const std::wstring resolvedVsPath = AssetPack::ResolvePath(vsPath);
    const std::wstring resolvedPsPath = AssetPack::ResolvePath(psPath);

    auto hr = D3DCompileFromFile(
        resolvedVsPath.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "vs_4_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &vsBlob,
        &errBlob
    );
    DX_SAFE_RELEASE(errBlob);
    if (FAILED(hr))
    {
        releaseTemporaryResources();
        return false;
    }

    // 頂点シェーダ作成(シェーダオブジェクト作成)
    hr = device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        &vertexShader
    );
    if (FAILED(hr))
    {
        releaseTemporaryResources();
        return false;
    }

    // 入力レイアウトオブジェクト作成
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = device->CreateInputLayout(
        layout,
        _countof(layout),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &inputLayout
    );
    if (FAILED(hr))
    {
        releaseTemporaryResources();
        return false;
    }

    // ピクセルシェーダー作成
    hr = D3DCompileFromFile(
        resolvedPsPath.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "ps_4_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &psBlob,
        &errBlob
    );
    DX_SAFE_RELEASE(errBlob);
    if (FAILED(hr))
    {
        releaseTemporaryResources();
        return false;
    }

    hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &pixelShader
    );
    if (FAILED(hr))
    {
        releaseTemporaryResources();
        return false;
    }

    DX_SAFE_RELEASE(outShader.pVertexShader);
    DX_SAFE_RELEASE(outShader.pPixelShader);
    DX_SAFE_RELEASE(outShader.pInputLayout);
    outShader.pVertexShader = vertexShader;
    outShader.pPixelShader = pixelShader;
    outShader.pInputLayout = inputLayout;
    vertexShader = nullptr;
    pixelShader = nullptr;
    inputLayout = nullptr;
    releaseTemporaryResources();

    return true;
}
