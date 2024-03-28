#pragma once

#include "utils.h"
#include <stdint.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <unordered_map>
#include <vector>

///////////////////////////////////////////////////////////////
// CONFIG 
///////////////////////////////////////////////////////////////

#define USE_FULLSCREEN 0
#define FRAME_COUNT 3
#define BACKBUFFER_COUNT 2
#define MAX_DESCRIPTORS (1<<16)

///////////////////////////////////////////////////////////////

#define D3D_PANIC(fmt, ...)                                                    \
    LOG(fmt, __VA_ARGS__);                                                  \
    __debugbreak();                                                            \
    ExitProcess(-1);
#define D3D_ASSERT(x, ...)                                                     \
    if ((x) != S_OK) {                                                         \
        D3D_PANIC(__VA_ARGS__);                                                \
    }
#define D3D_RELEASE(obj)                                                       \
    {                                                                          \
        if ((obj)) {                                                           \
            (obj)->Release();                                                  \
            obj = nullptr;                                                     \
        }                                                                      \
    }

#define GFX_IMAGE_STATE_NONE		(0b000)
#define GFX_IMAGE_STATE_CREATED		(0b001)
#define GFX_IMAGE_STATE_UPLOADED	(0b010)
#define GFX_IMAGE_STATE_BOUND		(0b100)

namespace gfx {

	enum BufferType {
		INDEX_BUFFER,
		VERTEX_BUFFER,
		CONSTANT_BUFFER,
		UNORDERED_BUFFER,
		UPLOAD_BUFFER,
		SHADER_RESOURCE_BUFFER
	};

	struct RootSignatureDescriptorRange {
		RootSignatureDescriptorRange() {}
		~RootSignatureDescriptorRange() {}

		RootSignatureDescriptorRange& addRange(D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t descriptorNum, uint32_t baseShaderRegister, uint32_t registerSpace);
		const D3D12_DESCRIPTOR_RANGE* operator*() const { return ranges.data(); }
		uint32_t getNum() const { return (uint32_t)ranges.size(); }

	private:
		std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
	};

	struct RootSignatureBuilder {
		RootSignatureBuilder() {}
		~RootSignatureBuilder() {}

		void addRootParameterDescriptorTable(const D3D12_DESCRIPTOR_RANGE* ranges, uint32_t rangeNum, D3D12_SHADER_VISIBILITY shaderVisibility);
		void addRootParameterDescriptorTable(const RootSignatureDescriptorRange& ranges, D3D12_SHADER_VISIBILITY shaderVisibility);
		void addRootParameterConstant(uint32_t shaderRegister, uint32_t registerSpace, uint32_t num32BitValues, D3D12_SHADER_VISIBILITY shaderVisibility);
		void addStaticSampler(D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressModeAll, uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility);
		ID3D12RootSignature* build(bool isCompute);

	private:
		std::vector<D3D12_ROOT_PARAMETER> rootParameters;
		std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
	};
	
	struct Resource {
		ID3D12Resource* resource;
		D3D12_RESOURCE_STATES state;
	};

	template<uint32_t NUM_BARRIERS>
	struct ResourceBarrierBatcher {

		struct BarrierData {
			D3D12_RESOURCE_BARRIER barrier;
			Resource* resource1;
			Resource* resource2;
		};

		ResourceBarrierBatcher() {
			barrierNum = 0;
		}

		void transition(Resource* resource, D3D12_RESOURCE_STATES afterState) {
			if (resource->state != afterState) {
				BarrierData data = { {}, resource, nullptr };
				data.barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				data.barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				data.barrier.Transition.Subresource = 0;
				data.barrier.Transition.pResource = resource->resource;
				data.barrier.Transition.StateBefore = resource->state;
				data.barrier.Transition.StateAfter = afterState;
				barriers[barrierNum++] = data;
			}
		}

		void reset() {
			barrierNum = 0;
		}

		void flush(ID3D12GraphicsCommandList* commandList) {
			if (barrierNum == 0) return;
			D3D12_RESOURCE_BARRIER barrierArray[NUM_BARRIERS] = {};
			for (uint32_t index = 0; index < barrierNum; ++index) {
				barrierArray[index] = barriers[index].barrier;
			}
			commandList->ResourceBarrier(barrierNum, barrierArray);
			for (uint32_t index = 0; index < barrierNum; ++index) {
				BarrierData& data = barriers[index];
				if (data.barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
					data.resource1->state = data.barrier.Transition.StateAfter;
				}
			}
			reset();
		}

