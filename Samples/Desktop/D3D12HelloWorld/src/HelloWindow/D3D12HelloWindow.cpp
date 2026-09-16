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
#include "D3D12HelloWindow.h"

// 【暂时略过】这两项告诉 D3D12 Agility SDK 加载器使用哪个 SDK 版本及其相对路径。
// 它们属于运行环境配置，不影响我们理解本节的构造和初始化流程。
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }

D3D12HelloWindow::D3D12HelloWindow(UINT width, UINT height, std::wstring name) :
    // 把窗口宽度、高度和标题交给 DXSample 保存。
    DXSample(width, height, name),

    // 这两个值的具体用途稍后遇到对应概念时再讲；现在只需知道它们从 0 开始。
    m_frameIndex(0),
    m_rtvDescriptorSize(0)
{
    // 构造函数体为空：此时既不创建窗口，也不创建 DirectX 对象。
}

// Win32Application 创建好窗口后会调用这里。
// 初始化分成两步，是为了先建立基础环境，再创建依赖这个环境的其他对象。
void D3D12HelloWindow::OnInit()
{
    // 第一步：建立程序、显卡与窗口之间的基础连接。
    LoadPipeline();

    // 第二步：创建这个示例运行时还需要的对象。
    LoadAssets();
}

// 创建渲染流程所依赖的基础环境；下一节从这里继续。
void D3D12HelloWindow::LoadPipeline()
{
    // 【可略】创建 DXGI Factory 时要使用的附加选项。
    // 默认值 0 表示不启用额外选项；Debug 构建中可能在下面加入调试标志。
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // 【核心】只在 Debug 构建中尝试开启 D3D12 调试层。
    // 调试层会检查错误的 API 用法和资源状态，并把诊断信息输出到调试器。
    // 它必须在 D3D12 设备创建之前开启；设备创建后再开启会使已有设备失效。
    {
        // 用临时 ComPtr 保存调试接口；离开这个花括号后它会自动释放。
        ComPtr<ID3D12Debug> debugController;

        // 尝试取得调试接口。IID_PPV_ARGS 是获取 COM 接口时常见的辅助宏，
        // 现在只需把整句理解成“请把 ID3D12Debug 接口放入 debugController”。
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            // 真正开启 D3D12 API 检查。
            debugController->EnableDebugLayer();

            // 【可略】同时要求稍后创建的 DXGI Factory 输出与交换链相关的调试信息。
            // |= 表示在保留已有标志的基础上，再加入 DXGI_CREATE_FACTORY_DEBUG。
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        // 如果系统没有安装 Graphics Tools，获取调试接口会失败，
        // 但这里不会中止程序，只是继续以没有调试层的方式运行。
    }
#endif

    // 【核心】DXGI 负责显卡枚举、交换链和窗口显示等工作。
    // Factory 是访问这些功能的入口对象；它本身不是显卡，也不负责执行绘制命令。
    ComPtr<IDXGIFactory4> factory;

    // 创建 DXGI Factory，并把结果保存到 factory。
    // dxgiFactoryFlags 决定是否附带调试功能；IID_PPV_ARGS 的内部细节现在可以略过。
    // ThrowIfFailed 表示：创建失败就立即抛出异常，不让程序带着无效对象继续执行。
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    // 【核心】Adapter 表示程序选择使用的“图形适配器”。
    // 通常它对应一块硬件显卡；WARP 则是 Windows 提供的 CPU 软件实现。
    if (m_useWarpDevice)
    {
        // 只有命令行使用 -warp 或 /warp 时才进入这里。
        // WARP 速度远慢于真实显卡，主要用于调试或没有合适硬件时验证程序正确性。
        ComPtr<IDXGIAdapter> warpAdapter;

        // 通过 DXGI Factory 取得系统内置的 WARP 软件适配器。
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        // 【核心】根据选中的 WARP Adapter 创建程序的 D3D12 Device。
        // warpAdapter.Get() 取出 ComPtr 内部保存的接口指针，交给创建函数使用。
        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),

            // 这里仍然创建的是 D3D12 Device；11_0 只表示要求硬件至少支持
            // Direct3D Feature Level 11_0，并不表示程序使用的是 D3D11 API。
            D3D_FEATURE_LEVEL_11_0,

            // 创建结果保存到成员变量 m_device，供整个示例后续持续使用。
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        // 默认进入硬件路径。这个局部变量将保存被选中的硬件显卡 Adapter。
        ComPtr<IDXGIAdapter1> hardwareAdapter;

        // GetHardwareAdapter 会枚举系统显卡，跳过软件适配器，
        // 并选出一块能够支持 D3D12 的硬件 Adapter；内部枚举细节现在可以略过。
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        // 【核心】根据选中的硬件 Adapter 创建程序的 D3D12 Device。
        // hardwareAdapter.Get() 只把内部接口指针借给函数使用，并不转移所有权。
        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),

            // 要求这块硬件至少支持 Feature Level 11_0。
            // 这是硬件能力门槛，不是所使用 Direct3D API 的版本号。
            D3D_FEATURE_LEVEL_11_0,

            // 创建结果保存到成员变量 m_device。
            IID_PPV_ARGS(&m_device)
            ));
    }

    // 【核心】创建 Command Queue（命令队列）。
    // CPU 以后会把录制好的命令列表提交到这个队列，GPU 按提交顺序执行。
    // DESC 是 description 的缩写，这个结构体用于描述要创建怎样的队列。
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};

    // 【可略】不使用特殊队列标志，保持默认行为。
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    // 【核心】DIRECT 是通用图形队列，能够执行绘制、计算和复制类命令。
    // HelloWindow 后面要对窗口图像执行清除操作，所以使用 DIRECT 队列。
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    // Device 根据上面的描述创建队列，并把结果保存到成员变量 m_commandQueue。
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // 【核心】开始描述 Swap Chain（交换链）。
    // 这里只是在填写“想创建怎样的交换链”，真正的创建调用还在后面。
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

    // 使用两张 Back Buffer 轮流进行渲染和显示，也就是双缓冲。
    swapChainDesc.BufferCount = FrameCount;

    // 每张 Back Buffer 的大小与窗口客户区一致。
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;

    // 【可略】每个像素由 8 位红、绿、蓝、透明度组成，数值按普通归一化颜色解释。
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    // 【核心】BufferUsage 声明 Back Buffer 允许承担什么用途。
    // RENDER_TARGET_OUTPUT 表示它将作为 GPU 的输出目标，可被绘制或清除；
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    // 【可略】使用现代 Flip 显示模式；Present 后不依赖保留旧 Back Buffer 内容。
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    // 【可略】采样数为 1，表示交换链图像本身不使用多重采样抗锯齿（MSAA）。
    swapChainDesc.SampleDesc.Count = 1;

    // 创建函数先返回基础版本的 IDXGISwapChain1，暂存在局部变量中；
    // 稍后再取得示例需要的 IDXGISwapChain3 接口。
    ComPtr<IDXGISwapChain1> swapChain;

    // 【核心】让 DXGI Factory 创建一个与当前窗口关联的 Swap Chain。
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),               // D3D12 使用的 DIRECT Command Queue。
        Win32Application::GetHwnd(),        // Swap Chain 要显示到的窗口句柄。
        &swapChainDesc,                     // 上一小节填写好的交换链描述。
        nullptr,                            // 【可略】nullptr 表示创建窗口模式交换链。
        nullptr,                            // 【可略】不把内容限制到某一台显示器。
        &swapChain                          // 接收创建出的 IDXGISwapChain1 接口。
        ));

    // 【可略】禁止 DXGI 自动响应 Alt+Enter 并切换窗口/全屏模式。
    // HelloWindow 没有实现完整的全屏切换和缓冲区重建，因此保持窗口模式更简单。
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    // 【核心】CreateSwapChainForHwnd 返回的是 IDXGISwapChain1；
    // ComPtr::As 为同一个 Swap Chain 取得较新的 IDXGISwapChain3 接口。
    // 这不是复制或创建第二个交换链，m_swapChain 与局部变量代表同一个对象。
    ThrowIfFailed(swapChain.As(&m_swapChain));

    // 【核心】询问 Swap Chain 当前轮到哪一张 Back Buffer，并保存其数组下标。
    // FrameCount 为 2，所以 m_frameIndex 的有效值是 0 或 1。
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // 【核心】D3D12 把“资源本身”和“如何使用资源”分开表示：
    // Back Buffer Resource 保存像素；RTV（Render Target View）描述如何把它作为渲染输出。
    // RTV 不会复制图像，也不拥有像素，只是一份供 D3D12 识别资源用途的描述符。
    // Descriptor 必须存放在 Descriptor Heap 中，所以下面先为两个 RTV 创建存放空间。
    {
        // 先填写 RTV Descriptor Heap 的创建描述；{} 把未填写字段初始化为 0。
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};

        // 双缓冲有两张 Back Buffer，因此 Heap 需要两个 RTV 位置。
        rtvHeapDesc.NumDescriptors = FrameCount;

        // 指定这个 Heap 专门存放 Render Target View 描述符。
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

        // 【可略】RTV 由 CPU 通过句柄交给命令列表，不创建 Shader 可见的 Heap。
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        // Device 根据描述创建空的 RTV Heap；此时只是分配两个位置，还没有写入 RTV。
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        // 【核心】不同设备的 Descriptor 间距可能不同，不能使用 sizeof 或写死数值。
        // 向 Device 查询 RTV Heap 中相邻两个位置的字节间距，供后面移动句柄使用。
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // 【核心】从 Swap Chain 取得两张 Back Buffer Resource，
    // 再分别创建 RTV，写入刚才建立的两个 Heap 位置。
    {
        // 取得 RTV Heap 的起始 CPU Handle，也就是第 0 个 Descriptor 位置。
        // CD3DX12_CPU_DESCRIPTOR_HANDLE 是 d3dx12.h 提供的辅助类型，方便后面移动 Handle。
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // FrameCount 为 2，所以循环分别处理 Back Buffer 0 和 Back Buffer 1。
        for (UINT n = 0; n < FrameCount; n++)
        {
            // 从 Swap Chain 取得第 n 张 Back Buffer 的 Resource 接口，
            // 保存到 m_renderTargets[n]，此处没有复制图像像素。
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));

            // 为这张 Back Buffer 创建 RTV，并把 Descriptor 写到 rtvHandle 指向的位置。
            // 第二个参数为 nullptr，表示根据 Resource 的格式和维度生成默认 RTV 描述。
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);

            // 把 Handle 向后移动一个 RTV 的间距，让下一轮写入 Heap 的下一个位置。
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    // 【核心】创建 Command Allocator，为后面的 DIRECT Command List 提供命令存储。
    // Allocator 的类型必须和使用它的 Command List 类型一致，所以这里同样选择 DIRECT。
    // 创建结果保存到成员变量 m_commandAllocator，供每一帧重复使用。
    ThrowIfFailed(m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_commandAllocator)));
}

