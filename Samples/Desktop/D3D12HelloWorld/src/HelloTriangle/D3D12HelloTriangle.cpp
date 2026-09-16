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
#include "D3D12HelloTriangle.h"

extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = 618;
}
extern "C"
{
    __declspec(dllexport) extern const char *D3D12SDKPath = u8".\\D3D12\\";
}

D3D12HelloTriangle::D3D12HelloTriangle(UINT width, UINT height, std::wstring name) : DXSample(width, height, name),
                                                                                     m_frameIndex(0),
                                                                                     m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
                                                                                     m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
                                                                                     m_rtvDescriptorSize(0)
{
    // 【核心】m_viewport 和 m_scissorRect 已在初始化列表中按窗口宽高设置为“覆盖整个客户区”。
    // Viewport 负责坐标映射，Scissor Rect 负责映射后的像素裁剪；二者作用不同，绘制前都要绑定。
}

void D3D12HelloTriangle::OnInit()
{
    LoadPipeline();
    LoadAssets();
}

// Load the rendering pipeline dependencies.
void D3D12HelloTriangle::LoadPipeline()
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
            IID_PPV_ARGS(&m_device)));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)));
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
        m_commandQueue.Get(), // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain));

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
        }
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void D3D12HelloTriangle::LoadAssets()
{
    // 【核心】创建一个“空根签名”。
    // 根签名不是资源本身，也不保存顶点或颜色；它规定着色器运行时允许从命令列表接收哪些外部绑定。
    // 本例的着色器只使用输入装配阶段给出的 POSITION/COLOR，没有常量缓冲区、纹理、UAV 或采样器，
    // 所以根参数和静态采样器都可以为 0。“空”表示绑定表为空，不表示可以完全省略根签名对象。
    {
        // 【核心】第一步：在 CPU 侧准备根签名描述。此时只是在填写结构体，还没有创建 D3D12 对象。
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;

        rootSignatureDesc.Init(
            0,       // 【核心】NumParameters：没有根常量、根描述符或描述符表。
            nullptr, // 【核心】pParameters：参数数量为 0，因此没有参数数组。
            0,       // 【核心】NumStaticSamplers：着色器不采样纹理，因此没有静态采样器。
            nullptr, // 【核心】pStaticSamplers：采样器数量为 0，因此没有采样器数组。
            // 【核心】虽然根绑定为空，本例仍通过输入装配阶段读取顶点；这个标志明确允许使用输入布局。
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // 【核心】第二步需要两个 CPU 侧 Blob：signature 接收序列化后的二进制描述；
        // error 可在序列化失败时接收诊断文本。它们都不是最终的 ID3D12RootSignature 对象。
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;

        // 【核心】第二步：把便于 C++ 填写的描述转换成版本明确的二进制 Blob。
        // D3D12SerializeRootSignature 的调用契约是验证并序列化描述；这里选择根签名 1.0 格式。
        // 【可略】本例把 error Blob 交给 API 接收诊断信息，但没有额外读取或打印其中的文本。
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));

        // 【核心】第三步：Device 根据序列化字节创建真正的根签名对象，并保存到 m_rootSignature。
        // 第一个 0 是 NodeMask；单适配器示例使用 0。随后两个参数是 Blob 的起始地址和字节数。
        // 创建成功仍不会绑定任何资源或执行 GPU 工作；该对象稍后会写入 PSO，并在录制绘制命令时绑定。
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    // 【核心】创建图形管线状态对象（PSO）。
    // 原英文注释容易让人以为下面会在运行时编译 HLSL；实际流程分成两个时间点：
    //   1. 构建时：项目调用 DXC，把 shaders.hlsl 的 VSMain/PSMain 编译成两个 .cso 字节码文件；
    //   2. 运行时：下面的 C++ 读取 .cso，连同输入布局等固定状态一起创建 PSO。
    // PSO 的意义是把一组必须互相兼容的绘制规则固定成一个可反复绑定的对象。
    {
        // 【核心】这四个变量只描述 CPU 内存中的两段“已编译着色器字节码”：指针 + 字节数。
        // 它们不是 HLSL 源码，也不是正在执行的着色器；CreateGraphicsPipelineState 会读取这些字节。
        UINT8 *pVertexShaderData = nullptr;
        UINT8 *pPixelShaderData = nullptr;
        UINT vertexShaderDataLength = 0;
        UINT pixelShaderDataLength = 0;

        // 【核心】读取构建阶段生成的顶点着色器字节码。GetAssetFullPath 负责得到运行目录中的完整路径；
        // ReadDataFromFile 成功后同时填写起始指针和实际字节数，失败则终止初始化流程。
        ThrowIfFailed(ReadDataFromFile(GetAssetFullPath(L"shaders_VSMain.cso").c_str(), &pVertexShaderData, &vertexShaderDataLength));

        // 【核心】以同样方式读取像素着色器字节码。文件名中的 VSMain/PSMain 对应各自的 HLSL 入口函数。
        ThrowIfFailed(ReadDataFromFile(GetAssetFullPath(L"shaders_PSMain.cso").c_str(), &pPixelShaderData, &pixelShaderDataLength));

        // 【核心】顶点缓冲区里只有连续字节，输入布局负责把每个 Vertex 内的字节解释成着色器输入。
        // 每个元素最关键的三项是：HLSL 语义名、数据格式、相对当前顶点起点的字节偏移。
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
            {
                // POSITION 对应 VSMain 的“: POSITION”。从顶点起点偏移 0 字节读取 3 个 32 位浮点数。
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},

                // COLOR 对应 VSMain 的“: COLOR”。position 占 3 × 4 = 12 字节，所以颜色从偏移 12 开始，
                // 再读取 4 个 32 位浮点数，对应 Vertex::color。
                {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

        // 【可略】两项的 SemanticIndex 都是 0，因为没有 POSITION1 或 COLOR1；InputSlot 都是 0，
        // 对应稍后绑定顶点缓冲区时使用的槽位 0。PER_VERTEX_DATA 表示每前进一个顶点就读取下一份数据，
        // 因此最后的 InstanceDataStepRate 必须为 0，本例不使用按实例变化的数据。
        // POSITION 只提供 x、y、z，而 VSMain 接收 float4；输入装配会把缺少的 w 分量补为 1。

        // 【核心】先在 CPU 侧填写完整的 PSO 描述，再一次性交给 Device 创建不可变的管线状态对象。
        // 零初始化保证尚未显式设置的可选字段从 0/空值开始，避免把未初始化内存当成配置。
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

        // 【核心】数组本身只是 CPU 侧描述；把它写入 PSO 描述后，它才成为这条图形管线的输入规则。
        psoDesc.InputLayout = {inputElementDescs, _countof(inputElementDescs)};

        // 【核心】指定着色器可见资源的绑定契约。HelloTriangle 使用前面创建的空根签名：
        // 着色器不读取常量缓冲区、纹理或采样器，但仍允许输入装配阶段提供顶点数据。
        psoDesc.pRootSignature = m_rootSignature.Get();

        // 【核心】把“字节码地址 + 字节数”包装成 D3D12_SHADER_BYTECODE。
        // CD3DX12_SHADER_BYTECODE 只是辅助构造描述，不会在这里重新编译或执行着色器。
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderData, vertexShaderDataLength);
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderData, pixelShaderDataLength);

        // 【核心】光栅化状态决定如何把三角形转换成候选像素；本例采用 D3D12 默认设置。
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

        // 【核心】混合状态决定像素着色器输出如何与渲染目标原值合成；默认状态不启用颜色混合。
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

        // 【核心】本例没有深度缓冲区和模板缓冲区，所以明确关闭深度测试与模板测试。
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        // 【可略】采样掩码按位决定允许写入哪些多重采样样本；UINT_MAX 表示全部允许。
        // 本例 SampleDesc.Count 为 1，没有开启 MSAA，因此它不会筛掉唯一的样本。
        psoDesc.SampleMask = UINT_MAX;

        // 【核心】PSO 接受“三角形类别”的图元。这里只限定大类；具体是 TRIANGLELIST 还是其他三角形拓扑，
        // 要在录制绘制命令时通过 IASetPrimitiveTopology 再指定。
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // 【核心】像素着色器只向 1 个颜色渲染目标输出。
        psoDesc.NumRenderTargets = 1;

        // 【核心】第 0 个渲染目标的格式必须与实际绑定的 RTV/交换链后台缓冲区格式兼容；
        // HelloTriangle 的后台缓冲区同样使用 RGBA8 UNORM。
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

        // 【核心】每个像素只有 1 个样本，即不使用多重采样抗锯齿（MSAA）。
        psoDesc.SampleDesc.Count = 1;

        // 【核心】CreateGraphicsPipelineState 的调用契约：检查并组合上述描述，成功后把 PSO 写入
        // m_pipelineState；若任一状态彼此不兼容或描述无效，ThrowIfFailed 会终止初始化并报告失败。
        // 此调用只创建管线对象，不会画出三角形；真正使用它还要靠命令列表绑定并发出 DrawInstanced。
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    // 【核心】创建图形命令列表，并把刚创建的 PSO 作为它的初始管线状态。
    // CreateCommandList 成功后命令列表处于“正在录制”状态；这里只完成对象创建，还没有实际绘制命令。
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    // 【核心】当前没有初始化命令要记录，而每帧的 PopulateCommandList 会先调用 Reset 重新开始录制；
    // Reset 要求命令列表已经关闭，因此创建后先 Close，把它放到可在主循环中重置的初始状态。
    ThrowIfFailed(m_commandList->Close());

    // Create the vertex buffer.
    {
        // 【核心】这里用两个彼此独立的三角形拼成矩形，因此 CPU 数组中共有 6 份顶点数据。
        // 本例没有索引缓冲区，两个三角形共享的角也要在数组中各写一份。
        // 这里只是在描述数据。GPU 还看不到它；后面还要把这些数据放入顶点缓冲区并在绘制时绑定。
        Vertex triangleVertices[] = {
            //                  position                                  color (R, G, B, A)
            {{-0.25f, 0.25f * m_aspectRatio, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // 左上：红色
            {{0.25f, -0.25f * m_aspectRatio, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}}, // 右下：绿色
            {{-0.25f, -0.25f * m_aspectRatio, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}, // 左下：蓝色
            {{-0.25f, 0.25f * m_aspectRatio, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}, // 左上：红色
            {{0.25f, 0.25f * m_aspectRatio, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},   // 右上：绿色
            {{0.25f, -0.25f * m_aspectRatio, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}  // 右下：蓝色
        };

        // 【可略】这里让 y 坐标乘以窗口宽高比，是为了补偿宽屏窗口在 x、y 方向上的显示比例差异，
        // 使矩形在 1280×720 窗口中不会因为窗口较宽而显得过扁。坐标空间会在着色器一节再解释。

        // 【核心】创建缓冲区前必须先给出容量；这里需要容纳整个六顶点数组的全部字节。
        const UINT vertexBufferSize = sizeof(triangleVertices);

        // 【核心】CPU 数组不是 D3D12 资源，GPU 不能把它直接当作绘制输入长期使用。
        // CreateCommittedResource 的调用契约是：按给定内存属性和资源描述创建一块资源，
        // 成功后把可长期持有的 ID3D12Resource 写入 m_vertexBuffer。
        //
        // 【可略】静态顶点在正式项目中通常先写入 UPLOAD 堆，再复制到更适合 GPU 长期读取的 DEFAULT 堆。
        // 本例只有 6 个顶点，为减少“上传命令与状态转换”等额外概念，直接把顶点缓冲区放在 UPLOAD 堆中。
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // 【核心】允许 CPU 映射并写入，GPU 随后可以读取。
            D3D12_HEAP_FLAG_NONE,                             // 【可略】不要求额外的堆行为。
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize), // 【核心】创建指定字节数的一维缓冲区。
            D3D12_RESOURCE_STATE_GENERIC_READ,                // 【核心】UPLOAD 堆资源保持为 GPU 通用读取状态。
            nullptr,                                          // 【可略】缓冲区不需要纹理使用的优化清除值。
            IID_PPV_ARGS(&m_vertexBuffer)));                  // 【核心】接收创建出的顶点缓冲区资源。

        // 【核心】资源已经创建，但其中还是未填写的字节；接下来把 CPU 数组复制进去。
        UINT8 *pVertexDataBegin;

        // Map 成功后返回一个 CPU 可写指针。空读取范围 [0, 0) 表示 CPU 不准备从资源中读取旧数据。
        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void **>(&pVertexDataBegin)));

        // 把六份 Vertex 的原始字节完整复制到缓冲区，然后结束本次 CPU 映射。
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        m_vertexBuffer->Unmap(0, nullptr);

        // 到这里，GPU 可读取的资源里已经有顶点字节；但 GPU 还不知道每个顶点多大、从哪里开始读取。
        // 这些解释规则由下一节的“顶点缓冲区视图”提供。

        // 【核心】缓冲区现在只有一段连续字节，还需要用视图给出三条读取规则：
        // 从哪里开始、每读取一个顶点向前跨多少字节、整段有效数据一共有多少字节。
        // 视图本身不复制数据，也不会触发 GPU 工作；它只是稍后绑定缓冲区时使用的描述信息。
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress(); // 【核心】第一个顶点的 GPU 起始地址。
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);                          // 【核心】相邻两个顶点起点之间的字节距离。
        m_vertexBufferView.SizeInBytes = vertexBufferSize;                          // 【核心】这次允许读取的顶点数据总字节数。

        // 【可略】这里用 sizeof(Vertex) 和 vertexBufferSize 计算，而不手写数字；
        // 将来 Vertex 结构发生变化时，视图仍能跟随真实内存布局更新。
        // 此视图只能划分“第几个顶点”；position 和 color 各占哪些字节，要由后面的输入布局继续说明。
    }

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command
        // list in our main loop but for now, we just want to wait for setup to
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

// Update frame-based values.
void D3D12HelloTriangle::OnUpdate()
{
}

// Render the scene.
void D3D12HelloTriangle::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList *ppCommandLists[] = {m_commandList.Get()};
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void D3D12HelloTriangle::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void D3D12HelloTriangle::PopulateCommandList()
{
    // 【核心】这个函数只在 CPU 上“录制一帧要执行的 GPU 命令”，调用到 DrawInstanced 时也不会立刻绘制。
    // OnRender 随后把关闭的命令列表提交给 Command Queue，GPU 才会按这里记录的顺序执行。

    // 【核心】Command Allocator 保存命令列表底层记录所需的内存。只有确认 GPU 不再使用上一帧记录后才能重置。
    // HelloTriangle 每帧末尾通过 WaitForPreviousFrame 等待，因此这里可以安全复用同一个 Allocator。
    ThrowIfFailed(m_commandAllocator->Reset());

    // 【核心】重新打开命令列表开始录制，并指定本帧初始使用的 PSO。
    // 第一个参数提供刚清空的记录内存；第二个参数让命令列表从 m_pipelineState 描述的图形管线开始。
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    // 【核心】绑定图形根签名。PSO 声明着色器采用这份绑定契约，命令列表也要把兼容的根签名设为当前状态。
    // 本例的根签名虽然没有资源参数，仍然必须作为真实对象绑定。
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // 【核心】RS = Rasterizer Stage。Viewport 把标准化坐标映射到整个窗口的像素区域。
    m_commandList->RSSetViewports(1, &m_viewport);

    // 【核心】Scissor Rect 限制允许继续处理的像素矩形；本例设置 1 个覆盖整个窗口的裁剪矩形。
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // 【核心】交换链交出的后台缓冲区原本处于 PRESENT 状态；清屏和绘制前必须把当前缓冲区转换为 RENDER_TARGET。
    // ResourceBarrier 仍然只是把转换命令写入列表，真正的状态变化发生在 GPU 执行该命令时。
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // 【核心】RTV Heap 中有两个描述符；根据 m_frameIndex 选择本帧当前后台缓冲区对应的 RTV CPU 句柄。
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);

    // 【核心】OM = Output Merger。把当前后台缓冲区绑定为颜色输出目标；之后像素着色器的 SV_TARGET 写到这里。
    // 参数 1 表示一个 RTV；最后的 nullptr 表示没有深度模板视图。
    // 【可略】FALSE 表示传入的是 RTV 句柄数组而不是一个连续描述符范围；这里只有一个句柄，差异不影响结果。
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // 【核心】先把整个颜色目标清成深蓝色。ClearRenderTargetView 只记录清除命令，不会立即修改屏幕。
    // RGBA 四个浮点分量使用 0～1 范围。
    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // 【核心】IA = Input Assembler。TRIANGLELIST 表示每连续 3 个顶点组成一个彼此独立的三角形。
    // 这比 PSO 中的 TRIANGLE 类型更具体：PSO 只限定图元大类，这里才确定采用“三角形列表”。
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 【核心】把一个顶点缓冲区视图绑定到输入槽 0。输入布局中的两个元素也都从槽 0 读取。
    // 视图告诉 IA 顶点数据的 GPU 起点、单顶点步长和总字节范围。
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    // 【核心】记录真正的绘制请求：从当前绑定的顶点缓冲区读取 6 个顶点，绘制 1 个实例。
    // 四个参数依次是：每实例顶点数 6、实例数 1、起始顶点 0、起始实例 0。
    // GPU 执行到这里时，6 个顶点依次经过 VSMain，并按每 3 个顶点组成一个三角形，最终得到两个三角形。
    // 此例没有索引缓冲区，因此使用 DrawInstanced，而不是 DrawIndexedInstanced。
    m_commandList->DrawInstanced(6, 1, 0, 0);

    // 【核心】绘制结束后，当前后台缓冲区将交给交换链呈现，因此转换回 PRESENT 状态。
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // 【核心】结束本帧录制。只有关闭后的命令列表才能交给 ExecuteCommandLists 提交执行。
    ThrowIfFailed(m_commandList->Close());
}

void D3D12HelloTriangle::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
