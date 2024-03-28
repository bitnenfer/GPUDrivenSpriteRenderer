
struct SpriteVertex {
	float2 position : POSITION0;
	float2 texCoord : TEXCOORD0;
	nointerpolation float4 color : COLOR0;
	nointerpolation uint textureId : TEXCOORD1;
};
struct PixelVertex {
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
	nointerpolation float4 color : COLOR0;
	nointerpolation uint textureId : TEXCOORD1;
};

PixelVertex main(SpriteVertex vtx) {
	PixelVertex vtxOut;
	vtxOut.position = float4(vtx.position, 0, 1);
	vtxOut.texCoord = vtx.texCoord;
	vtxOut.color = vtx.color;
	vtxOut.textureId = vtx.textureId;
	return vtxOut;
}