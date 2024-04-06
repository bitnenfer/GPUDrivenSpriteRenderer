#define WIN32_LEAN_AND_MEAN 1
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <Windows.h>
#include <Windowsx.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <d3d12.h>
#include <dxgidebug.h>
#include <shlobj.h>
#include <string>
#include <strsafe.h>
#include <new>
#include <random>
#include <chrono>

#include "ni.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define NI_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE  4096
#define NI_UTILS_WINDOWS_LOG_MAX_BUFFER_COUNT 4

static bool keysDown[512];
static bool mouseBtnsDown[3];
static ni::Renderer renderer = {};
static void loadPIX() {
    if (GetModuleHandleA("WinPixGpuCapture.dll") == 0) {

        LPWSTR programFilesPath = nullptr;
        SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL,
            &programFilesPath);

        std::wstring pixSearchPath =
            programFilesPath + std::wstring(L"\\Microsoft PIX\\*");

        WIN32_FIND_DATAW findData;
        bool foundPixInstallation = false;
        wchar_t newestVersionFound[MAX_PATH];

        HANDLE hFind = FindFirstFileW(pixSearchPath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ==
                    FILE_ATTRIBUTE_DIRECTORY) &&
                    (findData.cFileName[0] != '.')) {
                    if (!foundPixInstallation ||
                        wcscmp(newestVersionFound, findData.cFileName) <= 0) {
                        foundPixInstallation = true;
                        StringCchCopyW(newestVersionFound,
                            _countof(newestVersionFound),
                            findData.cFileName);
                    }
                }
            } while (FindNextFileW(hFind, &findData) != 0);
        }

        FindClose(hFind);

        if (foundPixInstallation) {
            wchar_t output[MAX_PATH];
            StringCchCopyW(output, pixSearchPath.length(),
                pixSearchPath.data());
            StringCchCatW(output, MAX_PATH, &newestVersionFound[0]);
            StringCchCatW(output, MAX_PATH, L"\\WinPixGpuCapturer.dll");
            LoadLibraryW(output);
        }
    }
}

ni::RootSignatureDescriptorRange& ni::RootSignatureDescriptorRange::addRange(D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t descriptorNum, uint32_t baseShaderRegister, uint32_t registerSpace) {
    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = type;
    range.NumDescriptors = descriptorNum;
    range.BaseShaderRegister = baseShaderRegister;
    range.RegisterSpace = registerSpace;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges.add(range);
    return *this;
}

void ni::RootSignatureBuilder::addRootParameterDescriptorTable(const D3D12_DESCRIPTOR_RANGE* ranges, uint32_t rangeNum, D3D12_SHADER_VISIBILITY shaderVisibility) {
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParam.DescriptorTable.pDescriptorRanges = ranges;
    rootParam.DescriptorTable.NumDescriptorRanges = rangeNum;
    rootParam.ShaderVisibility = shaderVisibility;
    rootParameters.add(rootParam);
}
void ni::RootSignatureBuilder::addRootParameterDescriptorTable(const RootSignatureDescriptorRange& ranges, D3D12_SHADER_VISIBILITY shaderVisibility) {
    addRootParameterDescriptorTable(*ranges, ranges.getNum(), shaderVisibility);
}
void ni::RootSignatureBuilder::addRootParameterConstant(uint32_t shaderRegister, uint32_t registerSpace, uint32_t num32BitValues, D3D12_SHADER_VISIBILITY shaderVisibility) {
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParam.Constants.ShaderRegister = shaderRegister;
    rootParam.Constants.RegisterSpace = registerSpace;
    rootParam.Constants.Num32BitValues = num32BitValues;
    rootParam.ShaderVisibility = shaderVisibility;
    rootParameters.add(rootParam);
}
void ni::RootSignatureBuilder::addStaticSampler(D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressModeAll, uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility) {
    D3D12_STATIC_SAMPLER_DESC staticSampler = {};
    staticSampler.Filter = filter;
    staticSampler.AddressU = addressModeAll;
    staticSampler.AddressV = addressModeAll;
    staticSampler.AddressW = addressModeAll;
    staticSampler.MipLODBias = 0.0f;
    staticSampler.MaxAnisotropy = 0;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    staticSampler.MinLOD = 0;
    staticSampler.MaxLOD = 0;
    staticSampler.ShaderRegister = shaderRegister;
    staticSampler.RegisterSpace = registerSpace;
    staticSampler.ShaderVisibility = shaderVisibility;
    staticSamplers.add(staticSampler);
}

