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

// ComPtr 自动管理 CPU 侧 COM 引用并在对象析构时调用 Release，
// 但它不知道 GPU 是否仍在使用这些对象。释放前仍需通过 Fence 确认 GPU 已完成，
// 这一生命周期边界由 OnDestroy() 中的等待保证。
using Microsoft::WRL::ComPtr;

// HelloWindow 的具体示例类。
// Win32Application 只依赖 DXSample 接口，真正的 DirectX 初始化和绘制由这个派生类完成。
class D3D12HelloWindow : public DXSample
{
public:
    // 构造函数目前只接收并保存窗口的基本信息，不在这里创建 DirectX 对象。
    D3D12HelloWindow(UINT width, UINT height, std::wstring name);

    // 窗口创建完成后由 Win32Application 调用；DirectX 初始化从这里正式开始。
    virtual void OnInit();

    // 每帧更新 CPU 侧状态；HelloWindow 没有动画，因此实现为空。
    virtual void OnUpdate();

    // 组织一帧的命令录制、提交、呈现和同步。
    virtual void OnRender();

    // 程序退出时等待 GPU 完成并释放非 COM 的 Windows Event 句柄。
    virtual void OnDestroy();

private:
    // 【核心】交换链包含两张轮流使用的窗口图像，也就是双缓冲。
    static const UINT FrameCount = 2;

    // Pipeline objects.

    // 管理窗口显示所用的多张 Back Buffer；具体创建过程在 LoadPipeline 中。
    ComPtr<IDXGISwapChain3> m_swapChain;

    // 【核心】程序的 D3D12 Device。它代表程序与所选 Adapter 建立的 D3D12 连接，
    // 后续的命令队列、资源和描述符堆等对象都通过它创建。
    ComPtr<ID3D12Device> m_device;

    // Swap Chain 中两张 Back Buffer 的实际图像资源，保存真正的像素数据。
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];

    // 【核心】为 Command List 录制命令提供内部存储。
    // 它不负责分配纹理、Back Buffer 或普通 GPU 显存。
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;

    // 【核心】GPU 命令队列。CPU 把录制完成的命令列表提交到这里，
    // GPU 再按照队列中的顺序执行这些命令。
    ComPtr<ID3D12CommandQueue> m_commandQueue;

    // 保存两份 RTV 描述符。RTV 只是说明如何把 Back Buffer 当作渲染目标使用，
    // Descriptor Heap 不保存 Back Buffer 的像素数据。
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    // 【核心】CPU 用它录制一组准备交给 GPU 的命令；录制本身不会立即执行命令。
    // 它使用 m_commandAllocator 提供的内部存储，完成后提交到 m_commandQueue。
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // 一个 RTV 描述符在当前设备的 Heap 中占用的间距。
    // 稍后从第 0 个 RTV 移动到第 1 个 RTV 时用它计算下一个位置。
    UINT m_rtvDescriptorSize;

    // Synchronization objects.

    // 当前应当渲染哪一张 Back Buffer 的下标；双缓冲时通常为 0 或 1。
    UINT m_frameIndex;

    // Windows Event 句柄。Fence 到达目标值时触发它，从而唤醒等待中的 CPU 线程。
    HANDLE m_fenceEvent;

    // 【核心】表示 GPU 队列执行进度的 Fence。CPU 可以读取它当前完成到哪个数值。
    ComPtr<ID3D12Fence> m_fence;

    // 下一次准备放到 GPU 队列时间线上的 Fence 数值；由程序自行递增管理。
    UINT64 m_fenceValue;

    // 创建从程序连接到窗口所需的基础 DirectX 环境。
    void LoadPipeline();

    // 创建当前示例运行时需要的其余对象。
    // HelloWindow 没有模型或纹理，因此这里的 “Assets” 是沿用示例框架的统一命名。
    void LoadAssets();

    // 每帧把资源状态转换和清屏操作录制进 Command List。
    void PopulateCommandList();

    // 在 Queue 上放置 Fence 进度标记，必要时让 CPU 等待 GPU，
    // 最后取得下一帧应使用的 Back Buffer 下标。
    void WaitForPreviousFrame();
};
