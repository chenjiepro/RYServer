#include "Stdafx.h"
#include "Afxinet.h"
#include "TCPNetworkEngin.h"
#include "TraceServiceManager.h"

//////////////////////////////////////////////////////////////////////////
//宏定义

//系数定义
#define DEAD_QUOTIETY				0									//死亡系数
#define DANGER_QUOTIETY				1									//危险系数
#define SAFETY_QUOTIETY				2									//安全系数

//动作定义
#define ASYNCHRONISM_SEND_DATA		1									//发送标识
#define ASYNCHRONISM_SEND_BATCH		2									//群体发送
#define ASYNCHRONISM_SHUT_DOWN		3									//安全关闭
#define ASYNCHRONISM_ALLOW_BATCH	4									//允许群发
#define ASYNCHRONISM_CLOSE_SOCKET	5									//关闭连接
#define ASYNCHRONISM_DETECT_SOCKET	6									//检测连接

//索引辅助
#define SOCKET_INDEX(dwSocketID)	LOWORD(dwSocketID)					//位置索引
#define SOCKET_ROUNTID(dwRountID)	HIWORD(dwRountID)					//循环索引

//////////////////////////////////////////////////////////////////////////
//结构定义

//发送请求
struct tagSendDataRequest
{
	WORD							wIndex;								//连接索引
	WORD							wRountID;							//循环索引
	WORD							wMainCmdID;							//主命令码
	WORD							wSubCmdID;							//子命令码
	WORD							wDataSize;							//数据大小
	BYTE							cbSendBuffer[SOCKET_TCP_BUFFER];	//发送缓冲
};																		

//群发请求
struct tagBatchSendRequest
{
	WORD							wMainCmdID;							//主命令码
	WORD							wSubCmdID;							//子命令码
	WORD							wDataSize;							//数据大小
	BYTE							cbBatchMask;						//数据掩码
	BYTE							cbSendBuffer[SOCKET_TCP_BUFFER];	//发送缓冲
};

//允许群发
struct tagAllowBatchSend
{
	WORD							wIndex;								//连接索引
	WORD							wRountid;							//循环索引
	BYTE							cbBatchMask;						//数据掩码
	BYTE							cbAllowBatch;						//允许标志
};

//关闭连接
struct tagCloseSocket
{
	WORD							wIndex;								//连接索引
	WORD							wRountid;							//循环索引
};

//安全关闭
struct tagShutDownSocket
{
	WORD							wIndex;								//连接索引
	WORD							wRountid;							//循环索引
};

//////////////////////////////////////////////////////////////////////////
//构造函数
COverLapped::COverLapped(enOperationType OperationType) : m_OperationType(OperationType)
{
	//设置变量
	ZeroMemory(&m_WSABuffer, sizeof(m_WSABuffer));
	ZeroMemory(&m_OverLapped, sizeof(m_OverLapped));

	return;
}
//析构函数
COverLapped::~COverLapped()
{
}

//////////////////////////////////////////////////////////////////////////

//构造函数
COverLappedSend::COverLappedSend() : COverLapped(enOperationType_Send)
{
	m_WSABuffer.len = 0;
	m_WSABuffer.buf = (char*)m_cbBuffer;
}
//析构函数
COverLappedSend::~COverLappedSend()
{

}

//////////////////////////////////////////////////////////////////////////
//构造函数
COverLappedRecv::COverLappedRecv() : COverLapped(enOperationType_Recv)
{
	m_WSABuffer.len = 0;
	m_WSABuffer.buf = NULL;
}
//析构函数
COverLappedRecv::~COverLappedRecv()
{

}

//////////////////////////////////////////////////////////////////////////

//构造函数
CTCPNetworkItem::CTCPNetworkItem(WORD wIndex, ITCPNetworkItemSink * pITCPNetworkItemSink)
{
	//连接属性
	m_dwClientIP = 0L;
	m_dwActiveTime = 0L;
	//内核变量
	m_wRountID = 1;
	m_wSurvivalTime = DEAD_QUOTIETY;
	m_hSocketHandle = INVALID_SOCKET;
	//状态变量
	m_bSendIng = false;
	m_bRecvIng = false;
	m_bShutDown = false;
	m_bAllowBatch = false;
	m_bBatchMask = 0xFF;
	//接受变量
	m_wRecvSize = 0;
	ZeroMemory(m_cbRecvBuf, sizeof(m_cbRecvBuf));

	//计数变量
	m_dwSendTickCount = 0L;
	m_dwRecvTickCount = 0L;
	m_dwSendPacketCount = 0L;
	m_dwRecvPacketCount = 0L;
	//加密数据
	m_cbSendRound = 0;
	m_cbRecvRound = 0;
	m_dwSendXorKey = 0;
	m_dwRecvXorKey = 0;

	return;
}
//析构函数
CTCPNetworkItem::~CTCPNetworkItem()
{
	//删除空闲
	for (INT_PTR i = 0; i < m_OverLappedSendBuffer.GetCount(); ++i)
	{
		delete m_OverLappedSendBuffer[i];
	}
	//删除活动
	for (INT_PTR i = 0; i < m_OverLappedSendActive.GetCount(); ++i)
	{
		delete m_OverLappedSendActive[i];
	}

	//删除数组
	m_OverLappedSendBuffer.RemoveAll();
	m_OverLappedSendActive.RemoveAll();

	return;
}