ID3D12RootSignature* ni::RootSignatureBuilder::build(bool isCompute) {
    ID3DBlob* rootSignatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = rootParameters.getNum();
    rootSignatureDesc.pParameters = rootParameters.getData();
    rootSignatureDesc.NumStaticSamplers = staticSamplers.getNum();
    rootSignatureDesc.pStaticSamplers = staticSamplers.getData();
    rootSignatureDesc.Flags = isCompute ? D3D12_ROOT_SIGNATURE_FLAG_NONE : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    if (D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errorBlob) != S_OK) {
        NI_PANIC("Failed to serialize root signature.\n%s", errorBlob->GetBufferPointer());
    }
    ID3D12RootSignature* rootSignature = nullptr;
    NI_D3D_ASSERT(renderer.device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)), "Failed to create root signature");
    return rootSignature;
}

void ni::DescriptorAllocator::reset() {
    descriptorAllocated = 0;
}

ni::DescriptorTable ni::DescriptorAllocator::allocateDescriptorTable(uint32_t descriptorNum) {
    NI_ASSERT(descriptorAllocated + descriptorNum <= NI_MAX_DESCRIPTORS, "Can't allocate %u descriptors", descriptorNum);
    DescriptorTable table = { { gpuBaseHandle.ptr + descriptorAllocated * descriptorHandleSize }, { cpuBaseHandle.ptr + descriptorAllocated * descriptorHandleSize }, descriptorHandleSize, 0, descriptorNum };
    descriptorAllocated += descriptorNum;
    return table;
}

