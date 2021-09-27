#include "stdafx.h"
#include "TextureDataLoader.h"

bool TextureDataLoader::GetTargetPixelFormat(const GUID* pSourceFormat, GUID* pTargetFormat)
{
    //查表确定兼容的最接近格式是哪个
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

DXGI_FORMAT TextureDataLoader::GetDXGIFormatFromPixelFormat(const GUID* pPixelFormat)
{
    //查表确定最终对应的DXGI格式是哪一个
    for (size_t i = 0; i < _countof(g_WICFormats); ++i)
    {
        if (InlineIsEqualGUID(g_WICFormats[i].wic, *pPixelFormat))
        {
            return g_WICFormats[i].format;
        }
    }
    return DXGI_FORMAT_UNKNOWN;
}

TextureDataLoader::TextureDataLoader(UINT HeapOffset, UINT TableOffset)
{
	stTextureFormat = DXGI_FORMAT_UNKNOWN;
	m_tmpTableOffsetInHeap = HeapOffset;
	m_tmpOffsetInTable = TableOffset;
	::CoInitialize(nullptr);
}

void TextureDataLoader::ReadTheTextureDataFromFile(LPCWSTR Filename)
{
    //使用纯COM方式创建WIC类厂对象，也是调用WIC第一步要做的事情
    ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIWICFactory)));

    //使用WIC类厂对象接口加载纹理图片，并得到一个WIC解码器对象接口，图片信息就在这个接口代表的对象中了
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    std::wstring str = assetsPath;
    
    ThrowIfFailed(pIWICFactory->CreateDecoderFromFilename(
        Filename,              // 文件名
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

void* TextureDataLoader::GetPixelData(UINT uploadBufferSize)
{
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
    return pbPicData;
}

void TextureDataLoader::UploadAndConnectResource
(
    ID3D12Device* device, 
    ID3D12GraphicsCommandList* commandList,
    ID3D12DescriptorHeap* heap,
    ID3D12CommandQueue* commandQueue
)
{
    
    // Create the texture.
    {

        
        // Describe and create a Texture2D.
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = m_TextureWidth;
        textureDesc.Height =m_TextureHeight;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_texture)));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

       

        // Create the GPU upload buffer.
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&textureUploadHeap)));


        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = GetPixelData(uploadBufferSize);
        textureData.RowPitch =m_PicRowPitch;
        textureData.SlicePitch = textureData.RowPitch * m_TextureHeight;
        UpdateSubresources(commandList, m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(heap->GetCPUDescriptorHandleForHeapStart(),1,device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
        device->CreateShaderResourceView(m_texture.Get(), &srvDesc, srvHandle);
    }

    // Close the command list and execute it to begin the initial GPU setup.
    ThrowIfFailed(commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { commandList};
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}


