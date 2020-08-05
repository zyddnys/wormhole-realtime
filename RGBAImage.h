#pragma once

#include "common.h"

#include <atlbase.h>
#include <wincodec.h>

class RGBAImage // FP32 from 0.0f to 1.0f, RGBARGBARGBA...
{
	//TODO: maybe add alignment?
public:
	float *data;
	std::uint32_t width, height;
public:
	std::tuple<std::uint32_t, std::uint32_t> GetSize() { return { width,height }; }
	float *GetRawData() { return data; }
public:
	void Setup(std::uint32_t width, std::uint32_t height)
	{
		float *new_data(new float[height * width * 4]{ 255.0f });
		Release();
		this->width = width;
		this->height = height;
		this->data = new_data;
	}
	std::tuple<float, float, float> At(std::uint32_t i, std::uint32_t j)
	{
		float *pos(data + (i * width + j) * 4);
		return { pos[0], pos[1], pos[2] };
	}
	void Set(std::uint32_t i, std::uint32_t j, float r2, float g2, float b2)
	{
		float *pos(data + (i * width + j) * 4);
		pos[0] = r2;
		pos[1] = g2;
		pos[2] = b2;
	}
public:
	RGBAImage() :data(nullptr), width(0), height(0)
	{

	}
	RGBAImage(LPCWSTR filename) :data(nullptr), width(0), height(0)
	{
		//CoInitialize(nullptr);
		{
			CComPtr<IWICImagingFactory> pFactory;
			CComPtr<IWICBitmapDecoder> pDecoder;
			CComPtr<IWICBitmapFrameDecode> pFrame;
			CComPtr<IWICFormatConverter> pFormatConverter;

			CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID *)&pFactory);

			pFactory->CreateDecoderFromFilename(filename, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder);
			// Check the return value to see if:
			//  - The specified file exists and can be read.
			//  - A decoder is found for this file format.

			UINT frameCount = 0;
			pDecoder->GetFrameCount(&frameCount);

			pDecoder->GetFrame(0, &pFrame);
			// The zero-based index should be smaller than the frame count.

			pFrame->GetSize(&width, &height);

			WICPixelFormatGUID pixelFormatGUID;
			pFrame->GetPixelFormat(&pixelFormatGUID);

			// The frame can use many different pixel formats.
			// You can copy the raw pixel values by calling "pFrame->CopyPixels( )".
			// This method needs a buffer that can hold all bytes.
			// The total number of bytes is: width x height x bytes per pixel

			// The disadvantage of this solution is that you have to deal with all possible pixel formats.

			// You can make your life easy by converting the frame to a pixel format of
			// your choice. The code below shows how to convert the pixel format to 24-bit RGB.

			pFactory->CreateFormatConverter(&pFormatConverter);

			pFormatConverter->Initialize(pFrame,                       // Input bitmap to convert
				GUID_WICPixelFormat24bppRGB,  // Destination pixel format
				WICBitmapDitherTypeNone,      // Specified dither pattern
				nullptr,                      // Specify a particular palette
				0.f,                          // Alpha threshold
				WICBitmapPaletteTypeCustom); // Palette translation type

			UINT bytesPerPixel = 3; // Because we have converted the frame to 24-bit RGB
			UINT stride = width * bytesPerPixel;
			UINT size = width * height * bytesPerPixel; // The size of the required memory buffer for
														// holding all the bytes of the frame.

			std::vector<BYTE> bitmap(size); // The buffer to hold all the bytes of the frame.
			pFormatConverter->CopyPixels(NULL, stride, size, bitmap.data());

			// Note: the WIC COM pointers should be released before 'CoUninitialize( )' is called.
			data = new float[width * height * 4];