void ni::init(uint32_t width, uint32_t height) {
    memset(&renderer, 0, sizeof(renderer));
	renderer.windowWidth = width;
	renderer.windowHeight = height;

    renderer.imagesToUpload = (Texture**)malloc(NI_MAX_DESCRIPTORS * sizeof(Texture*));
    renderer.imageToUploadNum = 0;

    memset(keysDown, 0, sizeof(keysDown));
    memset(mouseBtnsDown, 0, sizeof(mouseBtnsDown));

#if NI_USE_FULLSCREEN
    renderer.windowWidth = GetSystemMetrics(SM_CXSCREEN);
    renderer.windowHeight = GetSystemMetrics(SM_CXSCREEN);
#endif

    /* Create Window */
    WNDCLASS windowClass = {
        0,
        [](HWND windowHandle, UINT message, WPARAM wParam,
                          LPARAM lParam) -> LRESULT {
            switch (message) {
            case WM_SIZE:
                //windowWidth = LOWORD(lParam);
                //windowHeight = HIWORD(lParam);
                break;
            case WM_CONTEXTMENU:
                break;
            case WM_ENTERSIZEMOVE:
                break;
            case WM_EXITSIZEMOVE:
                break;
            case WM_CLOSE:
                DestroyWindow(windowHandle);
                break;
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            default:
                return DefWindowProc(windowHandle, message, wParam, lParam);
            }
            return S_OK;
        },
        0,
        0,
        GetModuleHandle(nullptr),
        LoadIcon(nullptr, IDI_APPLICATION),
        LoadCursor(nullptr, IDC_ARROW),
        (HBRUSH)(COLOR_WINDOW + 1),
        nullptr,
        L"WindowClass"
    };
    RegisterClass(&windowClass);

    DWORD windowStyle = WS_VISIBLE | WS_SYSMENU | WS_CAPTION | WS_BORDER;
#if NI_USE_FULLSCREEN    
    windowStyle = WS_POPUP | WS_VISIBLE;
#endif

    RECT windowRect = { 0, 0, (LONG)renderer.windowWidth, (LONG)renderer.windowHeight };
    AdjustWindowRect(&windowRect, windowStyle, false);
    int32_t adjustedWidth = windowRect.right - windowRect.left;
    int32_t adjustedHeight = windowRect.bottom - windowRect.top;



    renderer.windowHandle = CreateWindowExA(
        WS_EX_LEFT, "WindowClass", "GPU Driven Sprites", windowStyle, CW_USEDEFAULT,
        CW_USEDEFAULT, adjustedWidth, adjustedHeight, nullptr, nullptr,
        GetModuleHandle(nullptr), nullptr);
    loadPIX();

#if _DEBUG
    D3D_ASSERT(D3D12GetDebugInterface(IID_PPV_ARGS(&renderer.debugInterface)), "Failed to create debug interface");
    renderer.debugInterface->EnableDebugLayer();
    renderer.debugInterface->SetEnableGPUBasedValidation(true);
    UINT factoryFlag = DXGI_CREATE_FACTORY_DEBUG;
#else
    UINT factoryFlag = 0;
#endif
    NI_D3D_ASSERT(CreateDXGIFactory2(factoryFlag, IID_PPV_ARGS(&renderer.factory)), "Failed to create factory");
    NI_D3D_ASSERT(renderer.factory->EnumAdapters(0, (IDXGIAdapter**)(&renderer.adapter)), "Failed to aquire adapter");
    NI_D3D_ASSERT(D3D12CreateDevice((IUnknown*)renderer.adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&renderer.device)), "Failed to create device");

    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    commandQueueDesc.NodeMask = 0;
    NI_D3D_ASSERT(renderer.device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&renderer.commandQueue)), "Failed to create command queue");
    renderer.commandQueue->SetName(L"gfx::graphicsCommandQueue");

    for (uint32_t index = 0; index < NI_FRAME_COUNT; ++index) {
        FrameData& frame = renderer.frames[index];
        NI_D3D_ASSERT(renderer.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.commandAllocator)), "Failed to create command allocator");
        NI_D3D_ASSERT(renderer.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame.commandAllocator, nullptr, IID_PPV_ARGS(&frame.commandList)), "Failed to create command list");
        NI_D3D_ASSERT(renderer.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frame.fence)), "Failed to create fence");
        frame.fenceEvent = CreateEvent(nullptr, false, false, nullptr);
        NI_D3D_ASSERT(frame.commandList->Close(), "Failed to close command list");
        frame.commandAllocator->SetName(L"gfx::frame::commandAllocator");
        frame.commandList->SetName(L"gfx::frame::commandList");
        frame.fence->SetName(L"gfx::frame::fence");
        D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorHeapDesc.NumDescriptors = NI_MAX_DESCRIPTORS;
        descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        descriptorHeapDesc.NodeMask = 0;
        NI_D3D_ASSERT(renderer.device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&frame.descriptorAllocator.descriptorHeap)), "Failed to create descriptor heap");
        frame.descriptorAllocator.descriptorHandleSize = renderer.device->GetDescriptorHandleIncrementSize(descriptorHeapDesc.Type);
        frame.descriptorAllocator.descriptorAllocated = 0;
        frame.descriptorAllocator.gpuBaseHandle = frame.descriptorAllocator.descriptorHeap->GetGPUDescriptorHandleForHeapStart();
        frame.descriptorAllocator.cpuBaseHandle = frame.descriptorAllocator.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        frame.frameIndex = index;
    }

    NI_D3D_ASSERT(renderer.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderer.presentFence)), "Failed to create fence");
    renderer.presentFenceEvent = CreateEvent(nullptr, false, false, nullptr);
    renderer.presentFenceValue = 0;
    renderer.presentFence->SetName(L"gfx::presentFence");

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {
         { 
            renderer.windowWidth,
            renderer.windowHeight,
            { 0,  0},
            DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
            DXGI_MODE_SCALING_UNSPECIFIED},
        { 1, 0 },
        DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT,
        NI_BACKBUFFER_COUNT,
        renderer.windowHandle,
        true,
        DXGI_SWAP_EFFECT_FLIP_DISCARD,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH 
    };

    NI_D3D_ASSERT(renderer.factory->CreateSwapChain((IUnknown*)renderer.commandQueue, &swapChainDesc, (IDXGISwapChain**)&renderer.swapChain), "Failed to create swapchain");
    for (uint32_t index = 0; index < NI_BACKBUFFER_COUNT; ++index) {
        renderer.swapChain->GetBuffer(index, IID_PPV_ARGS(&renderer.backbuffers[index]));
        renderer.backbuffers[index]->SetName(L"gfx::backbuffer");
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
    rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDescriptorHeapDesc.NumDescriptors = NI_BACKBUFFER_COUNT;
    rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvDescriptorHeapDesc.NodeMask = 0;
    NI_D3D_ASSERT(renderer.device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&renderer.rtvDescriptorHeap)), "Failed to create RTV descriptor heap");
    renderer.rtvDescriptorHeap->SetName(L"gfx::rtvDescriptorHeap");

    D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
    dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDescriptorHeapDesc.NumDescriptors = NI_BACKBUFFER_COUNT;
    dsvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvDescriptorHeapDesc.NodeMask = 0;
    NI_D3D_ASSERT(renderer.device->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&renderer.dsvDescriptorHeap)), "Failed to create DSV descriptor heap");
    renderer.dsvDescriptorHeap->SetName(L"gfx::dsvDescriptorHeap");
}
void ni::setFrameUserData(uint32_t frame, void* data) {
    NI_ASSERT(frame < NI_FRAME_COUNT, "Can't store user data on frame %u because it doesn't exist. The frame count is %u", frame, NI_FRAME_COUNT);
    renderer.frames[frame].userData = data;
}
void ni::waitForCurrentFrame() {
    FrameData& frame = renderer.frames[renderer.currentFrame];
    if (frame.fence->GetCompletedValue() != frame.frameWaitValue) {
        frame.fence->SetEventOnCompletion(frame.frameWaitValue, frame.fenceEvent);
        WaitForSingleObject(frame.fenceEvent, INFINITE);
    }
}
void ni::waitForAllFrames() {
    for (uint32_t index = 0; index < NI_FRAME_COUNT; ++index) {
        FrameData& frame = renderer.frames[index];
        if (frame.fence->GetCompletedValue() != frame.frameWaitValue) {
            frame.fence->SetEventOnCompletion(frame.frameWaitValue, frame.fenceEvent);
            WaitForSingleObject(frame.fenceEvent, INFINITE);
        }
    }
}
void ni::destroy() {
    waitForAllFrames();
    for (uint32_t index = 0; index < NI_FRAME_COUNT; ++index) {
        FrameData& frame = renderer.frames[index];
        NI_D3D_RELEASE(frame.commandList);
        NI_D3D_RELEASE(frame.commandAllocator);
        NI_D3D_RELEASE(frame.fence);
        CloseHandle(frame.fenceEvent);
        NI_D3D_RELEASE(frame.descriptorAllocator.descriptorHeap);
    }
    for (uint32_t index = 0; index < NI_BACKBUFFER_COUNT; ++index) {
        NI_D3D_RELEASE(renderer.backbuffers[index]);
    }
    if (renderer.presentFence->GetCompletedValue() != renderer.presentFenceValue) {
        renderer.presentFence->SetEventOnCompletion(renderer.presentFenceValue, renderer.presentFenceEvent);
        WaitForSingleObject(renderer.presentFenceEvent, INFINITE);
    }
    CloseHandle(renderer.presentFenceEvent);
    NI_D3D_RELEASE(renderer.rtvDescriptorHeap);
    NI_D3D_RELEASE(renderer.presentFence);
    NI_D3D_RELEASE(renderer.swapChain);
    NI_D3D_RELEASE(renderer.commandQueue);
    NI_D3D_RELEASE(renderer.device);
    NI_D3D_RELEASE(renderer.adapter);
    NI_D3D_RELEASE(renderer.factory);
    free(renderer.imagesToUpload);

#if _DEBUG
    if (renderer.debugInterface) {
        IDXGIDebug1* dxgiDebug = nullptr;
        if (DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)) == S_OK) {
            dxgiDebug->ReportLiveObjects(
                DXGI_DEBUG_ALL,
                DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY |
                    DXGI_DEBUG_RLO_IGNORE_INTERNAL));
            dxgiDebug->Release();
        }
        D3D_RELEASE(renderer.debugInterface);
    }
