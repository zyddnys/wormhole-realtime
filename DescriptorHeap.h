#pragma once

#include "common.h"

struct DescriptorHeapWrapper
{
	ComPtr<ID3D12DescriptorHeap> heap;
	std::size_t inc;
	std::size_t size;
	D3D12_DESCRIPTOR_HEAP_FLAGS flags;
	D3D12_DESCRIPTOR_HEAP_TYPE type;

	DescriptorHeapWrapper() :heap(nullptr), inc(0), size(0), flags(D3D12_DESCRIPTOR_HEAP_FLAG_NONE), type(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		;
	}

	DescriptorHeapWrapper(
		ComPtr<ID3D12Device2> device,
		std::size_t size,
		D3D12_DESCRIPTOR_HEAP_TYPE type,
		D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	) :size(size), type(type), flags(flags)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = size;
		desc.Type = type;
		desc.Flags = flags;

		ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));

		inc = device->GetDescriptorHandleIncrementSize(type);
	}

	operator ComPtr<ID3D12DescriptorHeap>() { return heap; }

	auto operator[](std::size_t idx) const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(heap->GetCPUDescriptorHandleForHeapStart(), idx, inc);
	}

	auto at_cpu(std::size_t idx) const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(heap->GetCPUDescriptorHandleForHeapStart(), idx, inc);
	}

	auto at_gpu(std::size_t idx) const
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(heap->GetGPUDescriptorHandleForHeapStart(), idx, inc);
	}

	auto front_cpu() const
	{
		return heap->GetCPUDescriptorHandleForHeapStart();
	}

	auto front_gpu() const
	{
		return heap->GetGPUDescriptorHandleForHeapStart();
	}

	DescriptorHeapWrapper(DescriptorHeapWrapper const &a) = delete;
	DescriptorHeapWrapper &operator=(DescriptorHeapWrapper const &a) = delete;

	DescriptorHeapWrapper(DescriptorHeapWrapper &&a) noexcept
	{
		heap = std::move(a.heap);
		inc = a.inc;
		size = a.size;
		type = a.type;
		a.inc = 0;
		a.size = 0;
		a.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	}

	DescriptorHeapWrapper &operator=(DescriptorHeapWrapper &&a) noexcept
	{
		if (this != std::addressof(a))
		{
			heap = std::move(a.heap);
			inc = a.inc;
			size = a.size;
			type = a.type;
			a.inc = 0;
			a.size = 0;
			a.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		}
		return *this;
	}
};
