#ifndef TCP_NETWORK_ENGINE_HEAD_FILE
#define TCP_NETWORK_ENGINE_HEAD_FILE

#include "KernelEngineHead.h"

//////////////////////////////////////////////////////////////////////////
//枚举定义

//操作类型
enum enOperationType
{
	enOperationType_Send,		//发送类型
	enOperationType_Recv,		//接受类型
};

//////////////////////////////////////////////////////////////////////////

//类说明
class COverLappedSend;
class COverLappedRecv;
class CTCPNetworkItem;
class CTCPNetworkEngine;

//数字定义
typedef class CWHArray<COverLappedSend *>		COverLappedSendPtr;
typedef class CWHArray<COverLappedRecv *>		COverLappedRecvPtr;

//////////////////////////////////////////////////////////////////////////
//接口定义

//连接对象回调接口
interface ITCPNetworkItemSink
{
	//绑定事件
	virtual bool OnEventSocketBind(CTCPNetworkItem * pCTCPNetworkItem) = NULL;
	//关闭事件
	virtual bool OnEventSocketShut(CTCPNetworkItem * pCTCPNetworkItem) = NULL;
	//读取事件
	virtual bool OnEventSocketRead(TCP_Command Command, VOID *pData, WORD wDataSize, CTCPNetworkItem * pCTCPNetworkItem) = NULL;
};

//////////////////////////////////////////////////////////////////////////

//重叠结构定义
class COverLapped
{
	//变量定义
public:
	WSABUF						m_WSABuffer;						//数据指针
	OVERLAPPED					m_OverLapped;						//重叠结构
	const enOperationType		m_OperationType;					//操作类型

	//函数定义
public:
	//构造函数
	COverLapped(enOperationType OperationType);
	//析构函数
	virtual ~COverLapped();

	//信息函数
public:
	//获取类型
	enOperationType GetOperationType() { return m_OperationType; }
};

//发送重叠结构
class COverLappedSend : public COverLapped
{
	//变量定义
public:
	BYTE						m_cbBuffer[SOCKET_TCP_BUFFER];						//数据缓冲

	//函数定义
public:
	//构造函数
	COverLappedSend();
	//析构函数
	virtual ~COverLappedSend();
};

//接收重叠结构
class COverLappedRecv : public COverLapped
{
	//函数定义
public:
	//构造函数
	COverLappedRecv();
	//析构函数
	virtual ~COverLappedRecv();
};


//////////////////////////////////////////////////////////////////////////

//连接子项
class CTCPNetworkItem
{
	//友元声明
	friend class CTCPNetworkEngine;

	//连接属性
protected:
	DWORD						m_dwClientIP;						//连接地址
	DWORD						m_dwActiveTime;						//激活时间

	//内核变量
protected:
	WORD						m_wIndex;							//连接索引
	WORD						m_wRountID;							//循环索引
	WORD						m_wSurvivalTime;					//生存时间
	SOCKET						m_hSocketHandle;					//连接句柄
	CCriticalSection			m_CriticalSection;					//锁定对象
	ITCPNetworkItemSink *		m_pITCPNetworkItemSink;				//回调接口

	//状态变量
protected:
	bool						m_bSendIng;							//发送标志
	bool						m_bRecvIng;							//接收标志
	bool						m_bShutDown;						//关闭标志
	bool						m_bAllowBatch;						//接受群发
	BYTE						m_bBatchMask;						//群发标示

	//接受变量
protected:
	WORD						m_wRecvSize;						//接收长度
	BYTE						m_cbRecvBuf[SOCKET_TCP_BUFFER * 5];	//接收缓冲

	//计数变量
protected:
	DWORD						m_dwSendTickCount;					//发送时间
	DWORD						m_dwRecvTickCount;					//接收时间
	DWORD						m_dwSendPacketCount;				//发送计数
	DWORD						m_dwRecvPacketCount;				//接受计数

	//加密数据
protected:
	BYTE						m_cbSendRound;						//字节映射
	BYTE						m_cbRecvRound;						//字节映射
	DWORD						m_dwSendXorKey;						//发送密钥
	DWORD						m_dwRecvXorKey;						//接收密钥