// 创建 HelloWindow 运行时还需要的对象。
// 这个示例没有模型或纹理，因此这里主要创建 Command List 和同步对象。
void D3D12HelloWindow::LoadAssets()
{
    // 【核心】创建用于录制 GPU 命令的 Command List。
    ThrowIfFailed(m_device->CreateCommandList(
        0,                                  // 【可略】单 GPU 节点使用 0。
        D3D12_COMMAND_LIST_TYPE_DIRECT,     // 与 Allocator 和 Queue 的 DIRECT 类型一致。
        m_commandAllocator.Get(),           // 提供录制命令所需的内部存储。
        nullptr,                            // HelloWindow 只清屏，暂不指定初始 Pipeline State。
        IID_PPV_ARGS(&m_commandList)));      // 创建结果保存到成员变量。

    // 【核心】CreateCommandList 创建出的列表默认处于“正在录制”状态。
    // 初始化阶段还没有一帧的命令要写入，而后面重新录制前需要对已关闭的列表调用 Reset，
    // 所以先用 Close 结束这次空录制，让它进入可被重置和复用的初始状态。
    ThrowIfFailed(m_commandList->Close());

    // 创建 CPU/GPU 同步所需的对象。
    {
        // 【核心】创建 Fence，并把它的初始完成值设为 0。
        // Fence 可以看作 GPU 队列时间线上的进度数字，稍后通过 Signal 推进。
        ThrowIfFailed(m_device->CreateFence(
            0,                          // 当前还没有任何 GPU 工作完成，所以从 0 开始。
            D3D12_FENCE_FLAG_NONE,      // 【可略】不启用跨进程共享等特殊功能。
            IID_PPV_ARGS(&m_fence)));

        // 准备第一次 Signal 使用的目标值 1。
        // m_fence 的当前完成值仍是 0；这里只是设置 CPU 侧将要使用的下一个编号。
        m_fenceValue = 1;

        // 【核心】创建 Windows Event，稍后用它让 CPU 休眠等待 Fence。
        // Fence 负责表示 GPU 进度；Event 负责在目标进度到达时唤醒 CPU。
        m_fenceEvent = CreateEvent(
            nullptr,    // 【可略】使用默认安全属性，句柄不由子进程继承。
            FALSE,      // 自动重置：唤醒一个等待线程后自动回到未触发状态。
            FALSE,      // 初始为未触发状态，CPU 不会立刻从等待中通过。
            nullptr);   // 创建匿名 Event，只在当前进程中通过句柄使用。

        // CreateEvent 失败时返回 nullptr。把 Win32 错误码转换成 HRESULT 并抛出异常。
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }
}

