#pragma once

#include <stdint.h>
#include <d3d12.h>
#include <dxgi1_6.h>

///////////////////////////////////////////////////////////////
// CONFIG 
///////////////////////////////////////////////////////////////

#define NI_USE_FULLSCREEN 0
#define NI_FRAME_COUNT 3
#define NI_BACKBUFFER_COUNT 2
#define NI_MAX_DESCRIPTORS (1<<12)

///////////////////////////////////////////////////////////////

#define NI_EXIT(code) { ExitProcess(code); }
#define NI_DEBUG_BREAK() __debugbreak()
#define NI_LOG(fmt, ...) ni::logFmt(fmt "\n", ##__VA_ARGS__)
#define NI_PANIC(fmt, ...) { ni::logFmt("PANIC: " fmt "\n", ##__VA_ARGS__); NI_DEBUG_BREAK(); NI_EXIT(~0); }
#define NI_ASSERT(x, fmt, ...) if (!(x)) { ni::logFmt("ASSERT: " fmt "\n", ##__VA_ARGS__); NI_DEBUG_BREAK(); }
#define NI_D3D_ASSERT(x, ...) if ((x) != S_OK) { NI_PANIC(__VA_ARGS__); }
#define NI_D3D_RELEASE(obj) { if ((obj)) { (obj)->Release(); obj = nullptr; } }
#define NI_COLOR_UINT(color) (((color) & 0xff) << 24) | ((((color) >> 8) & 0xff) << 16) | ((((color) >> 16) & 0xff) << 8) | ((color) >> 24)
#define NI_COLOR_RGBA_UINT(r, g, b, a) ((uint32_t)(r)) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24)
#define NI_COLOR_RGB_UINT(r, g, b) NI_COLOR_RGBA_UINT(r, g, b, 0xff)
#define NI_COLOR_RGBA_FLOAT(r, g, b, a) NI_COLOR_RGBA_UINT((uint8_t)((r) * 255.0f), (uint8_t)((g) * 255.0f), (uint8_t)((b) * 255.0f), (uint8_t)((a) * 255.0f))
#define NI_COLOR_RGB_FLOAT(r, g, b) NI_COLOR_RGBA_FLOAT(r, g, b, 1.0f)
#define NI_IMAGE_STATE_NONE (0b000)
#define NI_IMAGE_STATE_CREATED (0b001)
#define NI_IMAGE_STATE_UPLOADED (0b010)
#define NI_IMAGE_STATE_BOUND (0b100)

namespace ni {

	void logFmt(const char* fmt, ...);

	enum KeyCode : uint32_t {
		ALT = 18,
		DOWN = 40,
		LEFT = 37,
		RIGHT = 39,
		UP = 38,
		BACKSPACE = 8,
		CAPS_LOCK = 20,
		CTRL = 17,
		DEL = 46,
		END = 35,
		ENTER = 13,
		ESC = 27,
		F1 = 112,
		F2 = 113,
		F3 = 114,
		F4 = 115,
		F5 = 116,
		F6 = 117,
		F7 = 118,
		F8 = 119,
		F9 = 120,
		F10 = 121,
		F11 = 122,
		F12 = 123,
		HOME = 36,
		INSERT = 45,
		NUM_LOCK = 144,
		NUMPAD_0 = 96,
		NUMPAD_1 = 97,
		NUMPAD_2 = 98,
		NUMPAD_3 = 99,
		NUMPAD_4 = 100,
		NUMPAD_5 = 101,
		NUMPAD_6 = 102,
		NUMPAD_7 = 103,
		NUMPAD_8 = 104,
		NUMPAD_9 = 105,
		NUMPAD_MINUS = 109,
		NUMPAD_ASTERIKS = 106,
		NUMPAD_DOT = 110,
		NUMPAD_SLASH = 111,
		NUMPAD_SUM = 107,
		PAGE_DOWN = 34,
		PAGE_UP = 33,
		PAUSE = 19,
		PRINT_SCREEN = 44,
		SCROLL_LOCK = 145,
		SHIFT = 16,
		SPACE = 32,
		TAB = 9,
		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,
		NUM_0 = 48,
		NUM_1 = 49,
		NUM_2 = 50,
		NUM_3 = 51,
		NUM_4 = 52,
		NUM_5 = 53,
		NUM_6 = 54,
		NUM_7 = 55,
		NUM_8 = 56,
		NUM_9 = 57,
		QUOTE = 192,
		MINUS = 189,
		COMMA = 188,
		SLASH = 191,
		SEMICOLON = 186,
		LEFT_SQRBRACKET = 219,
		RIGHT_SQRBRACKET = 221,
		BACKSLASH = 220,
		EQUALS = 187
	};

	enum MouseButton {
		MOUSE_BUTTON_LEFT = 0,
		MOUSE_BUTTON_MIDDLE = 1,
		MOUSE_BUTTON_RIGHT = 2
	};

