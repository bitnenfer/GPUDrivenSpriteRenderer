#pragma once

#include "gfx.h"
#include "utils.h"
#include "matrix.h"

// We can have 1 texture per draw.
#define MAX_DRAW_COMMANDS 1000000
#define SPRITE_VERTEX_COUNT 6
#define THREAD_GROUP_SIZE 1024
#define TEXTURE_ID_OFFSET 5

#define OP_CULL_SPRITES 0
#define OP_UPDATE_COMMANDS 1
#define OP_GENERATE_SPRITES 2

#if _DEBUG
#define OUTPUT_PATH "x64/Debug/"
#else 
#define OUTPUT_PATH "x64/Release/"
#endif

#define NK_COLOR_UINT(color)                                                   \
    (((color) & 0xff) << 24) | ((((color) >> 8) & 0xff) << 16) |               \
        ((((color) >> 16) & 0xff) << 8) | ((color) >> 24)
#define NK_COLOR_RGBA_UINT(r, g, b, a)                                         \
    ((uint32_t)(r)) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) |           \
        ((uint32_t)(a) << 24)
#define NK_COLOR_RGB_UINT(r, g, b) NK_COLOR_RGBA_UINT(r, g, b, 0xff)
#define NK_COLOR_RGBA_FLOAT(r, g, b, a)                                        \
    NK_COLOR_RGBA_UINT((uint8_t)((r) * 255.0f), (uint8_t)((g) * 255.0f),       \
                       (uint8_t)((b) * 255.0f), (uint8_t)((a) * 255.0f))
#define NK_COLOR_RGB_FLOAT(r, g, b) NK_COLOR_RGBA_FLOAT(r, g, b, 1.0f)

struct SpriteVertex {
    float position[2];
    float texCoord[2];
    uint32_t color;
    uint32_t textureId;
};

struct SpriteQuad {
    SpriteVertex v0;
    SpriteVertex v1;
    SpriteVertex v2;
    SpriteVertex v3;
    SpriteVertex v4;
    SpriteVertex v5;
};

struct DrawCommand {
    float image[4];
    float transform[4];
    uint32_t color;
    uint32_t textureId;
};

struct Transform {
    float x;
    float y;
    float scale;
    float rotation;
};

struct SpriteRenderer {
    struct IndirectCommand {
        D3D12_DRAW_ARGUMENTS draw;
    };

    SpriteRenderer();
    ~SpriteRenderer();

    void buildSpriteRender();
    void buildSpriteGen();
    inline void pushMatrix() { matrixStack.pushMatrix(); }
    inline void popMatrix() { matrixStack.popMatrix(); }
    inline void loadIdentity() { matrixStack.loadIdentity(); }
    inline void translate(float x, float y) { matrixStack.translate(x, y); }
    inline void rotate(float rad) { matrixStack.rotate(rad); }
    inline void scale(float x, float y) { matrixStack.scale(x, y); }
    void reset();
    void drawImage(float x, float y, float width, float height, uint32_t color, gfx::Image2D* image);
    void flushCommands(gfx::RenderFrame& frame);

private:
    gfx::Resource gpuDrawCommands[FRAME_COUNT];
    gfx::Resource gpuUploadBuffer;
    gfx::Resource gpuSpriteVertices;
    gfx::Resource gpuSpriteVerticesCounter;
    gfx::Resource gpuVisibleList;
    gfx::Resource gpuPerLaneOffset;
    gfx::Resource gpuCounterZero;
    gfx::Resource gpuIndirectCommandBuffer;
    gfx::Resource gpuClearIndirectCommandBuffer;
    ID3D12CommandSignature* gpuDrawCommandSignature;
    ID3D12RootSignature* gpuSpriteGenRootSignature;
    ID3D12PipelineState* gpuSpriteGenPSO;
    ID3D12RootSignature* gpuSpriteRenderRootSignature;
    ID3D12PipelineState* gpuSpriteRenderPSO;
    TransformStack matrixStack;
    DrawCommand* drawCommands;
    uint32_t drawCommandNum;
    gfx::Image2D** images;
    uint32_t imageNum;
};
