// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

//#include <SDKDDKVer.h>

#if _MSC_VER>=1600
#define _CRT_SECURE_NO_WARNINGS
#else  
#endif 
//////////////////////////////////////////////////////////////////////////

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#if _MSC_VER>=1600

#ifndef WINVER
#define WINVER 0x0501
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif						

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0501
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0601
#endif
#else  

#ifndef WINVER
#define WINVER 0x0400
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif						

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0400
#endif

#endif 


#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

#include <AfxWin.h>
#include <AfxExt.h>

#ifndef _AFX_NO_OLE_SUPPORT
#include <AfxOLE.h>
#include <AfxDisp.h>
#include <AfxoDlgs.h>
#endif

#include <AfxDtctl.h>
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <AfxCmn.h>
#endif