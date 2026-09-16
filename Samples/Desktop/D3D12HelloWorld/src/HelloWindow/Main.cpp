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

// 本项目使用预编译头。stdafx.h 必须是第一个被包含的头文件，
// 否则 Visual Studio 可能无法正确使用项目配置好的预编译头。
#include "stdafx.h"

// 声明具体的示例类 D3D12HelloWindow。
#include "D3D12HelloWindow.h"


// WinMain 是 Windows 图形界面程序的入口，可以暂时把它理解成普通程序的 main。
// _Use_decl_annotations_ 只用于辅助 Visual Studio 做静态代码分析，不改变运行逻辑。
// hInstance 表示当前程序实例；nCmdShow 告诉 ShowWindow 应当怎样显示窗口。
// 中间两个没有变量名的参数是本示例用不到的 WinMain 参数。
_Use_decl_annotations_ int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR,
                                          int nCmdShow)
{
  // 在当前函数的栈上创建示例对象。
  // 三个参数依次表示客户区宽度、客户区高度和窗口标题。
  // 此时只是构造一个普通 C++ 对象，还没有创建窗口，也没有开始绘制。
  D3D12HelloWindow sample(1280, 720, L"D3D12 Hello Window");

  // 把 sample 的地址交给通用 Win32 窗口框架。
  // Run 会创建窗口、初始化 sample，并一直运行到窗口关闭；它的返回值就是程序退出码。
  // Run 返回后，局部对象 sample 随 WinMain 结束而析构，其 ComPtr 成员会自动释放。
  return Win32Application::Run(&sample, hInstance, nCmdShow);
}