		BarrierData barriers[NUM_BARRIERS];
		uint32_t barrierNum;
	};

	struct DescriptorHandle {

		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	};

	struct DescriptorTable {

		inline void reset() {
			allocated = 0;
		}

		inline DescriptorHandle allocate() {
			ASSERT(allocated + 1 <= capacity, "Exceeded capacity of descriptors allocated");
			DescriptorHandle handle = { gpuHandle(allocated), cpuHandle(allocated) };
			allocated += 1;
			return handle;
		}

		inline D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle(uint64_t index) {
			D3D12_GPU_DESCRIPTOR_HANDLE handle = gpuBaseHandle;
			handle.ptr += (index * handleSize);
			return handle;
		}

		inline D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle(uint64_t index) {
			D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuBaseHandle;
			handle.ptr += (index * handleSize);
			return handle;
		}

		D3D12_GPU_DESCRIPTOR_HANDLE gpuBaseHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuBaseHandle;
		uint32_t handleSize;
		uint32_t allocated;
		uint32_t capacity;
	};

	struct DescriptorAllocator {

		void reset();
		DescriptorTable allocateDescriptorTable(uint32_t descriptorNum);

		ID3D12DescriptorHeap* descriptorHeap;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuBaseHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuBaseHandle;
		uint32_t descriptorHandleSize;
		uint32_t descriptorAllocated;
	};

	struct RenderFrame {
		DescriptorTable descriptorTable;
		DescriptorAllocator descriptorAllocator;
		ID3D12GraphicsCommandList* commandList;
		ID3D12CommandAllocator* commandAllocator;
		ID3D12Fence* fence;
		HANDLE fenceEvent;
		uint64_t frameWaitValue;
		uint32_t frameIndex;
	};

	struct Image2D {
		gfx::Resource texture;
		gfx::Resource upload;
		float width;
		float height;
		const void* cpuData;
		uint32_t textureId;
		uint32_t state;
	};

	struct Renderer {
		ID3D12Device5* device;
		ID3D12Debug3* debugInterface;
		IDXGIFactory1* factory;
		IDXGIAdapter1* adapter;
		ID3D12CommandQueue* commandQueue;
		IDXGISwapChain1* swapChain;
		ID3D12DescriptorHeap* rtvDescriptorHeap;
		ID3D12DescriptorHeap* dsvDescriptorHeap;
		RenderFrame frames[FRAME_COUNT];
		ID3D12Resource* backbuffers[FRAME_COUNT];
		ID3D12Fence* presentFence;
		HANDLE presentFenceEvent;
		HWND windowHandle;
		uint32_t windowWidth;
		uint32_t windowHeight;
		uint64_t presentFenceValue;
		uint64_t presentFrame;
		uint32_t currentFrame;
		Image2D** imagesToUpload;
		uint32_t imageToUploadNum;
		float mouseX;
		float mouseY;
		bool shouldQuit;
	};

	void init(uint32_t width, uint32_t height);
	void destroy();
	void pollEvents();
	bool shouldQuit();
	ID3D12Device* getDevice();
	gfx::RenderFrame& beginFrame();
	void endFrame();
	ID3D12Resource* getCurrentBackbuffer();
	void present(bool vsync = true);
	ID3D12PipelineState* createGraphicsPipelineState(const wchar_t* name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDesc);
	ID3D12PipelineState* createComputePipelineState(const wchar_t* name, const D3D12_COMPUTE_PIPELINE_STATE_DESC& psoDesc);
	float getViewWidth();
	float getViewHeight();
	Resource createBuffer(const wchar_t* name, size_t bufferSize, BufferType type, bool initToZero = false);
	D3D12_CPU_DESCRIPTOR_HANDLE getRenderTargetViewCPUHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE getRenderTargetViewGPUHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilViewCPUHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE getDepthStencilViewGPUHandle();
	float mouseX();
	float mouseY();
	float randomFloat();
	uint32_t randomUint();
	double getSeconds();
	size_t getDXGIFormatBits(DXGI_FORMAT format);
	size_t getDXGIFormatBytes(DXGI_FORMAT format);
	Image2D* createImage2D(const wchar_t* name, uint32_t width, uint32_t height, const void* pixels, DXGI_FORMAT dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	void destroyImage2D(Image2D*& image);
}