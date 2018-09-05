#include "StdAfx.h"
#include "AttemperEngine.h"
#include "TraceServiceManager.h"

//////////////////////////////////////////////////////////////////////////

//构造函数
CAttemperEngine::CAttemperEngine()
{
	m_pTCPNetworkEngine = NULL;
	m_pIAttemperEngineSink = NULL;

	ZeroMemory(m_cbBuffer, sizeof(m_cbBuffer));

	return;
}
//析构函数
CAttemperEngine::~CAttemperEngine()
{

}
//接口查询
VOID * CAttemperEngine::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(IAttemperEngine, Guid, dwQueryVer);
	QUERYINTERFACE(IAsynchronismEngineSink, Guid, dwQueryVer);
	QUERYINTERFACE(IDataBaseEngineEvent, Guid, dwQueryVer);
	QUERYINTERFACE(ITimerEngineEvent, Guid, dwQueryVer);
	QUERYINTERFACE(ITCPSocketEvent, Guid, dwQueryVer);
	QUERYINTERFACE(ITCPNetworkEngineEvent, Guid, dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(IAttemperEngine, Guid, dwQueryVer);

	return NULL;

}
//启动服务
bool CAttemperEngine::StartService()
{
	//校验参数
	ASSERT(m_pIAttemperEngineSink != NULL && m_pTCPNetworkEngine != NULL);
	if (m_pIAttemperEngineSink == NULL || m_pTCPNetworkEngine == NULL) return false;

	//注册对象
	IUnknownEx * pIAsynchronismEngineSink = QUERY_ME_INTERFACE(IUnknownEx);
	if (m_AsynchronismEngine.SetAsynchronismSink(pIAsynchronismEngineSink) == false)
	{
		ASSERT(FALSE);
		return false;
	}

	//异步引擎
	if (m_AsynchronismEngine.StartService() == false) return false;

	return true;
}
//停止服务
bool CAttemperEngine::ConcludeService()
{
	//异步引擎
	m_AsynchronismEngine.ConcludeService();

	return true;
}
//网络接口
bool CAttemperEngine::SetNetworkEngine(IUnknownEx * pIUnknownEx)
{
	if (pIUnknownEx != NULL)
	{
		//查询接口
		ASSERT(QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx, ITCPNetworkEngine) != NULL);
		m_pTCPNetworkEngine = QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx, ITCPNetworkEngine);

		//成功判断
		if (m_pTCPNetworkEngine == NULL) return false;
	}
	else
		m_pTCPNetworkEngine = NULL;

	return true;
}
//回调接口
bool CAttemperEngine::SetAttemperEngineSink(IUnknownEx * pIUnknownEx)
{
	if (pIUnknownEx != NULL)
	{
		//查询接口
		ASSERT(QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx, IAttemperEngineSink) != NULL);
		m_pIAttemperEngineSink = QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx, IAttemperEngineSink);

		//成功判断
		if (m_pIAttemperEngineSink == NULL) return false;
	}
	else
		m_pIAttemperEngineSink = NULL;

	return true;
}
//自定事件
bool CAttemperEngine::OnEventCustom(WORD wRequestID, VOID * pData, WORD wDataSize)
{
	//校验参数
	ASSERT((wRequestID & EVENT_MASK_CUSTOM) == wRequestID);
	if ((wRequestID & EVENT_MASK_CUSTOM) != wRequestID) return false;

	//投递数据
	return m_AsynchronismEngine.PostAsynchronismData(wRequestID, pData, wDataSize);
}
//控制事件
bool CAttemperEngine::OnEventControl(WORD wControlID, VOID * pData, WORD wDataSize)
{
	//校验参数
	ASSERT((wDataSize + sizeof(NTY_ControlEvent)) <= MAX_ASYNCHRONISM_DATA);
	if ((wDataSize + sizeof(NTY_ControlEvent)) > MAX_ASYNCHRONISM_DATA)	return false;

	//缓冲锁定
	CWHDataLocker ThreadLock(m_CriticalLocker);
	NTY_ControlEvent * pControlEvent = (NTY_ControlEvent *)pData;

	//构造数据
	pControlEvent->wControlID = wControlID;

	if (wDataSize > 0)
	{
		ASSERT(pData != NULL);
		CopyMemory(m_cbBuffer + sizeof(NTY_ControlEvent), pData, wDataSize);
	}

	//投递数据
	return m_AsynchronismEngine.PostAsynchronismData(EVENT_CONTROL, m_cbBuffer, sizeof(NTY_ControlEvent) + wDataSize);
}
//时间事件
bool CAttemperEngine::OnEventTimer(DWORD dwTimerID, WPARAM dwBindParameter)
{
	//缓冲锁定
	CWHDataLocker ThreadLock(m_CriticalLocker);
	NTY_TimerEvent * pTimerEvent = (NTY_TimerEvent *)m_cbBuffer;

	pTimerEvent->dwTimerID = dwTimerID;
	pTimerEvent->dwBindParameter = dwBindParameter;

	//投递数据
	return m_AsynchronismEngine.PostAsynchronismData(EVENT_TIMER, m_cbBuffer, sizeof(NTY_TimerEvent));
}
//数据库结果
bool CAttemperEngine::OnEventDataBaseResult(WORD wRequestID, DWORD dwContextID, VOID * pData, WORD wDataSize)
{
	//校验参数
	ASSERT(wDataSize + sizeof(NTY_DataBaseEvent) <= MAX_ASYNCHRONISM_DATA);
	if (wDataSize + sizeof(NTY_DataBaseEvent) > MAX_ASYNCHRONISM_DATA) return false;

	//缓冲锁定
	CWHDataLocker ThreadLock(m_CriticalLocker);
	NTY_DataBaseEvent * pDataBaseEvent = (NTY_DataBaseEvent *)m_cbBuffer;

	pDataBaseEvent->dwContextID = dwContextID;
	pDataBaseEvent->wRequestID = wRequestID;

	//附加数据
	if (wDataSize > 0)
	{
		ASSERT(pData != NULL);
		CopyMemory(m_cbBuffer + sizeof(NTY_DataBaseEvent), pData, wDataSize);
	}

	//投递数据
	return m_AsynchronismEngine.PostAsynchronismData(EVENT_DATABASE, m_cbBuffer, wDataSize + sizeof(NTY_DataBaseEvent));

}
//连接事件
bool CAttemperEngine::OnEventTCPSocketLink(WORD wServiceID, INT nErrorCode)
{
	//缓冲锁定
	CWHDataLocker ThreadLock(m_CriticalLocker);
	NTY_TCPSocketLinkEvent * pTCPSocketLinkEvent = (NTY_TCPSocketLinkEvent *)m_cbBuffer;

	//构造数据
	pTCPSocketLinkEvent->nErrorCode = nErrorCode;
	pTCPSocketLinkEvent->wServiceID = wServiceID;

	//投递数据
	return m_AsynchronismEngine.PostAsynchronismData(EVENT_TCP_SOCKET_LINK, m_cbBuffer, sizeof(NTY_TCPSocketLinkEvent));
}
//关闭事件
bool CAttemperEngine::OnEventTCPSocketShut(WORD wServiceID, BYTE cbShutReason)
{
	//缓冲锁定
	CWHDataLocker ThreadLock(m_CriticalLocker);
	NTY_TCPSocketShutEvent * pTCPSocketShutEvent = (NTY_TCPSocketShutEvent *)m_cbBuffer;

	//构造数据
	pTCPSocketShutEvent->wServiceID = wServiceID;
	pTCPSocketShutEvent->cbShutReason = cbShutReason;

	//投递数据
	return m_AsynchronismEngine.PostAsynchronismData(EVENT_TCP_SOCKET_SHUT, m_cbBuffer, sizeof(NTY_TCPSocketShutEvent));
}
//读取事件
bool CAttemperEngine::OnEventTCPSocketRead(WORD wServiceID, TCP_Command Command, VOID * pData, WORD wDataSize)
{
	//校验参数
	ASSERT(wDataSize + sizeof(NTY_TCPSocketReadEvent) <= MAX_ASYNCHRONISM_DATA);
	if (wDataSize + sizeof(NTY_TCPSocketReadEvent) > MAX_ASYNCHRONISM_DATA) return false;

	//缓冲锁定
	CWHDataLocker ThreadLock(m_CriticalLocker);
	NTY_TCPSocketReadEvent * pSocketReadEvent = (NTY_TCPSocketReadEvent *)m_cbBuffer;

	pSocketReadEvent->Command = Command;
	pSocketReadEvent->wServiceID = wServiceID;
	pSocketReadEvent->wDataSize = wDataSize;

	//附加数据
	if (wDataSize > 0)
	{
		ASSERT(pData != NULL);
		CopyMemory(m_cbBuffer + sizeof(NTY_TCPSocketReadEvent), pData, wDataSize);
	}

	//投递数据
	return m_AsynchronismEngine.PostAsynchronismData(EVENT_TCP_SOCKET_READ, m_cbBuffer, wDataSize + sizeof(NTY_TCPSocketReadEvent));
}
//应答事件
bool CAttemperEngine::OnEventTCPNetworkBind(DWORD dwSocketID, DWORD dwClientAddr)
{
	//缓冲锁定
	CWHDataLocker ThreadLock(m_CriticalLocker);
	NTY_TCPNetworkAcceptEvent * pNetworkAcceptEvent = (NTY_TCPNetworkAcceptEvent *)m_cbBuffer;

	//构造数据
	pNetworkAcceptEvent->dwClientAddr = dwClientAddr;
	pNetworkAcceptEvent->dwSocketID = dwSocketID;

	//投递数据
	return m_AsynchronismEngine.PostAsynchronismData(EVENT_TCP_NETWORK_ACCEPT, m_cbBuffer, sizeof(NTY_TCPNetworkAcceptEvent));
}
//关闭事件
bool CAttemperEngine::OnEventTCPNetworkShut(DWORD dwSocketID, DWORD dwClientAddr, DWORD dwActiveTime)
{
	//缓冲锁定
	CWHDataLocker ThreadLock(m_CriticalLocker);
	NTY_TCPNetworkShutEvent * pNetworkShutEvent = (NTY_TCPNetworkShutEvent *)m_cbBuffer;

	//构造数据
	pNetworkShutEvent->dwClientAddr = dwClientAddr;
	pNetworkShutEvent->dwSocketID = dwSocketID;
	pNetworkShutEvent->dwActiveTime = dwActiveTime;

	//投递数据
	return m_AsynchronismEngine.PostAsynchronismData(EVENT_TCP_NETWORK_SHUT, m_cbBuffer, sizeof(NTY_TCPNetworkShutEvent));
}
//读取事件
bool CAttemperEngine::OnEventTCPNetworkRead(DWORD dwSocketID, TCP_Command Command, VOID * pData, WORD wDataSize)
{
	//校验参数
	ASSERT(wDataSize + sizeof(NTY_TCPNetworkReadEvent) <= MAX_ASYNCHRONISM_DATA);
	if (wDataSize + sizeof(NTY_TCPNetworkReadEvent) > MAX_ASYNCHRONISM_DATA) return false;

	//缓冲锁定
	CWHDataLocker ThreadLock(m_CriticalLocker);
	NTY_TCPNetworkReadEvent * pNetworkReadEvent = (NTY_TCPNetworkReadEvent *)m_cbBuffer;

	pNetworkReadEvent->Command = Command;
	pNetworkReadEvent->dwSocketID= dwSocketID;
	pNetworkReadEvent->wDataSize = wDataSize;

	//附加数据
	if (wDataSize > 0)
	{
		ASSERT(pData != NULL);
		CopyMemory(m_cbBuffer + sizeof(NTY_TCPNetworkReadEvent), pData, wDataSize);
	}

	//投递数据
	return m_AsynchronismEngine.PostAsynchronismData(EVENT_TCP_NETWORK_READ, m_cbBuffer, wDataSize + sizeof(NTY_TCPNetworkReadEvent));
}
//启动事件
bool CAttemperEngine::OnAsynchronismEngineStart()
{
	//校验参数
	ASSERT(m_pIAttemperEngineSink != NULL);
	if (m_pIAttemperEngineSink == NULL) return false;

	//启动通知
	if (m_pIAttemperEngineSink->OnAttemperEngineStart(QUERY_ME_INTERFACE(IUnknownEx)) == false)
	{
		ASSERT(FALSE);
		return false;
	}

	return true;
}
//停止事件
bool CAttemperEngine::OnAsynchronismEngineConclude()
{
	//校验参数
	ASSERT(m_pIAttemperEngineSink != NULL);
	if (m_pIAttemperEngineSink == NULL) return false;

	//启动通知
	if (m_pIAttemperEngineSink->OnAttemperEngineConclude(QUERY_ME_INTERFACE(IUnknownEx)) == false)
	{
		ASSERT(FALSE);
		return false;
	}

	return true;
}
//异步数据
bool CAttemperEngine::OnAsynchronismEngineData(WORD wIdentifier, VOID *pData, WORD wDataSize)
{
	//校验参数
	ASSERT(m_pIAttemperEngineSink != NULL);
	ASSERT(m_pTCPNetworkEngine != NULL);

	//内核事件
	switch (wIdentifier)
	{
		case EVENT_TIMER:
			{
				//大小断言
				ASSERT(wDataSize == sizeof(NTY_TimerEvent));
				if (wDataSize != sizeof(NTY_TimerEvent)) return false;

				//处理消息
				NTY_TimerEvent * pTimerEvent = (NTY_TimerEvent *)pData;
				m_pIAttemperEngineSink->OnEventTimer(pTimerEvent->dwTimerID, pTimerEvent->dwBindParameter);

				return true;
			}
		case EVENT_CONTROL:
			{
				//大小断言
				ASSERT(wDataSize == sizeof(NTY_ControlEvent));
				if (wDataSize != sizeof(NTY_ControlEvent)) return false;

				//处理消息
				NTY_ControlEvent * pControlEvent = (NTY_ControlEvent *)pData;
				m_pIAttemperEngineSink->OnEventControl(pControlEvent->wControlID, pControlEvent + 1, wDataSize - sizeof(NTY_ControlEvent));

				return true;
			}
		case EVENT_DATABASE:
			{
				//大小断言
				ASSERT(wDataSize == sizeof(NTY_DataBaseEvent));
				if (wDataSize != sizeof(NTY_DataBaseEvent)) return false;

				//处理消息
				NTY_DataBaseEvent * pDataBaseEvent = (NTY_DataBaseEvent *)pData;
				m_pIAttemperEngineSink->OnEventDataBase(pDataBaseEvent->wRequestID, pDataBaseEvent->dwContextID, pDataBaseEvent + 1, wDataSize - sizeof(NTY_DataBaseEvent));

				return true;
			}
		case EVENT_TCP_SOCKET_READ:
			{
				//变量定义
				NTY_TCPSocketReadEvent * pSocketReadEvent = (NTY_TCPSocketReadEvent *)pData;

				ASSERT(wDataSize >= sizeof(NTY_TCPSocketReadEvent));
				ASSERT(wDataSize == sizeof(NTY_TCPSocketReadEvent) + pSocketReadEvent->wDataSize);

				if (wDataSize < sizeof(NTY_TCPSocketReadEvent)) return false;
				if (wDataSize != sizeof(NTY_TCPSocketReadEvent) + pSocketReadEvent->wDataSize) return false;

				m_pIAttemperEngineSink->OnEventTCPSocketRead(pSocketReadEvent->wServiceID, pSocketReadEvent->Command, pSocketReadEvent + 1, pSocketReadEvent->wDataSize);

				return true;
			}
		case EVENT_TCP_SOCKET_SHUT:
			{
				//大小断言
				ASSERT(wDataSize == sizeof(NTY_TCPSocketShutEvent));
				if (wDataSize != sizeof(NTY_TCPSocketShutEvent)) return false;

				//处理消息
				NTY_TCPSocketShutEvent * pSocketShutEvent = (NTY_TCPSocketShutEvent *)pData;
				m_pIAttemperEngineSink->OnEventTCPSocketShut(pSocketShutEvent->wServiceID, pSocketShutEvent->cbShutReason);

				return true;
			}
		case EVENT_TCP_SOCKET_LINK:
			{
				//大小断言
				ASSERT(wDataSize == sizeof(NTY_TCPSocketLinkEvent));
				if (wDataSize != sizeof(NTY_TCPSocketLinkEvent)) return false;

				//处理消息
				NTY_TCPSocketLinkEvent * pSocketLinkEvent = (NTY_TCPSocketLinkEvent *)pData;
				m_pIAttemperEngineSink->OnEventTCPSocketLink(pSocketLinkEvent->wServiceID, pSocketLinkEvent->nErrorCode);

				return true;
			}
		case EVENT_TCP_NETWORK_ACCEPT:
			{
				//大小断言
				ASSERT(wDataSize == sizeof(NTY_TCPNetworkAcceptEvent));
				if (wDataSize != sizeof(NTY_TCPNetworkAcceptEvent)) return false;

				//变量定义
				bool bSuccess = false;
				NTY_TCPNetworkAcceptEvent * pNetworkAcceptEvent = (NTY_TCPNetworkAcceptEvent *)pData;

				//消息处理
				try
				{
					bSuccess = m_pIAttemperEngineSink->OnEventTCPNetworkBind(pNetworkAcceptEvent->dwClientAddr, pNetworkAcceptEvent->dwSocketID);
				}
				catch (...)
				{

				}

				//失败处理
				if (bSuccess == false) m_pTCPNetworkEngine->CloseSocket(pNetworkAcceptEvent->dwSocketID);

				return true;
			}
		case EVENT_TCP_NETWORK_READ:
			{
				//校验大小
				NTY_TCPNetworkReadEvent * pReadEvent = (NTY_TCPNetworkReadEvent *)pData;

				//大小断言
				ASSERT(wDataSize >= sizeof(NTY_TCPNetworkReadEvent));
				ASSERT(wDataSize == sizeof(NTY_TCPNetworkReadEvent) + pReadEvent->wDataSize);

				//大小校验
				if (wDataSize < sizeof(NTY_TCPNetworkReadEvent))
				{
					m_pTCPNetworkEngine->CloseSocket(pReadEvent->dwSocketID);
					return false;
				}

				if (wDataSize != sizeof(NTY_TCPNetworkReadEvent) + pReadEvent->wDataSize)
				{
					m_pTCPNetworkEngine->CloseSocket(pReadEvent->dwSocketID);
					return false;
				}

				//消息处理
				bool bSuccess = false;

				try
				{
					bSuccess = m_pIAttemperEngineSink->OnEventTCPNetworkRead(pReadEvent->dwSocketID, pReadEvent->Command, pReadEvent + 1, pReadEvent->wDataSize);
				}
				catch (...)
				{
				}

				if (bSuccess == false)
					m_pTCPNetworkEngine->CloseSocket(pReadEvent->dwSocketID);

				return true;
			}
		case EVENT_TCP_NETWORK_SHUT:
			{
				//大小断言
				ASSERT(wDataSize == sizeof(NTY_TCPNetworkShutEvent));
				if (wDataSize != sizeof(NTY_TCPNetworkShutEvent)) return false;

				//处理消息
				NTY_TCPNetworkShutEvent * pCloseEvent = (NTY_TCPNetworkShutEvent *)pData;
				m_pIAttemperEngineSink->OnEventTCPNetworkShut(pCloseEvent->dwClientAddr, pCloseEvent->dwActiveTime, pCloseEvent->dwSocketID);

				return true;
			}
	}

	//其他事件
	return m_pIAttemperEngineSink->OnEventAttemperData(wIdentifier, pData, wDataSize);
}


//////////////////////////////////////////////////////////////////////////

//组件创建函数
DECLARE_CREATE_MODULE(AttemperEngine);

//////////////////////////////////////////////////////////////////////////