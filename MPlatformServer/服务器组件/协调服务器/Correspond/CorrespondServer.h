
// Correspond.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CCorrespondServerApp: 
// �йش����ʵ�֣������ Correspond.cpp
//

class CCorrespondServerApp : public CWinApp
{
public:
	CCorrespondServerApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CCorrespondServerApp theApp;