	//内部变量
protected:
	COverLappedRecv				m_OverLappedRecv;					//重叠结构
	COverLappedSendPtr			m_OverLappedSendActive;				//重叠结构

	//缓冲变量
protected:
	CCriticalSection			m_SendBufferSection;				//锁定对象
	COverLappedSendPtr			m_OverLappedSendBuffer;				//重叠结构

	//函数定义
public:
	//构造函数
	CTCPNetworkItem(WORD wIndex, ITCPNetworkItemSink * pITCPNetworkItemSink);
	//析构函数
	virtual ~CTCPNetworkItem();

	//获取标识函数
public:
	//获取索引
	inline WORD GetIndex(){ return m_wIndex; }
	//获取计数
	inline WORD GetRountID(){ return m_wRountID; }
	//获取标识
	inline DWORD GetIdentifierID(){ return MAKELONG(m_wIndex, m_wRountID); }

	//获取属性函数
public:
	//获取地址
	inline DWORD GetClientIP() { return m_dwClientIP; }
	//激活时间
	inline DWORD GetActiveTime() { return m_dwActiveTime; }
	//发送时间
	inline DWORD GetSendTickCount() { return m_dwSendTickCount; }
	//接收时间
	inline DWORD GetRecvTickCount() { return m_dwRecvTickCount; }
	//发送包数
	inline DWORD GetSendPacketCount() { return m_dwSendPacketCount; }
	//接收包数
	inline DWORD GetRecvPacketCount() { return m_dwRecvPacketCount; }
	//锁定对象
	inline CCriticalSection & GetCriticalSection() { return m_CriticalSection; }

	//获取状态函数
public:
	//群发允许
	inline bool IsAllowBatch() { return m_bAllowBatch; }
	//发送允许
	inline bool IsAllowSendData() { return m_dwRecvPacketCount > 0L; }
	//判断连接
	inline bool IsValidSocket() { return (m_hSocketHandle != INVALID_SOCKET); }
	//群发标示
	inline BYTE GetBatchMask() { return m_bBatchMask; }

	//管理函数
public:
	//绑定对象
	DWORD Attach(SOCKET hSocket, DWORD dwClientIP);
	//恢复数据
	DWORD ResumeData();

	//控制函数
public:
	//发送函数
	bool SendData(WORD wMainCmdID, WORD wSubCmdID, WORD wRountID);
	//发送函数
	bool SendData(VOID * pData, WORD wDataSize, WORD wMainCmdID, WORD wSubCmdID, WORD wRountID);
	//接收操作
	bool RecvData();
	//关闭连接
	bool CloseSocket(WORD wRountID);

	//状态管理
public:
	//设置关闭
	bool ShutDownSocket(WORD wRountID);
	//允许群发
	bool AllowBatchSend(WORD wRountID, bool bAllowBatch, BYTE cbBatchMask);

	//通知接口
public:
	//发送完成
	bool OnSendCompleted(COverLappedSend * pOverLappedSend, DWORD dwThancferred);
	//接收完成
	bool OnrRecvCompleted(COverLappedSend * pOverLappedRecv, DWORD dwThancferred);
	//关闭完成
	bool OnCloseCompleted();

	//加密函数
private:
	//加密数据
	WORD EncryptBuffer(BYTE pcbDataBuffer[], WORD wDataSize, WORD wBufferSize);
	//解密数据
	WORD CrevasseBuffer(BYTE pcbDataBuffer[], WORD wDataSize);

	//内联函数
private:
	//随机映射
	inline WORD SeedRandMap(WORD wSeed);
	//发送映射数据
	inline BYTE MapSendByte(BYTE cbData);
	//接收映射数据
	inline BYTE MapRecvByte(BYTE cbData);

	//辅助函数
private:
	//发送判断
	inline bool SendVerdict(WORD wRountID);
	//获取发送重叠结构
	inline COverLappedSend * GetSendOverLapped(WORD wPacketSize);
};


#endif