#endif

}

void ni::pollEvents() {
    MSG message;
    while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
        switch (message.message) {
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_CHAR:
        case WM_KEYDOWN:
            if (message.wParam == 27) {
                renderer.shouldQuit = true;
            }
            keysDown[(uint8_t)message.wParam] = true;
            break;
        case WM_KEYUP:
            keysDown[(uint8_t)message.wParam] = false;
            break;
        case WM_MOUSEMOVE:
            renderer.mouseX = (float)GET_X_LPARAM(message.lParam);
            renderer.mouseY = (float)GET_Y_LPARAM(message.lParam);
            break;
        case WM_LBUTTONDOWN:
            mouseBtnsDown[0] = true;
            break;
        case WM_LBUTTONUP:
            mouseBtnsDown[0] = false;
            break;
        case WM_RBUTTONDOWN:
            mouseBtnsDown[2] = true;
            break;
        case WM_RBUTTONUP:
            mouseBtnsDown[2] = false;
            break;
        case WM_MBUTTONDOWN:
            mouseBtnsDown[1] = true;
            break;
        case WM_MBUTTONUP:
            mouseBtnsDown[1] = false;
            break;
        case WM_QUIT:
            renderer.shouldQuit = true;
            return;
        default:
            DispatchMessageA(&message);
            break;
        }
    }
}

