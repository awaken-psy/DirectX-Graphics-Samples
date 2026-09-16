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

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class D3D12HelloTriangle : public DXSample
{
public:
    D3D12HelloTriangle(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();

private:
    static const UINT FrameCount = 2;

    // 【核心】“顶点”不是屏幕上的一个像素，而是送入图形管线的一份输入数据。
    // 一个三角形至少需要 3 份 Vertex 数据；这里约定每份数据同时携带位置和颜色。
    // 有了统一的结构，CPU 才能按相同顺序排好数据，后续管线也才能知道每段数据代表什么。
    struct Vertex
    {
        XMFLOAT3 position; // 【核心】顶点的位置：x、y、z 三个浮点数。
        XMFLOAT4 color;    // 【核心】顶点的颜色：红、绿、蓝、透明度四个浮点数。
                          // 【可略】XMFLOAT3/4 只是便于 CPU 保存这些数值的内存结构。
    };

    // Pipeline objects.
    // 【核心】Viewport 把顶点着色器输出经过透视除法后的坐标映射到渲染目标中的像素区域；
    // 本例从窗口左上角 (0, 0) 开始，覆盖整个 width × height 客户区。
    CD3DX12_VIEWPORT m_viewport;

    // 【核心】Scissor Rect 是像素级裁剪边界：光栅化产生的候选像素只有落在矩形内才允许继续；
    // 本例同样覆盖整个窗口，因此正常情况下不会额外裁掉三角形。
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;

    // 【核心】根签名规定“着色器可以从命令列表接收哪些外部绑定资源”，是应用与着色器之间的绑定契约。
    // HelloTriangle 不读取常量缓冲区、纹理或采样器，因此它持有的是一个真实存在但没有根参数的空根签名。
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;

    // 【核心】PSO（图形管线状态对象）把一次绘制所需且彼此必须兼容的固定配置组合在一起：
    // 输入布局、根签名、顶点/像素着色器、光栅化、混合、深度模板以及渲染目标格式等。
    // PSO 创建后不能逐字段修改；需要另一套配置时应创建并绑定另一个 PSO。
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;

    // App resources.
    // 【核心】保存三角形顶点字节的 D3D12 缓冲区资源。
    // 普通 C++ 数组只属于 CPU；这个长期存在的资源才是后续 GPU 绘制时读取顶点数据的地方。
    ComPtr<ID3D12Resource> m_vertexBuffer;

    // 【核心】顶点缓冲区视图不保存或拥有顶点数据；它只描述 m_vertexBuffer 中哪段字节应当怎样按“顶点”分组读取。
    // 这是一个普通的小型结构体，不需要通过 Device 创建。录制绘制命令时会把它交给输入装配阶段。
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();
};
