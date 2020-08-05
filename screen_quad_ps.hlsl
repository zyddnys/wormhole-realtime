Texture2D t1 : register(t0);
SamplerState s1 : register(s0);

struct VS_OUT
{
	float4 pos		: SV_Position;
	float2 texcoord	: TEXCOORD;
};

float4 main(VS_OUT input) : SV_TARGET
{
	//return float4(1,0,0,1);
    // return interpolated color
    return t1.Sample(s1, input.texcoord);
}
