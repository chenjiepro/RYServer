#include "StdAfx.h"
#include "TimerEngine.h"
#include "TraceServiceManager.h"

//////////////////////////////////////////////////////////////////////////
//�궨��
#define NO_TIME_LEAVE				DWORD(-1)							//����Ӧʱ��


	//���캯��
CTimerThread::CTimerThread()
{
	m_dwTimerSpace = 30;
	m_dwLastTickCount = 0;

	m_pTimerEngine = NULL;

	return;
}
	//��������
CTimerThread::~CTimerThread()
{

}
	//���ú���
bool CTimerThread::InitThread(CTimerEngine * pTimerEngine, DWORD dwTimerSpace)
{
	//У�����
	ASSERT(dwTimerSpace >= 10L);
	ASSERT(pTimerEngine != NULL);
	if (pTimerEngine == NULL) return false;

	//���ñ���
	m_dwLastTickCount = 0;
	m_dwTimerSpace = dwTimerSpace;
	m_pTimerEngine = pTimerEngine;

	return true;
}
	//���к���
bool CTimerThread::OnEventThreadRun()
{
	//У�����
	ASSERT(m_pTimerEngine != NULL);

	//��ȡʱ��
	DWORD dwTimerSpace = m_dwTimerSpace;
	DWORD dwNowTickCount = GetTickCount();

	//�ȴ�����
	if (m_dwLastTickCount != 0 && dwNowTickCount > m_dwLastTickCount)
	{
		DWORD dwHandleTickCount = dwNowTickCount - m_dwLastTickCount;
		dwTimerSpace = (dwTimerSpace > dwHandleTickCount) ? dwTimerSpace - dwHandleTickCount : 0;
	}

	//��ʱ����
	Sleep(dwTimerSpace);
	m_dwLastTickCount = GetTickCount();

	//ʱ�䴦��
	m_pTimerEngine->OnTimerThreadSink();

	return true;
}