// 每帧更新会变化的 CPU 数据。
// HelloWindow 只显示固定的清屏颜色，没有动画、相机或输入状态需要更新。
void D3D12HelloWindow::OnUpdate()
{
}

// 【核心】组织一帧的完整流程：录制命令 → 提交 → 呈现 → 等待完成。
void D3D12HelloWindow::OnRender()
{
    // 第一步：CPU 把这一帧需要的操作录制到 Command List；此时 GPU 尚未执行。
    PopulateCommandList();

    // 第二步：把已经 Close 的 Command List 提交到 GPU Command Queue。
    // ExecuteCommandLists 接收数组，所以即使只有一份列表，也先放进单元素数组。
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // 第三步：请求 Swap Chain 呈现当前 Back Buffer。
    // 第一个参数 1 表示等待一个垂直刷新间隔；第二个参数 0 表示不使用额外 Present 标志。
    ThrowIfFailed(m_swapChain->Present(1, 0));

    // 第四步：等待 GPU 完成这一帧，并更新下一帧要使用的 Back Buffer 下标。
    // 这种逐帧等待便于教学，但会限制 CPU/GPU 并行度。
    WaitForPreviousFrame();
}

void D3D12HelloWindow::OnDestroy()
{
    // 【核心】销毁前先等待 Command Queue 中此前提交的工作完成，
    // 避免 C++ 对象析构并释放 ComPtr 时，GPU 仍然引用对应的 Device、Resource 等对象。
    // 即使本例每帧末尾已经等待，这里再次确认也能守住统一的退出安全边界。
    WaitForPreviousFrame();

    // m_fenceEvent 是 CreateEvent 返回的普通 Windows HANDLE，不由 ComPtr 管理，
    // 因此必须显式关闭。其他 D3D12 COM 对象会随后由成员 ComPtr 自动 Release。
    CloseHandle(m_fenceEvent);
}

