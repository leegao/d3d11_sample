#include <windows.h>
#include <d3d11.h>
#include <DirectXTex.h>
#include <iostream>

int main() {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL level;

    D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context
    );

    if (!device) {
        std::cerr << "Failed to create D3D11 Device." << std::endl;
        return 1;
    }

    DirectX::ScratchImage image;
    HRESULT hr = DirectX::LoadFromDDSFile(
        L"sample_bc7.dds", DirectX::DDS_FLAGS_NONE, nullptr, image
    );

    if (SUCCEEDED(hr)) {
        ID3D11Resource* texture = nullptr;
        hr = DirectX::CreateTexture(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), &texture);

        if (SUCCEEDED(hr)) {
            std::cout << "Created BC7 texture" << std::endl;
            texture->Release();
        }
    } else {
        std::cerr << "Failed to load BC7 texture, make sure sample_bc7.dds exists" << std::endl;
    }

    context->Release();
    device->Release();
    return 0;
}
