#include "stdafx.h"
#include "TCPSocketService.h"


//消息定义
#define WM_SOCKET_NOTIFY			WM_USER+100							//消息标识
#define WM_SERVICE_REQUEST			WM_USER+101							//服务请求

//////////////////////////////////////////////////////////////////////////
//动作定义

#define REQUEST_CONNECT				1									//请求连接
#define REQUEST_SEND_DATA			2									//请求发送
#define REQUEST_SEND_DATA_EX		3									//请求发送
#define REQUEST_CLOSE_SOCKET		4									//请求关闭


//构造函数
CTCPSocketServiceThread::CTCPSocketServiceThread()
{
	//内核变量
	m_hWnd = NULL;
	m_hSocket = INVALID_SOCKET;
	m_TCPSocketStatus = SOCKET_STATUS_IDLE;
	//接收变量
	m_wRecvSize = 0;
	ZeroMemory(m_cbRecvBuf, sizeof(m_cbRecvBuf));

	//缓冲变量
	m_bNeedBuffer = false;
	m_dwBufferData = 0;
	m_dwBufferSize = 0;
	m_pcbDataBuffer = NULL;
	//加密数据
	m_cbSendRound = 0;
	m_cbRecvRound = 0;
	m_dwSendXorKey = 0;
	m_dwRecvXorKey = 0;

	m_dwSendTickCount = 0;
	m_dwRecvTickCount = 0;
	m_dwSendPacketCount = 0;
	m_dwRecvPacketCount = 0;
	return;
}
//析构函数
CTCPSocketServiceThread::~CTCPSocketServiceThread()
{
	//关闭连接
	if (m_hSocket != INVALID_SOCKET)
	{
		closesocket(m_hSocket);
		m_hSocket = INVALID_SOCKET;
	}

	//删除缓冲
	SafeDeleteArray(m_pcbDataBuffer);

	return;
}
//运行事件
bool CTCPSocketServiceThread::OnEventThreadRun()
{
	//获取消息
	MSG Message;
	if (GetMessage(&Message, NULL, 0, 0) == FALSE) return false;

	//消息处理
	switch (Message.message)
	{
		case WM_SOCKET_NOTIFY:		//网络消息
			{
				OnSocketNotify(Message.wParam, Message.lParam);
				return true;
			}
		case WM_SERVICE_REQUEST:	//服务请求
			{
				OnServiceRequest(Message.wParam, Message.lParam);
				return true;
			}
		default:					//默认处理
			{
				DefWindowProc(Message.hwnd, Message.message, Message.wParam, Message.lParam);
				return true;
			}
	}

	return false;
}
//开始事件
bool CTCPSocketServiceThread::OnEventThreadStart()
{
	//变量定义
	WNDCLASS WndClass;
	ZeroMemory(&WndClass, sizeof(WndClass));

	//设置变量
	WndClass.lpfnWndProc = DefWindowProc;
	WndClass.hInstance = AfxGetInstanceHandle();
	WndClass.lpszClassName = TEXT("TCPSocketStatusServiceThread");

	//注册窗口
	RegisterClass(&WndClass);

	//创建窗口
	HWND hParentWnd = GetDesktopWindow();
	m_hWnd = CreateWindow(WndClass.lpszClassName, NULL, WS_CHILD, 0, 0, 0, 0, hParentWnd, NULL, WndClass.hInstance, NULL);

	return true;
}
//终止事件
bool CTCPSocketServiceThread::OnEventThreadConclude()
{
	//关闭窗口
	if (m_hWnd != NULL)
	{
		DestroyWindow(m_hWnd);
		m_hWnd = NULL;
	}

	//关闭连接
	PerformCloseSocket(SHUT_REASON_INSIDE);

	//清理队列
	CWHDataLocker ThreadLock(m_CriticalSection);
	m_DataQueue.RemoveData(false);

	return true;
}
//结束线程
bool CTCPSocketServiceThread::ConcludeThread(DWORD dwMillSeconds)
{
	//退出消息
	if (isRuning() == true)
	{
		PostThreadMessage(WM_QUIT, 0, 0);
	}

	return __super::ConcludeThread(dwMillSeconds);
}
//投递请求
bool CTCPSocketServiceThread::PostThreadRequest(WORD wIdentifier, VOID * const pBuffer, WORD wDataSize)
{
	CWHDataLocker ThreadLock(m_CriticalSection);
	m_DataQueue.InsertData(wIdentifier, pBuffer, wDataSize);

	//发送消息
	ASSERT(m_hWnd != NULL);
	if (m_hWnd != NULL)
		PostMessage(m_hWnd, WM_SERVICE_REQUEST, wDataSize, GetCurrentThreadId());

	return true;
}
//关闭连接
VOID CTCPSocketServiceThread::PerformCloseSocket(BYTE cbShutReason)
{
	//内核变量
	m_wRecvSize = 0;
	m_dwBufferData = 0L;
	m_bNeedBuffer = false;
	m_TCPSocketStatus = SOCKET_STATUS_IDLE;

	//加密数据
	m_cbSendRound = 0;
	m_cbRecvRound = 0;
	m_dwSendXorKey = 0;
	m_dwRecvXorKey = 0;

	//计数变量
	m_dwSendTickCount = 0;
	m_dwRecvTickCount = 0;
	m_dwSendPacketCount = 0;
	m_dwRecvPacketCount = 0;

	//关闭判断
	if (m_hSocket != INVALID_SOCKET)
	{
		//关闭连接
		closesocket(m_hSocket);
		m_hSocket == INVALID_SOCKET;

		//关闭原因
		if (cbShutReason != SHUT_REASON_INSIDE)
		{
			CTCPSocketService * pTCPSocketStatusService = CONTAINING_RECORD(this, CTCPSocketServiceThread, m_TCPSocketServiceThread);
			pTCPSocketStatusService->OnSocketShut(cbShutReason);
		}
	}

	return;
}
//连接服务器
DWORD CTCPSocketServiceThread::PerformConnect(DWORD dwServerIP, WORD wPort)
{
	//校验参数
	ASSERT(m_hSocket == INVALID_SOCKET);
	ASSERT(m_TCPSocketStatus == SOCKET_STATUS_IDLE);
	ASSERT(dwServerIP != INADDR_NONE && dwServerIP != 0);

	try
	{
		//校验状态
		if (m_hSocket != INVALID_SOCKET) throw CONNECT_EXCEPTION;
		if (m_TCPSocketStatus != SOCKET_STATUS_IDLE) throw CONNECT_EXCEPTION;
		if (dwServerIP == INADDR_NONE && dwServerIP == 0) throw CONNECT_EXCEPTION;

		//设置参数
		m_wRecvSize = 0;
		m_cbRecvRound = 0;
		m_cbSendRound = 0;
		m_dwRecvXorKey = 0xA8C543FF;
		m_dwSendXorKey = 0xA8C543FF;
		m_dwSendTickCount = GetTickCount() / 1000L;
		m_dwRecvTickCount = GetTickCount() / 1000L;

		//建立SOCKET
		m_hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_hSocket == INVALID_SOCKET) throw CONNECT_EXCEPTION;

		//变量定义
		SOCKADDR_IN SocketAddr;
		ZeroMemory(&SocketAddr, sizeof(SocketAddr));

		//设置变量
		SocketAddr.sin_family = AF_INET;
		SocketAddr.sin_port = htons(wPort);
		SocketAddr.sin_addr.S_un.S_addr = dwServerIP;

		//绑定窗口
		WSAAsyncSelect(m_hSocket, m_hWnd, WM_SOCKET_NOTIFY, FD_READ | FD_CONNECT | FD_CLOSE | FD_WRITE);

		//连接服务器
		INT nErrorCode = connect(m_hSocket, (SOCKADDR *)&SocketAddr, sizeof(SocketAddr));
		if ((nErrorCode == SOCKET_ERROR) && (WSAGetLastError() != WSAEWOULDBLOCK)) throw CONNECT_EXCEPTION;

		//设置变量
		m_TCPSocketStatus = SOCKET_STATUS_WAIT;

		return CONNECT_SUCCESS;
	}
	catch (...)
	{

	}

	return CONNECT_EXCEPTION;
}
//发送函数
DWORD CTCPSocketServiceThread::PerformSendData(WORD wMainCmdID, WORD wSubCmdID)
{
	//校验状态
	if (m_hSocket == INVALID_SOCKET) return false;
	if (m_TCPSocketStatus != SOCKET_STATUS_CONNECT) return false;

	//变量定义
	BYTE cbDataBuffer[SOCKET_TCP_BUFFER];
	TCP_Head * pHead = (TCP_Head *)cbDataBuffer;

	//设置变量
	pHead->CommandInfo.wMainCmdID = wMainCmdID;
	pHead->CommandInfo.wSubCmdID = wSubCmdID;

	//加密数据
	WORD wSendSize = EncryptBuffer(cbDataBuffer, sizeof(TCP_Head), sizeof(cbDataBuffer));

	//发送数据
	return SendBuffer(cbDataBuffer, wSendSize);
}
//发送函数
DWORD CTCPSocketServiceThread::PerformSendData(WORD wMainCmdID, WORD wSubCmdID, VOID * pData, WORD wDataSize)
{
	//校验状态
	if (m_hSocket == INVALID_SOCKET) return 0;
	if (m_TCPSocketStatus != SOCKET_STATUS_CONNECT) return 0;

	//校验大小
	ASSERT(wDataSize <= SOCKET_TCP_BUFFER);
	if (wDataSize > SOCKET_TCP_BUFFER) return 0;

	//变量定义
	BYTE cbDataBuffer[SOCKET_TCP_BUFFER];
	TCP_Head * pHead = (TCP_Head *)cbDataBuffer;

	//设置变量
	pHead->CommandInfo.wMainCmdID = wMainCmdID;
	pHead->CommandInfo.wSubCmdID = wSubCmdID;

	if (wDataSize > 0)
	{
		ASSERT(pData != NULL);
		CopyMemory(pHead + 1, pData, wDataSize);
	}

	//加密数据
	WORD wSendSize = EncryptBuffer(cbDataBuffer, sizeof(TCP_Head) + wDataSize, sizeof(cbDataBuffer));

	//发送数据
	return SendBuffer(cbDataBuffer, wSendSize);
}
//发送函数
DWORD CTCPSocketServiceThread::SendBuffer(VOID * pBuffer, WORD wSendSize)
{
	//变量定义
	WORD wTotalCount = 0;
	
	//设置变量
	m_dwSendTickCount = GetTickCount() / 1000L;

	//发送数据
	while (m_bNeedBuffer == false && wTotalCount < wSendSize)
	{
		//发送数据
		INT nSendCount = send(m_hSocket, (char*)pBuffer + wTotalCount, wSendSize + wTotalCount, 0);

		//错误判断
		if (nSendCount == SOCKET_ERROR)
		{
			//缓冲判断
			if (WSAGetLastError() == WSAEWOULDBLOCK)
			{
				AmortizeBuffer((LPBYTE)pBuffer + wTotalCount, wSendSize - wTotalCount);
				return wSendSize;
			}

			//关闭连接
			PerformCloseSocket(SHUT_REASON_EXCEPTION);

			return 0;
		}
		else
		{
			//设置变量
			wTotalCount += nSendCount;
		}
	}

	//缓冲数据
	if (wTotalCount>wSendSize)
	{
		AmortizeBuffer((LPBYTE)pBuffer + wTotalCount, wSendSize - wTotalCount);
	}

	return wSendSize;
}
//缓冲数据
VOID CTCPSocketServiceThread::AmortizeBuffer(VOID * pData, WORD wDataSize)
{
	//申请缓冲
	if (m_dwBufferData + wDataSize > m_dwBufferSize)
	{
		//变量定义
		LPBYTE pcbDataBuffer = NULL;
		LPBYTE pcbDeleteBuffer = m_pcbDataBuffer;

		//计算大小
		DWORD dwNeedSize = m_dwBufferData + wDataSize;
		DWORD dwApplySize = __max(dwNeedSize, m_dwBufferSize * 2);

		//申请缓冲
		try
		{
			pcbDataBuffer = new BYTE[dwApplySize];
		}
		catch (...)
		{ }

		//失败判断
		if (pcbDataBuffer == NULL)
		{
			PerformCloseSocket(SHUT_REASON_EXCEPTION);
			return;
		}

		//设置变量
		m_dwBufferSize = dwApplySize;
		m_pcbDataBuffer = pcbDataBuffer;
		CopyMemory(m_pcbDataBuffer, pcbDeleteBuffer, m_dwBufferData);

		//删除缓冲
		SafeDeleteArray(pcbDeleteBuffer);
	}

	//设置变量
	m_bNeedBuffer = true;
	m_dwBufferData += wDataSize;
	CopyMemory(m_pcbDataBuffer + m_dwBufferData - wDataSize, pData, wDataSize);

	return;
}
//解密数据
WORD CTCPSocketServiceThread::CrevasseBuffer(BYTE cbDataBuffer[], WORD wDataSize)
{

}
//加密数据
WORD CTCPSocketServiceThread::EncryptBuffer(BYTE cbDataBuffer[], WORD wDataSize, WORD wBufferSize)
{

}
//网络消息
LRESULT CTCPSocketServiceThread::OnSocketNotify(WPARAM wParam, LPARAM lParam)
{

}
//请求消息
LRESULT CTCPSocketServiceThread::OnServiceRequest(WPARAM wParam, LPARAM lParam)
{

}
//网络读取
LRESULT CTCPSocketServiceThread::OnSocketNotifyRead(WPARAM wParam, LPARAM lParam)
{

}
//网络发送
LRESULT CTCPSocketServiceThread::OnSocketNotifyWrite(WPARAM wParam, LPARAM lParam)
{

}
//网络关闭
LRESULT CTCPSocketServiceThread::OnSocketNotifyClose(WPARAM wParam, LPARAM lParam)
{

}
//网络连接
LRESULT CTCPSocketServiceThread::OnSocketNotifyConnect(WPARAM wParam, LPARAM lParam)
{

}
//随机映射
WORD CTCPSocketServiceThread::SeedRandMap(WORD wSeed)
{

}
//发送映射
BYTE CTCPSocketServiceThread::MapSendByte(BYTE cbData)
{

}
//接收映射
BYTE CTCPSocketServiceThread::MapRecvByte(BYTE cbData)
{

}