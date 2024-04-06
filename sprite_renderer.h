#pragma once

#include "ni.h"
#include "matrix.h"

// We can have 1 texture per draw.
#define MAX_DRAW_COMMANDS 1000000
#define SPRITE_VERTEX_COUNT 6
#define THREAD_GROUP_SIZE 1024
#define TEXTURE_ID_OFFSET 5

#define OP_CULL_SPRITES 0
#define OP_GENERATE_SPRITES 1

#if _DEBUG
#define OUTPUT_PATH "x64/Debug/"
#else 
#define OUTPUT_PATH "x64/Release/"
#endif



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
    void drawImage(float x, float y, float width, float height, uint32_t color, ni::Texture* image);
    void flushCommands(ni::FrameData& frame);

private:
    ni::Resource gpuDrawCommands[NI_FRAME_COUNT];
    ni::Resource gpuUploadBuffer;
    ni::Resource gpuSpriteVertices;
    ni::Resource gpuSpriteVerticesCounter;
    ni::Resource gpuVisibleList;
    ni::Resource gpuPerLaneOffset;
    ni::Resource gpuCounterZero;
    ni::Resource gpuIndirectCommandBuffer;
    ni::Resource gpuClearIndirectCommandBuffer;
    ID3D12CommandSignature* gpuDrawCommandSignature;
    ID3D12RootSignature* gpuSpriteGenRootSignature;
    ID3D12PipelineState* gpuSpriteGenPSO;
    ID3D12RootSignature* gpuSpriteRenderRootSignature;
    ID3D12PipelineState* gpuSpriteRenderPSO;
    TransformStack matrixStack;
    DrawCommand* drawCommands;
    uint32_t drawCommandNum;
    ni::Texture** images;
    uint32_t imageNum;
};
