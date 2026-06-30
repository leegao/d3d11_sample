#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

const UINT WindowWidth = 1080;
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

cbuffer CBuffer : register(b0) {
    uint g_Value;
    float3 padding;
};

static const uint font[10][5] = {
    {7, 5, 5, 5, 7}, // 0
    {2, 2, 2, 2, 2}, // 1
    {7, 1, 7, 4, 7}, // 2
    {7, 1, 7, 1, 7}, // 3
    {5, 5, 7, 1, 1}, // 4
    {7, 4, 7, 1, 7}, // 5
    {7, 4, 7, 5, 7}, // 6
    {7, 1, 1, 1, 1}, // 7
    {7, 5, 7, 5, 7}, // 8
    {7, 5, 7, 1, 7}  // 9
};

float SampleDigit(uint digit, float2 uv) {
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) return 0.0f;
    uint col = (uint)(uv.x * 3.0f);
    uint row = (uint)(uv.y * 5.0f);
    if (col > 2 || row > 4) return 0.0f;

    uint rowValue = font[digit][row];
    uint bit = 2 - col; // Check bits left-to-right
    return (float)((rowValue >> bit) & 1);
}

float4 PS(VS_OUTPUT input) : SV_Target {
    float4 baseColor = tex.Sample(sam, input.Tex);

    float2 textUV = (input.Tex - float2(0.05f, 0.05f)) / float2(0.15f, 0.10f);

    if (textUV.x >= 0.0f && textUV.x <= 1.0f && textUV.y >= 0.0f && textUV.y <= 1.0f) {
        uint tens = (g_Value / 10) % 10;
        uint ones = g_Value % 10;

        float isText = 0.0f;
        if (textUV.x < 0.45f) {
            float2 digitUV = float2(textUV.x / 0.45f, textUV.y);
            isText = SampleDigit(tens, digitUV);
        } else if (textUV.x > 0.55f) {
            float2 digitUV = float2((textUV.x - 0.55f) / 0.45f, textUV.y);
            isText = SampleDigit(ones, digitUV);
        }

        if (isText > 0.5f) {
            return float4(1.0f, 1.0f, 0.0f, 1.0f); // Render text overlay in bright yellow
        }
    }

    return baseColor;
}
)";

const char* csSource = R"(
RWStructuredBuffer<uint> BufferOut : register(u0);

