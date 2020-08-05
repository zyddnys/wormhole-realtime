#pragma once

#include "common.h"

struct Wormhole
{
	float mass;
	float radius;
	float length;
	float pad;

	Wormhole() :mass(0.1f), radius(0.5f), length(0.0f), pad(0)
	{
		;
	}
};
