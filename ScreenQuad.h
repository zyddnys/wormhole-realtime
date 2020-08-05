#pragma once

#include "common.h"
#include "DescriptorHeap.h"

struct ScreenQuad
{
	struct ScreenQuadVertex
	{
		float x, y;
		float u, v;
	};


	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pipelineState;

	static std::array<ScreenQuadVertex, 4> constexpr vertices = {
		ScreenQuadVertex{-1, 1, 0, 0},
		ScreenQuadVertex{1, 1, 1, 0},
		ScreenQuadVertex{-1, -1, 0, 1},
		ScreenQuadVertex{1, -1, 1, 1},
	};

	static std::array<UINT, 6> constexpr indices{ 2, 0, 1, 1, 3, 2 };

	ComPtr<ID3D12Resource> vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	ComPtr<ID3D12Resource> indexBuffer;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	ComPtr<ID3D12Resource> intermediateVertexBuffer;
	ComPtr<ID3D12Resource> intermediateIndexBuffer;

	ScreenQuad() :
		vertexBuffer(nullptr),
		vertexBufferView(),
		indexBuffer(nullptr),
		indexBufferView(),
		intermediateVertexBuffer(nullptr),
		intermediateIndexBuffer(nullptr)
	{
		;
	}

	void UpdateBufferResource(
		ComPtr<ID3D12Device2> device,
		ComPtr<ID3D12GraphicsCommandList> commandList,
		ID3D12Resource **pDestinationResource,
		ID3D12Resource **pIntermediateResource,
		size_t numElements, size_t elementSize, const void *bufferData,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
	{

		size_t bufferSize = numElements * elementSize;
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(pDestinationResource)));
		if (bufferData)
		{
			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(pIntermediateResource)));
			D3D12_SUBRESOURCE_DATA subresourceData = {};
			subresourceData.pData = bufferData;
			subresourceData.RowPitch = bufferSize;
			subresourceData.SlicePitch = subresourceData.RowPitch;

			UpdateSubresources(commandList.Get(),
				*pDestinationResource, *pIntermediateResource,
				0, 0, 1, &subresourceData);
		}
	}

	ScreenQuad(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		UpdateBufferResource(device, commandList,
			&vertexBuffer, &intermediateVertexBuffer,
			vertices.size(), sizeof(ScreenQuadVertex), vertices.data());
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
		// Create the vertex buffer view.
		vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = sizeof(ScreenQuadVertex) * vertices.size();
		vertexBufferView.StrideInBytes = sizeof(ScreenQuadVertex);

		UpdateBufferResource(device, commandList,
			&indexBuffer, &intermediateIndexBuffer,
			indices.size(), sizeof(UINT), indices.data());
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
		// Create the vertex buffer view.
		indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		indexBufferView.SizeInBytes = sizeof(UINT) * indices.size();

		// Load the vertex shader.
		ComPtr<ID3DBlob> vertexShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"screen_quad_vs.cso", &vertexShaderBlob));

		ComPtr<ID3DBlob> pixelShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"screen_quad_ps.cso", &pixelShaderBlob));

		// Create the vertex input layout
		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

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
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		// A single 32-bit constant root parameter that is used by the vertex shader.
		CD3DX12_ROOT_PARAMETER1 rootParameters[1] = {};

		// create a descriptor range (descriptor table) and fill it out
		// this is a range of descriptors inside a descriptor heap
		//D3D12_DESCRIPTOR_RANGE1  descriptorTableRanges{}; // only one range right now
		//descriptorTableRanges.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // this is a range of shader resource views (descriptors)
		//descriptorTableRanges.NumDescriptors = 1; // we only have one texture right now, so the range is only 1
		//descriptorTableRanges.BaseShaderRegister = 0; // start index of the shader registers in the range
		//descriptorTableRanges.RegisterSpace = 0; // space 0. can usually be zero
		//descriptorTableRanges.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables

		//D3D12_ROOT_DESCRIPTOR_TABLE1 descriptorTable{};
		//descriptorTable.NumDescriptorRanges = 1; // we only have one range
		//descriptorTable.pDescriptorRanges = &descriptorTableRanges; // the pointer to the beginning of our ranges array

		//rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // this is a descriptor table
		//rootParameters[0].DescriptorTable = descriptorTable; // this is our descriptor table for this root parameter
		//rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // our pixel shader will be the only shader accessing this parameter for now

		auto r1 = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		rootParameters[0].InitAsDescriptorTable(1, std::addressof(r1), D3D12_SHADER_VISIBILITY_PIXEL);

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
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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
				MessageBoxA(NULL, errorString.c_str(), "ScreenQuad", MB_OK | MB_ICONERROR);
			}
			__debugbreak();
		}
		// Create the root signature.
		ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
			rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

		struct PipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		} pipelineStateStream;

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		pipelineStateStream.pRootSignature = rootSignature.Get();
		pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout) };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.DSVFormat = DXGI_FORMAT_UNKNOWN;
		pipelineStateStream.RTVFormats = rtvFormats;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream), &pipelineStateStream
		};
		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));

		//// fill out an input layout description structure
		//D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

		//// we can get the number of elements in an array by "sizeof(array) / sizeof(arrayElementType)"
		//inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
		//inputLayoutDesc.pInputElementDescs = inputLayout;

		//DXGI_SAMPLE_DESC sampleDesc = {};
		//sampleDesc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)

		//D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
		//psoDesc.InputLayout = inputLayoutDesc; // the structure describing our input layout
		//psoDesc.pRootSignature = rootSignature.Get(); // the root signature that describes the input data this pso needs
		//psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get()); // structure describing where to find the vertex shader bytecode and how large it is
		//psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get()); // same as VS but for pixel shader
		//psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // type of topology we are drawing
		//psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // format of the render target
		//psoDesc.SampleDesc = sampleDesc; // must be the same sample description as the swapchain and depth/stencil buffer
		//psoDesc.SampleMask = 0xffffffff; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
		//psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
		////psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		//psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
		//psoDesc.NumRenderTargets = 1; // we are only binding one render target
		//psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
		//psoDesc.DepthStencilState.DepthEnable = FALSE;
		////psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG;

		//ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));
	}

	void Render(ComPtr<ID3D12GraphicsCommandList> commandList, DescriptorHeapWrapper& textureSRVHeap, std::size_t textuteHeapOffset)
	{
		commandList->SetPipelineState(pipelineState.Get());
		commandList->SetGraphicsRootSignature(rootSignature.Get());

		ID3D12DescriptorHeap *descriptorHeaps[] = { textureSRVHeap.heap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		commandList->SetGraphicsRootDescriptorTable(0, textureSRVHeap.at_gpu(textuteHeapOffset));

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&indexBufferView);

		commandList->DrawIndexedInstanced(indices.size(), 1, 0, 0, 0);
	}
};
