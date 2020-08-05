
struct VS_IN
{
	float2 pos		: POSITION;
	float2 texcoord	: TEXCOORD;
};

struct VS_OUT
{
	float4 pos		: SV_Position;
	float2 texcoord	: TEXCOORD;
};

VS_OUT main(VS_IN input)
{
	VS_OUT output;
	output.pos = float4(input.pos, 0.0f, 1.0f);
	output.texcoord = input.texcoord;
	return output;
}
