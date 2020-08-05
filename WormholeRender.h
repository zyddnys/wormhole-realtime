#pragma once

#include "common.h"
#include "Wormhole.h"
#include "Camera.h"
#include "DescriptorHeap.h"

std::uint32_t constexpr g_PhiCacheSize = 16384;

struct WormholeRender
{
	ComPtr<ID3D12PipelineState> pipelineState;
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pipelineStatePhiCache;
	ComPtr<ID3D12RootSignature> rootSignaturePhiCache;
	ComPtr<ID3D12Resource> phiCache;

	float last_l;

	WormholeRender() :pipelineState(nullptr), rootSignature(nullptr), pipelineStatePhiCache(nullptr), rootSignaturePhiCache(nullptr), phiCache(nullptr), last_l(0.0f)
	{
		;
	}

	WormholeRender(WormholeRender const &a) = delete;
	WormholeRender &operator=(WormholeRender const &a) = delete;

	WormholeRender(WormholeRender &&a) noexcept
	{
		pipelineState = std::move(a.pipelineState);
		rootSignature = std::move(a.rootSignature);
		pipelineStatePhiCache = std::move(a.pipelineStatePhiCache);
		rootSignaturePhiCache = std::move(a.rootSignaturePhiCache);
		phiCache = std::move(a.phiCache);
	}

	WormholeRender &operator=(WormholeRender &&a) noexcept
	{
		if (this != std::addressof(a))
		{
			pipelineState = std::move(a.pipelineState);
			rootSignature = std::move(a.rootSignature);
			pipelineStatePhiCache = std::move(a.pipelineStatePhiCache);
			rootSignaturePhiCache = std::move(a.rootSignaturePhiCache);
			phiCache = std::move(a.phiCache);
		}
		return *this;
	}

	void BuildPhiCacheResources(ComPtr<ID3D12Device2> device, DescriptorHeapWrapper& heap, std::size_t heapOffset)
	{
		D3D12_RESOURCE_DESC phiCacheDesc = {};

		phiCacheDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		phiCacheDesc.Alignment = 0; // may be 0, 4KB, 64KB, or 4MB. 0 will let runtime decide between 64KB and 4MB (4MB for multi-sampled textures)
		phiCacheDesc.Width = g_PhiCacheSize * sizeof(float) * 2;
		phiCacheDesc.Height = 1;
		phiCacheDesc.DepthOrArraySize = 1;
		phiCacheDesc.MipLevels = 1;
		phiCacheDesc.Format = DXGI_FORMAT_UNKNOWN;
		phiCacheDesc.SampleDesc.Count = 1;
		phiCacheDesc.SampleDesc.Quality = 0;
		phiCacheDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		phiCacheDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		THROW(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&phiCacheDesc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, // default: using
			nullptr,
			IID_PPV_ARGS(&phiCache)
		));

		D3D12_BUFFER_SRV bufferSRVDesc = {};
		bufferSRVDesc.FirstElement = 0;
		bufferSRVDesc.NumElements = g_PhiCacheSize;
		bufferSRVDesc.StructureByteStride = sizeof(float) * 2;
		bufferSRVDesc.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = phiCacheDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer = bufferSRVDesc;
		device->CreateShaderResourceView(phiCache.Get(), &srvDesc, heap.at_cpu(heapOffset));

		D3D12_BUFFER_UAV bufferUAVDesc = {};
		bufferUAVDesc.FirstElement = 0;
		bufferUAVDesc.NumElements = g_PhiCacheSize;
		bufferUAVDesc.StructureByteStride = sizeof(float) * 2;
		bufferUAVDesc.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = phiCacheDesc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer = bufferUAVDesc;
		device->CreateUnorderedAccessView(phiCache.Get(), nullptr, &uavDesc, heap.at_cpu(heapOffset + 1));


