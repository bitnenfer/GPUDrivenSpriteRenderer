#define THREAD_GROUP_SIZE 1024
#define CULL_OFFSET 0

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

float2 transform(float2 position, DrawCommand cmd) {
    float2 v = position;
    float cr = cos(cmd.transform.w);
    float sr = sin(cmd.transform.w);
    v *= cmd.transform.z;
    v = float2(v.x * cr - v.y * sr, v.x * sr + v.y * cr);
    v += cmd.transform.xy;
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

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint drawCmdIndex = dispatchThreadId.x;
    DrawCommand cmd = drawCommands[drawCmdIndex];
    float4 image = cmd.image;
    float2 v0 = transform(image.xy, cmd);
    float2 v1 = transform(float2(image.x, image.y + image.w), cmd);
    float2 v2 = transform(float2(image.x + image.z, image.y + image.w), cmd);
    float2 v3 = transform(float2(image.x + image.z, image.y), cmd);
    float visible = float(isQuadVisible(v0, v1, v2, v3));
    SpriteVertex sv0 = { v0 * visible, float2(0, 0), cmd.color, cmd.textureId };
    SpriteVertex sv1 = { v1 * visible, float2(0, 1), cmd.color, cmd.textureId };
    SpriteVertex sv2 = { v2 * visible, float2(1, 1), cmd.color, cmd.textureId };
    SpriteVertex sv3 = { v3 * visible, float2(1, 0), cmd.color, cmd.textureId };
    SpriteQuad quad;
    quad.vertices[0] = sv0;
    quad.vertices[1] = sv1;
    quad.vertices[2] = sv2;
    quad.vertices[3] = sv0;
    quad.vertices[4] = sv2;
    quad.vertices[5] = sv3;
    spriteVertices[drawCmdIndex] = quad;
    InterlockedAdd(indirectCommands[0].draw.vertexCountPerInstance, 6);
}