bool ni::shouldQuit() {
    return renderer.shouldQuit;
}

ni::FrameData& ni::getFrameData() {
    FrameData& frame = renderer.frames[renderer.currentFrame];
    return frame;
}

ID3D12Device* ni::getDevice() {
    return renderer.device;
}

ni::FrameData& ni::beginFrame() {
    FrameData& frame = renderer.frames[renderer.currentFrame];
    NI_D3D_ASSERT(frame.commandAllocator->Reset(), "Failed to reset command allocator");
    NI_D3D_ASSERT(frame.commandList->Reset(frame.commandAllocator, nullptr), "Failed to reset command list");
    frame.descriptorAllocator.reset();
    // Allocate all descriptors to allow for bindless resources.
    frame.descriptorTable = frame.descriptorAllocator.allocateDescriptorTable(NI_MAX_DESCRIPTORS);
    frame.commandList->SetDescriptorHeaps(1, &frame.descriptorAllocator.descriptorHeap);

    // Upload texture data
    for (uint32_t index = 0; index < renderer.imageToUploadNum; ++index) {
        Texture* image = renderer.imagesToUpload[index];
        NI_ASSERT((image->state & NI_IMAGE_STATE_CREATED) > 0, "Invalid image");
        if ((image->state & NI_IMAGE_STATE_UPLOADED) > 0) {
            continue;
        }

        ID3D12Resource* texture = image->texture.resource;
        ID3D12Resource* uploadBuffer = image->upload.resource;
        D3D12_RESOURCE_DESC desc = image->texture.resource->GetDesc();

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint32_t numRows = 0;
        uint64_t rowSizeInBytes = 0, totalBytes = 0;
        renderer.device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, &numRows, &rowSizeInBytes, &totalBytes);
        void* mapped = nullptr;
        uploadBuffer->Map(0, nullptr, &mapped);
        size_t pixelSize = ni::getDXGIFormatBytes(desc.Format);

        for (uint32_t index = 0; index < numRows * layout.Footprint.Depth; ++index) {
            void* dstAddr = offsetPtr(mapped, (intptr_t)(index * layout.Footprint.RowPitch));
            const void* srcAddr = offsetPtr((void*)image->cpuData, (intptr_t)(index * (desc.Width * pixelSize)));
            memcpy(dstAddr, srcAddr, (size_t)rowSizeInBytes);
        }
        uploadBuffer->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.pResource = uploadBuffer;
        srcLocation.PlacedFootprint = layout;
        D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.pResource = texture;
        dstLocation.SubresourceIndex = 0;

        D3D12_RESOURCE_BARRIER textureBufferBarrier[1] = {};
        textureBufferBarrier[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        textureBufferBarrier[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        textureBufferBarrier[0].Transition.Subresource = 0;
        textureBufferBarrier[0].Transition.pResource = texture;
        textureBufferBarrier[0].Transition.StateBefore = image->texture.state;
        textureBufferBarrier[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

        frame.commandList->ResourceBarrier(1, textureBufferBarrier);
        frame.commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

        textureBufferBarrier[0].Transition.StateAfter = image->texture.state;
        textureBufferBarrier[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        frame.commandList->ResourceBarrier(1, textureBufferBarrier);

        free((void*)image->cpuData);
        image->cpuData = nullptr;
        image->state |= NI_IMAGE_STATE_UPLOADED;
    }
    renderer.imageToUploadNum = 0;

    return frame;
}

void ni::endFrame() {
    FrameData& frame = renderer.frames[renderer.currentFrame];
    NI_D3D_ASSERT(frame.commandList->Close(), "Failed to close command list");
    ID3D12CommandList* commandLists[] = { frame.commandList };
    renderer.commandQueue->ExecuteCommandLists(1, commandLists);
    NI_D3D_ASSERT(renderer.commandQueue->Signal(frame.fence, ++frame.frameWaitValue), "Failed to signal frame fence");
    renderer.currentFrame = (renderer.currentFrame + 1) % NI_FRAME_COUNT;
}

ID3D12Resource* ni::getCurrentBackbuffer() {
    return renderer.backbuffers[renderer.presentFrame];
}

void ni::present(bool vsync) {
    if (renderer.presentFence->GetCompletedValue() != renderer.presentFenceValue) {
        renderer.presentFence->SetEventOnCompletion(renderer.presentFenceValue, renderer.presentFenceEvent);
        WaitForSingleObject(renderer.presentFenceEvent, INFINITE);
    }

    NI_D3D_ASSERT(renderer.swapChain->Present(vsync ? 1 : 0, 0), "Failed to present");
    NI_D3D_ASSERT(renderer.commandQueue->Signal(renderer.presentFence, ++renderer.presentFenceValue), "Failed to signal present fence");
    renderer.presentFrame = renderer.presentFenceValue % NI_BACKBUFFER_COUNT;
}

ID3D12PipelineState* ni::createGraphicsPipelineState(const wchar_t* name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDesc) {
    ID3D12PipelineState* pso = nullptr;
    NI_D3D_ASSERT(renderer.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)), "Failed to create graphics pipeline state");
    pso->SetName(name);
    return pso;
}

ID3D12PipelineState* ni::createComputePipelineState(const wchar_t* name, const D3D12_COMPUTE_PIPELINE_STATE_DESC& psoDesc) {
    ID3D12PipelineState* pso = nullptr;
    NI_D3D_ASSERT(renderer.device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso)), "Failed to create compute pipeline state");
    pso->SetName(name);
    return pso;
}

