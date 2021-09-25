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
#include "TextureDataLoader.h"

class TextureDataLoader;









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
    static const UINT nSampleMaxCnt = 5;
   
    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT2 uv;
        XMFLOAT3 n;

    };

    struct MVP_BUFFER
    {
      
        XMMATRIX World;
        XMMATRIX View;
        XMMATRIX Pro;			//经典的Model-view-projection(MVP)矩阵.
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
    TextureDataLoader* Loader;

    //signature
    ComPtr<ID3D12RootSignature> m_rootSignature;


    //descriptor size
    UINT m_rtvDescriptorSize;
    UINT m_samplerDescriptorSize;

    XMVECTOR Eye;
    XMVECTOR At;
    XMVECTOR Up;

    double fPalstance;	//物体旋转的角速度，单位：弧度/秒






    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FrameCount];

    //App resources and relatives
    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_IndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
    ComPtr<ID3D12Resource> m_constantBufferUpload;

    MVP_BUFFER* MVPBufferData;
    SIZE_T szMVPBuffer;
   
    UINT m_nInstance;
    ULONGLONG n64tmFrameStart;
    ULONGLONG n64tmCurrent;
    double dModelRotationYAngle;
    double dModelRotationXAngle;
    double dModelRotationZAngle;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForGPU();
    void MoveToNextFrame();

};