void D3D12HelloWindow::PopulateCommandList()
{
    // 【核心】重置 Allocator，回收上一轮录制命令占用的内部存储。
    // 只有确认 GPU 已经执行完依赖它的命令后才能这样做；
    // HelloWindow 在上一帧末尾调用 WaitForPreviousFrame，所以此处是安全的。
    ThrowIfFailed(m_commandAllocator->Reset());

    // 【核心】把已关闭的 Command List 重置回 Recording 状态，并重新关联 Allocator。
    // m_pipelineState 在 HelloWindow 中为空，因为本例只清屏、不执行图形 Draw。
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    // 【核心】当前 Back Buffer 原本处于 PRESENT 状态，只适合交给显示系统呈现。
    // 清屏前必须把它转换为 RENDER_TARGET 状态，表示接下来 GPU 要向它写入颜色。
    // ResourceBarrier 只是把这条转换命令录入 Command List；提交后 GPU 才真正执行。
    // Transition 的三个关键参数依次是：当前 Back Buffer、转换前状态、转换后状态。
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // 根据 m_frameIndex 计算当前 Back Buffer 对应的 RTV Handle：
    // Heap 起始位置 + 当前下标 × 单个 RTV 间距。
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);

    // 【核心】准备 RGBA 清屏颜色，各分量通常使用 0.0 到 1.0。
    // 这里得到深蓝色：R=0.0、G=0.2、B=0.4、A=1.0。
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };

    // 把“使用当前 RTV 将整个 Back Buffer 清成 clearColor”录入 Command List。
    // 0 和 nullptr 表示不限定局部矩形，而是清除整个 RTV。
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // 【核心】清屏写入完成后，把 Back Buffer 从 RENDER_TARGET 切回 PRESENT，
    // 因为 Swap Chain 的 Present 只能呈现处于可呈现状态的 Back Buffer。
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // 结束本帧命令录制。Close 后列表才能提交给 Command Queue；
    // 如果录制期间发现无效命令，Close 也可能在这里返回错误。
    ThrowIfFailed(m_commandList->Close());
}

void D3D12HelloWindow::WaitForPreviousFrame()
{
    // 【核心】HelloWindow 为了简单，每帧都等待 GPU 完成后才继续。
    // 这会限制 CPU/GPU 并行度，不是高性能程序的最佳实践；
    // D3D12HelloFrameBuffering 会展示更高效的多帧并行方式。

    // 保存本次准备等待的目标值。使用局部变量是因为 m_fenceValue 随后会递增，
    // 但本次检查和等待必须始终使用同一个目标值。
    const UINT64 fence = m_fenceValue;

    // 把 Signal 排到 Command Queue 中。GPU 执行完此前提交的命令并到达此处时，
    // 会把 m_fence 的完成值推进到 fence。
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));

    // 提前准备下一帧要使用的新编号；这只是修改 CPU 变量，不会推进真实 Fence。
    m_fenceValue++;

    // 如果 Fence 已完成值小于目标值，说明 GPU 尚未执行到本次 Signal。
    // 如果已经达到或超过目标值，则无需等待，直接继续。
    if (m_fence->GetCompletedValue() < fence)
    {
        // 注册通知：当 Fence 到达目标值时，触发 m_fenceEvent。
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));

        // 当前 CPU 线程休眠，直到 Event 被触发；避免循环查询浪费 CPU。
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Present 后 Swap Chain 已推进，重新查询下一帧应该操作的 Back Buffer。
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