float ni::getViewWidth() { return (float)renderer.windowWidth; }
float ni::getViewHeight() { return (float)renderer.windowHeight; }

ni::Resource ni::createBuffer(const wchar_t* name, size_t bufferSize, BufferType type, bool initToZero) {
    D3D12_HEAP_TYPE heapType;
    D3D12_RESOURCE_STATES initialState;
    D3D12_RESOURCE_FLAGS flags;
    switch (type) {
    case INDEX_BUFFER:
        initialState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
        flags = D3D12_RESOURCE_FLAG_NONE;
        heapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    case VERTEX_BUFFER:
        initialState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        flags = D3D12_RESOURCE_FLAG_NONE;
        heapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    case CONSTANT_BUFFER:
        initialState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        flags = D3D12_RESOURCE_FLAG_NONE;
        heapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    case UNORDERED_BUFFER:
        initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        heapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    case UPLOAD_BUFFER:
        initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
        flags = D3D12_RESOURCE_FLAG_NONE;
        heapType = D3D12_HEAP_TYPE_UPLOAD;
        break;
    case SHADER_RESOURCE_BUFFER:
        initialState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        flags = D3D12_RESOURCE_FLAG_NONE;
        heapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    default:
        NI_PANIC("Error: Invalid buffer type"); // Invalid Buffer Type
        break;
    }

    D3D12_RESOURCE_DESC resourceDesc = {
        D3D12_RESOURCE_DIMENSION_BUFFER,
        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
        alignSize(bufferSize, 256),
        1,
        1,
        1,
        DXGI_FORMAT_UNKNOWN,
        { 1, 0},
        D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        flags 
    };
    D3D12_HEAP_PROPERTIES heapProps = {
        heapType,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };

    D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
    if (!initToZero) {
        heapFlags |= D3D12_HEAP_FLAG_CREATE_NOT_ZEROED;
    }
    ID3D12Resource* resource = nullptr;
    NI_D3D_ASSERT(renderer.device->CreateCommittedResource(
        &heapProps,
        heapFlags,
        &resourceDesc, initialState, nullptr,
        IID_PPV_ARGS(&resource)),
        "Failed to create buffer resource");
    resource->SetName(name);
    return { resource, initialState };
}

