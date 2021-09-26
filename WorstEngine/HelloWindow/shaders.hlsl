
struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

cbuffer MVPBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Pro;
};


Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD)
{
	PSInput result;
	float4 pos = position;

	// Transform the position from object space to homogeneous projection space
	pos = mul(World, pos);
	pos = mul(View, pos);
	pos = mul(Pro, pos);

	/*pos = mul(pos,transpose(World));
	pos = mul(pos, transpose(View));
	pos = mul(pos, transpose(Pro));*/


	result.position = pos;
	result.uv = uv;

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return g_texture.Sample(g_sampler, input.uv);
}