#include "stdafx.h"
#include "TextureDataLoader.h"

bool TextureDataLoader::GetTargetPixelFormat(const GUID* pSourceFormat, GUID* pTargetFormat)
{
    //���ȷ�����ݵ���ӽ���ʽ���ĸ�
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
    //���ȷ�����ն�Ӧ��DXGI��ʽ����һ��
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
    //ʹ�ô�COM��ʽ����WIC�೧����Ҳ�ǵ���WIC��һ��Ҫ��������
    ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIWICFactory)));

    //ʹ��WIC�೧����ӿڼ�������ͼƬ�����õ�һ��WIC����������ӿڣ�ͼƬ��Ϣ��������ӿڴ���Ķ�������
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    std::wstring str = assetsPath;
    
    ThrowIfFailed(pIWICFactory->CreateDecoderFromFilename(
        Filename,              // �ļ���
        NULL,                            // ��ָ����������ʹ��Ĭ��
        GENERIC_READ,                    // ����Ȩ��
        WICDecodeMetadataCacheOnDemand,  // ����Ҫ�ͻ������� 
        &pIWICDecoder                    // ����������
    ));

    // ��ȡ��һ֡ͼƬ(��ΪGIF�ȸ�ʽ�ļ����ܻ��ж�֡ͼƬ�������ĸ�ʽһ��ֻ��һ֡ͼƬ)
    // ʵ�ʽ���������������λͼ��ʽ����
    ThrowIfFailed(pIWICDecoder->GetFrame(0, &pIWICFrame));

    WICPixelFormatGUID wpf = {};
    //��ȡWICͼƬ��ʽ
    ThrowIfFailed(pIWICFrame->GetPixelFormat(&wpf));
    GUID tgFormat = {};

    //ͨ����һ��ת��֮���ȡDXGI�ĵȼ۸�ʽ
    if (GetTargetPixelFormat(&wpf, &tgFormat))
    {
        stTextureFormat = GetDXGIFormatFromPixelFormat(&tgFormat);
    }

    if (DXGI_FORMAT_UNKNOWN == stTextureFormat)
    {// ��֧�ֵ�ͼƬ��ʽ Ŀǰ�˳����� 
     // һ�� ��ʵ�ʵ����浱�ж����ṩ�����ʽת�����ߣ�
     // ͼƬ����Ҫ��ǰת���ã����Բ�����ֲ�֧�ֵ�����
        throw HrException(S_FALSE);
    }

    if (!InlineIsEqualGUID(wpf, tgFormat))
    {// ����жϺ���Ҫ�����ԭWIC��ʽ����ֱ����ת��ΪDXGI��ʽ��ͼƬʱ
     // ������Ҫ���ľ���ת��ͼƬ��ʽΪ�ܹ�ֱ�Ӷ�ӦDXGI��ʽ����ʽ
        //����ͼƬ��ʽת����
        ComPtr<IWICFormatConverter> pIConverter;
        ThrowIfFailed(pIWICFactory->CreateFormatConverter(&pIConverter));

        //��ʼ��һ��ͼƬת������ʵ��Ҳ���ǽ�ͼƬ���ݽ����˸�ʽת��
        ThrowIfFailed(pIConverter->Initialize(
            pIWICFrame.Get(),                // ����ԭͼƬ����
            tgFormat,						 // ָ����ת����Ŀ���ʽ
            WICBitmapDitherTypeNone,         // ָ��λͼ�Ƿ��е�ɫ�壬�ִ��������λͼ�����õ�ɫ�壬����ΪNone
            NULL,                            // ָ����ɫ��ָ��
            0.f,                             // ָ��Alpha��ֵ
            WICBitmapPaletteTypeCustom       // ��ɫ�����ͣ�ʵ��û��ʹ�ã�����ָ��ΪCustom
        ));
        // ����QueryInterface������ö����λͼ����Դ�ӿ�
        ThrowIfFailed(pIConverter.As(&pIBMP));
    }
    else
    {
        //ͼƬ���ݸ�ʽ����Ҫת����ֱ�ӻ�ȡ��λͼ����Դ�ӿ�
        ThrowIfFailed(pIWICFrame.As(&pIBMP));
    }
    //���ͼƬ��С����λ�����أ�
    ThrowIfFailed(pIBMP->GetSize(&m_TextureWidth, &m_TextureHeight));

    //��ȡͼƬ���ص�λ��С��BPP��Bits Per Pixel����Ϣ�����Լ���ͼƬ�����ݵ���ʵ��С����λ���ֽڣ�
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

    // ���������ڿ��Եõ�BPP�ˣ���Ҳ���ҿ��ıȽ���Ѫ�ĵط���Ϊ��BPP��Ȼ������ô�໷��
    ThrowIfFailed(pIWICPixelinfo->GetBitsPerPixel(&m_TexturePixelSize));

    // ����ͼƬʵ�ʵ��д�С����λ���ֽڣ�������ʹ����һ����ȡ����������A+B-1��/B ��
    // ����������˵��΢���������,ϣ�����Ѿ���������ָ��
    m_PicRowPitch = GRS_UPPER_DIV(uint64_t(m_TextureWidth) * uint64_t(m_TexturePixelSize), 8);

}

void* TextureDataLoader::GetPixelData(UINT uploadBufferSize)
{
    //������Դ�����С������ʵ��ͼƬ���ݴ洢���ڴ��С
    void* pbPicData = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, uploadBufferSize);
    if (nullptr == pbPicData)
    {
        throw HrException(HRESULT_FROM_WIN32(GetLastError()));
    }

    //��ͼƬ�ж�ȡ������
    ThrowIfFailed(pIBMP->CopyPixels(nullptr
        , m_PicRowPitch
        , static_cast<UINT>(m_PicRowPitch * m_TextureHeight)   //ע���������ͼƬ������ʵ�Ĵ�С�����ֵͨ��С�ڻ���Ĵ�С
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


