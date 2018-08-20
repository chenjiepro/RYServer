// ServiceCoreDll.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include "AfxDllx.h"
#include "ServiceCoreDll.h"
//��̬����
static AFX_EXTENSION_MODULE ServiceCoreDLL = { NULL, NULL };

extern "C" BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		if (!AfxInitExtensionModule(ServiceCoreDLL, hModule))
			return FALSE;
		new CDynLinkLibrary(ServiceCoreDLL);
	}
	case DLL_PROCESS_DETACH:
	{
		AfxTermExtensionModule(ServiceCoreDLL);
	}
	}
	return TRUE;
}