D3D12_CPU_DESCRIPTOR_HANDLE ni::getRenderTargetViewCPUHandle() {
    return renderer.rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE ni::getRenderTargetViewGPUHandle() {
    return renderer.rtvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE ni::getDepthStencilViewCPUHandle() {
    return renderer.dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}
D3D12_GPU_DESCRIPTOR_HANDLE ni::getDepthStencilViewGPUHandle() {
    return renderer.dsvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

float ni::mouseX() {
    return renderer.mouseX;
}
float ni::mouseY() {
    return renderer.mouseY;
}

bool ni::mouseDown(MouseButton button) {
    return mouseBtnsDown[(uint32_t)button];
}

bool ni::keyDown(KeyCode keyCode) {
    return keysDown[(uint32_t)keyCode];
}

float ni::randomFloat() {
    static std::random_device randDevice;
    static std::mt19937 mtGen(randDevice());
    static std::uniform_real_distribution<float> udt;
    return udt(mtGen);
}

uint32_t ni::randomUint() {
    static std::random_device randDevice;
    static std::mt19937 mtGen(randDevice());
    static std::uniform_int_distribution<unsigned int> udt;
    return udt(mtGen);
}

double ni::getSeconds() {
    double time = std::chrono::time_point_cast<std::chrono::duration<double>>(
        std::chrono::high_resolution_clock::now())
        .time_since_epoch()
        .count();
    return time;
}

size_t ni::getDXGIFormatBits(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 96;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        return 64;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_YUY2:
        return 32;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        return 24;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
        return 16;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_NV11:
        return 12;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
        return 8;

    case DXGI_FORMAT_R1_UNORM:
        return 1;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 4;

    default:
        return 0;
    }
}

size_t ni::getDXGIFormatBytes(DXGI_FORMAT format) {
    return getDXGIFormatBits(format) / 8;
}

ni::Texture* ni::createTexture(const wchar_t* name, uint32_t width, uint32_t height, uint32_t depth, const void* pixels, DXGI_FORMAT dxgiFormat, D3D12_RESOURCE_FLAGS flags) {
    NI_ASSERT(depth == 1, "No 3D textures supported yet.");
    Texture* texture = new Texture();
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_DIMENSION dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    D3D12_RESOURCE_DESC resourceDesc = {
        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
        (uint64_t)width,
        height,
        (uint16_t)depth,
        1,
        dxgiFormat,
        { 1,  0},
        D3D12_TEXTURE_LAYOUT_UNKNOWN,
        flags
    };
    D3D12_HEAP_PROPERTIES heapProps = {
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };

    D3D12_CLEAR_VALUE* clearValuePtr = nullptr;
    D3D12_CLEAR_VALUE clearValue = {};

    if ((flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) > 0) {
        clearValue.Format = dxgiFormat;
        clearValue.DepthStencil.Depth = 1;
        clearValue.DepthStencil.Stencil = 0;
        clearValuePtr = &clearValue;
    } 
    
    if ((flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) > 0) {
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 1.0f;
        clearValuePtr = &clearValue;
    }

    NI_D3D_ASSERT(ni::getDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, &resourceDesc, initialState, clearValuePtr, IID_PPV_ARGS(&texture->texture.resource)), "Failed to create image resource");
    texture->texture.resource->SetName(name);
    texture->texture.state = initialState;
    texture->width = width;
    texture->height = height;
    texture->depth = depth;
    texture->state |= NI_IMAGE_STATE_CREATED;

    if (pixels != nullptr) {
        size_t pixelSize = ni::getDXGIFormatBytes(dxgiFormat);
        uint64_t alignedWidth = alignSize(width, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        uint64_t dataSize = alignedWidth * height * pixelSize;
        uint64_t bufferSize = dataSize < 256 ? 256 : dataSize;
        texture->upload = ni::createBuffer(L"SpriteImage::upload", bufferSize, ni::UPLOAD_BUFFER, false);
        texture->cpuData = malloc(pixelSize * width * height);
        NI_ASSERT(texture->cpuData != nullptr, "Failed to allocate cpu data for uploading to texture memory");
        if (texture->cpuData != nullptr) {
            memcpy((void*)texture->cpuData, pixels, pixelSize * width * height);
        }
        renderer.imagesToUpload[renderer.imageToUploadNum++] = texture;
    }
    return texture;
}

void ni::destroyTexture(Texture*& texture) {
    NI_D3D_RELEASE(texture->texture.resource);
    NI_D3D_RELEASE(texture->upload.resource);
    delete texture;
    texture = nullptr;
}

void ni::logFmt(const char* fmt, ...) {
    static char bufferLarge[NI_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE * NI_UTILS_WINDOWS_LOG_MAX_BUFFER_COUNT] = {};
    static uint32_t bufferIndex = 0;
    va_list args;
    va_start(args, fmt);
    char* buffer = &bufferLarge[bufferIndex * NI_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE];
    vsprintf_s(buffer, NI_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE, fmt, args);
    bufferIndex = (bufferIndex + 1) % NI_UTILS_WINDOWS_LOG_MAX_BUFFER_COUNT;
    va_end(args);
    OutputDebugStringA(buffer);
    printf("%s", buffer);
}

// Origin: https://github.com/niklas-ourmachinery/bitsquid-foundation/blob/master/murmur_hash.cpp
uint64_t ni::murmurHash(const void* key, uint64_t keyLength, uint64_t seed) {
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const uint32_t r = 47;
    uint64_t h = seed ^ (keyLength * m);
    const uint64_t* data = (const uint64_t*)key;
    const uint64_t* end = data + (keyLength / 8);
    while (data != end) {
        uint64_t k = *data++;
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }
    const unsigned char* data2 = (const unsigned char*)data;
    switch (keyLength & 7) {
    case 7: h ^= uint64_t(data2[6]) << 48;
    case 6: h ^= uint64_t(data2[5]) << 40;
    case 5: h ^= uint64_t(data2[4]) << 32;
    case 4: h ^= uint64_t(data2[3]) << 24;
    case 3: h ^= uint64_t(data2[2]) << 16;
    case 2: h ^= uint64_t(data2[1]) << 8;
    case 1: h ^= uint64_t(data2[0]);
        h *= m;
    };
    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

size_t ni::getFileSize(const char* path) {
    HANDLE fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSize = {};
        if (GetFileSizeEx(fileHandle, &fileSize)) {
            return fileSize.QuadPart;
        }
        CloseHandle(fileHandle);
    }
    return 0;
}

bool ni::readFile(const char* path, void* outBuffer) {
    HANDLE fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSize = {};
        GetFileSizeEx(fileHandle, &fileSize);
        DWORD readBytes = 0;
        if (!ReadFile(fileHandle, outBuffer, (DWORD)fileSize.QuadPart, &readBytes, nullptr)) {
            CloseHandle(fileHandle);
            return false;
        }
        CloseHandle(fileHandle);
        return true;
    }
    return false;
}

void* ni::allocReadFile(const char* path) {
    HANDLE fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSize = {};
        GetFileSizeEx(fileHandle, &fileSize);
        void* buffer = malloc(fileSize.QuadPart);
        DWORD readBytes = 0;
        if (!ReadFile(fileHandle, buffer, (DWORD)fileSize.QuadPart, &readBytes, nullptr)) {
            CloseHandle(fileHandle);
            free(buffer);
            return nullptr;
        }
        CloseHandle(fileHandle);
        return buffer;
    }
    return nullptr;
}

ni::FileReader::FileReader(const char* path) {
    size = getFileSize(path);
    buffer = allocReadFile(path);
}

ni::FileReader::~FileReader() {
    free(buffer);
}

void* ni::FileReader::operator*() const {
    return buffer;
}

size_t ni::FileReader::getSize() const {
    return size;
}

void ni::DescriptorTable::reset() {
    allocated = 0;
}

ni::DescriptorHandle ni::DescriptorTable::allocate() {
    NI_ASSERT(allocated + 1 <= capacity, "Exceeded capacity of descriptors allocated");
    DescriptorHandle handle = { gpuHandle(allocated), cpuHandle(allocated) };
    allocated += 1;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ni::DescriptorTable::gpuHandle(uint64_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = gpuBaseHandle;
    handle.ptr += (index * handleSize);
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE ni::DescriptorTable::cpuHandle(uint64_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuBaseHandle;
    handle.ptr += (index * handleSize);
    return handle;
}