			float *dst(data);
			BYTE const *src = bitmap.data();
			for (std::size_t i(0); i < width * height; ++i, src += 3, dst += 4)
			{
				dst[0] = static_cast<float>(src[0]) / 255.0f;
				dst[1] = static_cast<float>(src[1]) / 255.0f;
				dst[2] = static_cast<float>(src[2]) / 255.0f;
				dst[3] = 1.0f;
			}
		}
		//CoUninitialize();
	}
	void Release() noexcept
	{
		try {
			if (data) {
				delete[] data;
				data = nullptr;
			}
			width = height = 0;
		}
		catch (...) {}
	}
	~RGBAImage()
	{
		Release();
	}

	RGBAImage(RGBAImage const &other) :data(nullptr), width(other.width), height(other.height)
	{
		data = new float[width * height * 4];
		std::uninitialized_copy_n(other.data, width * height * 4, data);
	}

	RGBAImage &operator=(RGBAImage const &other)
	{
		if (std::addressof(other) != this)
		{
			float *new_data(new float[other.height * other.width * 4]);
			std::uninitialized_copy_n(other.data, other.width * other.height * 4, new_data);
			Release();
			data = new_data;
			width = other.height;
			height = other.height;
		}
		return *this;
	}

	RGBAImage(RGBAImage &&other) noexcept :data(other.data), width(other.width), height(other.height)
	{
		other.data = nullptr;
		other.width = 0;
		other.height = 0;
	}

	RGBAImage &operator=(RGBAImage &&other) noexcept
	{
		if (std::addressof(other) != this)
		{
			Release();
			data = other.data;
			width = other.width;
			height = other.height;

			other.data = nullptr;
			other.width = 0;
			other.height = 0;
		}
		return *this;
	}
	operator bool() const noexcept
	{
		return width != 0 && height != 0 && data != nullptr;
	}
};

struct RGBAImageGPU
{
	std::uint32_t width, height;
	D3D12_RESOURCE_STATES state;
	ComPtr<ID3D12Resource> texture;
	D3D12_RESOURCE_DESC textureDesc;

	ComPtr<ID3D12Resource> tmp;

	RGBAImageGPU(RGBAImageGPU const &a) = delete;
	RGBAImageGPU &operator=(RGBAImageGPU const &a) = delete;

	RGBAImageGPU &operator=(RGBAImageGPU &&a) noexcept
	{
		if (this != std::addressof(a))
		{
			width = a.width;
			height = a.height;
			state = a.state;
			texture = a.texture;
			textureDesc = a.textureDesc;
			tmp = a.tmp;
			a.width = 0;
			a.height = 0;
			a.state = D3D12_RESOURCE_STATE_COMMON;
			a.texture = nullptr;
			a.textureDesc = {};
			a.tmp = nullptr;
		}
		return *this;
	}

	RGBAImageGPU() :width(0), height(0), state(D3D12_RESOURCE_STATE_COMMON), texture(nullptr), textureDesc(), tmp(nullptr)
	{
		;
	}

	RGBAImageGPU(
		ComPtr<ID3D12Device> device,
		std::uint32_t width,
		std::uint32_t height,
		D3D12_CPU_DESCRIPTOR_HANDLE srvDescriptorDest
	) :width(width), height(height), state(D3D12_RESOURCE_STATE_COMMON), textureDesc()
	{
		// allocate GPU space for empty texture

		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Alignment = 0; // may be 0, 4KB, 64KB, or 4MB. 0 will let runtime decide between 64KB and 4MB (4MB for multi-sampled textures)
		textureDesc.Width = width; // width of the texture
		textureDesc.Height = height; // height of the texture
		textureDesc.DepthOrArraySize = 1; // if 3d image, depth of 3d image. Otherwise an array of 1D or 2D textures (we only have one image, so we set 1)
		textureDesc.MipLevels = 1; // Number of mipmaps. We are not generating mipmaps for this texture, so we have only one level
		textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // This is the dxgi format of the image (format of the pixels)
		textureDesc.SampleDesc.Count = 1; // This is the number of samples per pixel, we just want 1 sample
		textureDesc.SampleDesc.Quality = 0; // The quality level of the samples. Higher is better quality, but worse performance
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // The arrangement of the pixels. Setting to unknown lets the driver choose the most efficient one
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE; // no flags

		THROW(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			state,
			nullptr,
			IID_PPV_ARGS(&texture)
		));

