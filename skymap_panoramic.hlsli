
float2 dir2uv(float3 dir)
{
	float phi = atan2(dir.x, dir.z);
	phi = fmod(phi + g_2PI, g_2PI) / g_2PI;

	return float2(phi, (dir.y + 1.0f) * 0.5f);
}
