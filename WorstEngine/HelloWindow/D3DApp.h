//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "DXSample.h"
#include <wincodec.h>








using namespace DirectX;
// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class D3DApp : public DXSample
{
public:
    D3DApp(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();

private:
    static const UINT FrameCount = 4;
    UINT m_TextureWidth;
    UINT m_TextureHeight;
    UINT m_TexturePixelSize;    // The number of bytes used to represent a pixel in the texture.
    UINT m_PicRowPitch;
    static const UINT nSampleMaxCnt = 5;
   
    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT2 uv;

    };
    struct SceneConstantBuffer

    {

        XMFLOAT4 offset;

        float padding[60]; // 256�ֽڶ���

    };


    // Rects
    CD3DX12_VIEWPORT m_viewportRect;
    CD3DX12_RECT m_scissorRect;

    //Swap chain
    ComPtr<IDXGISwapChain3> m_swapChain;

    //devie
    ComPtr<ID3D12Device> m_device;

    //render targets
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];

    //Allocators 
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    ComPtr<ID3D12CommandAllocator>  m_bundleAllocator;

    //Command queue
    ComPtr<ID3D12CommandQueue> m_commandQueue;
   
    
    //command list
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12GraphicsCommandList> m_bundle;
    //Heaps on GPU
    //rendering target view heap
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    //cbv heap can also be used as cbv_srv_uav_....
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    ComPtr<ID3D12DescriptorHeap> m_SamplersHeap;
    //pipeline state object
    ComPtr<ID3D12PipelineState> m_pipelineState;
    

    //signature
    ComPtr<ID3D12RootSignature> m_rootSignature;


    //descriptor size
    UINT m_rtvDescriptorSize;
    UINT m_samplerDescriptorSize;







    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FrameCount];

    //App resources and relatives
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_constantBuffer;
    ComPtr<ID3D12Resource> m_texture;
    SceneConstantBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForGPU();
    void MoveToNextFrame();
    std::vector<UINT8> GenerateTextureData();
};
