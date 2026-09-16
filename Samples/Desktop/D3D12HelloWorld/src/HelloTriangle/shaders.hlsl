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

// HLSL 与 GLSL 的最小语法对照：
// 【核心】当前 HLSL 用“函数参数 + 返回值”表达阶段输入输出，并在冒号后用语义标明数据用途；
//          GLSL 更常用全局 in/out 变量和 layout(location = ...) 连接数据。
// 【核心】入口函数不要求命名为 main。构建命令分别选择 VSMain 和 PSMain，并指定它们属于哪个着色器阶段。
// 【可略】本文件没有写 #version；使用的 Shader Model 由 DXC 的编译目标（本例为 vs_6_0/ps_6_0）决定。

// 【核心】顶点着色器每处理一个顶点，都要产生一份供后续阶段继续使用的输出。
// 这个结构同时是 VSMain 的输出和像素着色器的输入；名字 PSInput 只是本例的命名，不是 HLSL 关键字。
// HLSL 的 struct 声明、局部变量和成员访问与 C/C++、GLSL 很接近；主要差异是成员后面可以附加语义。
struct PSInput
{
    // “类型 变量名 : 语义”是 HLSL 着色器接口的常见写法。
    // SV_ 前缀表示系统语义，由图形管线赋予特殊含义；普通 COLOR 则是用户语义，用来连接相邻阶段。
    float4 position : SV_POSITION; // 【核心】裁剪空间位置；SV_POSITION 是后续光栅化阶段要求的系统语义。

    // 【核心】同一个成员的方向取决于 PSInput 出现的位置：
    // - 当 PSInput 是 VSMain 的返回类型时，COLOR 是“顶点着色器输出语义”；
    // - 当 PSInput 是 PSMain 的参数类型时，COLOR 是“像素着色器输入语义”。
    // 两端使用同名 COLOR（省略的索引默认为 0，即 COLOR0），管线便把它们连接成一条跨阶段通道。
    float4 color : COLOR;
};

// 【核心】VSMain 是本例的顶点着色器入口。一次调用只处理一个顶点：
// 返回类型是 PSInput，函数名是 VSMain；括号内仍是普通的“类型 + 参数名”，只是每个入口参数多了语义。
// POSITION 和 COLOR 把参数连接到 C++ 输入布局中同名的元素，而不是依赖 position/color 变量名。
PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    PSInput result;
    result.position = position;

    // 【核心】右侧 color 是输入装配阶段提供给 VSMain 的当前顶点颜色；
    // 左侧 result.color 则把这个值写入顶点着色器的 COLOR0 输出通道。
    result.color = color;

    // 【可略】返回的是一份结构体；图形管线根据字段后的语义，而不是根据 C++/HLSL 变量名连接各阶段。
    return result;
}

// 【核心】光栅化阶段把三角形覆盖区域转换成许多候选像素，并为每个候选像素调用像素着色器。
// input.color 不是固定取某个顶点的颜色，而是由三个顶点的 COLOR 在当前位置插值得到。
// HLSL 允许直接给整个函数返回值标注语义；这里的 SV_TARGET 相当于 GLSL 片元着色器的颜色 out 变量。
// 因而函数声明可读成：“PSMain 接收一份 PSInput，返回一个写入颜色渲染目标的 float4”。
float4 PSMain(PSInput input) : SV_TARGET
{
    // 【核心】本例没有纹理、光照或其他颜色计算，直接把插值后的 RGBA 颜色作为结果返回。
    // 因此三个角分别保持红、绿、蓝，三角形内部则出现平滑的颜色过渡。
    // 此处 input.color 是像素着色器的 COLOR0 输入：它接收的已不是某个角的原值，而是光栅化后的插值结果。
    return input.color;
}
