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

class DXSample;

// 对 Win32 窗口创建和消息处理的简单封装。
// 这个类本身不负责具体绘制，而是通过 DXSample 接口驱动不同的示例。
class Win32Application
{
public:
    // 创建窗口并运行示例，直到收到退出消息。
    // pSample 虽然是 DXSample*，实际可以指向 D3D12HelloWindow 等派生类对象。
    static int Run(DXSample* pSample, HINSTANCE hInstance, int nCmdShow);

    // 返回当前窗口的句柄。后面创建与窗口关联的交换链时会用到它。
    static HWND GetHwnd() { return m_hwnd; }

protected:
    // Windows 把窗口事件发送给这个回调函数，例如创建、按键、重绘和关闭事件。
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    // HWND 是由 Windows 管理的窗口句柄；nullptr 表示窗口尚未创建。
    // 它是静态成员，因为 Run、GetHwnd 和 WindowProc 都是静态函数。
    static HWND m_hwnd;
};
