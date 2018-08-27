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

}
//开始事件
bool CAsynchronismThread::OnEventThreadStrat()
{

}
//终止事件
bool CAsynchronismThread::OnEventThreadConclude()
{

}