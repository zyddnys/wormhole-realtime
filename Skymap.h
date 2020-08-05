#pragma once

#include "common.h"
#include "DescriptorHeap.h"
#include "Camera.h"

struct Skymap
{
	ComPtr<ID3D12PipelineState> pipelineState;
	ComPtr<ID3D12RootSignature> rootSignature;

	Skymap() :pipelineState(nullptr), rootSignature(nullptr)
	{
		;
	}

	Skymap(Skymap const &a) = delete;
	Skymap &operator=(Skymap const &a) = delete;

	Skymap(Skymap &&a) noexcept
	{
		pipelineState = a.pipelineState;
		rootSignature = a.rootSignature;
	}

	Skymap &operator=(Skymap &&a) noexcept
	{
		if (this != std::addressof(a))
		{
			pipelineState = std::move(a.pipelineState);
			rootSignature = std::move(a.rootSignature);
		}
		return *this;
	}

	Skymap(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		ComPtr<ID3DBlob> computeShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"skymap_panoramic_render.cso", &computeShaderBlob));

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

		rootParameters[0].InitAsConstants(20, 0);
		rootParameters[1].InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0));
		rootParameters[2].InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0));

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
				MessageBoxA(NULL, errorString.c_str(), "Skymap", MB_OK | MB_ICONERROR);
			}
			__debugbreak();
		}
		// Create the root signature.
		ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
			rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

		//struct PipelineStateStream
		//{
		//	CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		//	CD3DX12_PIPELINE_STATE_STREAM_CS CS;
		//} pipelineStateStream;

		//pipelineStateStream.pRootSignature = rootSignature.Get();
		//pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());

		//D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		//	sizeof(PipelineStateStream), &pipelineStateStream
		//};
		//ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
		psoDesc.pRootSignature = rootSignature.Get(); // the root signature that describes the input data this pso needs
		psoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());
		//psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG;

		ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));
	}

	void Render(
		ComPtr<ID3D12GraphicsCommandList> commandList,
		Camera &cam,
		DescriptorHeapWrapper &textureHeap,
		std::size_t srcTextureSRVHeapOffset,
		std::size_t dstTextureUAVHeapOffset
	)
	{
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

		static_assert(sizeof(cam_data) == 20 * 4);

		commandList->SetComputeRoot32BitConstants(0, 20, &cam_data, 0);
		commandList->SetComputeRootDescriptorTable(1, textureHeap.at_gpu(srcTextureSRVHeapOffset));
		commandList->SetComputeRootDescriptorTable(2, textureHeap.at_gpu(dstTextureUAVHeapOffset));

		UINT groupX(((cam.GetWidth() - 1) / 32) + 1);
		UINT groupY(((cam.GetHeight() - 1) / 32) + 1);

		commandList->Dispatch(groupX, groupY, 1);
	}
};