[numthreads(8, 8, 1)]
void CS(uint3 globalID : SV_DispatchThreadID) {
    BufferOut[globalID.y * 64 + globalID.x] += 1;
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
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"D3D11Window";
    RegisterClassExW(&wc);

    RECT wr = { 0, 0, static_cast<LONG>(WindowWidth), static_cast<LONG>(WindowHeight) };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowExW(0, L"D3D11Window", L"D3D11 BC3 CopyImage", WS_OVERLAPPEDWINDOW,
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
    if (FAILED(hr)) return 1;

    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    backBuffer->Release();

    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(WindowWidth), static_cast<float>(WindowHeight), 0.0f, 1.0f };
    DirectX::ScratchImage image;

    ID3D11ShaderResourceView* textureSRV = nullptr;
    HRSRC hResource = FindResource(nullptr, MAKEINTRESOURCE(102), RT_RCDATA);
    if (!hResource) {
        std::cerr << "Failed to get BC3 texture." << std::endl;
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

    ID3D11Resource* destResource = nullptr;
    hr = DirectX::CreateTexture(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), &destResource);
    if (FAILED(hr)) return 1;

    ID3D11Texture2D* destTexture = nullptr;
    destResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&destTexture));
    destResource->Release();

    struct BC3Block {
        UINT32 channels[4];
    };
    std::vector<BC3Block> magentaPayload(16 * 16);
    for (size_t i = 0; i < magentaPayload.size(); ++i) {
        magentaPayload[i].channels[0] = 65535;
        magentaPayload[i].channels[1] = 0;
        magentaPayload[i].channels[2] = 4162844703;
        magentaPayload[i].channels[3] = 0;
    }

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = magentaPayload.data();
    initData.SysMemPitch = 16 * sizeof(BC3Block);

    D3D11_TEXTURE2D_DESC srcDesc = {};
    srcDesc.Width = 16;
    srcDesc.Height = 16;
    srcDesc.MipLevels = 1;
    srcDesc.ArraySize = 1;
    srcDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    srcDesc.SampleDesc.Count = 1;
    srcDesc.Usage = D3D11_USAGE_DEFAULT;
    srcDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ID3D11Texture2D* srcTexture = nullptr;
    hr = device->CreateTexture2D(&srcDesc, &initData, &srcTexture);
    if (FAILED(hr)) return 1;

    ID3DBlob* csBlob = nullptr;
    hr = D3DCompile(csSource, strlen(csSource), nullptr, nullptr, nullptr, "CS", "cs_5_0", 0, 0, &csBlob, nullptr);
    if (FAILED(hr)) return 1;

    ID3D11ComputeShader* computeShader = nullptr;
    hr = device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &computeShader);
    csBlob->Release();
    if (FAILED(hr)) return 1;

    UINT totalElements = 64 * 64; // 8x8 groups * 8x8 threads
    ID3D11Buffer* ssboBuffer = nullptr;
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = totalElements * sizeof(UINT32);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(UINT32);

    std::vector<UINT32> initialBufferData(totalElements, 0);
    D3D11_SUBRESOURCE_DATA bufferInitData = {};
    bufferInitData.pSysMem = initialBufferData.data();
    hr = device->CreateBuffer(&bufferDesc, &bufferInitData, &ssboBuffer);
    if (FAILED(hr)) return 1;

    ID3D11UnorderedAccessView* ssboUAV = nullptr;
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = totalElements;
    hr = device->CreateUnorderedAccessView(ssboBuffer, &uavDesc, &ssboUAV);
    if (FAILED(hr)) return 1;

    ID3D11Buffer* stagingBuffer = nullptr;
    D3D11_BUFFER_DESC stagingDesc = {};
    stagingDesc.ByteWidth = sizeof(UINT32);
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = device->CreateBuffer(&stagingDesc, nullptr, &stagingBuffer);
    if (FAILED(hr)) return 1;

    ID3D11Buffer* constantBuffer = nullptr;
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = 16;
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device->CreateBuffer(&cbDesc, nullptr, &constantBuffer);
    if (FAILED(hr)) return 1;

    // Dispatch #1
    context->CSSetShader(computeShader, nullptr, 0);
    context->CSSetUnorderedAccessViews(0, 1, &ssboUAV, nullptr);
    std::cout << "Dispatching incremental cs #1 ..." << std::endl;
    context->Dispatch(8, 8, 1);

    ID3D11UnorderedAccessView* nullUAV = nullptr;
    // context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

    // CopyImageToImage
    D3D11_BOX srcBox = {};
    srcBox.left = 0;
    srcBox.top = 0;
    srcBox.front = 0;
    srcBox.right = 16;
    srcBox.bottom = 16;
    srcBox.back = 1;

    std::cout << "CopySubresourceRegion ..." << std::endl;
    context->CopySubresourceRegion(
        destTexture, 0,
        0, 0, 0,
        srcTexture, 0,
        &srcBox
    );

    // Dispatch #2
    context->CSSetShader(computeShader, nullptr, 0);
    context->CSSetUnorderedAccessViews(0, 1, &ssboUAV, nullptr);
    std::cout << "Dispatching incremental cs #2 ..." << std::endl;
    context->Dispatch(8, 8, 1);

    context->CSSetShader(nullptr, nullptr, 0);
    context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

    D3D11_BOX srcBoxBuffer = {};
    srcBoxBuffer.left = 0; srcBoxBuffer.right = sizeof(UINT32);
    srcBoxBuffer.top = 0; srcBoxBuffer.bottom = 1;
    srcBoxBuffer.front = 0; srcBoxBuffer.back = 1;
    context->CopySubresourceRegion(stagingBuffer, 0, 0, 0, 0, ssboBuffer, 0, &srcBoxBuffer);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = context->Map(stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
    UINT32 finalValue = 0;
    if (SUCCEEDED(hr)) {
        finalValue = *reinterpret_cast<UINT32*>(mappedResource.pData);
        context->Unmap(stagingBuffer, 0);
    }

    std::cout << "ssboBuffer[0]: " << finalValue << std::endl;

    D3D11_MAPPED_SUBRESOURCE cbMapped;
    hr = context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);
    if (SUCCEEDED(hr)) {
        *reinterpret_cast<UINT32*>(cbMapped.pData) = finalValue;
        context->Unmap(constantBuffer, 0);
    }

    device->CreateShaderResourceView(destTexture, nullptr, &textureSRV);

    ID3D11SamplerState* samplerState = nullptr;
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
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
            context->PSSetConstantBuffers(0, 1, &constantBuffer);
            context->OMSetRenderTargets(1, &rtv, nullptr);
            context->RSSetViewports(1, &viewport);

            context->Draw(3, 0);
            swapChain->Present(1, 0);
        }
    }

    constantBuffer->Release();
    stagingBuffer->Release();
    ssboUAV->Release();
    ssboBuffer->Release();
    computeShader->Release();
    vs->Release();
    ps->Release();
    samplerState->Release();
    textureSRV->Release();
    srcTexture->Release();
    destTexture->Release();
    rtv->Release();
    swapChain->Release();
    context->Release();
    device->Release();

    return 0;
}
