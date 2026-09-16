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
#include "Win32Application.h"

// 为头文件中声明的静态成员分配存储空间。
// CreateWindow 成功返回前，这个句柄保持为空。
HWND Win32Application::m_hwnd = nullptr;

// pSample 指向 Main.cpp 中创建的 D3D12HelloWindow 对象。
// hInstance 标识当前程序实例，nCmdShow 指定窗口最初的显示方式。
int Win32Application::Run(DXSample* pSample, HINSTANCE hInstance, int nCmdShow)
{
    // 把 Windows 提供的完整命令行拆分成 argc/argv 形式，交给示例处理。
    // HelloWindow 支持通过 -warp 或 /warp 参数选择软件渲染设备。
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    pSample->ParseCommandLineArgs(argv, argc);

    // CommandLineToArgvW 返回的数组由 Windows 分配，使用完必须用 LocalFree 释放。
    LocalFree(argv);

    // 第一步：描述并注册一种窗口类型。
    // WNDCLASSEX 是交给 Windows 的配置结构体，{} 会先把所有字段清零。
    WNDCLASSEX windowClass = { 0 };

    // 告诉 Windows 当前传入的结构体大小，便于系统识别结构体版本。
    windowClass.cbSize = sizeof(WNDCLASSEX);

    // 当窗口宽度或高度变化时，请求重新绘制窗口内容。
    windowClass.style = CS_HREDRAW | CS_VREDRAW;

    // 指定窗口消息回调。Windows 收到创建、按键、重绘等消息时会调用 WindowProc。
    windowClass.lpfnWndProc = WindowProc;

    // 说明这个窗口类型属于当前可执行程序实例。
    windowClass.hInstance = hInstance;

    // 使用系统提供的标准箭头鼠标指针。
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);

    // 给这种窗口类型起一个名字；稍后 CreateWindow 会使用同一个名字。
    windowClass.lpszClassName = L"DXSampleClass";

    // 这里只是向 Windows 注册窗口类型，还没有创建具体窗口。
    RegisterClassEx(&windowClass);

    // pSample 中保存的 1280×720 表示窗口客户区，也就是可绘制的内部区域。
    RECT windowRect = { 0, 0, static_cast<LONG>(pSample->GetWidth()), static_cast<LONG>(pSample->GetHeight()) };

    // 窗口外部还有标题栏和边框。根据窗口样式把外部尺寸扩大，
    // 从而保证最终客户区仍然是示例要求的 1280×720。
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // 第二步：按照刚才注册的窗口类型创建一个具体窗口，并保存它的 HWND 句柄。
    m_hwnd = CreateWindow(
        windowClass.lpszClassName,                   // 使用上面注册的窗口类型。
        pSample->GetTitle(),                         // 窗口标题：D3D12 Hello Window。
        WS_OVERLAPPEDWINDOW,                         // 普通桌面窗口，带标题栏、边框和系统按钮。
        CW_USEDEFAULT,                               // 让 Windows 选择窗口左上角的 x 坐标。
        CW_USEDEFAULT,                               // 让 Windows 选择窗口左上角的 y 坐标。
        windowRect.right - windowRect.left,          // 包含边框和标题栏的窗口总宽度。
        windowRect.bottom - windowRect.top,          // 包含边框和标题栏的窗口总高度。
        nullptr,                                     // 没有父窗口，它是一个顶层窗口。
        nullptr,                                     // 本示例没有使用窗口菜单。
        hInstance,                                   // 当前程序实例。
        pSample);                                    // 随窗口创建消息传递 sample 指针。

    // 第三步：窗口已经创建并且 m_hwnd 已经有效，现在初始化具体示例。
    // pSample 实际指向 D3D12HelloWindow，因此这里会调用 D3D12HelloWindow::OnInit()。
    // 把初始化放在窗口创建之后，是因为后面的交换链需要关联这个 HWND。
    pSample->OnInit();

    // CreateWindow 创建了窗口对象，但窗口最初尚未显示；这一步才让它出现在桌面上。
    ShowWindow(m_hwnd, nCmdShow);

    // Main sample loop.
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // Process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    pSample->OnDestroy();

    // Return this part of the WM_QUIT message to Windows.
    return static_cast<char>(msg.wParam);
}

// Main message handler for the sample.
LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    DXSample* pSample = reinterpret_cast<DXSample*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
        {
            // Save the DXSample* passed in to CreateWindow.
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        }
        return 0;

    case WM_KEYDOWN:
        if (pSample)
        {
            pSample->OnKeyDown(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_KEYUP:
        if (pSample)
        {
            pSample->OnKeyUp(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_PAINT:
        if (pSample)
        {
            pSample->OnUpdate();
            pSample->OnRender();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}
