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

}
//发送函数
bool CTCPNetworkItem::SendData(VOID * pData, WORD wDataSize, WORD wMainCmdID, WORD wSubCmdID, WORD wRountID)
{

}
//接收操作
bool CTCPNetworkItem::RecvData()
{

}
//关闭连接
bool CTCPNetworkItem::CloseSocket(WORD wRountID)
{

}
//设置关闭
bool CTCPNetworkItem::ShutDownSocket(WORD wRountID)
{

}
//允许群发
bool CTCPNetworkItem::AllowBatchSend(WORD wRountID, bool bAllowBatch, BYTE cbBatchMask)
{

}
//发送完成
bool CTCPNetworkItem::OnSendCompleted(COverLappedSend * pOverLappedSend, DWORD dwThancferred)
{

}
//接收完成
bool CTCPNetworkItem::OnrRecvCompleted(COverLappedSend * pOverLappedRecv, DWORD dwThancferred)
{

}
//关闭完成
bool CTCPNetworkItem::OnCloseCompleted()
{

}
//加密数据
WORD CTCPNetworkItem::EncryptBuffer(BYTE pcbDataBuffer[], WORD wDataSize, WORD wBufferSize)
{

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

}