//绑定对象
DWORD CTCPNetworkItem::Attach(SOCKET hSocket, DWORD dwClientIP)
{
	//校验参数
	ASSERT(dwClientIP != 0);
	ASSERT(hSocket != INVALID_SOCKET);

	//校验状态
	ASSERT(m_bSendIng == false);
	ASSERT(m_bRecvIng == false);
	ASSERT(m_hSocketHandle == INVALID_SOCKET);

	//状态变量
	m_bSendIng = false;
	m_bRecvIng = false;
	m_bShutDown = false;
	m_bAllowBatch = false;
	m_bBatchMask = 0xFF;

	//设置变量
	m_dwClientIP = dwClientIP;
	m_hSocketHandle = hSocket;
	m_wSurvivalTime = SAFETY_QUOTIETY;
	m_dwActiveTime = (DWORD)time(NULL);
	m_dwRecvTickCount = GetTickCount();

	//发送通知
	m_pITCPNetworkItemSink->OnEventSocketBind(this);

	return GetIdentifierID();
}
//恢复数据
DWORD CTCPNetworkItem::ResumeData()
{
	//状态校验
	ASSERT(m_hSocketHandle == INVALID_SOCKET);
	
	//连接属性
	m_dwClientIP = 0L;
	m_dwActiveTime = 0L;
	//内核变量
	m_wRountID = __max(1, m_wRountID + 1);
	m_wSurvivalTime = DEAD_QUOTIETY;
	m_hSocketHandle = INVALID_SOCKET;
	//状态变量
	m_bSendIng = false;
	m_bRecvIng = false;
	m_bShutDown = false;
	m_bAllowBatch = false;
	
	//接受变量
	m_wRecvSize = 0;
	ZeroMemory(m_cbRecvBuf, sizeof(m_cbRecvBuf));

	//计数变量
	m_dwSendTickCount = 0L;
	m_dwRecvTickCount = 0L;
	m_dwSendPacketCount = 0L;
	m_dwRecvPacketCount = 0L;
	//加密数据
	m_cbSendRound = 0;
	m_cbRecvRound = 0;
	m_dwSendXorKey = 0;
	m_dwRecvXorKey = 0;

	//重叠数组
	m_OverLappedSendBuffer.Append(m_OverLappedSendActive);
	m_OverLappedSendBuffer.RemoveAll();

	return GetIdentifierID();
}

