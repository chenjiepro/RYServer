#include "StdAfx.h"
#include "AsynchronismEngine.h"


//////////////////////////////////////////////////////////////////////////
//数据线程

//构造函数
CAsynchronismThread::CAsynchronismThread()
{
	m_hCompletionPort = 0;
	m_pIAsynchronismEngineSink = NULL;
	//辅助变量
	ZeroMemory(m_cbBuffer, sizeof(m_cbBuffer));

	return;
}
//析构函数
CAsynchronismThread::~CAsynchronismThread()
{

}
//配置函数
VOID CAsynchronismThread::SetCompletionPort(HANDLE hCompletionPort)
{
	//校验参数
	ASSERT(hCompletionPort != NULL);
	//设置变量
	m_hCompletionPort = hCompletionPort;

	return;
}
//设置函数
VOID CAsynchronismThread::SetAsynchronismEngineSink(IAsynchronismEngineSink * pIAsynchronismEngineSink)
{
	//校验参数
	ASSERT(pIAsynchronismEngineSink != NULL);
	//设置变量
	m_pIAsynchronismEngineSink = pIAsynchronismEngineSink;

	return;
}
//运行事件
bool CAsynchronismThread::OnEventThreadRun()
{
	//校验参数
	ASSERT(m_hCompletionPort != NULL);
	ASSERT(m_pIAsynchronismEngineSink != NULL);

	//变量定义
	DWORD dwThancferred = 0L;
	OVERLAPPED * pOverLapped = NULL;
	CAsynchronismEngine * pAsynchronismEngine = NULL;

	//完成端口
	if (GetQueuedCompletionStatus(m_hCompletionPort, &dwThancferred, (PULONG_PTR)&pAsynchronismEngine, &pOverLapped, INFINITE))
	{
		//退出判断
		if (pAsynchronismEngine == NULL) return false;

		//队列锁定
		CWHDataLocker ThreadLock(pAsynchronismEngine->m_CriticalSection);

		//提取数据
		tagDataHead DataHead;
		pAsynchronismEngine->m_DataQueue.DistillData(DataHead, m_cbBuffer, sizeof(m_cbBuffer));

		//队列解锁
		ThreadLock.UnLock();
	}
}
//开始事件
bool CAsynchronismThread::OnEventThreadStrat()
{

}
//终止事件
bool CAsynchronismThread::OnEventThreadConclude()
{

}