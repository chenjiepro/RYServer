#include "stdafx.h"
#include "WHThread.h"

//////////////////////////////////////////////////////////////////////////////////
//�ṹ����

//��������
struct tagThreadParameter
{
	bool					bSuccess;					//�Ƿ����
	HANDLE					hEventFinish;				//�¼����
	CWHThread	*			pServiceThread;				//�߳�ָ��
};

//////////////////////////////////////////////////////////////////////////////////

//���캯��
CWHThread::CWHThread()
{
	//���ñ���
	m_bRun = false;
	m_uThreadID = 0;
	m_hThreadHandle = NULL;
	return;
}

//��������
CWHThread::~CWHThread()
{
	ConcludeThread(INFINITE);
	return;
}

//��ȡ״̬
bool CWHThread::isRuning()
{
	//���м��
	if (m_hThreadHandle == NULL) return false;

	if (WaitForSingleObject(m_hThreadHandle, 0) != WAIT_TIMEOUT) return false;

	return true;
}
//�����߳�
bool CWHThread::StartThread()
{
	//У��״̬
	ASSERT(isRuning() == false);
	if (isRuning() == true) return false;

	//�������
	if (m_hThreadHandle != NULL)
	{
		CloseHandle(m_hThreadHandle);

		m_uThreadID = 0;
		m_hThreadHandle = NULL;
	}

	//��������
	tagThreadParameter ThreadParameter;
	ZeroMemory(&ThreadParameter, sizeof(ThreadParameter));

	//���ñ���
	ThreadParameter.bSuccess = false;
	ThreadParameter.pServiceThread = this;
	ThreadParameter.hEventFinish = CreateEvent(NULL, FALSE, FALSE, NULL);

	//У��״̬
	ASSERT(ThreadParameter.hEventFinish != NULL);
	if (ThreadParameter.hEventFinish == NULL) return false;

	//�����߳�
	m_bRun = true;
	m_hThreadHandle = (HANDLE)::_beginthreadex(NULL, 0, ThreadFunction, &ThreadParameter, 0, &m_uThreadID);

	//�����ж�
	if (m_hThreadHandle == INVALID_HANDLE_VALUE)
	{
		CloseHandle(ThreadParameter.hEventFinish);
		return false;
	}

	//�ȴ��¼�
	WaitForSingleObject(ThreadParameter.hEventFinish, INFINITE);
	CloseHandle(ThreadParameter.hEventFinish);
	//�����ｫbSuccess��Ϊtrue�ģ�
	if (ThreadParameter.bSuccess == false)
	{
		ConcludeThread(INFINITE);
		return false;
	}

	return true;
}
//�����߳�
bool CWHThread::ConcludeThread(DWORD dwMillSeconds)
{
	//ֹͣ�߳�
	if (isRuning() == true)
	{
		//���ñ���
		m_bRun = false;

		//ֹͣ�ȴ�
		if (WaitForSingleObject(m_hThreadHandle, dwMillSeconds) == WAIT_TIMEOUT)
		{
			return false;
		}
	}

	//���ñ���
	if (m_hThreadHandle != NULL)
	{
		//�رվ��
		CloseHandle(m_hThreadHandle);

		m_uThreadID = 0;
		m_hThreadHandle = NULL;
	}

	return true;
}

//Ͷ����Ϣ
LRESULT CWHThread::PostThreadMessage(UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	//״̬У��
	ASSERT(m_uThreadID != 0 && m_hThreadHandle != NULL);
	if (m_uThreadID == 0 || m_hThreadHandle == NULL) return false;

	if (::PostThreadMessage(m_uThreadID, uMessage, wParam, lParam) == FALSE)
	{
		DWORD dwLastError = GetLastError();
		return false;
	}

	return 0L;
}

//�̺߳���
unsigned __stdcall CWHThread::ThreadFunction(LPVOID pThreadData)
{
	//�������
	srand((DWORD)time(NULL));

	//��������
	tagThreadParameter *pThreadParameter = (tagThreadParameter*)pThreadData;
	//ͨ��ָ����÷Ǿ�̬��Ա
	CWHThread * pServiceThread = pThreadParameter->pServiceThread;

	//����֪ͨ
	try
	{
		pThreadParameter->bSuccess = pServiceThread->OnEventThreadStart();
	}
	catch (...)
	{
		//���ñ���
		ASSERT(FALSE);
		pThreadParameter->bSuccess = false;
	}

	//�����¼�
	bool bSuccess = pThreadParameter->bSuccess;
	ASSERT(pThreadParameter->hEventFinish != NULL);
	if (pThreadParameter->hEventFinish != NULL) SetEvent(pThreadParameter->hEventFinish);

	//�̴߳���
	if (bSuccess)
	{
		//�߳�����
		while (pServiceThread->m_bRun)
		{
#ifndef _DEBUG
			//���а汾
			try
			{
				if (pServiceThread->OnEventThreadRun() == false)
					break;
			}
			catch (...)
			{

			}
#else
			//���԰汾
			if (pServiceThread->OnEventThreadRun() == false)
				break;
#endif
		}

		//ֹ֪ͣͨ
		try
		{
			pServiceThread->OnEventThreadConclude();
		}
		catch (...)
		{
			ASSERT(FALSE);
		}
	}

	//��ֹ�߳�
	_endthreadex(0L);

	return 0L;
}