//发送函数
bool CTCPNetworkItem::SendData(WORD wMainCmdID, WORD wSubCmdID, WORD wRountID)
{
	//发送判断
	if (SendVerdict(wRountID) == false) return false;

	//获取缓冲
	WORD wPacketSize = sizeof(TCP_Head);
	COverLappedSend * pOverLappedSend = GetSendOverLapped(wPacketSize);

	//关闭判断
	if (pOverLappedSend == NULL)
	{
		CloseSocket(wRountID);
		return false;
	}

	//变量定义
	WORD wSourceLen = (WORD)pOverLappedSend->m_WSABuffer.len;
	TCP_Head * pHead = (TCP_Head *)(pOverLappedSend->m_cbBuffer + wSourceLen);

	//设置数据
	pHead->CommandInfo.wMainCmdID = wMainCmdID;
	pHead->CommandInfo.wSubCmdID = wSubCmdID;

	//加密数据
	WORD wEncryptLen = EncryptBuffer(pOverLappedSend->m_cbBuffer + wSourceLen, wPacketSize, sizeof(pOverLappedSend->m_cbBuffer) - wSourceLen);
	pOverLappedSend->m_WSABuffer.len += wEncryptLen;

	//发送数据
	if (m_bSendIng == false)
	{
		//发送数据
		DWORD dwTanceferred = 0;
		INT nResultCode = WSASend(m_hSocketHandle, &pOverLappedSend->m_WSABuffer, 
								  1, &dwTanceferred, 0, &pOverLappedSend->m_OverLapped, NULL);

		//结果处理 WSA_IO_PENDING 表示没有错误
		if (nResultCode == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			CloseSocket(m_wRountID);
			return false;
		}

		//设置变量
		m_bSendIng = true;
	}

	return true;
}
//发送函数
bool CTCPNetworkItem::SendData(VOID * pData, WORD wDataSize, WORD wMainCmdID, WORD wSubCmdID, WORD wRountID)
{
	//校验参数
	ASSERT(wDataSize <= SOCKET_TCP_PACKET);

	//发送判断
	if (wDataSize > SOCKET_TCP_PACKET) return false;
	if (SendVerdict(wRountID) == false) return false;

	//获取缓冲
	WORD wPacketSize = sizeof(TCP_Head) + wDataSize;
	COverLappedSend * pOverLappedSend = GetSendOverLapped(wPacketSize);

	//关闭判断
	if (pOverLappedSend == NULL)
	{
		CloseSocket(wRountID);
		return false;
	}

	//变量定义
	WORD wSourceLen = (WORD)pOverLappedSend->m_WSABuffer.len;
	TCP_Head * pHead = (TCP_Head *)(pOverLappedSend->m_cbBuffer + wSourceLen);

	//设置变量
	pHead->CommandInfo.wSubCmdID = wSubCmdID;
	pHead->CommandInfo.wMainCmdID = wMainCmdID;

	//附加数据
	if (wDataSize > 0)
	{
		ASSERT(pData != NULL);
		CopyMemory(pHead + 1, pData, wDataSize);
	}

	//加密数据
	WORD wEncryptLen = EncryptBuffer(pOverLappedSend->m_cbBuffer + wSourceLen, wPacketSize, sizeof(pOverLappedSend->m_cbBuffer) - wSourceLen);
	pOverLappedSend->m_WSABuffer.len += wEncryptLen;

	//发送数据
	if (m_bSendIng == false)
	{
		//发送数据
		DWORD dwThancferred = 0;
		INT nResultCode = WSASend(m_hSocketHandle, &pOverLappedSend->m_WSABuffer, 1, &dwThancferred, 0, &pOverLappedSend->m_OverLapped, NULL);

		//结果处理
		if ((nResultCode == SOCKET_ERROR) && (WSAGetLastError() != WSA_IO_PENDING))
		{
			CloseSocket(m_wRountID);
			return false;
		}

		//设置变量
		m_bSendIng = true;
	}

	return true;
}
//接收操作
bool CTCPNetworkItem::RecvData()
{
	//校验变量
	ASSERT(m_bRecvIng == false);
	ASSERT(m_hSocketHandle != INVALID_SOCKET);

	//接收数据
	DWORD dwThancferred = 0;
	DWORD dwFlags = 0;

	INT nResultCode = WSARecv(m_hSocketHandle, &m_OverLappedRecv.m_WSABuffer, 1, &dwThancferred, &dwFlags,
		&m_OverLappedRecv.m_OverLapped, NULL);

	//结果处理
	if (nResultCode == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		CloseSocket(m_wRountID);
		return false;
	}

	//设置变量
	m_bRecvIng = true;

	return true;
}
//关闭连接
bool CTCPNetworkItem::CloseSocket(WORD wRountID)
{
	//状态判断
	if (m_wRountID != wRountID) return false;

	//关闭连接
	if (m_hSocketHandle != INVALID_SOCKET)
	{
		closesocket(m_hSocketHandle);
		m_hSocketHandle = INVALID_SOCKET;
	}

	//判断关闭
	if (m_bRecvIng == false && m_bSendIng == false)
	{
		OnCloseCompleted();
	}
	
	return true;
}
//设置关闭
bool CTCPNetworkItem::ShutDownSocket(WORD wRountID)
{
	//状态判断
	if (m_hSocketHandle == INVALID_SOCKET) return false;
	if (m_bShutDown == true || m_wRountID != wRountID) return false;

	//设置变量
	m_wRecvSize = 0;
	m_bShutDown = true;

	return true;
}
//允许群发
bool CTCPNetworkItem::AllowBatchSend(WORD wRountID, bool bAllowBatch, BYTE cbBatchMask)
{
	//状态判断
	if (m_hSocketHandle == INVALID_SOCKET) return false;
	if (m_wRountID != wRountID) return false;

	//设置变量
	m_bAllowBatch = bAllowBatch;

	m_bBatchMask = cbBatchMask;

	return true;
}
//发送完成
bool CTCPNetworkItem::OnSendCompleted(COverLappedSend * pOverLappedSend, DWORD dwThancferred)
{
	//校验变量
	ASSERT(m_bSendIng == true);
	ASSERT(m_OverLappedSendActive.GetCount() > 0);
	ASSERT(pOverLappedSend == m_OverLappedSendActive[0]);

	//释放结构
	m_OverLappedSendActive.RemoveAt(0);
	m_OverLappedSendBuffer.Add(pOverLappedSend);

	//设置变量
	m_bSendIng = false;

	//判断关闭
	if (m_hSocketHandle == INVALID_SOCKET)
	{
		CloseSocket(m_wRountID);
	}

	//设置变量
	if (dwThancferred != 0)
	{
		m_wSurvivalTime = SAFETY_QUOTIETY;
		m_dwSendTickCount = GetTickCount();
	}

	//继续发送
	if (m_OverLappedSendActive.GetCount() > 0)
	{
		//获取数据
		pOverLappedSend = m_OverLappedSendActive[0];

		//发送数据
		DWORD dwThancferred = 0;
		INT nResultCode = WSASend(m_hSocketHandle, &pOverLappedSend->m_WSABuffer, 1, &dwThancferred, 0,
								  &pOverLappedSend->m_OverLapped, NULL);

		//结果处理
		if (nResultCode == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			CloseSocket(m_wRountID);
			return false;
		}

		//设置变量
		m_bSendIng = true;		
	}
	
	return true;
}
//接收完成
bool CTCPNetworkItem::OnRecvCompleted(COverLappedRecv * pOverLappedRecv, DWORD dwThancferred)
{
	//校验变量
	ASSERT(m_bRecvIng == true);

	//设置变量
	m_bRecvIng = false;

	//判断关闭
	if (m_hSocketHandle == INVALID_SOCKET)
	{
		CloseSocket(m_wRountID);
		return true;
	}

	//接受数据
	INT nResultCode = recv(m_hSocketHandle, (char*)m_cbRecvBuf + m_wRecvSize, sizeof(m_cbRecvBuf) - m_wRecvSize, 0);

	//关闭判断
	if (nResultCode <= 0)
	{
		CloseSocket(m_wRountID);
		return true;
	}

	//中断判断
	if (m_bShutDown == true) return true;

	//设置变量
	m_wRecvSize += nResultCode;
	m_wSurvivalTime = SAFETY_QUOTIETY;
	m_dwRecvPacketCount = GetTickCount();

	//接收完成
	BYTE cbBuffer[SOCKET_TCP_BUFFER];
	TCP_Head * pHead = (TCP_Head *)m_cbRecvBuf;

	//处理数据
	try
	{
		while (m_wRecvSize >= sizeof(TCP_Head))
		{
			//校验数据
			WORD wPacketSize = pHead->TCPInfo.wPacketSize;

			//数据判断
			if (wPacketSize > SOCKET_TCP_BUFFER) throw TEXT("数据包长度太长");
			if (wPacketSize < sizeof(TCP_Head))
			{
				TCHAR szBuffer[512];
				_sntprintf(szBuffer, CountArray(szBuffer), TEXT("数据包长度太短 %d,%d,%d,%d,%d,%d,%d,%d"), m_cbRecvBuf[0],
					m_cbRecvBuf[1],
					m_cbRecvBuf[2],
					m_cbRecvBuf[3],
					m_cbRecvBuf[4],
					m_cbRecvBuf[5],
					m_cbRecvBuf[6],
					m_cbRecvBuf[7]
					);
				g_TraceServiceManager.TraceString(szBuffer, TraceLevel_Exception);

				_sntprintf(szBuffer, CountArray(szBuffer), TEXT("包长 %d, 版本 %d, 效验码 %d"), pHead->TCPInfo.wPacketSize,
					pHead->TCPInfo.cbDataKind, pHead->TCPInfo.cbCheckCode);

				g_TraceServiceManager.TraceString(szBuffer, TraceLevel_Exception);
				throw TEXT("数据包长度太短");
			}
			if (pHead->TCPInfo.cbDataKind != DK_MAPPED && pHead->TCPInfo.cbDataKind != 0x05) {
				CString aa;
				aa.Format(TEXT("0x%x"), pHead->TCPInfo.cbDataKind);
				g_TraceServiceManager.TraceString(aa, TraceLevel_Exception);
				throw TEXT("数据包版本不匹配");
			}
			//完成判断
			if (m_wRecvSize < wPacketSize) break;

			//提取数据
			CopyMemory(cbBuffer, m_cbRecvBuf, wPacketSize);
			WORD wRealySize = CrevasseBuffer(cbBuffer, wPacketSize);

			//设置变量
			m_dwRecvPacketCount++;

			//解释数据
			LPVOID pData = cbBuffer + sizeof(TCP_Head);
			WORD wDataSize = wRealySize + sizeof(TCP_Head);
			TCP_Command Command = ((TCP_Head *)cbBuffer)->CommandInfo;

			//消息处理
			if (Command.wMainCmdID != MDM_KN_COMMAND)
				m_pITCPNetworkItemSink->OnEventSocketRead(Command, pData, wDataSize, this);

			//删除缓冲
			m_wRecvSize -= wPacketSize;
			if (m_wRecvSize > 0)
				MoveMemory(m_cbRecvBuf, m_cbRecvBuf + wPacketSize, m_wRecvSize);
		}
	}
	catch (LPCTSTR pszMessage)
	{
		//错误信息
		TCHAR szString[512] = TEXT("");
		_sntprintf(szString, CountArray(szString), TEXT("SocketEngine Index=%ld，RountID=%ld，OnRecvCompleted 发生“%s”异常"), m_wIndex, m_wRountID, pszMessage);
		g_TraceServiceManager.TraceString(szString, TraceLevel_Exception);

		//关闭链接
		CloseSocket(m_wRountID);

		return false;
	}
	catch (...)
	{
		//错误信息
		TCHAR szString[512] = TEXT("");
		_sntprintf(szString, CountArray(szString), TEXT("SocketEngine Index=%ld，RountID=%ld，OnRecvCompleted 发生“非法”异常"), m_wIndex, m_wRountID);
		g_TraceServiceManager.TraceString(szString, TraceLevel_Exception);

		//关闭链接
		CloseSocket(m_wRountID);

		return false;
	}

	return RecvData();
}
//关闭完成
bool CTCPNetworkItem::OnCloseCompleted()
{
	//校验状态
	ASSERT(m_hSocketHandle == INVALID_SOCKET);
	ASSERT(m_bSendIng == false && m_bRecvIng == false);

	//关闭事件
	m_pITCPNetworkItemSink->OnEventSocketShut(this);

	//恢复数据
	ResumeData();

	return true;
}
//加密数据
WORD CTCPNetworkItem::EncryptBuffer(BYTE pcbDataBuffer[], WORD wDataSize, WORD wBufferSize)
{
	WORD i = 0;
	//校验参数
	ASSERT(wDataSize >= sizeof(TCP_Head));
	ASSERT(wDataSize <= sizeof(TCP_Head) + SOCKET_TCP_BUFFER);
	ASSERT(wBufferSize >= (wDataSize + sizeof(DWORD) * 2));

	//填写信息头
	TCP_Head * pHead = (TCP_Head *)pcbDataBuffer;
	pHead->TCPInfo.wPacketSize = wDataSize;
	pHead->TCPInfo.cbDataKind = DK_MAPPED;

	BYTE checkCode = 0;

	for (WORD i = sizeof(TCP_Info); i < wDataSize; ++i)
	{
		checkCode += pcbDataBuffer[i];
		pcbDataBuffer[i] = MapSendByte(pcbDataBuffer[i]);
	}
	pHead->TCPInfo.cbCheckCode = ~checkCode + 1;

	//设置变量
	m_dwSendPacketCount++;

	return wDataSize;
}
//解密数据
WORD CTCPNetworkItem::CrevasseBuffer(BYTE pcbDataBuffer[], WORD wDataSize)
{
	WORD i = 0;
	//校验参数
	ASSERT(wDataSize >= sizeof(TCP_Head));
	ASSERT(((TCP_Head*)pcbDataBuffer)->TCPInfo.wPacketSize == wDataSize);

	//校验码与字节映射
	TCP_Head * pHead = (TCP_Head*)pcbDataBuffer;
	for (i = sizeof(TCP_Head); i < wDataSize; ++i)
	{
		pcbDataBuffer[i] = MapRecvByte(pcbDataBuffer[i]);
	}

	return wDataSize;
}
//随机映射
WORD CTCPNetworkItem::SeedRandMap(WORD wSeed)
{
	DWORD dwHold = wSeed;
	return (WORD)((dwHold = dwHold * 241103L + 2533101L) >> 16);
}
//发送映射数据
BYTE CTCPNetworkItem::MapSendByte(BYTE cbData)
{
	BYTE cbMap;
	cbMap = g_SendByteMap[cbData];
	return cbMap;
}
//接收映射数据
BYTE CTCPNetworkItem::MapRecvByte(BYTE cbData)
{
	BYTE cbMap;
	cbMap = g_RecvByteMap[cbData];
	return cbMap;
}
//发送判断
bool CTCPNetworkItem::SendVerdict(WORD wRountID)
{
	if (m_wRountID != wRountID || m_bShutDown == true) return false;
	if (m_hSocketHandle == INVALID_SOCKET || m_dwRecvPacketCount == 0) return false;

	return true;
}
//获取发送重叠结构 
COverLappedSend * CTCPNetworkItem::GetSendOverLapped(WORD wPacketSize)
{
	//重用判断 上次还没发送完的要继续发送
	if (m_OverLappedSendActive.GetCount() > 1)
	{
		INT_PTR nActiveCount = m_OverLappedSendActive.GetCount();
		COverLappedSend * pOverLappedSend = m_OverLappedSendActive[nActiveCount - 1];
		if (sizeof(pOverLappedSend->m_cbBuffer) >= (pOverLappedSend->m_WSABuffer.len + wPacketSize + sizeof(DWORD) * 2))
			return pOverLappedSend;
	}

	//空闲对象
	if (m_OverLappedSendActive.GetCount() > 0)
	{
		//获取对象
		INT_PTR nFreeCount = m_OverLappedSendBuffer.GetCount();
		COverLappedSend * pOverLappedSend = m_OverLappedSendBuffer[nFreeCount - 1];

		//设置变量
		pOverLappedSend->m_WSABuffer.len = 0;
		m_OverLappedSendActive.Add(pOverLappedSend);
		m_OverLappedSendBuffer.RemoveAt(nFreeCount - 1);

		return pOverLappedSend;
	}

	try
	{
		//创建对象
		COverLappedSend * pOverLappedSend = new COverLappedSend;
		ASSERT(pOverLappedSend != NULL);

		pOverLappedSend->m_WSABuffer.len = 0;
		m_OverLappedSendActive.Add(pOverLappedSend);

		return pOverLappedSend;
	}
	catch (...)
	{
		ASSERT(FALSE);
	}

	return NULL;
}