//���캯��
CTimerEngine::CTimerEngine()
{
	m_bService = false;
	m_dwTimePass = 0;
	m_dwTimeLeave = NO_TIME_LEAVE;

	m_dwTimeSpace = 30L;
	m_pITimerEngineEvent = NULL;

	return;
}
//��������
CTimerEngine::~CTimerEngine()
{
	//ֹͣ����
	ConcludeService();

	//�����ڴ�
	tagTimerItem * pTimerItem = NULL;

	for (INT_PTR i = 0; i < m_TimerItemFree.GetCount(); ++i)
	{
		pTimerItem = m_TimerItemFree[i];
		ASSERT(pTimerItem != NULL);
		SafeDelete(pTimerItem);
	}

	for (INT_PTR i = 0; i < m_TimerItemActive.GetCount(); ++i)
	{
		pTimerItem = m_TimerItemActive[i];
		ASSERT(pTimerItem != NULL);
		SafeDelete(pTimerItem);
	}

	m_TimerItemFree.RemoveAll();
	m_TimerItemActive.RemoveAll();

	return;
}
//�ӿڲ�ѯ
VOID * CTimerEngine::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(IServiceModule, Guid, dwQueryVer);
	QUERYINTERFACE(ITimerEngine, Guid, dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(IUnknownEx, Guid, dwQueryVer);
	return NULL;
}
//��������
bool CTimerEngine::StartService()
{
	//״̬У��
	ASSERT(m_bService == false);
	if (m_bService == true) return false;

	//���ñ���
	m_dwTimePass = 0;
	m_dwTimeLeave = NO_TIME_LEAVE;
	if (m_TimerThread.InitThread(this, m_dwTimeSpace) == false) return false;

	//��������
	if (m_TimerThread.StartThread() == false) return false;

	//���ñ���
	m_bService = true;

	return true;
}
//ֹͣ����
bool CTimerEngine::ConcludeService()
{
	//���ñ���
	m_bService = false;

	//ֹͣ�߳�
	m_TimerThread.ConcludeThread(INFINITE);

	//ɾ����ʱ��
	KillAllTimer();

	return true;
}
//���ýӿ�
bool CTimerEngine::SetTimerEngineEvent(IUnknownEx * pIUnknownEx)
{
	//У�����
	ASSERT(m_bService == false);
	if (m_bService == true) return false;

	//���ýӿ�
	if (pIUnknownEx != NULL)
	{
		//��ѯ�ӿ�
		ASSERT(QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx, ITimerEngineEvent) != NULL);
		m_pITimerEngineEvent = QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx, ITimerEngineEvent);

		//�ɹ��ж�
		if (m_pITimerEngineEvent == NULL) return false;
	}
	else
		m_pITimerEngineEvent = NULL;

	return true;
}
//���ö�ʱ��
bool CTimerEngine::SetTimer(DWORD dwTimerID, DWORD dwElapse, DWORD dwRepeat, WPARAM dwBindParameter)
{
	//������Դ
	CWHDataLocker ThreadLock(m_CriticalSection);
	
	//У�����
	ASSERT(dwRepeat > 0);
	if (dwRepeat == 0) return false;
	dwElapse = (dwElapse + m_dwTimeSpace - 1) / m_dwTimeSpace * m_dwTimeSpace;

	//���Ҷ�ʱ��
	bool bTimerExist = false;
	tagTimerItem * pTimerItem = NULL;
	for (INT_PTR i = 0; i < m_TimerItemActive.GetCount(); ++i)
	{
		pTimerItem = m_TimerItemActive[i];
		if (pTimerItem->dwTimerID = dwTimerID)
		{
			bTimerExist = true;
			break;
		}
	}

	//������ʱ��
	if (bTimerExist == false)
	{
		INT_PTR nFreeCount = m_TimerItemFree.GetCount();
		if (nFreeCount > 0)
		{
			pTimerItem = m_TimerItemFree[nFreeCount - 1];
			ASSERT(pTimerItem !=NULL);
			m_TimerItemFree.RemoveAt(nFreeCount - 1);
		}
		else
		{
			try
			{
				pTimerItem = new tagTimerItem;
				ASSERT(pTimerItem == NULL);
				if (pTimerItem == NULL) return false;
			}
			catch (...)
			{
				return false;
			}
		}
	}

	//���ò���
	ASSERT(pTimerItem == NULL);
	pTimerItem->dwTimerID = dwTimerID;
	pTimerItem->dwElapse = dwElapse;
	pTimerItem->dwRepeatTimes = dwRepeat;
	pTimerItem->dwBindParameter = dwBindParameter;
	pTimerItem->dwTimeLeave = dwElapse + m_dwTimePass;

	//���ʱ��
	m_dwTimeLeave = __min(m_dwTimeLeave, dwElapse);
	if (bTimerExist == false) m_TimerItemActive.Add(pTimerItem);

	return true;
}
//���ö�ʱ��
bool CTimerEngine::SetTimerCell(DWORD dwTimerID)
{
	//������Դ
	CWHDataLocker ThreadLock(m_CriticalSection);

	DWORD dwElapse = 1; DWORD dwRepeat = 1; WPARAM dwBindParameter = 0;

	dwElapse = (dwElapse + m_dwTimeSpace - 1) / m_dwTimeSpace * m_dwTimeSpace;

	//���Ҷ�ʱ��
	bool bTimerExist = false;
	tagTimerItem * pTimerItem = NULL;
	for (INT_PTR i = 0; i < m_TimerItemActive.GetCount(); ++i)
	{
		pTimerItem = m_TimerItemActive[i];
		if (pTimerItem->dwTimerID = dwTimerID)
		{
			bTimerExist = true;
			break;
		}
	}

	//������ʱ��
	if (bTimerExist == false)
	{
		INT_PTR nFreeCount = m_TimerItemFree.GetCount();
		if (nFreeCount > 0)
		{
			pTimerItem = m_TimerItemFree[nFreeCount - 1];
			ASSERT(pTimerItem != NULL);
			m_TimerItemFree.RemoveAt(nFreeCount - 1);
		}
		else
		{
			try
			{
				pTimerItem = new tagTimerItem;
				ASSERT(pTimerItem == NULL);
				if (pTimerItem == NULL) return false;
			}
			catch (...)
			{
				return false;
			}
		}
	}

	//���ò���
	ASSERT(pTimerItem == NULL);
	pTimerItem->dwTimerID = dwTimerID;
	pTimerItem->dwElapse = dwElapse;
	pTimerItem->dwRepeatTimes = dwRepeat;
	pTimerItem->dwBindParameter = dwBindParameter;
	pTimerItem->dwTimeLeave = dwElapse + m_dwTimePass;

	//���ʱ��
	m_dwTimeLeave = __min(m_dwTimeLeave, dwElapse);
	if (bTimerExist == false) m_TimerItemActive.Add(pTimerItem);

	return true;
}
//ɾ����ʱ��
bool CTimerEngine::KillTimer(DWORD dwTimerID)
{
	//������Դ
	CWHDataLocker ThreadLock(m_CriticalSection);

	//���Ҷ�ʱ��
	for (INT_PTR i = 0; i < m_TimerItemActive.GetCount(); ++i)
	{
		//��ȡ����
		tagTimerItem * pTimerItem = m_TimerItemActive[i];
		if (pTimerItem->dwTimerID != dwTimerID) continue;

		//���ö���
		m_TimerItemActive.RemoveAt(i);
		m_TimerItemFree.Add(pTimerItem);

		//ֹͣ����
		if (m_TimerItemActive.GetCount() == 0)
		{
			m_dwTimePass = 0;
			m_dwTimeLeave = NO_TIME_LEAVE;
		}

		return true;
	}

	return false;
}
//ɾ�����ж�ʱ��
bool CTimerEngine::KillAllTimer()
{
	//������Դ
	CWHDataLocker ThreadLock(m_CriticalSection);

	m_TimerItemFree.Append(m_TimerItemActive);
	m_TimerItemActive.RemoveAll();

	m_dwTimePass = 0;
	m_dwTimeLeave = NO_TIME_LEAVE;

	return true;
}
//��ʱ��֪ͨ
VOID CTimerEngine::OnTimerThreadSink()
{
	//������Դ
	CWHDataLocker ThreadLock(m_CriticalSection);

	//����ʱ
	if (m_dwTimeLeave == NO_TIME_LEAVE)
	{
		ASSERT(m_TimerItemActive.GetCount() == 0);
		return;
	}

	//����ʱ��
	m_dwTimePass += m_dwTimeSpace;
	m_dwTimeLeave -= m_dwTimeSpace;

	//��ѯ��ʱ��
	if (m_dwTimeLeave == 0)
	{
		//��������
		bool bKillTimer = false;
		DWORD dwTimerLeave = NO_TIME_LEAVE;

		for (INT_PTR i = 0; i < m_TimerItemActive.GetCount();)
		{
			//��ȡ����
			tagTimerItem * pTimerItem = m_TimerItemActive[i];
			ASSERT(pTimerItem->dwTimeLeave >= m_dwTimePass);

			//���ñ���
			bKillTimer = false;
			pTimerItem->dwTimeLeave -= m_dwTimePass;

			//֪ͨ�ж�
			if (pTimerItem->dwTimeLeave = 0)
			{
				//����֪ͨ
				ASSERT(m_pITimerEngineEvent != NULL);
				m_pITimerEngineEvent->OnEventTimer(pTimerItem->dwTimerID, pTimerItem->dwBindParameter);

				//���ô���
				if (pTimerItem->dwRepeatTimes != TIMES_INFINITY)
				{
					if (pTimerItem->dwRepeatTimes == 1)
					{
						bKillTimer = true;
						m_TimerItemActive.RemoveAt(i);
						m_TimerItemFree.Add(pTimerItem);
					}
					else
						pTimerItem->dwRepeatTimes--;
				}

				//����ʱ��
				if (bKillTimer == false)
					pTimerItem->dwTimeLeave = pTimerItem->dwElapse;
			}

			//��������
			if (bKillTimer == false)
			{
				++i;
				dwTimerLeave = __min(dwTimerLeave, pTimerItem->dwTimeLeave);
				ASSERT(dwTimerLeave % m_dwTimeSpace == 0);
			}
		}

		//������Ӧ
		m_dwTimePass = 0L;
		m_dwTimeLeave = dwTimerLeave;
	}

	return;
}


//////////////////////////////////////////////////////////////////////////

//�����������
DECLARE_CREATE_MODULE(TimerEngine);

//////////////////////////////////////////////////////////////////////////