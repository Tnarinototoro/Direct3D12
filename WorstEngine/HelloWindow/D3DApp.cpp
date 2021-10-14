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

#include "stdafx.h"
#include "D3DApp.h"
#include "TextureDataLoader.h"


D3DApp::D3DApp(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
    m_rtvDescriptorSize(0),
    m_viewportRect(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0,0,static_cast<LONG>(width),static_cast<LONG>(height)),
    m_fenceValues{},
    Eye(XMVectorSet(0.0f, 0.0f, -100.0f, 0.0f)),
    Up(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),
    At(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f)),
    fPalstance(10.0f * XM_PI / 180.0f),
    szMVPBuffer(GRS_UPPER_DIV(sizeof(MVP_BUFFER), 256) * 256),
    n64tmFrameStart(::GetTickCount64()),
    dModelRotationYAngle(0.0f),
    dModelRotationXAngle(0.0f),
    dModelRotationZAngle(0.0f),
    MVPBufferData(nullptr)
{
    n64tmCurrent = n64tmFrameStart;
}

void D3DApp::OnInit()
{
    LoadPipeline();
    LoadAssets();
}

// Load the rendering pipeline dependencies.
void D3DApp::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


        //cbv_heap
        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = 2;

        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
        
        //Sampler heap creation
        D3D12_DESCRIPTOR_HEAP_DESC samplerDesc = {};
        samplerDesc.NumDescriptors = nSampleMaxCnt;
        samplerDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        samplerDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

        ThrowIfFailed(m_device->CreateDescriptorHeap(&samplerDesc,
            IID_PPV_ARGS(&m_SamplersHeap)));
        m_samplerDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    
    
    
    
    
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);

            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
        }

       
    }

    //create a bundle allocator
    ThrowIfFailed(
        m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_BUNDLE,
        IID_PPV_ARGS(&m_bundleAllocator)));
   
}

