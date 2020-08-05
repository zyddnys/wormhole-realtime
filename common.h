#pragma once

#include <Windows.h>
#include <wrl.h>

#undef min
#undef max
#undef CreateWindow

#include <vector>
#include <array>
#include <memory>
#include <cassert>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <exception>
#include <system_error>

using Microsoft::WRL::ComPtr;

#define THROW(x) { HRESULT const hr{x}; if( FAILED(hr) ) { throw std::runtime_error(std::system_category().message(hr)); } }
#define ThrowIfFailed THROW

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <dinput.h>

// D3D12 extension library.
#include "d3dx12.h"

using namespace DirectX;

template<typename TDst, typename TSrc>
auto comptr_cast(ComPtr<TSrc> src)
{
	ComPtr<TDst> dst;
	src.As(&dst);
	return dst;
}

struct COMScope
{
	COMScope()
	{
		CoInitialize(nullptr);
	}
	~COMScope()
	{
		CoUninitialize();
	}
};