//////////////////////////////////////////////////////////////////////////

//读写线程类
//构造函数
CTCPNetworkThreadReadWrite::CTCPNetworkThreadReadWrite()
{
	m_hCompletionPort = NULL;
	return;
}
//析构函数
CTCPNetworkThreadReadWrite::~CTCPNetworkThreadReadWrite()
{

}
//配置函数
bool CTCPNetworkThreadReadWrite::InitThread(HANDLE hCompletionPort)
{
	ASSERT(hCompletionPort != NULL);

	m_hCompletionPort = hCompletionPort;

	return true;
}
//运行函数
bool CTCPNetworkThreadReadWrite::OnEventThreadRun()
{
	//校验参数
	ASSERT(m_hCompletionPort != NULL);

	//变量定义
	DWORD dwThancferred = 0;
	OVERLAPPED * pOverLapped = NULL;
	CTCPNetworkItem * pTCPNetworkItem = NULL;

	//完成端口
	BOOL bSuccess = GetQueuedCompletionStatus(m_hCompletionPort, &dwThancferred, 
											 (PULONG_PTR)&pTCPNetworkItem, &pOverLapped, INFINITE);

	if (bSuccess == FALSE && GetLastError() != ERROR_NETNAME_DELETED) return false;

	//退出判断
	if (pTCPNetworkItem == NULL && pOverLapped == NULL) return false;

	//变量定义
	COverLapped * pSocketLapped = CONTAINING_RECORD(pOverLapped, COverLapped, m_OverLapped);

	//操作处理
	switch (pSocketLapped->GetOperationType())
	{
		case enOperationType_Send: //数据发送
			{
				CWHDataLocker ThreadLock(pTCPNetworkItem->GetCriticalSection());
				pTCPNetworkItem->OnSendCompleted((COverLappedSend *)pSocketLapped, dwThancferred);
				break;
			}
		case enOperationType_Recv: //数据读取
			{
				CWHDataLocker ThreadLock(pTCPNetworkItem->GetCriticalSection());
				pTCPNetworkItem->OnRecvCompleted((COverLappedRecv *)pSocketLapped, dwThancferred);
				break;
			}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//应答线程对象
//构造函数
CTCPNetworkThreadAccept::CTCPNetworkThreadAccept()
{
	m_hListenSocket = INVALID_SOCKET;
	m_hCompletionPort = NULL;
	m_pTCPNetworkEngine = NULL;
}
//析构函数
CTCPNetworkThreadAccept::~CTCPNetworkThreadAccept()
{

}
//配置函数
bool CTCPNetworkThreadAccept::InitThread(HANDLE hCompletionPort, SOCKET hListenSocket, CTCPNetworkEngine * pNetworkEngine)
{
	//校验参数
	ASSERT(hCompletionPort != NULL);
	ASSERT(hListenSocket != INVALID_SOCKET);
	ASSERT(pNetworkEngine != NULL);
	//设置变量
	m_hListenSocket = hListenSocket;
	m_hCompletionPort = hCompletionPort;
	m_pTCPNetworkEngine = pNetworkEngine;

	return true;
}
//运行函数
bool CTCPNetworkThreadAccept::OnEventThreadRun()
{
	//校验参数
	ASSERT(m_hCompletionPort != NULL);
	ASSERT(m_pTCPNetworkEngine != NULL);

	//变量定义
	SOCKET hConnectSocket = INVALID_SOCKET;
	CTCPNetworkItem * pTCPNetworkItem = NULL;

	try
	{
		//监听连接
		SOCKADDR_IN SocketAddr;
		INT nBufferSize = sizeof(SocketAddr);
		hConnectSocket = WSAAccept(m_hListenSocket, (SOCKADDR*)&SocketAddr, &nBufferSize, NULL, NULL);

		//退出判断
		if (hConnectSocket == INVALID_SOCKET) return false;

		//获取连接
		pTCPNetworkItem = m_pTCPNetworkEngine->ActiveNetworkItem();

		//失败判断
		if (pTCPNetworkItem == NULL)
		{
			ASSERT(FALSE);
			throw TEXT("申请连接对象失败");
		}

		//锁定对象
		CWHDataLocker ThreadLock(pTCPNetworkItem->GetCriticalSection());

		//绑定对象
		pTCPNetworkItem->Attach(hConnectSocket, SocketAddr.sin_addr.S_un.S_addr);
		CreateIoCompletionPort((HANDLE)hConnectSocket, m_hCompletionPort, (ULONG_PTR)pTCPNetworkItem, 0);

		//发起接收
		pTCPNetworkItem->RecvData();
	}
	catch (...)
	{
		//清理对象
		ASSERT(pTCPNetworkItem == NULL);

		//关闭连接
		if (hConnectSocket != INVALID_SOCKET)
		{
			closesocket(hConnectSocket);
		}
	}

	return true;
}

//检测线程类

//构造函数
CTCPNetworkThreadDetect::CTCPNetworkThreadDetect()
{
	m_dwPileTime = 0L;
	m_dwDetectTime = 10000L;
	m_pTCPNetworkEngine = NULL;
}
//析构函数
CTCPNetworkThreadDetect::~CTCPNetworkThreadDetect()
{

}
//配置函数
bool CTCPNetworkThreadDetect::InitThread(CTCPNetworkEngine * pNetworkEngine, DWORD dwDetectTime)
{
	//校验参数
	ASSERT(pNetworkEngine != NULL);

	//设置变量
	m_dwPileTime = 0L;
	m_dwDetectTime = dwDetectTime;
	m_pTCPNetworkEngine = pNetworkEngine;

	return true;
}
//运行函数
bool CTCPNetworkThreadDetect::OnEventThreadRun()
{
	//校验参数
	ASSERT(m_pTCPNetworkEngine != NULL);

	//设置间隔
	Sleep(200);
	m_dwPileTime += 200L;

	//检测连接
	if (m_dwPileTime >= m_dwDetectTime)
	{
		m_dwPileTime = 0L;
		m_pTCPNetworkEngine->DetectSocket();
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////

//网络引擎

//构造函数
CTCPNetworkEngine::CTCPNetworkEngine()
{
	//验证变量
	m_bValidate = false;
	m_bNormalRun = true;

	//辅助变量
	m_bService = false;
	ZeroMemory(m_cbBuffer, sizeof(m_cbBuffer));


	//配置变量
	m_wMaxConnect = 0;
	m_wServicePort = 0;
	m_dwDetectTime = 10000L;

	//内核变量
	m_hServerSocket = INVALID_SOCKET;
	m_hCompletionPort = NULL;
	m_pITCPNetworkEngineEvent = NULL;
}
//析构函数
CTCPNetworkEngine::~CTCPNetworkEngine()
{

}

//接口查询
VOID * CTCPNetworkEngine::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(IServiceModule, Guid, dwQueryVer);
	QUERYINTERFACE(ITCPNetworkEngine, Guid, dwQueryVer);
	QUERYINTERFACE(IAsynchronismEngineSink, Guid, dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(ITCPNetworkEngine, Guid, dwQueryVer);

	return NULL;
}

//启动服务
bool CTCPNetworkEngine::StartService()
{
	//状态校验
	ASSERT(m_bService == false);
	if (m_bService == true) return false;

	//判断端口
	if (m_wServicePort == 0)
	{
		m_wServicePort = 3000;
	}

	//系统信息
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);
	DWORD dwThreadCount = SystemInfo.dwNumberOfProcessors;

	//完成端口
	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, SystemInfo.dwNumberOfProcessors);
	if (m_hCompletionPort == NULL) return false;

	//建立网络
	SOCKADDR_IN SocketAddr;
	ZeroMemory(&SocketAddr, sizeof(SocketAddr));

	SocketAddr.sin_family = AF_INET;
	SocketAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	SocketAddr.sin_port = htons(m_wServicePort);
	m_hServerSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	//错误判断
	if (m_hServerSocket == INVALID_SOCKET)
	{
		LPCTSTR pszString = TEXT("系统资源不足或者 TCP/IP 协议没有安装，网络启动失败");
		g_TraceServiceManager.TraceString(pszString, TraceLevel_Exception);
		return false;
	}
	//将完成端口与socket绑定 用于监听
	for (;;)
	{
		if (0 != bind(m_hServerSocket, (SOCKADDR*)&SocketAddr, sizeof(SocketAddr)))
		{
			closesocket(m_hServerSocket);
		}
		else
		{
			break;
		}
		//绑定失败再次尝试
		m_hServerSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		//错误判断
		if (m_hServerSocket == INVALID_SOCKET)
		{
			LPCTSTR pszString = TEXT("系统资源不足或者 TCP/IP 协议没有安装，网络启动失败");
			g_TraceServiceManager.TraceString(pszString, TraceLevel_Exception);
			return false;
		}
		m_wServicePort++;
		SocketAddr.sin_family = AF_INET;
		SocketAddr.sin_addr.S_un.S_addr = INADDR_ANY;
		SocketAddr.sin_port = htons(m_wServicePort);
	}

	//监听端口
	if (listen(m_hServerSocket, 200) == SOCKET_ERROR)
	{
		TCHAR szString[512] = TEXT("");
		_sntprintf(szString, CountArray(szString), TEXT("端口正被其他服务占用，监听 %ld 端口失败"), m_wServicePort);
		g_TraceServiceManager.TraceString(szString, TraceLevel_Exception);
		return false;
	}

	//异步引擎
	IUnknownEx * pIUnknownEx = QUERY_ME_INTERFACE(IUnknownEx);
	if (m_AsynchronismEngine.SetAsynchronismSink(pIUnknownEx) == false)
	{
		ASSERT(FALSE);
		return false;
	}

	//网页验证
	WebAttestation();

	//启动服务
	if (m_AsynchronismEngine.StartService() == false)
	{
		ASSERT(FALSE);
		return false;
	}

	//创建读写线程
	for (DWORD i = 0; i < dwThreadCount; ++i)
	{
		CTCPNetworkThreadReadWrite * pNetworkRSThread = new CTCPNetworkThreadReadWrite();
		if (pNetworkRSThread->InitThread(m_hCompletionPort) == false) return false;
		m_SocketRWThreadArray.Add(pNetworkRSThread);
	}

	//应答线程
	if (m_SocketAcceptThread.InitThread(m_hCompletionPort, m_hServerSocket, this) == false) return false;
	if (m_SocketAcceptThread.StartThread() == false) return false;

	//启动读写线程
	for (DWORD i = 0; i < dwThreadCount; ++i)
	{
		CTCPNetworkThreadReadWrite * pNetworkRWThread = m_SocketRWThreadArray[i];
		ASSERT(pNetworkRWThread != NULL);
		if (pNetworkRWThread->StartThread() == false) return false;
	}

	//检测线程
	m_SocketDetectThread.InitThread(this, m_dwDetectTime);
	if (m_SocketDetectThread.StartThread() == false) return false;

	//设置变量
	m_bService = true;

	return true;
}
//停止服务
bool CTCPNetworkEngine::ConcludeService()
{
	//设置变量
	m_bService = false;

	//检测线程
	m_SocketDetectThread.ConcludeThread(INFINITE);

	//应答线程
	if (m_hServerSocket != INVALID_SOCKET)
	{
		closesocket(m_hServerSocket);
		m_hServerSocket = INVALID_SOCKET;
	}

	m_SocketAcceptThread.ConcludeThread(INFINITE);

	//异步引擎
	m_AsynchronismEngine.ConcludeService();

	//读写线程
	INT_PTR nCount = m_SocketRWThreadArray.GetCount();
	if (m_hCompletionPort != NULL)
	{
		for (INT_PTR i = 0; i < nCount; ++i)
			//指示线程立即结束并退出
			PostQueuedCompletionStatus(m_hCompletionPort, 0, NULL, NULL);
	}
	for (INT_PTR i = 0; i < nCount; ++i)
	{
		CTCPNetworkThreadReadWrite * pSocketThread = m_SocketRWThreadArray[i];
		ASSERT(pSocketThread != NULL);
		pSocketThread->ConcludeThread(INFINITE);
		SafeDelete(pSocketThread);
	}
	m_SocketRWThreadArray.RemoveAll();

	//完成端口
	if (m_hCompletionPort != NULL)
	{
		CloseHandle(m_hCompletionPort);
		m_hCompletionPort = NULL;
	}

	//关闭连接
	CTCPNetworkItem * pTCPNetworkItem = NULL;
	for (INT_PTR i = 0; i < nCount; ++i)
	{
		pTCPNetworkItem = m_NetworkItemActive[i];
		pTCPNetworkItem->CloseSocket(pTCPNetworkItem->GetRountID());
		pTCPNetworkItem->ResumeData();
	}

	//重置数据
	m_NetworkItemBuffer.Append(m_NetworkItemActive);
	m_NetworkItemActive.RemoveAll();
	m_TempNetworkItemArray.RemoveAll();

	return true;
}
//配置端口
WORD CTCPNetworkEngine::GetServicePort()
{
	return m_wServicePort;
}
//当前端口
WORD CTCPNetworkEngine::GetCurrentPort()
{
	return m_wServicePort;
}
//设置接口
bool CTCPNetworkEngine::SetTCPNetworkEngineEvent(IUnknownEx * pIUnknownEx)
{
	//状态校验
	ASSERT(m_bService == false);
	if (m_bService == true) return false;

	//查询接口
	m_pITCPNetworkEngineEvent = QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx, ITCPNetworkEngineEvent);

	//错误判断
	if (m_pITCPNetworkEngineEvent == NULL)
	{
		ASSERT(FALSE);
		return false;
	}

	return true;
}
//设置参数
bool CTCPNetworkEngine::SetServiceParameter(WORD wServicePort, WORD wMaxConnect, LPCTSTR  pszCompilation)
{
	//状态校验
	ASSERT(m_bService == false);
	if (m_bService == true) return false;

	//设置变量
	ASSERT(wServicePort != 0);
	m_wMaxConnect = wMaxConnect;
	m_wServicePort = wServicePort;

	return true;
}
//发送函数
bool CTCPNetworkEngine::SendData(DWORD dwSocketID, WORD wMainCmdID, WORD wSubCmdID)
{
	//缓冲锁定
	CWHDataLocker ThreadLock(m_BufferLocked);
	tagSendDataRequest * pSendDataRequest = (tagSendDataRequest *)m_cbBuffer;

	//构造数据
	pSendDataRequest->wDataSize = 0;
	pSendDataRequest->wSubCmdID = wSubCmdID;
	pSendDataRequest->wMainCmdID = wMainCmdID;
	pSendDataRequest->wIndex = SOCKET_INDEX(dwSocketID);
	pSendDataRequest->wRountID = SOCKET_ROUNTID(dwSocketID);

	//发送请求
	WORD wSendSize = sizeof(tagSendDataRequest) - sizeof(pSendDataRequest->cbSendBuffer);
	return m_AsynchronismEngine.PostAsynchronismData(ASYNCHRONISM_SEND_DATA, m_cbBuffer, wSendSize);
}
//发送函数
bool CTCPNetworkEngine::SendData(DWORD dwSocketID, WORD wMainCmdID, WORD wSubCmdID, VOID * pData, WORD wDataSize)
{
	//校验数据
}
//批量发送
bool CTCPNetworkEngine::SendDataBatch(WORD wMainCmdID, WORD wSubCmdID, VOID * pData, WORD wDataSize, BYTE cbBatchMask)
{

}