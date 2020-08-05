RWStructuredBuffer<float2> g_phiMapping : register(u0);

struct MappingData
{
	int bufferSize;
	float l;
	float r;
	uint pad;
};

struct Wormhole
{
	float mass;
	float radius;
	float length;
	float pad;
};

ConstantBuffer<MappingData> g_MappingData : register(b0);
ConstantBuffer<Wormhole> g_Wormhole : register(b1);

#define g_PI 3.141592653589793238462643383279502884197169399375105820974f
#define g_2PI 6.283185307179586476925286766559005768394338798750211641949f

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

float2 phi_mapping(float phi, float l, float r)
{
	//if (g_Wormhole.radius < 0.0001f && g_Wormhole.mass <= 0.0001f)
	//{
	//	return float2(phi, l);
	//}
	float l_c = l;

	float n_l = cos(phi);
	float n_phi = -sin(phi);

	float p_l = n_l;
	float p_phi = r * n_phi;

	float b = p_phi;
	float B_sqr = r * r * (n_phi * n_phi);

	float h = 0.01;

	float3 l_phi_pl = float3(l_c, 0.0f, p_l);

	for (int i = 0; i < 10000; ++i)
	{
		float3 k1 = h * null_geodesic_2d(l_phi_pl, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);
		float3 k2 = h * null_geodesic_2d(l_phi_pl + 0.5f * k1, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);
		float3 k3 = h * null_geodesic_2d(l_phi_pl + 0.5f * k2, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);
		float3 k4 = h * null_geodesic_2d(l_phi_pl + k3, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);

		l_phi_pl = l_phi_pl + (1.0f / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
	}

	h *= 10.0f;

	float3 k1 = h * null_geodesic_2d(l_phi_pl, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);
	float3 k2 = h * null_geodesic_2d(l_phi_pl + 0.5f * k1, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);
	float3 k3 = h * null_geodesic_2d(l_phi_pl + 0.5f * k2, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);
	float3 k4 = h * null_geodesic_2d(l_phi_pl + k3, g_Wormhole.length, g_Wormhole.mass, g_Wormhole.radius, B_sqr, b);

	float3 l_phi_pl_last = l_phi_pl + (1.0f / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);

	float x1, y1, x2, y2;
	sincos(l_phi_pl.y, y1, x1);
	x1 *= l_phi_pl.x;
	y1 *= l_phi_pl.x;
	sincos(l_phi_pl_last.y, y2, x2);
	x2 *= l_phi_pl_last.x;
	y2 *= l_phi_pl_last.x;

	float phi_traced = atan2(y2 - y1, x2 - x1);
	float l_traced = l_phi_pl_last.x;

	phi_traced = fmod(phi_traced + g_2PI, g_2PI);

	return float2(-phi_traced, l_traced);
}

[numthreads(1024, 1, 1)]
void main( uint3 tid : SV_DispatchThreadID )
{
	int buffer_size = g_MappingData.bufferSize;
	float l = g_MappingData.l;
	float r = g_MappingData.r;

	float input_phi_value = float(tid.x) / float(buffer_size) * g_2PI;

	float2 result = phi_mapping(input_phi_value, l, r);

	g_phiMapping[tid.x] = result;
}
