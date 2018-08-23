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
	WORD							wRountid;							//循环索引
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

}
//接收完成
bool CTCPNetworkItem::OnrRecvCompleted(COverLappedSend * pOverLappedRecv, DWORD dwThancferred)
{

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

}
//随机映射
WORD CTCPNetworkItem::SeedRandMap(WORD wSeed)
{

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