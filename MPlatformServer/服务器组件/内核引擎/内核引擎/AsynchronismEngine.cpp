#include "StdAfx.h"
#include "AsynchronismEngine.h"


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

}
//��ʼ�¼�
bool CAsynchronismThread::OnEventThreadStrat()
{

}
//��ֹ�¼�
bool CAsynchronismThread::OnEventThreadConclude()
{

}