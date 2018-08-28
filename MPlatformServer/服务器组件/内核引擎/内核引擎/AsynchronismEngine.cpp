#include "StdAfx.h"
#include "AsynchronismEngine.h"
#include "TraceServiceManager.h"

//////////////////////////////////////////////////////////////////////////
//�����߳�

//���캯��
CAsynchronismThread::CAsynchronismThread()
{
	m_hCompletionPort = 0;
	m_pIAsynchronismEngineSink = NULL;
	//��������
	ZeroMemory(m_cbBuffer, sizeof(m_cbBuffer));

	return;
}
//��������
CAsynchronismThread::~CAsynchronismThread()
{

}
//���ú���
VOID CAsynchronismThread::SetCompletionPort(HANDLE hCompletionPort)
{
	//У�����
	ASSERT(hCompletionPort != NULL);
	//���ñ���
	m_hCompletionPort = hCompletionPort;

	return;
}
//���ú���
VOID CAsynchronismThread::SetAsynchronismEngineSink(IAsynchronismEngineSink * pIAsynchronismEngineSink)
{
	//У�����
	ASSERT(pIAsynchronismEngineSink != NULL);
	//���ñ���
	m_pIAsynchronismEngineSink = pIAsynchronismEngineSink;

	return;
}
//�����¼�
bool CAsynchronismThread::OnEventThreadRun()
{
	//У�����
	ASSERT(m_hCompletionPort != NULL);
	ASSERT(m_pIAsynchronismEngineSink != NULL);

	//��������
	DWORD dwThancferred = 0L;
	OVERLAPPED * pOverLapped = NULL;
	CAsynchronismEngine * pAsynchronismEngine = NULL;

	//��ɶ˿�
	if (GetQueuedCompletionStatus(m_hCompletionPort, &dwThancferred, (PULONG_PTR)&pAsynchronismEngine, &pOverLapped, INFINITE))
	{
		//�˳��ж�
		if (pAsynchronismEngine == NULL) return false;

		//��������
		CWHDataLocker ThreadLock(pAsynchronismEngine->m_CriticalSection);

		//��ȡ����
		tagDataHead DataHead;
		pAsynchronismEngine->m_DataQueue.DistillData(DataHead, m_cbBuffer, sizeof(m_cbBuffer));

		//���н���
		ThreadLock.UnLock();

		//���ݴ���
		try
		{
			m_pIAsynchronismEngineSink->OnAsynchronismEngineData(DataHead.wIdentifier, m_cbBuffer, DataHead.wDataSzie);
		}
		catch (...)
		{
			//������Ϣ
			TCHAR szDescribe[256] = TEXT("");
			_sntprintf(szDescribe, CountArray(szDescribe), TEXT("CAsynchronismEngine::OnAsynchronismEngineData [ wIdentifier=%d wDataSize=%ld ]"),
				DataHead.wIdentifier, DataHead.wDataSzie);

			//�����Ϣ
			g_TraceServiceManager.TraceString(szDescribe, TraceLevel_Exception);
		}

		return true;
	}

	return false;
}
//��ʼ�¼�
bool CAsynchronismThread::OnEventThreadStrat()
{
	//�¼�֪ͨ
	ASSERT(m_pIAsynchronismEngineSink != NULL);
	bool bSuccess = m_pIAsynchronismEngineSink->OnAsynchronismEngineStart();

	//���ñ���
	CAsynchronismEngine * pAsynchronismEngine = CONTAINING_RECORD(this, CAsynchronismEngine, m_AsynchronismThread);
	pAsynchronismEngine->m_bService = true;

	return bSuccess;
}
//��ֹ�¼�
bool CAsynchronismThread::OnEventThreadConclude()
{
	//���ñ���
	CAsynchronismEngine * pAsynchronismEngine = CONTAINING_RECORD(this, CAsynchronismEngine, m_AsynchronismThread);
	pAsynchronismEngine->m_bService = false;

	//�¼�֪ͨ
	ASSERT(m_pIAsynchronismEngineSink != NULL);
	bool bSuccess = m_pIAsynchronismEngineSink->OnAsynchronismEngineConclude();

	return bSuccess;
}



//////////////////////////////////////////////////////////////////////////

//�첽����

//���캯��
CAsynchronismEngine::CAsynchronismEngine()
{
	//���ñ���
	m_bService = false;
	m_hCompletionPort = NULL;
	m_pIAsynchronismEngineSink = NULL;

	return;
}
//��������
CAsynchronismEngine::~CAsynchronismEngine()
{
	//��ֹ����
	ConcludeService();

	return;
}
//�ӿڲ�ѯ
VOID * CAsynchronismEngine::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(IServiceModule, Guid, dwQueryVer);
	QUERYINTERFACE(IAsynchronismEngine, Guid, dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(IAsynchronismEngine, Guid, dwQueryVer);

	return NULL;
}
//��������
bool CAsynchronismEngine::StartService()
{
	//�����ж�
	ASSERT(m_bService == false);
	if (m_bService != false) return false;

	//��ɶ˿�
	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
	if (m_hCompletionPort == NULL) return false;

	//���ö���
	m_AsynchronismThread.SetCompletionPort(m_hCompletionPort);
	m_AsynchronismThread.SetAsynchronismEngineSink(m_pIAsynchronismEngineSink);

	//�����߳�
	if (m_AsynchronismThread.StartThread() == false)
	{
		ASSERT(FALSE);
		return false;
	}

	return true;
}
//ֹͣ����
bool CAsynchronismEngine::ConcludeService()
{
	//���ñ���
	m_bService = false;

	//Ͷ��֪ͨ
	if (m_hCompletionPort != NULL)
	{
		PostQueuedCompletionStatus(m_hCompletionPort, 0, NULL, NULL);
	}

	//ֹͣ�߳�
	m_AsynchronismThread.ConcludeThread(INFINITE);

	//�رն���
	if (m_hCompletionPort != NULL) CloseHandle(m_hCompletionPort);

	//���ñ���
	m_hCompletionPort = NULL;
	m_pIAsynchronismEngineSink = NULL;

	//���ö���
	m_AsynchronismThread.SetCompletionPort(NULL);
	m_AsynchronismThread.SetAsynchronismEngineSink(NULL);

	//��������
	m_DataQueue.RemoveData(false);

	return true;
}
//���и���
bool CAsynchronismEngine::GetBurthenInfo(tagBurthenInfo & BurthenInfo)
{
	CWHDataLocker lock(m_CriticalSection);
	m_DataQueue.GetBurthenInfo(BurthenInfo);

	return true;
}
//����ģ��
bool CAsynchronismEngine::SetAsynchronismSink(IUnknownEx * pIUnknownEx)
{
	//�����ж�
	ASSERT(m_bService == false);
	if (m_bService == true) return false;

	//��ѯ�ӿ�
	m_pIAsynchronismEngineSink = QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx, IAsynchronismEngineSink);

	//�����ж�
	if (m_pIAsynchronismEngineSink == NULL)
	{
		ASSERT(FALSE);
		return false;
	}

	return true;
}
//Ͷ������
bool CAsynchronismEngine::PostAsynchronismData(WORD wIdentifier, VOID * pData, WORD wDataSize)
{
	//�����ж�
	ASSERT(m_bService == true && m_hCompletionPort != NULL);
	if (m_bService == false || m_hCompletionPort == NULL) return false;

	//��������
	CWHDataLocker ThreadLock(m_CriticalSection);
	if (m_DataQueue.InsertData(wIdentifier, pData, wDataSize) == false)
	{
		ASSERT(FALSE);
		return false;
	}

	//Ͷ��֪ͨ
	PostQueuedCompletionStatus(m_hCompletionPort, wDataSize, (ULONG_PTR)this, NULL);

	return true;
}
//Ͷ������
bool CAsynchronismEngine::PostAsynchronismData(WORD wIdentifier, tagDataBuffer DataBuffer[], WORD wDataCount)
{
	//�����ж�
	ASSERT(wDataCount > 0);
	if (wDataCount == 0) return false;

	//��������
	CWHDataLocker ThreadLock(m_CriticalSection);
	if (m_DataQueue.InsertData(wIdentifier, DataBuffer, wDataCount) == false)
	{
		ASSERT(FALSE);
		return false;
	}

	//Ͷ��֪ͨ
	for (WORD i = 0; i < wDataCount; ++i)
	{
		PostQueuedCompletionStatus(m_hCompletionPort, 0, (ULONG_PTR)this, NULL);
	}
	
	return true;
}


//////////////////////////////////////////////////////////////////////////

//�����������
DECLARE_CREATE_MODULE(AsynchronismEngine);

//////////////////////////////////////////////////////////////////////////