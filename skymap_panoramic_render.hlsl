Texture2D t1 : register(t0);
RWTexture2D<float4> g_dst : register(u0);
SamplerState s1 : register(s0);

struct CameraData
{
	float4 position;
	float4 forward;
	float4 up;
	float4 right;
	float fovX;
	float fovY;
	float width;
	float height;
};

#define g_PI 3.141592653589793238462643383279502884197169399375105820974f
#define g_2PI 6.283185307179586476925286766559005768394338798750211641949f

ConstantBuffer<CameraData> g_Camera : register(b0);

#include "skymap_panoramic.hlsli"

[numthreads(32, 32, 1)]
void main( uint3 tid : SV_DispatchThreadID )
{
	float width = g_Camera.width;
	float height = g_Camera.height;

	float fovX = g_Camera.fovX;
	float fovY = g_Camera.fovY;

	float3 ray_pos = g_Camera.position.xyz;
	float3 dir = g_Camera.forward.xyz;
	float3 up = g_Camera.up.xyz;
	float3 right = g_Camera.right.xyz;

	float x = float(tid.x) / float(g_Camera.width) * 2.0f - 1.0f;
	float y = float(tid.y) / float(g_Camera.height) * 2.0f - 1.0f;

	float3 r = right * g_Camera.fovX * x * 0.5f;
	float3 u = up * g_Camera.fovY * y * 0.5f;
	float3 ray_dir = normalize(r + u + dir);

	float2 texCoord = dir2uv(ray_dir);
	g_dst[tid.xy] = t1.SampleLevel(s1, texCoord, 0);
}
