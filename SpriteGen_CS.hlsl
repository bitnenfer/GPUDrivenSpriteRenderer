#define THREAD_GROUP_SIZE 1024
#define OP_CULL_SPRITES 0
#define OP_GENERATE_SPRITES 1
#define CULL_OFFSET 100

struct DrawCommand {
    float4 image;
    float4 transform;
    uint color;
    uint textureId;
};

struct SpriteVertex {
    float2 position;
    float2 texCoord;
    uint color;
    uint textureId;
};

struct SpriteQuad {
    SpriteVertex vertices[6];
};

cbuffer ConstantData : register(b0) {
    float2 resolution;
    uint totalDrawCmds;
    uint operationId;
};

struct Draw {
    uint vertexCountPerInstance;
    uint instanceCount;
    uint startVertexLocation;
    uint startInstanceLocation;
};

struct IndirectCommand {
    Draw draw;
};

RWStructuredBuffer<DrawCommand> drawCommands : register(u0);
RWStructuredBuffer<SpriteQuad> spriteVertices : register(u1);
RWStructuredBuffer<IndirectCommand> indirectCommands : register(u2);
RWStructuredBuffer<uint> visibleList : register(u3);
RWStructuredBuffer<uint> perLaneOffset : register(u4);

float2 transform(float2 position, DrawCommand cmd) {
    float2 v = position;
    float cr = cos(cmd.transform.w);
    float sr = sin(cmd.transform.w);
#if 0
    v *= cmd.transform.z;
    v = float2(v.x * cr - v.y * sr, v.x * sr + v.y * cr);
    v += cmd.transform.xy;
#else
    v = float2(v.x * cr - v.y * sr, v.x * sr + v.y * cr);
    v += cmd.transform.xy;
    v *= cmd.transform.z;
#endif
    return v;
}

bool isQuadVisible(float2 v0, float2 v1, float2 v2, float2 v3) {
    float minX = min(min(min(v0.x, v1.x), v2.x), v3.x);
    float minY = min(min(min(v0.y, v1.y), v2.y), v3.y);
    float maxX = max(max(max(v0.x, v1.x), v2.x), v3.x);
    float maxY = max(max(max(v0.y, v1.y), v2.y), v3.y);
    float4 boundingBox = float4(minX, minY, maxX, maxY);
    bool overlapX = boundingBox.x < resolution.x - CULL_OFFSET && boundingBox.z > CULL_OFFSET;
    bool overlapY = boundingBox.y < resolution.y - CULL_OFFSET && boundingBox.w > CULL_OFFSET;
    return overlapX && overlapY;
}

SpriteVertex createVertex(float2 position, float2 texCoord, uint textureId, uint color) {
    SpriteVertex vert;
    float2 resultPosition = (position * resolution) * 2.0 - 1.0;
    vert.position = float2(resultPosition.x, -resultPosition.y);
    vert.texCoord = texCoord;
    vert.textureId = textureId;
    vert.color = color;
    return vert;
}

void calculatePerLaneOffset() {
    uint steps = totalDrawCmds / WaveGetLaneCount() + 1;
    for (uint index = 1; index < steps; ++index) {
        perLaneOffset[index] = perLaneOffset[index - 1] + perLaneOffset[index];
    }
}

void cullSprite(uint drawCmdIndex) {
    DrawCommand cmd = drawCommands[drawCmdIndex];
    float4 image = cmd.image;
    float2 v0 = transform(image.xy, cmd);
    float2 v1 = transform(float2(image.x, image.y + image.w), cmd);
    float2 v2 = transform(float2(image.x + image.z, image.y + image.w), cmd);
    float2 v3 = transform(float2(image.x + image.z, image.y), cmd);
    uint visible = uint(isQuadVisible(v0, v1, v2, v3));
    uint drawIndex = WavePrefixSum(visible) + 1;
    visibleList[drawCmdIndex] = drawIndex;
    drawCommands[drawCmdIndex].textureId |= (0xfffff << 12) * uint(!visible);
    if ((drawCmdIndex % WaveGetLaneCount()) == (WaveGetLaneCount() - 1)) {
        perLaneOffset[drawCmdIndex / WaveGetLaneCount() + 1] = drawIndex - uint((!visible && drawIndex > 0));
    }
}

void generateSprites(uint drawCmdIndex) {
    uint localOffset = visibleList[drawCmdIndex];
    uint laneIndex = drawCmdIndex / WaveGetLaneCount();
    uint laneOffset = perLaneOffset[laneIndex];
    uint drawOffset = localOffset + laneOffset;
    DrawCommand cmd = drawCommands[drawCmdIndex];
    uint textureId = cmd.textureId;
    uint visibleBits = (textureId >> 12);
    uint visibleMask = visibleBits & 0xfffff;
    if (visibleMask == 0) {
        visibleList[drawCmdIndex] = drawOffset;
        drawCommands[drawCmdIndex].textureId |= drawOffset << 12;
        uint textureId = drawCommands[drawCmdIndex].textureId;
        uint color = cmd.color;
        SpriteQuad quad;
        float4 image = cmd.image;
        float2 v0 = transform(image.xy, cmd);
        float2 v1 = transform(float2(image.x, image.y + image.w), cmd);
        float2 v2 = transform(float2(image.x + image.z, image.y + image.w), cmd);
        float2 v3 = transform(float2(image.x + image.z, image.y), cmd);
        SpriteVertex sv0 = createVertex(v0, float2(0, 0), textureId, color);
        SpriteVertex sv1 = createVertex(v1, float2(0, 1), textureId, color);
        SpriteVertex sv2 = createVertex(v2, float2(1, 1), textureId, color);
        SpriteVertex sv3 = createVertex(v3, float2(1, 0), textureId, color);
        quad.vertices[0] = sv0;
        quad.vertices[1] = sv1;
        quad.vertices[2] = sv2;
        quad.vertices[3] = sv0;
        quad.vertices[4] = sv2;
        quad.vertices[5] = sv3;
        spriteVertices[drawOffset] = quad;
        InterlockedAdd(indirectCommands[0].draw.vertexCountPerInstance, 6);
    }
}

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint drawCmdIndex = dispatchThreadId.x;
    if (drawCmdIndex < totalDrawCmds) {
        if (operationId == OP_CULL_SPRITES) {
            cullSprite(drawCmdIndex);
            if (drawCmdIndex == totalDrawCmds - 1) {
                calculatePerLaneOffset();
            }
        } else if (operationId == OP_GENERATE_SPRITES) {
            generateSprites(drawCmdIndex);
        }
        AllMemoryBarrierWithGroupSync();
    }
}
