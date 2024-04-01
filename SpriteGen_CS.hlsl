#define THREAD_GROUP_SIZE 64

#define OP_CULL_SPRITES 0
#define OP_UPDATE_COMMANDS 1
#define OP_GENERATE_SPRITES 2

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
    SpriteVertex v0;
    SpriteVertex v1;
    SpriteVertex v2;
    SpriteVertex v3;
    SpriteVertex v4;
    SpriteVertex v5;
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

float4 bounds(float2 v0, float2 v1, float2 v2, float2 v3) {
    float minX = min(min(min(v0.x, v1.x), v2.x), v3.x);
    float minY = min(min(min(v0.y, v1.y), v2.y), v3.y);
    float maxX = max(max(max(v0.x, v1.x), v2.x), v3.x);
    float maxY = max(max(max(v0.y, v1.y), v2.y), v3.y);
    return float4(minX, minY, maxX, maxY);
}

bool isQuadVisible(float2 v0, float2 v1, float2 v2, float2 v3) {
    float4 boundingBox = bounds(v0, v1, v2, v3);
    bool overlapX = boundingBox.x < resolution.x && boundingBox.z > 0;
    bool overlapY = boundingBox.y < resolution.y && boundingBox.w > 0;
    return overlapX && overlapY;
}

SpriteVertex createVertex(float2 position, float2 texCoord, uint textureId, uint color) {
    SpriteVertex vert;
    float2 resultPosition = (position * resolution) * 2.0 - 1.0;
    resultPosition.y = -resultPosition.y;
    vert.position = float2(resultPosition);
    if (abs(vert.position.x) > 2.0) {
        vert.position.x = 0.0 / 0.0;
    }
    if (abs(vert.position.y) > 2.0) {
        vert.position.y = 0.0 / 0.0;
    }
    vert.texCoord = texCoord;
    vert.textureId = textureId;
    vert.color = color;
    return vert;
}

// OP_CULL_SPRITES 0
void cullSprite(uint drawCmdIndex) {
    DrawCommand cmd = drawCommands[drawCmdIndex];
    uint color = cmd.color;
    visibleList[drawCmdIndex] = 0;
    drawCommands[drawCmdIndex].textureId |= (0xfffff << 12);
    if (((color >> 24) & 0xFF) > 0) {
        float4 image = cmd.image;
        float2 v0 = transform(image.xy, cmd);
        float2 v1 = transform(float2(image.x, image.y + image.w), cmd);
        float2 v2 = transform(float2(image.x + image.z, image.y + image.w), cmd);
        float2 v3 = transform(float2(image.x + image.z, image.y), cmd);
        uint visible = uint(isQuadVisible(v0, v1, v2, v3));
        uint drawIndex = WavePrefixSum(visible) + 1;
        visibleList[drawCmdIndex] = drawIndex;
        uint4 visibleLanesBits = WaveActiveBallot(visible);
        if (visible) {
            drawCommands[drawCmdIndex].textureId &= ~(0xfffffu << 12u);
        }
        if ((drawCmdIndex % WaveGetLaneCount()) == (WaveGetLaneCount() - 1)) {
            if (!visible && drawIndex > 0) {
                drawIndex -= 1;
            }
            perLaneOffset[drawCmdIndex / WaveGetLaneCount() + 1] = drawIndex;
        }
    }
    if (drawCmdIndex == totalDrawCmds - 1) {
        uint steps = totalDrawCmds / WaveGetLaneCount() + 1;
        for (uint index = 1; index < steps; ++index) {
            perLaneOffset[index] = perLaneOffset[index - 1] + perLaneOffset[index];
        }
    }
}

// OP_UPDATE_COMMANDS 1
void updateCommands(uint drawCmdIndex) {
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
        quad.v0 = createVertex(v0, float2(0, 0), textureId, color);
        quad.v1 = createVertex(v1, float2(0, 1), textureId, color);
        quad.v2 = createVertex(v2, float2(1, 1), textureId, color);
        quad.v3 = quad.v0;
        quad.v4 = quad.v2;
        quad.v5 = createVertex(v3, float2(1, 0), textureId, color);
        spriteVertices[drawOffset] = quad;
        InterlockedAdd(indirectCommands[0].draw.vertexCountPerInstance, 6);
    } else {
        visibleList[drawCmdIndex] = ~0u;
    }
}

// OP_GENERATE_SPRITES 2
void generateSprites(uint drawCmdIndex) {
    uint spriteOffset = visibleList[drawCmdIndex];
    if (spriteOffset != ~0u) {
        DrawCommand cmd = drawCommands[drawCmdIndex];
        uint textureId = cmd.textureId & 0xfff;
        uint color = cmd.color;
        SpriteQuad quad;
        float4 image = cmd.image;
        float2 v0 = transform(image.xy, cmd);
        float2 v1 = transform(float2(image.x, image.y + image.w), cmd);
        float2 v2 = transform(float2(image.x + image.z, image.y + image.w), cmd);
        float2 v3 = transform(float2(image.x + image.z, image.y), cmd);
        quad.v0 = createVertex(v0, float2(0, 0), textureId, color);
        quad.v1 = createVertex(v1, float2(0, 1), textureId, color);
        quad.v2 = createVertex(v2, float2(1, 1), textureId, color);
        quad.v3 = quad.v0;
        quad.v4 = quad.v2;
        quad.v5 = createVertex(v3, float2(1, 0), textureId, color);
        spriteVertices[spriteOffset] = quad;
        InterlockedAdd(indirectCommands[0].draw.vertexCountPerInstance, 6);
    }
}

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID, uint3 groupId : SV_GroupID) {
    uint drawCmdIndex = dispatchThreadId.x;
    if (drawCmdIndex < totalDrawCmds) {
        if (operationId == OP_CULL_SPRITES) {
            cullSprite(drawCmdIndex);
        } else if (operationId == OP_UPDATE_COMMANDS) {
            updateCommands(drawCmdIndex);
        } else if (operationId == OP_GENERATE_SPRITES) {
            //generateSprites(drawCmdIndex);
        }
    }
}
