Texture2D<float4> mainTexture[] : register(t0);
SamplerState samplerPoint  : register(s0);

struct PixelVertex {
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
	nointerpolation float4 color : COLOR0;
	nointerpolation uint textureId : TEXCOORD1;
};

struct PixelOutput {
	float4 color : SV_TARGET;
};

PixelOutput main(PixelVertex vtx) {
	PixelOutput output;
	float4 color = mainTexture[vtx.textureId].SampleLevel(samplerPoint, vtx.texCoord, 0);
	if (color.a == 0.0) {
		discard;
		return output;
	}
	if (dot(vtx.color, float4(1, 1, 1, 1)) != 1.0) {
		color *= vtx.color;
	}
	output.color = color;
	return output;
}