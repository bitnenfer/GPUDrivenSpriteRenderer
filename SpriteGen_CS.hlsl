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

struct ConstantData {
    float2 resolution;
    uint totalDrawCmds;
};

const ConstantData constantData : register(b0);
StructuredBuffer<DrawCommand> drawCommands : register(t0);
RWStructuredBuffer<SpriteVertex> spriteVertices : register(u0);
RWStructuredBuffer<uint> spriteIndices : register(u1);
RWStructuredBuffer<uint> processCounter : register(u2);

float2 transform(float2 position, DrawCommand cmd) {
    float2 v = position;
    v *= cmd.transform.z;
    float cr = cos(cmd.transform.w);
    float sr = sin(cmd.transform.w);
    v = float2(v.x * cr - v.y * sr, v.x * sr + v.y * cr);
    v += cmd.transform.xy;
    return v;
}

bool isQuadVisible(float2 v0, float2 v1, float2 v2, float2 v3) {
    if ((v0.x >= 0 && v0.x <= constantData.resolution.x && v0.y >= 0 && v0.y <= constantData.resolution.y) ||
        (v1.x >= 0 && v1.x <= constantData.resolution.x && v1.y >= 0 && v1.y <= constantData.resolution.y) ||
        (v2.x >= 0 && v2.x <= constantData.resolution.x && v2.y >= 0 && v2.y <= constantData.resolution.y) ||
        (v3.x >= 0 && v3.x <= constantData.resolution.x && v3.y >= 0 && v3.y <= constantData.resolution.y))
    {
        return true;
    }

    return false;
}

SpriteVertex createVertex(float2 position, float2 texCoord, uint textureId, uint color) {
    SpriteVertex vert;
    float2 resultPosition = (position / constantData.resolution) * 2.0 - 1.0;
    resultPosition.y = -resultPosition.y;
    vert.position = float2(resultPosition);
    vert.texCoord = texCoord;
    vert.textureId = textureId;
    vert.color = color;
    return vert;
}


[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint totalProcess = 0;
    InterlockedAdd(processCounter[0], 1, totalProcess);
    if (totalProcess < constantData.totalDrawCmds) {
        uint drawCmdIndex = dispatchThreadId.x;
        DrawCommand cmd = drawCommands[drawCmdIndex];
        float4 image = cmd.image;
        float2 v0 = transform(image.xy, cmd);
        float2 v1 = transform(float2(image.x, image.y + image.w), cmd);
        float2 v2 = transform(float2(image.x + image.z, image.y + image.w), cmd);
        float2 v3 = transform(float2(image.x + image.z, image.y), cmd);

        if (isQuadVisible(v0, v1, v2, v3)) {
            uint vertexIndex = drawCmdIndex * 4;
            uint color = cmd.color;
            uint textureId = cmd.textureId;;
            spriteVertices[vertexIndex + 0] = createVertex(v0, float2(0, 0), textureId, color);
            spriteVertices[vertexIndex + 1] = createVertex(v1, float2(0, 1), textureId, color);
            spriteVertices[vertexIndex + 2] = createVertex(v2, float2(1, 1), textureId, color);
            spriteVertices[vertexIndex + 3] = createVertex(v3, float2(1, 0), textureId, color);

            uint indexOffset = drawCmdIndex * 6;
            spriteIndices[indexOffset + 0] = (vertexIndex + 0);
            spriteIndices[indexOffset + 1] = (vertexIndex + 1);
            spriteIndices[indexOffset + 2] = (vertexIndex + 2);
            spriteIndices[indexOffset + 3] = (vertexIndex + 0);
            spriteIndices[indexOffset + 4] = (vertexIndex + 2);
            spriteIndices[indexOffset + 5] = (vertexIndex + 3);
        }
    }
}
