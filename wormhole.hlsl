Texture2D t1 : register(t0);
Texture2D t2 : register(t1);
StructuredBuffer<float2> g_phi : register(t2);
SamplerState s1 : register(s0);
RWTexture2D<float4> g_dst : register(u0);

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

struct Wormhole
{
	float mass;
	float radius;
	float length;
	float pad;
};

struct CacheSize
{
	uint size;
	uint3 pad;
};

#define g_PI 3.141592653589793238462643383279502884197169399375105820974
#define g_2PI 6.283185307179586476925286766559005768394338798750211641949

ConstantBuffer<CameraData> g_Camera			: register(b0);
ConstantBuffer<Wormhole> g_Wormhole			: register(b1);
ConstantBuffer<CacheSize> g_PhiCahceSize	: register(b2);

#include "skymap_panoramic.hlsli"
#include "matrix_ops.hlsli"

float3 null_geodesic_2d(float3 l_phi_pl, float a, float M, float rho, float B_sqr, float b)
{
	float l = l_phi_pl.x;
	float phi = l_phi_pl.y;
	float pl = l_phi_pl.z;

	int outside_wormhole = abs(l) > a;

	float x = 2.0f * (abs(l) - a) / (g_PI * M);
	float atanx = atan(x);
	float b_sqr = b * b;

	// r (5)
	float r = rho + outside_wormhole * (M * (x * atanx - 0.5f * log(1.0f + x * x)));
	// dr/dl
	float dr_dl = outside_wormhole * (atanx * 2.0f * sign(l) / g_PI);

	// dl/dt (A.7a)
	float dl_dt = pl;
	// dφ/dt (A.7c)
	float dphi_dt = b / (r * r);
	// dpl/dt (A.7d)
	float dpl_dt = B_sqr * dr_dl / (r * r * r);

	return float3(dl_dt, dphi_dt, dpl_dt);
}

float2 phi_mapping(float phi)
{
	phi = fmod(phi + g_2PI, g_2PI);
	float phi_lookup = phi / g_2PI;
	uint lookup_idx = min(uint(phi_lookup * g_PhiCahceSize.size), g_PhiCahceSize.size - 1);
	return g_phi[lookup_idx];
}

float2 phi_mapping_old(float phi, float l)
{
	phi = fmod(phi + g_2PI, g_2PI);

	float l_c = l;

	float n_l = cos(phi);
	float n_phi = -sin(phi);

	float p_l = n_l;
	float p_phi = l_c * n_phi;

	float b = p_phi;
	float B_sqr = l_c * l_c * (n_phi * n_phi);

	float h = 0.1;

	float3 l_phi_pl = float3(l_c, 0.0f, p_l);

	for (int i = 0; i < 100; ++i)
	{
		float3 k1 = h * null_geodesic_2d(l_phi_pl, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);
		float3 k2 = h * null_geodesic_2d(l_phi_pl + 0.5f * k1, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);
		float3 k3 = h * null_geodesic_2d(l_phi_pl + 0.5f * k2, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);
		float3 k4 = h * null_geodesic_2d(l_phi_pl + k3, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);

		l_phi_pl = l_phi_pl + (1.0f / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
	}

	float phi_traced = l_phi_pl.y;
	float l_traced = l_phi_pl.x;

	phi_traced = fmod(phi_traced + g_2PI, g_2PI);

	return float2(-phi_traced * sign(l), l_traced);
	//return float2(-phi_traced, l_traced);
}

[numthreads(32, 32, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
	float width = g_Camera.width;
	float height = g_Camera.height;

	float fovX = g_Camera.fovX;
	float fovY = g_Camera.fovY;

	float3 ray_pos = g_Camera.position.xyz;
	//float ray_l = g_Camera.position.w;
	float3 dir = g_Camera.forward.xyz;
	float3 up = g_Camera.up.xyz;
	float3 right = g_Camera.right.xyz;

	float x = float(tid.x) / float(g_Camera.width) * 2.0f - 1.0f;
	float y = float(tid.y) / float(g_Camera.height) * 2.0f - 1.0f;

	float3 r = right * g_Camera.fovX * x * 0.5f;
	float3 u = up * g_Camera.fovY * y * 0.5f;
	float3 ray_dir = normalize(r + u + dir);

	float3 new_x = normalize(ray_pos);
	float3 new_y;
	float3 new_z = float3(0, 0, 1);

	if (abs(abs(dot(ray_dir, new_x)) - 1.0f) < 1e-8f)
	{
		new_y = cross(new_x, new_z);
		new_z = cross(new_y, new_x);
	}
	else
	{
		new_z = cross(ray_dir, new_x);
		new_y = cross(new_x, new_z);
	}
	new_y = normalize(new_y);
	new_z = normalize(new_z);

	// get inverse transformation
	float4x4 local2global;
	local2global._11 = new_x.x; local2global._12 = new_x.y; local2global._13 = new_x.z; local2global._14 = 0;
	local2global._21 = new_y.x; local2global._22 = new_y.y; local2global._23 = new_y.z; local2global._24 = 0;
	local2global._31 = new_z.x; local2global._32 = new_z.y; local2global._33 = new_z.z; local2global._34 = 0;
	local2global._41 = 0; local2global._42 = 0; local2global._43 = 0; local2global._44 = 1.0;

	float4x4 global2local = inverse(local2global); // matrix inverse to get forward transformation

	float3 ray_dir_local_frame = mul(float4(ray_dir, 0.0f), global2local).xyz;
	float ray_phi_camera = atan2(ray_dir_local_frame.y, ray_dir_local_frame.x);

	float2 traced_result = phi_mapping(ray_phi_camera); // phi mapping
	float phi_traced = traced_result.x;
	float l_traced = traced_result.y;

	float local_x, local_y, local_z = 0.0f;
	sincos(phi_traced, local_y, local_x);

	float3 traced_ray_dir = float3(local_x, local_y, local_z);
	float3 traced_ray_global_frame = mul(float4(traced_ray_dir, 0.0f), local2global).xyz;

	float2 texCoord = dir2uv(traced_ray_global_frame);
	if (l_traced < 0)
		g_dst[tid.xy] = t2.SampleLevel(s1, texCoord, 0);
	else
		g_dst[tid.xy] = t1.SampleLevel(s1, texCoord, 0);
}
