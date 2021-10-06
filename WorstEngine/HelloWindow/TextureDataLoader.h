#pragma once
#include"stdafx.h"
#include "DXSampleHelper.h"

//This class loads a picture from hard driver and register the buffer to the outside descriptor heap
class TextureDataLoader
{
    bool GetTargetPixelFormat(const GUID* pSourceFormat, GUID* pTargetFormat);
    void* GetPixelData(UINT m_uploadBufferSize);
    DXGI_FORMAT GetDXGIFormatFromPixelFormat(const GUID* pPixelFormat);
protected:
    UINT m_TextureWidth;
    UINT m_TextureHeight;
    UINT m_TexturePixelSize;    // The number of bytes used to represent a pixel in the texture.
    UINT m_PicRowPitch;
    UINT m_tmpTableOffsetInHeap;
    UINT m_tmpOffsetInTable;
    ComPtr<IWICImagingFactory>			pIWICFactory;
    ComPtr<IWICBitmapDecoder>			pIWICDecoder;
    ComPtr<IWICBitmapFrameDecode>		pIWICFrame;
    ComPtr<IWICBitmapSource>			pIBMP;
    DXGI_FORMAT stTextureFormat;

    ComPtr<ID3D12Resource> textureUploadHeap;
    ComPtr<ID3D12Resource> m_texture;
public:


    
    TextureDataLoader(UINT SlotNumer, UINT SlotOffSet);

    void ReadTheTextureDataFromFile(LPCWSTR Filename);
   
    void UploadAndConnectResource(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
        ID3D12DescriptorHeap* heap,
        ID3D12CommandQueue* commandQueue
        );
};


