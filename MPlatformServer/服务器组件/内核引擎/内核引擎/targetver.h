#pragma once

// ���� SDKDDKVer.h ��������õ���߰汾�� Windows ƽ̨��

// ���ҪΪ��ǰ�� Windows ƽ̨����Ӧ�ó�������� WinSDKVer.h������
// WIN32_WINNT ������ΪҪ֧�ֵ�ƽ̨��Ȼ���ٰ��� SDKDDKVer.h��

//MFC �ļ�
#include <Nb30.h>
#include <Afxmt.h>
#include <AtlBase.h>

//ϵͳ�ļ�
#include "..\..\..\ȫ�ֶ���\Platform.h"

//////////////////////////////////////////////////////////////////////////////////
//��������

//��������
#ifndef SERVICE_CORE_CLASS
#ifdef  SERVICE_CORE_DLL
#define SERVICE_CORE_CLASS _declspec(dllexport)
#else
#define SERVICE_CORE_CLASS _declspec(dllimport)
#endif
#endif

//ģ�鶨��
#ifndef _DEBUG
#define SERVICE_CORE_DLL_NAME	TEXT("ServiceCore.dll")			//�������
#else
#define SERVICE_CORE_DLL_NAME	TEXT("ServiceCoreD.dll")		//�������
#endif

//////////////////////////////////////////////////////////////////////////////////
//�����ļ�

#ifndef SERVICE_CORE_DLL
#include "WHThread.h"
#include "WHRegKey.h"
#include "WHEncrypt.h"
#include "WHService.h"
#include "WHIniData.h"
#include "WHDataQueue.h"
#include "WHDataLocker.h"
#include "WHCommandLine.h"
#include "WHMD5CheckSum.h"
#endif