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

bool GetTargetPixelFormat(const GUID* pSourceFormat, GUID* pTargetFormat)
{//查表确定兼容的最接近格式是哪个
    *pTargetFormat = *pSourceFormat;
    for (size_t i = 0; i < _countof(g_WICConvert); ++i)
    {
        if (InlineIsEqualGUID(g_WICConvert[i].source, *pSourceFormat))
        {
            *pTargetFormat = g_WICConvert[i].target;
            return true;
        }
    }
    return false;
}

DXGI_FORMAT GetDXGIFormatFromPixelFormat(const GUID* pPixelFormat)
{//查表确定最终对应的DXGI格式是哪一个
    for (size_t i = 0; i < _countof(g_WICFormats); ++i)
    {
        if (InlineIsEqualGUID(g_WICFormats[i].wic, *pPixelFormat))
        {
            return g_WICFormats[i].format;
        }
    }
    return DXGI_FORMAT_UNKNOWN;
}


D3DApp::D3DApp(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
    m_rtvDescriptorSize(0),
    m_viewportRect(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0,0,static_cast<LONG>(width),static_cast<LONG>(height)),
    m_fenceValues{}
{
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
                          12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }

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

        // Create the vertex buffer.

    {

        // Define the geometry for a triangle.

        Vertex triangleVertices[] =

        {

            { { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 0.5f,0.0f } },

            { { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 1.0f, 1.0f} },

            { { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f} }

        };



        const UINT vertexBufferSize = sizeof(triangleVertices);

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

        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));

        //搞完之后unlock

        m_vertexBuffer->Unmap(0, nullptr);



        // Initialize the vertex buffer view.

       //果然view就是关于vertex的地址，stride等的信息

        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();

        m_vertexBufferView.StrideInBytes = sizeof(Vertex);

        m_vertexBufferView.SizeInBytes = vertexBufferSize;

    }


    // Create the main command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

    //Create command list
    {
        ThrowIfFailed(
            m_device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_BUNDLE,
                m_bundleAllocator.Get(),
                m_pipelineState.Get(),
                IID_PPV_ARGS(&m_bundle)));


        //m_bundle->SetGraphicsRootSignature(m_rootSignature.Get());
        m_bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_bundle->IASetVertexBuffers(0, 1, &m_vertexBufferView);

        m_bundle->DrawInstanced(3, 1, 0, 0);

        ThrowIfFailed(m_bundle->Close());
    }


    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
   // ThrowIfFailed(m_commandList->Close());














    ComPtr<IWICImagingFactory>			pIWICFactory;
    ComPtr<IWICBitmapDecoder>			pIWICDecoder;
    ComPtr<IWICBitmapFrameDecode>		pIWICFrame;
    ComPtr<IWICBitmapSource>			pIBMP;
    DXGI_FORMAT stTextureFormat = DXGI_FORMAT_UNKNOWN;
    ::CoInitialize(nullptr);
      //get the picture data
    {

        
        //---------------------------------------------------------------------------------------------
        //使用纯COM方式创建WIC类厂对象，也是调用WIC第一步要做的事情
        ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIWICFactory)));

        //使用WIC类厂对象接口加载纹理图片，并得到一个WIC解码器对象接口，图片信息就在这个接口代表的对象中了

        ThrowIfFailed(pIWICFactory->CreateDecoderFromFilename(
            GetAssetFullPath(L"flower.jpg").c_str(),              // 文件名
            NULL,                            // 不指定解码器，使用默认
            GENERIC_READ,                    // 访问权限
            WICDecodeMetadataCacheOnDemand,  // 若需要就缓冲数据 
            &pIWICDecoder                    // 解码器对象
        ));

        // 获取第一帧图片(因为GIF等格式文件可能会有多帧图片，其他的格式一般只有一帧图片)
        // 实际解析出来的往往是位图格式数据
        ThrowIfFailed(pIWICDecoder->GetFrame(0, &pIWICFrame));

        WICPixelFormatGUID wpf = {};
        //获取WIC图片格式
        ThrowIfFailed(pIWICFrame->GetPixelFormat(&wpf));
        GUID tgFormat = {};

        //通过第一道转换之后获取DXGI的等价格式
        if (GetTargetPixelFormat(&wpf, &tgFormat))
        {
            stTextureFormat = GetDXGIFormatFromPixelFormat(&tgFormat);
        }

        if (DXGI_FORMAT_UNKNOWN == stTextureFormat)
        {// 不支持的图片格式 目前退出了事 
         // 一般 在实际的引擎当中都会提供纹理格式转换工具，
         // 图片都需要提前转换好，所以不会出现不支持的现象
            throw HrException(S_FALSE);
        }

        if (!InlineIsEqualGUID(wpf, tgFormat))
        {// 这个判断很重要，如果原WIC格式不是直接能转换为DXGI格式的图片时
         // 我们需要做的就是转换图片格式为能够直接对应DXGI格式的形式
            //创建图片格式转换器
            ComPtr<IWICFormatConverter> pIConverter;
            ThrowIfFailed(pIWICFactory->CreateFormatConverter(&pIConverter));

            //初始化一个图片转换器，实际也就是将图片数据进行了格式转换
            ThrowIfFailed(pIConverter->Initialize(
                pIWICFrame.Get(),                // 输入原图片数据
                tgFormat,						 // 指定待转换的目标格式
                WICBitmapDitherTypeNone,         // 指定位图是否有调色板，现代都是真彩位图，不用调色板，所以为None
                NULL,                            // 指定调色板指针
                0.f,                             // 指定Alpha阀值
                WICBitmapPaletteTypeCustom       // 调色板类型，实际没有使用，所以指定为Custom
            ));
            // 调用QueryInterface方法获得对象的位图数据源接口
            ThrowIfFailed(pIConverter.As(&pIBMP));
        }
        else
        {
            //图片数据格式不需要转换，直接获取其位图数据源接口
            ThrowIfFailed(pIWICFrame.As(&pIBMP));
        }
        //获得图片大小（单位：像素）
        ThrowIfFailed(pIBMP->GetSize(&m_TextureWidth, &m_TextureHeight));

        //获取图片像素的位大小的BPP（Bits Per Pixel）信息，用以计算图片行数据的真实大小（单位：字节）
        ComPtr<IWICComponentInfo> pIWICmntinfo;
        ThrowIfFailed(pIWICFactory->CreateComponentInfo(tgFormat, pIWICmntinfo.GetAddressOf()));

        WICComponentType type;
        ThrowIfFailed(pIWICmntinfo->GetComponentType(&type));

        if (type != WICPixelFormat)
        {
            throw HrException(S_FALSE);
        }

        ComPtr<IWICPixelFormatInfo> pIWICPixelinfo;
        ThrowIfFailed(pIWICmntinfo.As(&pIWICPixelinfo));

        // 到这里终于可以得到BPP了，这也是我看的比较吐血的地方，为了BPP居然饶了这么多环节
        ThrowIfFailed(pIWICPixelinfo->GetBitsPerPixel(&m_TexturePixelSize));

        // 计算图片实际的行大小（单位：字节），这里使用了一个上取整除法即（A+B-1）/B ，
        // 这曾经被传说是微软的面试题,希望你已经对它了如指掌
        m_PicRowPitch = GRS_UPPER_DIV(uint64_t(m_TextureWidth) * uint64_t(m_TexturePixelSize), 8);
    }










    ComPtr<ID3D12Resource> textureUploadHeap;

    // Create the texture.
    {
        // Describe and create a Texture2D.
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = m_TextureWidth;
        textureDesc.Height = m_TextureHeight;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_texture)));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

        // Create the GPU upload buffer.
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&textureUploadHeap)));

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the Texture2D.
        //std::vector<UINT8> texture = GenerateTextureData();




        //按照资源缓冲大小来分配实际图片数据存储的内存大小
        void* pbPicData = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, uploadBufferSize);
        if (nullptr == pbPicData)
        {
            throw HrException(HRESULT_FROM_WIN32(GetLastError()));
        }

        //从图片中读取出数据
        ThrowIfFailed(pIBMP->CopyPixels(nullptr
            , m_PicRowPitch
            , static_cast<UINT>(m_PicRowPitch * m_TextureHeight)   //注意这里才是图片数据真实的大小，这个值通常小于缓冲的大小
            , reinterpret_cast<BYTE*>(pbPicData)));




        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = reinterpret_cast<BYTE*>(pbPicData);
        textureData.RowPitch = m_PicRowPitch;
        textureData.SlicePitch = textureData.RowPitch * m_TextureHeight;

        UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
        cbvHandle.Offset(1, m_rtvDescriptorSize);
        m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, cbvHandle);
    }

    // Close the command list and execute it to begin the initial GPU setup.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
   
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

        const UINT constantBufferSize = sizeof(SceneConstantBuffer);    // CB size is required to be 256-byte aligned.



        ThrowIfFailed(m_device->CreateCommittedResource(

            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),

            D3D12_HEAP_FLAG_NONE,

            &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),

            D3D12_RESOURCE_STATE_GENERIC_READ,

            nullptr,

            IID_PPV_ARGS(&m_constantBuffer)));



        // Describe and create a constant buffer view.

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};

        cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();

        cbvDesc.SizeInBytes = constantBufferSize;

        m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());



        // Map and initialize the constant buffer. We don't unmap this until the

        // app closes. Keeping things mapped for the lifetime of the resource is okay.

        CD3DX12_RANGE readRange(0, 0);        
        // We do not intend to read from this resource on the CPU.

    //把resource的buffer的指针给m_pCbvDataBegin赋值，然后把m_constantBufferData的值赋给资源

        ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));

        memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));

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
    const float translationSpeed = 0.005f;

    const float offsetBounds = 1.25f;
    


    m_constantBufferData.offset.x += translationSpeed;
    m_constantBufferData.offset.y += translationSpeed;
    if (m_constantBufferData.offset.x > offsetBounds)

    {

        m_constantBufferData.offset.x = -offsetBounds;

    }
    if (m_constantBufferData.offset.y > offsetBounds)

    {

        m_constantBufferData.offset.y = -offsetBounds;

    }


    memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
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
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get(),m_SamplersHeap.Get()};

    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    //set cbv table
    m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
    //
    //CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), 1,
    //    m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    ////set srv
    //m_commandList->SetGraphicsRootDescriptorTable(1, srvHandle);

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

std::vector<UINT8> D3DApp::GenerateTextureData()
{
    const UINT rowPitch = m_PicRowPitch;
    const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
    const UINT cellHeight = m_TextureWidth >> 3;    // The height of a cell in the checkerboard texture.
    const UINT textureSize = rowPitch * m_TextureHeight;

    std::vector<UINT8> data(textureSize);
    UINT8* pData = &data[0];

    for (UINT n = 0; n < textureSize; n += m_TexturePixelSize)
    {
        UINT x = n % rowPitch;
        UINT y = n / rowPitch;
        UINT i = x / cellPitch;
        UINT j = y / cellHeight;

        if (i % 2 == j % 2)
        {
            pData[n] = 0x00;        // R
            pData[n + 1] = 0x00;    // G
            pData[n + 2] = 0x00;    // B
            pData[n + 3] = 0xff;    // A
        }
        else
        {
            pData[n] = 0xff;        // R
            pData[n + 1] = 0xff;    // G
            pData[n + 2] = 0xff;    // B
            pData[n + 3] = 0xff;    // A
        }
    }

    return data;
}