	enum BufferType {
		INDEX_BUFFER,
		VERTEX_BUFFER,
		CONSTANT_BUFFER,
		UNORDERED_BUFFER,
		UPLOAD_BUFFER,
		SHADER_RESOURCE_BUFFER
	};

	struct FileReader {
		FileReader(const char* path);
		~FileReader();
		void* operator*() const;
		size_t getSize() const;
	private:
		void* buffer = nullptr;
		size_t size = 0;
	};

	template<typename T, typename TSize = uint64_t>
	struct Array {
		Array() : data(nullptr), num(0), capacity(0) {}

		void reset() {
			num = 0;
		}

		void add(const T& element) {
			checkResize();
			data[num++] = element;
		}

		void remove(TSize index) {
			NI_ASSERT(index < num, "Index out of bounds");
			for (TSize start = index; start < num - 1; ++start) {
				data[start] = data[start + 1];
			}
			num--;
		}

		void resize(TSize newCapacity) {
			T* newData = (T*)realloc(data, newCapacity * sizeof(T));
			NI_ASSERT(newData != nullptr, "Failed to reallocate array");
			data = newData;
			capacity = newCapacity;
		}

		void checkResize() {
			if (num + 1 >= capacity) {
				resize(capacity > 0 ? capacity * 2 : 16);
			}
		}

		T* getData() { return data; }
		const T* getData() const { return data; }

		TSize getNum() const { return num; }
		TSize getCapacity() const { return capacity;  }

	private:
		T* data;
		TSize num;
		TSize capacity;
	};

	struct RootSignatureDescriptorRange {
		RootSignatureDescriptorRange() {}
		~RootSignatureDescriptorRange() {}

		RootSignatureDescriptorRange& addRange(D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t descriptorNum, uint32_t baseShaderRegister, uint32_t registerSpace);
		const D3D12_DESCRIPTOR_RANGE* operator*() const { return ranges.getData(); }
		uint32_t getNum() const { return ranges.getNum(); }

	private:
		Array<D3D12_DESCRIPTOR_RANGE, uint32_t> ranges;
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
		Array<D3D12_ROOT_PARAMETER, uint32_t> rootParameters;
		Array<D3D12_STATIC_SAMPLER_DESC, uint32_t> staticSamplers;
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
			memset(barriers, 0, sizeof(barriers));
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
		void reset();
		DescriptorHandle allocate();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle(uint64_t index);
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle(uint64_t index);
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

	struct FrameData {
		DescriptorTable descriptorTable;
		DescriptorAllocator descriptorAllocator;
		ID3D12GraphicsCommandList* commandList;
		ID3D12CommandAllocator* commandAllocator;
		ID3D12Fence* fence;
		HANDLE fenceEvent;
		uint64_t frameWaitValue;
		uint64_t frameIndex;
		void* userData;
	};

	struct Texture {
		ni::Resource texture;
		ni::Resource upload;
		uint32_t width;
		uint32_t height;
		uint32_t depth;
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
		FrameData frames[NI_FRAME_COUNT];
		ID3D12Resource* backbuffers[NI_FRAME_COUNT];
		ID3D12Fence* presentFence;
		HANDLE presentFenceEvent;
		HWND windowHandle;
		uint32_t windowWidth;
		uint32_t windowHeight;
		uint64_t presentFenceValue;
		uint64_t presentFrame;
		uint64_t currentFrame;
		Texture** imagesToUpload;
		uint32_t imageToUploadNum;
		float mouseX;
		float mouseY;
		bool shouldQuit;
	};

	void init(uint32_t width, uint32_t height);
	void setFrameUserData(uint32_t frame, void* data);
	void waitForCurrentFrame();
	void waitForAllFrames();
	void destroy();
	void pollEvents();
	bool shouldQuit();
	ni::FrameData& getFrameData();
	ID3D12Device* getDevice();
	ni::FrameData& beginFrame();
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
	bool mouseDown(MouseButton button);
	bool keyDown(KeyCode keyCode);
	float randomFloat();
	uint32_t randomUint();
	double getSeconds();
	size_t getDXGIFormatBits(DXGI_FORMAT format);
	size_t getDXGIFormatBytes(DXGI_FORMAT format);
	Texture* createTexture(const wchar_t* name, uint32_t width, uint32_t height, uint32_t depth, const void* pixels, DXGI_FORMAT dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	void destroyTexture(Texture*& image);
	uint64_t murmurHash(const void* key, uint64_t keyLength, uint64_t seed);
	inline void* offsetPtr(void* Ptr, intptr_t Offset) { return (void*)((intptr_t)Ptr + Offset); }
	inline void* alignPtr(void* Ptr, size_t Alignment) { return (void*)(((uintptr_t)(Ptr)+((uintptr_t)(Alignment)-1LL)) & ~((uintptr_t)(Alignment)-1LL)); }
	inline size_t alignSize(size_t Value, size_t Alignment) { return ((Value)+((Alignment)-1LL)) & ~((Alignment)-1LL); }
	size_t getFileSize(const char* path);
	bool readFile(const char* path, void* outBuffer);
	void* allocReadFile(const char* path);
}
