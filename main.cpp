#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

const UINT WindowWidth = 800;
const UINT WindowHeight = 600;

const char* shaderSource = R"(
struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

VS_OUTPUT VS(uint id : SV_VertexID) {
    VS_OUTPUT output;
    output.Tex = float2((id << 1) & 2, id & 2);
    output.Pos = float4(output.Tex * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

Texture2D tex : register(t0);
SamplerState sam : register(s0);

float4 PS(VS_OUTPUT input) : SV_Target {
    return tex.Sample(sam, input.Tex);
}
)";

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int main() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        WindowProc,
        0L,
        0L,
        hInstance,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"D3D11Window",
        nullptr,
    };
    RegisterClassEx(&wc);

    RECT wr = { 0, 0, static_cast<LONG>(WindowWidth), static_cast<LONG>(WindowHeight) };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowEx(0, L"D3D11Window", L"BC7 Sample", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
                               nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, SW_SHOWDEFAULT);

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    D3D_FEATURE_LEVEL level;

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = WindowWidth;
    scd.BufferDesc.Height = WindowHeight;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &scd, &swapChain, &device, &level, &context
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 Device." << std::endl;
        return 1;
    }

    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    backBuffer->Release();

    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(WindowWidth), static_cast<float>(WindowHeight), 0.0f, 1.0f };
    DirectX::ScratchImage image;

    ID3D11ShaderResourceView* textureSRV = nullptr;
    HRSRC hResource = FindResource(nullptr, MAKEINTRESOURCE(101), RT_RCDATA);
    if (!hResource) {
        std::cerr << "Failed to get BC7 texture." << std::endl;
        return 1;
    }

    HGLOBAL hResourceData = LoadResource(nullptr, hResource);
    DWORD resourceSize = SizeofResource(nullptr, hResource);
    void* pResourceBuffer = LockResource(hResourceData);

    hr = DirectX::LoadFromDDSMemory(
        static_cast<const uint8_t*>(pResourceBuffer),
        static_cast<size_t>(resourceSize),
        DirectX::DDS_FLAGS_NONE,
        nullptr,
        image
    );
    if (SUCCEEDED(hr)) {
        hr = DirectX::CreateShaderResourceView(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), &textureSRV);
    } else {
        std::cerr << "Failed to load BC7 texture." << std::endl;
        return 1;
    }

    ID3D11SamplerState* samplerState = nullptr;
    D3D11_SAMPLER_DESC sampDesc = {
        .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
    };
    device->CreateSamplerState(&sampDesc, &samplerState);

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_5_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps);

    vsBlob->Release();
    psBlob->Release();

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
            context->ClearRenderTargetView(rtv, clearColor);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context->VSSetShader(vs, nullptr, 0);
            context->PSSetShader(ps, nullptr, 0);
            context->PSSetShaderResources(0, 1, &textureSRV);
            context->PSSetSamplers(0, 1, &samplerState);
            context->OMSetRenderTargets(1, &rtv, nullptr);
            context->RSSetViewports(1, &viewport);
            context->Draw(3, 0);
            swapChain->Present(1, 0);
        }
    }

    if (vs) vs->Release();
    if (ps) ps->Release();
    if (samplerState) samplerState->Release();
    if (textureSRV) textureSRV->Release();
    if (rtv) rtv->Release();
    if (swapChain) swapChain->Release();
    if (context) context->Release();
    if (device) device->Release();

    return 0;
}
