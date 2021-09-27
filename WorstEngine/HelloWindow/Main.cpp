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
#include "D3DApp.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    int line;
    auto filename=CommandLineToArgvW(GetCommandLineW(),&line);
    /*0---exe path
    1---firtst parameter */

    //for(int i=0;i<line;i++)
    //MessageBox(0,filename[1], L"2333", 0);
    D3DApp sample(1280, 720, L"D3D12 Hello Window");
    sample.SetcommandLines(filename, line);
    return Win32Application::Run(&sample, hInstance, nCmdShow);
   
    
}