		ComPtr<ID3DBlob> computeShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"equatorial_phi_mapping.cso", &computeShaderBlob));

		// Create a root signature.
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			OutputDebugString(L"Using Root Signature v1.0\n");
		}
		else
			OutputDebugString(L"Using Root Signature v1.1\n");

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		// A single 32-bit constant root parameter that is used by the vertex shader.
		CD3DX12_ROOT_PARAMETER1 rootParameters[3] = {};

		rootParameters[0].InitAsConstants(4, 0);
		rootParameters[1].InitAsConstants(4, 1);
		auto r1 = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		rootParameters[2].InitAsDescriptorTable(1, std::addressof(r1));

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription{};
		rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

		// Serialize the root signature.
		ComPtr<ID3DBlob> rootSignatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		auto hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
														featureData.HighestVersion, &rootSignatureBlob, &errorBlob);
		if (FAILED(hr))
		{
			std::string errorString;

			if (errorBlob)
			{
				errorString.append(": ");

				errorString.append(
					reinterpret_cast<char*>(
					errorBlob->GetBufferPointer()),
					errorBlob->GetBufferSize());
				MessageBoxA(NULL, errorString.c_str(), "WormholeRender", MB_OK | MB_ICONERROR);
			}
			__debugbreak();
		}
		// Create the root signature.
		ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
					  rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignaturePhiCache)));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
		psoDesc.pRootSignature = rootSignaturePhiCache.Get(); // the root signature that describes the input data this pso needs
		psoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());

		ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStatePhiCache)));
	}

	WormholeRender(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList> commandList, DescriptorHeapWrapper& heap, std::size_t heapOffset)
	{
		ComPtr<ID3DBlob> computeShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"wormhole.cso", &computeShaderBlob));

		// Create a root signature.
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			OutputDebugString(L"Using Root Signature v1.0\n");
		}
		else
			OutputDebugString(L"Using Root Signature v1.1\n");

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		CD3DX12_ROOT_PARAMETER1 rootParameters[6] = {};

		rootParameters[0].InitAsConstants(20, 0); // camera
		rootParameters[1].InitAsConstants(4, 1);  // wormhole
		rootParameters[2].InitAsConstants(4, 2);  // cache size
		auto r1 = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
		rootParameters[3].InitAsDescriptorTable(1, std::addressof(r1));
		auto r2 = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		rootParameters[4].InitAsDescriptorTable(1, std::addressof(r2));
		auto r3 = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
		rootParameters[5].InitAsDescriptorTable(1, std::addressof(r3));

		// create a static sampler
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription{};
		rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, std::addressof(sampler), rootSignatureFlags);

		// Serialize the root signature.
		ComPtr<ID3DBlob> rootSignatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		auto hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
			featureData.HighestVersion, &rootSignatureBlob, &errorBlob);
		if (FAILED(hr))
		{
			std::string errorString;

			if (errorBlob)
			{
				errorString.append(": ");

				errorString.append(
					reinterpret_cast<char *>(
					errorBlob->GetBufferPointer()),
					errorBlob->GetBufferSize());
				MessageBoxA(NULL, errorString.c_str(), "WormholeRender", MB_OK | MB_ICONERROR);
			}
			__debugbreak();
		}
		// Create the root signature.
		ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
			rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
		psoDesc.pRootSignature = rootSignature.Get(); // the root signature that describes the input data this pso needs
		psoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());
		//psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG;

		ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));

		BuildPhiCacheResources(device, heap, heapOffset);
	}

	void FillPhiCache(ComPtr<ID3D12GraphicsCommandList> commandList,
					  Camera& cam,
					  Wormhole& wormhole,
					  DescriptorHeapWrapper& heap,
					  std::size_t emptyPhiCacheUAVHeapOffset)
	{
		commandList->SetPipelineState(pipelineStatePhiCache.Get());
		commandList->SetComputeRootSignature(rootSignaturePhiCache.Get());

		ID3D12DescriptorHeap* descriptorHeaps[] = { heap.heap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(phiCache.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

		struct
		{
			int bufferSize;
			float l;
			float r;
			UINT pad;
		} cb{ g_PhiCacheSize, cam.GetL(), cam.GetR(), 0 };

		commandList->SetComputeRoot32BitConstants(0, 4, &cb, 0);
		commandList->SetComputeRoot32BitConstants(1, 4, &wormhole, 0);
		commandList->SetComputeRootDescriptorTable(2, heap.at_gpu(emptyPhiCacheUAVHeapOffset + 1));

		UINT groupX(((g_PhiCacheSize - 1) / 1024) + 1);

		commandList->Dispatch(groupX, 1, 1);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(phiCache.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	}

	void Render(
		ComPtr<ID3D12GraphicsCommandList> commandList,
		Camera& cam,
		Wormhole& wormhole,
		DescriptorHeapWrapper& textureHeap,
		std::size_t srcTextureSRVHeapOffset, // 2 SRVs for two skymaps
		std::size_t dstTextureUAVHeapOffset,
		std::size_t emptyPhiCacheUAVHeapOffset
	)
	{
		//if (std::abs(last_l - cam.GetL()) > 1e-8f) // recalculate phi mapping only if l changed
		//{
		//	FillPhiCache(commandList, cam, wormhole, textureHeap, emptyPhiCacheUAVHeapOffset); // fill phi cache
		//	last_l = cam.GetL();
		//}
		FillPhiCache(commandList, cam, wormhole, textureHeap, emptyPhiCacheUAVHeapOffset); // fill phi cache

		commandList->SetPipelineState(pipelineState.Get());
		commandList->SetComputeRootSignature(rootSignature.Get());

		ID3D12DescriptorHeap *descriptorHeaps[] = { textureHeap.heap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		struct
		{
			XMVECTOR position;
			XMVECTOR forward;
			XMVECTOR up;
			XMVECTOR right;
			float fovX;
			float fovY;
			float width;
			float height;
		} cam_data{ cam.GetPositionXM(),cam.GetLookDirXM(),cam.GetUpXM(),cam.GetRightXM(),cam.GetFovX(),cam.GetFovY(),static_cast<float>(cam.GetWidth()),static_cast<float>(cam.GetHeight()) };
		//cam_data.position.m128_f32[3] = 0.00005f;// cam.GetL();

		struct
		{
			std::uint32_t size;
			std::uint32_t pad1, pad2, pad3;
		} cache_size{ g_PhiCacheSize, 0, 0, 0 };

		static_assert(sizeof(cam_data) == 20 * 4);

		commandList->SetComputeRoot32BitConstants(0, 20, &cam_data, 0);
		commandList->SetComputeRoot32BitConstants(1, 4, &wormhole, 0);
		commandList->SetComputeRoot32BitConstants(2, 4, &cache_size, 0);
		commandList->SetComputeRootDescriptorTable(3, textureHeap.at_gpu(srcTextureSRVHeapOffset));
		commandList->SetComputeRootDescriptorTable(4, textureHeap.at_gpu(dstTextureUAVHeapOffset));
		commandList->SetComputeRootDescriptorTable(5, textureHeap.at_gpu(emptyPhiCacheUAVHeapOffset));

		UINT groupX(((cam.GetWidth() - 1) / 32) + 1);
		UINT groupY(((cam.GetHeight() - 1) / 32) + 1);

		commandList->Dispatch(groupX, groupY, 1);
	}
};