		// now we create a shader resource view (descriptor that points to the texture and describes it)
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(texture.Get(), &srvDesc, srvDescriptorDest);
	}

	RGBAImageGPU(
		ComPtr<ID3D12Device> device,
		std::uint32_t width,
		std::uint32_t height,
		D3D12_CPU_DESCRIPTOR_HANDLE srvDescriptorDest,
		D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorDest
	) :width(width), height(height), state(D3D12_RESOURCE_STATE_COMMON), textureDesc()
	{
		// allocate GPU space for empty texture

		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Alignment = 0; // may be 0, 4KB, 64KB, or 4MB. 0 will let runtime decide between 64KB and 4MB (4MB for multi-sampled textures)
		textureDesc.Width = width; // width of the texture
		textureDesc.Height = height; // height of the texture
		textureDesc.DepthOrArraySize = 1; // if 3d image, depth of 3d image. Otherwise an array of 1D or 2D textures (we only have one image, so we set 1)
		textureDesc.MipLevels = 1; // Number of mipmaps. We are not generating mipmaps for this texture, so we have only one level
		textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // This is the dxgi format of the image (format of the pixels)
		textureDesc.SampleDesc.Count = 1; // This is the number of samples per pixel, we just want 1 sample
		textureDesc.SampleDesc.Quality = 0; // The quality level of the samples. Higher is better quality, but worse performance
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // The arrangement of the pixels. Setting to unknown lets the driver choose the most efficient one
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS

		THROW(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			state,
			nullptr,
			IID_PPV_ARGS(&texture)
		));

		// now we create a shader resource view (descriptor that points to the texture and describes it)
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(texture.Get(), &srvDesc, srvDescriptorDest);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = textureDesc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		device->CreateUnorderedAccessView(texture.Get(), nullptr, &uavDesc, uavDescriptorDest);
	}

	void AsUAV(ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	void AsGraphicsSRV(ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	void AsComputeSRV(ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), state, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
		state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	}

	void Upload(
		ComPtr<ID3D12Device> device,
		ComPtr<ID3D12GraphicsCommandList> commandList,
		RGBAImage const &img
	) // upload to GPU
	{
		if (img.height != height || img.width != width)
			throw std::runtime_error("image shape mismatch");


		UINT64 textureUploadBufferSize;
		// this function gets the size an upload buffer needs to be to upload a texture to the gpu.
		// each row must be 256 byte aligned except for the last row, which can just be the size in bytes of the row
		// eg. textureUploadBufferSize = ((((width * numBytesPerPixel) + 255) & ~255) * (height - 1)) + (width * numBytesPerPixel);
		//textureUploadBufferSize = (((imageBytesPerRow + 255) & ~255) * (textureDesc.Height - 1)) + imageBytesPerRow;
		device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

		// now we create an upload heap to upload our texture to the GPU
		THROW(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
			D3D12_RESOURCE_STATE_GENERIC_READ, // We will copy the contents from this heap to the default heap above
			nullptr,
			IID_PPV_ARGS(&tmp)));

		// store vertex buffer in upload heap
		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = img.data; // pointer to our image data
		textureData.RowPitch = width * 16; // size of all our triangle vertex data
		textureData.SlicePitch = width * 16 * height; // also the size of our triangle vertex data

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), state, D3D12_RESOURCE_STATE_COPY_DEST));
		state = D3D12_RESOURCE_STATE_COPY_DEST;

		// Now we copy the upload buffer contents to the default heap
		UpdateSubresources(commandList.Get(), texture.Get(), tmp.Get(), 0, 0, 1, &textureData); // this will handle the alignment and pitch row stuffs

		// transition the texture default heap to a pixel shader resource (we will be sampling from this heap in the pixel shader to get the color of pixels)
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	void Download() // Download image to CPU
	{
		;
	}
};