// Load the sample assets.
void D3DApp::LoadAssets()
{
    // 创建一个rootsignature    
    {
        //检查root singnature支持哪一个版本
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};


        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE,
            &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }
       

        //创建descriptor table

        CD3DX12_DESCRIPTOR_RANGE1 ranges[3];

        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV,1, 0, 0,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);



        CD3DX12_ROOT_PARAMETER1 rootParameters[2];

        rootParameters[0].InitAsDescriptorTable(2,
            &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
       

        rootParameters[1].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);






        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =

            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;



        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;

        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0,nullptr, rootSignatureFlags);

        //Blob其实是d3d12里的一团数据的意思用于序列化，这样省空间

        ComPtr<ID3DBlob> signature;

        ComPtr<ID3DBlob> error;

        //先创建序列化的signature然后创建解压版的

        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));

        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

    }

    //创建一个包括pixelshader和vertex shader的pipeline object

    {

        ComPtr<ID3DBlob> vertexShader;

        ComPtr<ID3DBlob> pixelShader;

        //debug 的分支编译选项

        #if defined(_DEBUG)

        //debug 的话就没有优化了

                // Enable better shader debugging with the graphics debugging tools.

                UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

        #else

                UINT compileFlags = 0;

        #endif

                //从文件编译shader文件

              

                      ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));

                      ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));



      


                      D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =

                      {

                          { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
                          0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

                          { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,0,
                          12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

                          { "NORMAL", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 
                          20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }

                      };



                  // Describe and create the graphics pipeline state object (PSO).

                 //创建pipeline 管线

                  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // 渲染管线状态描述符

                  psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) }; //输入elment 的描述

                  psoDesc.pRootSignature = m_rootSignature.Get(); //指定root signature一个signature就指定了资源和layout

                  psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get()); // 设定vertex shader

                  psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get()); // 设定pixel shader

                  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); //光栅化状态

                  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // blend 状态主要是α混合时候的计算公式在这里指定

                  psoDesc.DepthStencilState.DepthEnable = FALSE; //直接取消depth 

                  psoDesc.DepthStencilState.StencilEnable = FALSE; // 直接取消stencil

                  psoDesc.SampleMask = UINT_MAX; // 采样mask

                  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // primitive 类型

                  psoDesc.NumRenderTargets = 1; //渲染喂过去的target有几个

                  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; //render target view 格式

                  psoDesc.SampleDesc.Count = 1; //采样DESC的个数

                  ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

    }

        // Create the vertex buffer. and index buffer

    {

        // Define the geometry for a triangle.
        float fBoxSize = 20.0f;
        float fTCMax = 3.0f;
        Vertex stTriangleVertices[] = {
            { {-1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f * fTCMax}, {0.0f,  0.0f, -1.0f} },
            { {1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  0.0f, -1.0f} },
            { {-1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax}, {0.0f,  0.0f, -1.0f} },
            { {-1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax}, {0.0f,  0.0f, -1.0f} },
            { {1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f, 0.0f, -1.0f} },
            { {1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  0.0f, -1.0f} },
            { {1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
            { {1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
            { {1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
            { {1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
            { {1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
            { {1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
            { {1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  0.0f,  1.0f} },
            { {-1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  0.0f,  1.0f} },
            { {1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax}, {0.0f,  0.0f,  1.0f} },
            { {1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  0.0f,  1.0f} },
            { {-1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  0.0f,  1.0f} },
            { {-1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  0.0f,  1.0f} },
            { {-1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
            { {-1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
            { {-1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
            { {-1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
            { {-1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
            { {-1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
            { {-1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  1.0f,  0.0f} },
            { {1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  1.0f,  0.0f} },
            { {-1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  1.0f,  0.0f} },
            { {-1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  1.0f,  0.0f} },
            { {1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  1.0f,  0.0f} },
            { {1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  1.0f,  0.0f }},
            { {-1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
            { {1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
            { {-1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
            { {-1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
            { {1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
            { {1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
        };

      
        const UINT vertexBufferSize = sizeof(stTriangleVertices);

        UINT32 pBoxIndices[] 
            = {
            0,1,2,
            3,4,5,

            6,7,8,
            9,10,11,

            12,13,14,
            15,16,17,

            18,19,20,
            21,22,23,

            24,25,26,
            27,28,29,

            30,31,32,
            33,34,35,
        };

       
       
        const UINT nszIndexBuffer = sizeof(pBoxIndices);
        m_nInstance = _countof(pBoxIndices);
        //这里的创建committed resource就是在创建顶点缓存, D3D12_HEAP_TYPE_UPLOAD其实不太适合这种静态数据, 这里只是为了代码简洁

        ThrowIfFailed(m_device->CreateCommittedResource(

            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),

            D3D12_HEAP_FLAG_NONE,

            & CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),

            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,

            IID_PPV_ARGS(&m_vertexBuffer)));



        // 把geometry的数据复制到vertex buffer，跟d3d9差不多

        UINT8* pVertexDataBegin;

        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.

        //其实就是d3d9里面的锁定

        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));

        //其实就是把pVertexDataBegin的地址换成了vertexbuffer里面的raw指针的内存首地址.

        //然后直接用pVertexDataBegin的地址来进行拷贝

        memcpy(pVertexDataBegin, stTriangleVertices, sizeof(stTriangleVertices));

        //搞完之后unlock

        m_vertexBuffer->Unmap(0, nullptr);



        // Initialize the vertex buffer view.

       //果然view就是关于vertex的地址，stride等的信息

        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();

        m_vertexBufferView.StrideInBytes = sizeof(Vertex);

        m_vertexBufferView.SizeInBytes = vertexBufferSize;

        //create index buffer

        ThrowIfFailed(m_device->CreateCommittedResource(

            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),

            D3D12_HEAP_FLAG_NONE,

            & CD3DX12_RESOURCE_DESC::Buffer(nszIndexBuffer),

            D3D12_RESOURCE_STATE_GENERIC_READ,

            nullptr,

            IID_PPV_ARGS(&m_IndexBuffer)));

        UINT8* pIndexDataBegin = nullptr;
        ThrowIfFailed(m_IndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
        memcpy(pIndexDataBegin, pBoxIndices, nszIndexBuffer);
        m_IndexBuffer->Unmap(0, nullptr);

        m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
        m_IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
        m_IndexBufferView.SizeInBytes = nszIndexBuffer;
    }


    // Create the main command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

    //Create command bundle list
    {
        ThrowIfFailed(
            m_device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_BUNDLE,
                m_bundleAllocator.Get(),
                m_pipelineState.Get(),
                IID_PPV_ARGS(&m_bundle)));


       
        m_bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_bundle->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        m_bundle->IASetIndexBuffer(&m_IndexBufferView);
        m_bundle->DrawInstanced(m_nInstance, 1, 0, 0);

        ThrowIfFailed(m_bundle->Close());
    }



    //create the texture
    {
        Loader = new TextureDataLoader(0, 1);
        
        Loader->ReadTheTextureDataFromFile(m_Commands[1]);
        Loader->UploadAndConnectResource(m_device.Get(), m_commandList.Get(), m_cbvHeap.Get(), m_commandQueue.Get());
    }
    



    //create a lot of samplers
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE hSamplerHeap(m_SamplersHeap->GetCPUDescriptorHandleForHeapStart());

        D3D12_SAMPLER_DESC stSamplerDesc = {};
        stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

        stSamplerDesc.MinLOD = 0;
        stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        stSamplerDesc.MipLODBias = 0.0f;
        stSamplerDesc.MaxAnisotropy = 1;
        stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        // Sampler 1
        stSamplerDesc.BorderColor[0] = 1.0f;
        stSamplerDesc.BorderColor[1] = 0.0f;
        stSamplerDesc.BorderColor[2] = 1.0f;
        stSamplerDesc.BorderColor[3] = 1.0f;
        stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        m_device->CreateSampler(&stSamplerDesc, hSamplerHeap);

        hSamplerHeap.Offset(m_samplerDescriptorSize);

        // Sampler 2
        stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        m_device->CreateSampler(&stSamplerDesc, hSamplerHeap);

        hSamplerHeap.Offset(m_samplerDescriptorSize);

        // Sampler 3
        stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        m_device->CreateSampler(&stSamplerDesc, hSamplerHeap);

        hSamplerHeap.Offset(m_samplerDescriptorSize);

        // Sampler 4
        stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        m_device->CreateSampler(&stSamplerDesc, hSamplerHeap);

        hSamplerHeap.Offset(m_samplerDescriptorSize);

        // Sampler 5
        stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        m_device->CreateSampler(&stSamplerDesc, hSamplerHeap);

    }


    // create const buffer
    {
       
           // CB size is required to be 256-byte aligned.
        ThrowIfFailed(m_device->CreateCommittedResource(

            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),

            D3D12_HEAP_FLAG_NONE,

            &CD3DX12_RESOURCE_DESC::Buffer(szMVPBuffer),

            D3D12_RESOURCE_STATE_GENERIC_READ,

            nullptr,

            IID_PPV_ARGS(&m_constantBufferUpload)));
        ThrowIfFailed(m_constantBufferUpload->Map(0, nullptr, reinterpret_cast<void**>(&MVPBufferData)));


        



        

    //把resource的buffer的指针给m_pCbvDataBegin赋值，然后把m_constantBufferData的值赋给资源

      

        // Describe and create a constant buffer view.

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};

        cbvDesc.BufferLocation = m_constantBufferUpload->GetGPUVirtualAddress();

        cbvDesc.SizeInBytes = static_cast<UINT>(szMVPBuffer);

        m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // Create synchronization objects.
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValues[m_frameIndex]++;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }
    WaitForGPU();
}

// Update frame-based values.
void D3DApp::OnUpdate()
{
    n64tmCurrent = ::GetTickCount();
    //计算旋转的角度：旋转角度(弧度) = 时间(秒) * 角速度(弧度/秒)
    //下面这句代码相当于经典游戏消息循环中的OnUpdate函数中需要做的事情
    dModelRotationYAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * fPalstance*10;
    dModelRotationXAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * fPalstance * 10;
    dModelRotationZAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * fPalstance * 10;
    n64tmFrameStart = n64tmCurrent;
    //MessageBox(0, std::to_wstring(dModelRotationYAngle).c_str(), L"", 0);

    //旋转角度是2PI周期的倍数，去掉周期数，只留下相对0弧度开始的小于2PI的弧度即可
    if (dModelRotationYAngle > XM_2PI)
    {
        dModelRotationYAngle = fmod(dModelRotationYAngle, XM_2PI);
    }
    if (dModelRotationXAngle > XM_2PI)
    {
        dModelRotationXAngle = fmod(dModelRotationXAngle, XM_2PI);
    }
    if (dModelRotationZAngle > XM_2PI)
    {
        dModelRotationZAngle = fmod(dModelRotationZAngle, XM_2PI);
    }

    //模型矩阵 model
    XMMATRIX xmRot = XMMatrixMultiply(XMMatrixRotationY(static_cast<float>(dModelRotationYAngle)), XMMatrixRotationX(static_cast<float>(dModelRotationXAngle)));
    xmRot = XMMatrixMultiply(xmRot, XMMatrixRotationZ(static_cast<float>(dModelRotationZAngle)));
    //计算 模型矩阵 model * 视矩阵 view
    XMMATRIX view = XMMatrixLookAtLH(Eye, At, Up);

    //投影矩阵 projection
    XMMATRIX pro = XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)m_width / (FLOAT)m_height, 0.1f, 1000.0f);
   /* MVPBufferData->World = xmRot;
    MVPBufferData->View = view;
    MVPBufferData->Pro = pro;*/
    XMStoreFloat4x4(&MVPBufferData->World, xmRot);
    XMStoreFloat4x4(&MVPBufferData->View, view);
    XMStoreFloat4x4(&MVPBufferData->Pro, pro);
 
   
}

// Render the scene.
void D3DApp::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    MoveToNextFrame();
}

void D3DApp::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForGPU();

    CloseHandle(m_fenceEvent);
}

void D3DApp::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewportRect);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
    //把cbv_heap设置为当前使用的descriptor heap
    ID3D12DescriptorHeap* ppHeaps[] = { m_SamplersHeap.Get(), m_cbvHeap.Get()};

    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    //set cbv table
    m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
   

    //set ampler no 2;
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpusamplerHandle(m_SamplersHeap->GetGPUDescriptorHandleForHeapStart(),
        2, m_samplerDescriptorSize);

    m_commandList->SetGraphicsRootDescriptorTable(1,hGpusamplerHandle);
    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    
    m_commandList->ExecuteBundle(m_bundle.Get());

    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
}

void D3DApp::WaitForGPU()
{

    // Schedule a Signal command in the queue.

    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));



    // Wait until the fence has been processed.

    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));

    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);



    // Increment the fence value for the current frame.

    m_fenceValues[m_frameIndex]++;
}

void D3DApp::MoveToNextFrame()
{
    // Schedule a Signal command in the queue.

    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];

    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

    // Update the frame index.

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.

    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])

    {

        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));

        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

    }

    // Set the fence value for the next frame.

    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

