#include "stdafx.h"
#include "TCPSocketService.h"


//��Ϣ����
#define WM_SOCKET_NOTIFY			WM_USER+100							//��Ϣ��ʶ
#define WM_SERVICE_REQUEST			WM_USER+101							//��������

//////////////////////////////////////////////////////////////////////////
//��������

#define REQUEST_CONNECT				1									//��������
#define REQUEST_SEND_DATA			2									//������
#define REQUEST_SEND_DATA_EX		3									//������
#define REQUEST_CLOSE_SOCKET		4									//����ر�


//���캯��
CTCPSocketServiceThread::CTCPSocketServiceThread()
{
	//�ں˱���
	m_hWnd = NULL;
	m_hSocket = INVALID_SOCKET;
	m_TCPSocketStatus = SOCKET_STATUS_IDLE;
	//���ձ���
	m_wRecvSize = 0;
	ZeroMemory(m_cbRecvBuf, sizeof(m_cbRecvBuf));

	//�������
	m_bNeedBuffer = false;
	m_dwBufferData = 0;
	m_dwBufferSize = 0;
	m_pcbDataBuffer = NULL;
	//��������
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
//��������
CTCPSocketServiceThread::~CTCPSocketServiceThread()
{
	//�ر�����
	if (m_hSocket != INVALID_SOCKET)
	{
		closesocket(m_hSocket);
		m_hSocket = INVALID_SOCKET;
	}

	//ɾ������
	SafeDeleteArray(m_pcbDataBuffer);

	return;
}
//�����¼�
bool CTCPSocketServiceThread::OnEventThreadRun()
{
	//��ȡ��Ϣ
	MSG Message;
	if (GetMessage(&Message, NULL, 0, 0) == FALSE) return false;

	//��Ϣ����
	switch (Message.message)
	{
		case WM_SOCKET_NOTIFY:		//������Ϣ
			{
				OnSocketNotify(Message.wParam, Message.lParam);
				return true;
			}
		case WM_SERVICE_REQUEST:	//��������
			{
				OnServiceRequest(Message.wParam, Message.lParam);
				return true;
			}
		default:					//Ĭ�ϴ���
			{
				DefWindowProc(Message.hwnd, Message.message, Message.wParam, Message.lParam);
				return true;
			}
	}

	return false;
}
//��ʼ�¼�
bool CTCPSocketServiceThread::OnEventThreadStart()
{
	//��������
	WNDCLASS WndClass;
	ZeroMemory(&WndClass, sizeof(WndClass));

	//���ñ���
	WndClass.lpfnWndProc = DefWindowProc;
	WndClass.hInstance = AfxGetInstanceHandle();
	WndClass.lpszClassName = TEXT("TCPSocketStatusServiceThread");

	//ע�ᴰ��
	RegisterClass(&WndClass);

	//��������
	HWND hParentWnd = GetDesktopWindow();
	m_hWnd = CreateWindow(WndClass.lpszClassName, NULL, WS_CHILD, 0, 0, 0, 0, hParentWnd, NULL, WndClass.hInstance, NULL);

	return true;
}
//��ֹ�¼�
bool CTCPSocketServiceThread::OnEventThreadConclude()
{
	//�رմ���
	if (m_hWnd != NULL)
	{
		DestroyWindow(m_hWnd);
		m_hWnd = NULL;
	}

	//�ر�����
	PerformCloseSocket(SHUT_REASON_INSIDE);

	//�������
	CWHDataLocker ThreadLock(m_CriticalSection);
	m_DataQueue.RemoveData(false);

	return true;
}
//�����߳�
bool CTCPSocketServiceThread::ConcludeThread(DWORD dwMillSeconds)
{
	//�˳���Ϣ
	if (isRuning() == true)
	{
		PostThreadMessage(WM_QUIT, 0, 0);
	}

	return __super::ConcludeThread(dwMillSeconds);
}
//Ͷ������
bool CTCPSocketServiceThread::PostThreadRequest(WORD wIdentifier, VOID * const pBuffer, WORD wDataSize)
{
	CWHDataLocker ThreadLock(m_CriticalSection);
	m_DataQueue.InsertData(wIdentifier, pBuffer, wDataSize);

	//������Ϣ
	ASSERT(m_hWnd != NULL);
	if (m_hWnd != NULL)
		PostMessage(m_hWnd, WM_SERVICE_REQUEST, wDataSize, GetCurrentThreadId());

	return true;
}
//�ر�����
VOID CTCPSocketServiceThread::PerformCloseSocket(BYTE cbShutReason)
{
	//�ں˱���
	m_wRecvSize = 0;
	m_dwBufferData = 0L;
	m_bNeedBuffer = false;
	m_TCPSocketStatus = SOCKET_STATUS_IDLE;

	//��������
	m_cbSendRound = 0;
	m_cbRecvRound = 0;
	m_dwSendXorKey = 0;
	m_dwRecvXorKey = 0;

	//��������
	m_dwSendTickCount = 0;
	m_dwRecvTickCount = 0;
	m_dwSendPacketCount = 0;
	m_dwRecvPacketCount = 0;

	//�ر��ж�
	if (m_hSocket != INVALID_SOCKET)
	{
		//�ر�����
		closesocket(m_hSocket);
		m_hSocket == INVALID_SOCKET;

		//�ر�ԭ��
		if (cbShutReason != SHUT_REASON_INSIDE)
		{
			CTCPSocketService * pTCPSocketStatusService = CONTAINING_RECORD(this, CTCPSocketServiceThread, m_TCPSocketServiceThread);
			pTCPSocketStatusService->OnSocketShut(cbShutReason);
		}
	}

	return;
}
//���ӷ�����
DWORD CTCPSocketServiceThread::PerformConnect(DWORD dwServerIP, WORD wPort)
{
	//У�����
	ASSERT(m_hSocket == INVALID_SOCKET);
	ASSERT(m_TCPSocketStatus == SOCKET_STATUS_IDLE);
	ASSERT(dwServerIP != INADDR_NONE && dwServerIP != 0);

	try
	{
		//У��״̬
		if (m_hSocket != INVALID_SOCKET) throw CONNECT_EXCEPTION;
		if (m_TCPSocketStatus != SOCKET_STATUS_IDLE) throw CONNECT_EXCEPTION;
		if (dwServerIP == INADDR_NONE && dwServerIP == 0) throw CONNECT_EXCEPTION;

		//���ò���
		m_wRecvSize = 0;
		m_cbRecvRound = 0;
		m_cbSendRound = 0;
		m_dwRecvXorKey = 0xA8C543FF;
		m_dwSendXorKey = 0xA8C543FF;
		m_dwSendTickCount = GetTickCount() / 1000L;
		m_dwRecvTickCount = GetTickCount() / 1000L;

		//����SOCKET
		m_hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_hSocket == INVALID_SOCKET) throw CONNECT_EXCEPTION;

		//��������
		SOCKADDR_IN SocketAddr;
		ZeroMemory(&SocketAddr, sizeof(SocketAddr));

		//���ñ���
		SocketAddr.sin_family = AF_INET;
		SocketAddr.sin_port = htons(wPort);
		SocketAddr.sin_addr.S_un.S_addr = dwServerIP;

		//�󶨴���
		WSAAsyncSelect(m_hSocket, m_hWnd, WM_SOCKET_NOTIFY, FD_READ | FD_CONNECT | FD_CLOSE | FD_WRITE);

		//���ӷ�����
		INT nErrorCode = connect(m_hSocket, (SOCKADDR *)&SocketAddr, sizeof(SocketAddr));
		if ((nErrorCode == SOCKET_ERROR) && (WSAGetLastError() != WSAEWOULDBLOCK)) throw CONNECT_EXCEPTION;

		//���ñ���
		m_TCPSocketStatus = SOCKET_STATUS_WAIT;

		return CONNECT_SUCCESS;
	}
	catch (...)
	{

	}

	return CONNECT_EXCEPTION;
}
//���ͺ���
DWORD CTCPSocketServiceThread::PerformSendData(WORD wMainCmdID, WORD wSubCmdID)
{
	//У��״̬
	if (m_hSocket == INVALID_SOCKET) return false;
	if (m_TCPSocketStatus != SOCKET_STATUS_CONNECT) return false;

	//��������
	BYTE cbDataBuffer[SOCKET_TCP_BUFFER];
	TCP_Head * pHead = (TCP_Head *)cbDataBuffer;

	//���ñ���
	pHead->CommandInfo.wMainCmdID = wMainCmdID;
	pHead->CommandInfo.wSubCmdID = wSubCmdID;

	//��������
	WORD wSendSize = EncryptBuffer(cbDataBuffer, sizeof(TCP_Head), sizeof(cbDataBuffer));

	//��������
	return SendBuffer(cbDataBuffer, wSendSize);
}
//���ͺ���
DWORD CTCPSocketServiceThread::PerformSendData(WORD wMainCmdID, WORD wSubCmdID, VOID * pData, WORD wDataSize)
{
	//У��״̬
	if (m_hSocket == INVALID_SOCKET) return 0;
	if (m_TCPSocketStatus != SOCKET_STATUS_CONNECT) return 0;

	//У���С
	ASSERT(wDataSize <= SOCKET_TCP_BUFFER);
	if (wDataSize > SOCKET_TCP_BUFFER) return 0;

	//��������
	BYTE cbDataBuffer[SOCKET_TCP_BUFFER];
	TCP_Head * pHead = (TCP_Head *)cbDataBuffer;

	//���ñ���
	pHead->CommandInfo.wMainCmdID = wMainCmdID;
	pHead->CommandInfo.wSubCmdID = wSubCmdID;

	if (wDataSize > 0)
	{
		ASSERT(pData != NULL);
		CopyMemory(pHead + 1, pData, wDataSize);
	}

	//��������
	WORD wSendSize = EncryptBuffer(cbDataBuffer, sizeof(TCP_Head) + wDataSize, sizeof(cbDataBuffer));

	//��������
	return SendBuffer(cbDataBuffer, wSendSize);
}
//���ͺ���
DWORD CTCPSocketServiceThread::SendBuffer(VOID * pBuffer, WORD wSendSize)
{
	//��������
	WORD wTotalCount = 0;
	
	//���ñ���
	m_dwSendTickCount = GetTickCount() / 1000L;

	//��������
	while (m_bNeedBuffer == false && wTotalCount < wSendSize)
	{
		//��������
		INT nSendCount = send(m_hSocket, (char*)pBuffer + wTotalCount, wSendSize + wTotalCount, 0);

		//�����ж�
		if (nSendCount == SOCKET_ERROR)
		{
			//�����ж�
			if (WSAGetLastError() == WSAEWOULDBLOCK)
			{
				AmortizeBuffer((LPBYTE)pBuffer + wTotalCount, wSendSize - wTotalCount);
				return wSendSize;
			}

			//�ر�����
			PerformCloseSocket(SHUT_REASON_EXCEPTION);

			return 0;
		}
		else
		{
			//���ñ���
			wTotalCount += nSendCount;
		}
	}

	//��������
	if (wTotalCount>wSendSize)
	{
		AmortizeBuffer((LPBYTE)pBuffer + wTotalCount, wSendSize - wTotalCount);
	}

	return wSendSize;
}
//��������
VOID CTCPSocketServiceThread::AmortizeBuffer(VOID * pData, WORD wDataSize)
{
	//���뻺��
	if (m_dwBufferData + wDataSize > m_dwBufferSize)
	{
		//��������
		LPBYTE pcbDataBuffer = NULL;
		LPBYTE pcbDeleteBuffer = m_pcbDataBuffer;

		//�����С
		DWORD dwNeedSize = m_dwBufferData + wDataSize;
		DWORD dwApplySize = __max(dwNeedSize, m_dwBufferSize * 2);

		//���뻺��
		try
		{
			pcbDataBuffer = new BYTE[dwApplySize];
		}
		catch (...)
		{ }

		//ʧ���ж�
		if (pcbDataBuffer == NULL)
		{
			PerformCloseSocket(SHUT_REASON_EXCEPTION);
			return;
		}

		//���ñ���
		m_dwBufferSize = dwApplySize;
		m_pcbDataBuffer = pcbDataBuffer;
		CopyMemory(m_pcbDataBuffer, pcbDeleteBuffer, m_dwBufferData);

		//ɾ������
		SafeDeleteArray(pcbDeleteBuffer);
	}

	//���ñ���
	m_bNeedBuffer = true;
	m_dwBufferData += wDataSize;
	CopyMemory(m_pcbDataBuffer + m_dwBufferData - wDataSize, pData, wDataSize);

	return;
}
//��������
WORD CTCPSocketServiceThread::CrevasseBuffer(BYTE cbDataBuffer[], WORD wDataSize)
{

}
//��������
WORD CTCPSocketServiceThread::EncryptBuffer(BYTE cbDataBuffer[], WORD wDataSize, WORD wBufferSize)
{

}
//������Ϣ
LRESULT CTCPSocketServiceThread::OnSocketNotify(WPARAM wParam, LPARAM lParam)
{

}
//������Ϣ
LRESULT CTCPSocketServiceThread::OnServiceRequest(WPARAM wParam, LPARAM lParam)
{

}
//�����ȡ
LRESULT CTCPSocketServiceThread::OnSocketNotifyRead(WPARAM wParam, LPARAM lParam)
{

}
//���緢��
LRESULT CTCPSocketServiceThread::OnSocketNotifyWrite(WPARAM wParam, LPARAM lParam)
{

}
//����ر�
LRESULT CTCPSocketServiceThread::OnSocketNotifyClose(WPARAM wParam, LPARAM lParam)
{

}
//��������
LRESULT CTCPSocketServiceThread::OnSocketNotifyConnect(WPARAM wParam, LPARAM lParam)
{

}
//���ӳ��
WORD CTCPSocketServiceThread::SeedRandMap(WORD wSeed)
{

}
//����ӳ��
BYTE CTCPSocketServiceThread::MapSendByte(BYTE cbData)
{

}
//����ӳ��
BYTE CTCPSocketServiceThread::MapRecvByte(BYTE cbData)
{

}