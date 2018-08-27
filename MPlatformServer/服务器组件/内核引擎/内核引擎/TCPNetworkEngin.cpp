#include "Stdafx.h"
#include "Afxinet.h"
#include "TCPNetworkEngin.h"
#include "TraceServiceManager.h"

//////////////////////////////////////////////////////////////////////////
//�궨��

//ϵ������
#define DEAD_QUOTIETY				0									//����ϵ��
#define DANGER_QUOTIETY				1									//Σ��ϵ��
#define SAFETY_QUOTIETY				2									//��ȫϵ��

//��������
#define ASYNCHRONISM_SEND_DATA		1									//���ͱ�ʶ
#define ASYNCHRONISM_SEND_BATCH		2									//Ⱥ�巢��
#define ASYNCHRONISM_SHUT_DOWN		3									//��ȫ�ر�
#define ASYNCHRONISM_ALLOW_BATCH	4									//����Ⱥ��
#define ASYNCHRONISM_CLOSE_SOCKET	5									//�ر�����
#define ASYNCHRONISM_DETECT_SOCKET	6									//�������

//��������
#define SOCKET_INDEX(dwSocketID)	LOWORD(dwSocketID)					//λ������
#define SOCKET_ROUNTID(dwRountID)	HIWORD(dwRountID)					//ѭ������

//////////////////////////////////////////////////////////////////////////
//�ṹ����

//��������
struct tagSendDataRequest
{
	WORD							wIndex;								//��������
	WORD							wRountID;							//ѭ������
	WORD							wMainCmdID;							//��������
	WORD							wSubCmdID;							//��������
	WORD							wDataSize;							//���ݴ�С
	BYTE							cbSendBuffer[SOCKET_TCP_BUFFER];	//���ͻ���
};																		

//Ⱥ������
struct tagBatchSendRequest
{
	WORD							wMainCmdID;							//��������
	WORD							wSubCmdID;							//��������
	WORD							wDataSize;							//���ݴ�С
	BYTE							cbBatchMask;						//��������
	BYTE							cbSendBuffer[SOCKET_TCP_BUFFER];	//���ͻ���
};

//����Ⱥ��
struct tagAllowBatchSend
{
	WORD							wIndex;								//��������
	WORD							wRountid;							//ѭ������
	BYTE							cbBatchMask;						//��������
	BYTE							cbAllowBatch;						//�����־
};

//�ر�����
struct tagCloseSocket
{
	WORD							wIndex;								//��������
	WORD							wRountid;							//ѭ������
};

//��ȫ�ر�
struct tagShutDownSocket
{
	WORD							wIndex;								//��������
	WORD							wRountid;							//ѭ������
};

//////////////////////////////////////////////////////////////////////////
//���캯��
COverLapped::COverLapped(enOperationType OperationType) : m_OperationType(OperationType)
{
	//���ñ���
	ZeroMemory(&m_WSABuffer, sizeof(m_WSABuffer));
	ZeroMemory(&m_OverLapped, sizeof(m_OverLapped));

	return;
}
//��������
COverLapped::~COverLapped()
{
}

//////////////////////////////////////////////////////////////////////////

//���캯��
COverLappedSend::COverLappedSend() : COverLapped(enOperationType_Send)
{
	m_WSABuffer.len = 0;
	m_WSABuffer.buf = (char*)m_cbBuffer;
}
//��������
COverLappedSend::~COverLappedSend()
{

}

//////////////////////////////////////////////////////////////////////////
//���캯��
COverLappedRecv::COverLappedRecv() : COverLapped(enOperationType_Recv)
{
	m_WSABuffer.len = 0;
	m_WSABuffer.buf = NULL;
}
//��������
COverLappedRecv::~COverLappedRecv()
{

}

//////////////////////////////////////////////////////////////////////////

//���캯��
CTCPNetworkItem::CTCPNetworkItem(WORD wIndex, ITCPNetworkItemSink * pITCPNetworkItemSink)
{
	//��������
	m_dwClientIP = 0L;
	m_dwActiveTime = 0L;
	//�ں˱���
	m_wRountID = 1;
	m_wSurvivalTime = DEAD_QUOTIETY;
	m_hSocketHandle = INVALID_SOCKET;
	//״̬����
	m_bSendIng = false;
	m_bRecvIng = false;
	m_bShutDown = false;
	m_bAllowBatch = false;
	m_bBatchMask = 0xFF;
	//���ܱ���
	m_wRecvSize = 0;
	ZeroMemory(m_cbRecvBuf, sizeof(m_cbRecvBuf));

	//��������
	m_dwSendTickCount = 0L;
	m_dwRecvTickCount = 0L;
	m_dwSendPacketCount = 0L;
	m_dwRecvPacketCount = 0L;
	//��������
	m_cbSendRound = 0;
	m_cbRecvRound = 0;
	m_dwSendXorKey = 0;
	m_dwRecvXorKey = 0;

	return;
}
//��������
CTCPNetworkItem::~CTCPNetworkItem()
{
	//ɾ������
	for (INT_PTR i = 0; i < m_OverLappedSendBuffer.GetCount(); ++i)
	{
		delete m_OverLappedSendBuffer[i];
	}
	//ɾ���
	for (INT_PTR i = 0; i < m_OverLappedSendActive.GetCount(); ++i)
	{
		delete m_OverLappedSendActive[i];
	}

	//ɾ������
	m_OverLappedSendBuffer.RemoveAll();
	m_OverLappedSendActive.RemoveAll();

	return;
}

//�󶨶���
DWORD CTCPNetworkItem::Attach(SOCKET hSocket, DWORD dwClientIP)
{
	//У�����
	ASSERT(dwClientIP != 0);
	ASSERT(hSocket != INVALID_SOCKET);

	//У��״̬
	ASSERT(m_bSendIng == false);
	ASSERT(m_bRecvIng == false);
	ASSERT(m_hSocketHandle == INVALID_SOCKET);

	//״̬����
	m_bSendIng = false;
	m_bRecvIng = false;
	m_bShutDown = false;
	m_bAllowBatch = false;
	m_bBatchMask = 0xFF;

	//���ñ���
	m_dwClientIP = dwClientIP;
	m_hSocketHandle = hSocket;
	m_wSurvivalTime = SAFETY_QUOTIETY;
	m_dwActiveTime = (DWORD)time(NULL);
	m_dwRecvTickCount = GetTickCount();

	//����֪ͨ
	m_pITCPNetworkItemSink->OnEventSocketBind(this);

	return GetIdentifierID();
}
//�ָ�����
DWORD CTCPNetworkItem::ResumeData()
{
	//״̬У��
	ASSERT(m_hSocketHandle == INVALID_SOCKET);
	
	//��������
	m_dwClientIP = 0L;
	m_dwActiveTime = 0L;
	//�ں˱���
	m_wRountID = __max(1, m_wRountID + 1);
	m_wSurvivalTime = DEAD_QUOTIETY;
	m_hSocketHandle = INVALID_SOCKET;
	//״̬����
	m_bSendIng = false;
	m_bRecvIng = false;
	m_bShutDown = false;
	m_bAllowBatch = false;
	
	//���ܱ���
	m_wRecvSize = 0;
	ZeroMemory(m_cbRecvBuf, sizeof(m_cbRecvBuf));

	//��������
	m_dwSendTickCount = 0L;
	m_dwRecvTickCount = 0L;
	m_dwSendPacketCount = 0L;
	m_dwRecvPacketCount = 0L;
	//��������
	m_cbSendRound = 0;
	m_cbRecvRound = 0;
	m_dwSendXorKey = 0;
	m_dwRecvXorKey = 0;

	//�ص�����
	m_OverLappedSendBuffer.Append(m_OverLappedSendActive);
	m_OverLappedSendBuffer.RemoveAll();

	return GetIdentifierID();
}

//���ͺ���
bool CTCPNetworkItem::SendData(WORD wMainCmdID, WORD wSubCmdID, WORD wRountID)
{
	//�����ж�
	if (SendVerdict(wRountID) == false) return false;

	//��ȡ����
	WORD wPacketSize = sizeof(TCP_Head);
	COverLappedSend * pOverLappedSend = GetSendOverLapped(wPacketSize);

	//�ر��ж�
	if (pOverLappedSend == NULL)
	{
		CloseSocket(wRountID);
		return false;
	}

	//��������
	WORD wSourceLen = (WORD)pOverLappedSend->m_WSABuffer.len;
	TCP_Head * pHead = (TCP_Head *)(pOverLappedSend->m_cbBuffer + wSourceLen);

	//��������
	pHead->CommandInfo.wMainCmdID = wMainCmdID;
	pHead->CommandInfo.wSubCmdID = wSubCmdID;

	//��������
	WORD wEncryptLen = EncryptBuffer(pOverLappedSend->m_cbBuffer + wSourceLen, wPacketSize, sizeof(pOverLappedSend->m_cbBuffer) - wSourceLen);
	pOverLappedSend->m_WSABuffer.len += wEncryptLen;

	//��������
	if (m_bSendIng == false)
	{
		//��������
		DWORD dwTanceferred = 0;
		INT nResultCode = WSASend(m_hSocketHandle, &pOverLappedSend->m_WSABuffer, 
								  1, &dwTanceferred, 0, &pOverLappedSend->m_OverLapped, NULL);

		//������� WSA_IO_PENDING ��ʾû�д���
		if (nResultCode == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			CloseSocket(m_wRountID);
			return false;
		}

		//���ñ���
		m_bSendIng = true;
	}

	return true;
}
//���ͺ���
bool CTCPNetworkItem::SendData(VOID * pData, WORD wDataSize, WORD wMainCmdID, WORD wSubCmdID, WORD wRountID)
{
	//У�����
	ASSERT(wDataSize <= SOCKET_TCP_PACKET);

	//�����ж�
	if (wDataSize > SOCKET_TCP_PACKET) return false;
	if (SendVerdict(wRountID) == false) return false;

	//��ȡ����
	WORD wPacketSize = sizeof(TCP_Head) + wDataSize;
	COverLappedSend * pOverLappedSend = GetSendOverLapped(wPacketSize);

	//�ر��ж�
	if (pOverLappedSend == NULL)
	{
		CloseSocket(wRountID);
		return false;
	}

	//��������
	WORD wSourceLen = (WORD)pOverLappedSend->m_WSABuffer.len;
	TCP_Head * pHead = (TCP_Head *)(pOverLappedSend->m_cbBuffer + wSourceLen);

	//���ñ���
	pHead->CommandInfo.wSubCmdID = wSubCmdID;
	pHead->CommandInfo.wMainCmdID = wMainCmdID;

	//��������
	if (wDataSize > 0)
	{
		ASSERT(pData != NULL);
		CopyMemory(pHead + 1, pData, wDataSize);
	}

	//��������
	WORD wEncryptLen = EncryptBuffer(pOverLappedSend->m_cbBuffer + wSourceLen, wPacketSize, sizeof(pOverLappedSend->m_cbBuffer) - wSourceLen);
	pOverLappedSend->m_WSABuffer.len += wEncryptLen;

	//��������
	if (m_bSendIng == false)
	{
		//��������
		DWORD dwThancferred = 0;
		INT nResultCode = WSASend(m_hSocketHandle, &pOverLappedSend->m_WSABuffer, 1, &dwThancferred, 0, &pOverLappedSend->m_OverLapped, NULL);

		//�������
		if ((nResultCode == SOCKET_ERROR) && (WSAGetLastError() != WSA_IO_PENDING))
		{
			CloseSocket(m_wRountID);
			return false;
		}

		//���ñ���
		m_bSendIng = true;
	}

	return true;
}
//���ղ���
bool CTCPNetworkItem::RecvData()
{
	//У�����
	ASSERT(m_bRecvIng == false);
	ASSERT(m_hSocketHandle != INVALID_SOCKET);

	//��������
	DWORD dwThancferred = 0;
	DWORD dwFlags = 0;

	INT nResultCode = WSARecv(m_hSocketHandle, &m_OverLappedRecv.m_WSABuffer, 1, &dwThancferred, &dwFlags,
		&m_OverLappedRecv.m_OverLapped, NULL);

	//�������
	if (nResultCode == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		CloseSocket(m_wRountID);
		return false;
	}

	//���ñ���
	m_bRecvIng = true;

	return true;
}
//�ر�����
bool CTCPNetworkItem::CloseSocket(WORD wRountID)
{
	//״̬�ж�
	if (m_wRountID != wRountID) return false;

	//�ر�����
	if (m_hSocketHandle != INVALID_SOCKET)
	{
		closesocket(m_hSocketHandle);
		m_hSocketHandle = INVALID_SOCKET;
	}

	//�жϹر�
	if (m_bRecvIng == false && m_bSendIng == false)
	{
		OnCloseCompleted();
	}
	
	return true;
}
//���ùر�
bool CTCPNetworkItem::ShutDownSocket(WORD wRountID)
{
	//״̬�ж�
	if (m_hSocketHandle == INVALID_SOCKET) return false;
	if (m_bShutDown == true || m_wRountID != wRountID) return false;

	//���ñ���
	m_wRecvSize = 0;
	m_bShutDown = true;

	return true;
}
//����Ⱥ��
bool CTCPNetworkItem::AllowBatchSend(WORD wRountID, bool bAllowBatch, BYTE cbBatchMask)
{
	//״̬�ж�
	if (m_hSocketHandle == INVALID_SOCKET) return false;
	if (m_wRountID != wRountID) return false;

	//���ñ���
	m_bAllowBatch = bAllowBatch;

	m_bBatchMask = cbBatchMask;

	return true;
}
//�������
bool CTCPNetworkItem::OnSendCompleted(COverLappedSend * pOverLappedSend, DWORD dwThancferred)
{
	//У�����
	ASSERT(m_bSendIng == true);
	ASSERT(m_OverLappedSendActive.GetCount() > 0);
	ASSERT(pOverLappedSend == m_OverLappedSendActive[0]);

	//�ͷŽṹ
	m_OverLappedSendActive.RemoveAt(0);
	m_OverLappedSendBuffer.Add(pOverLappedSend);

	//���ñ���
	m_bSendIng = false;

	//�жϹر�
	if (m_hSocketHandle == INVALID_SOCKET)
	{
		CloseSocket(m_wRountID);
	}

	//���ñ���
	if (dwThancferred != 0)
	{
		m_wSurvivalTime = SAFETY_QUOTIETY;
		m_dwSendTickCount = GetTickCount();
	}

	//��������
	if (m_OverLappedSendActive.GetCount() > 0)
	{
		//��ȡ����
		pOverLappedSend = m_OverLappedSendActive[0];

		//��������
		DWORD dwThancferred = 0;
		INT nResultCode = WSASend(m_hSocketHandle, &pOverLappedSend->m_WSABuffer, 1, &dwThancferred, 0,
								  &pOverLappedSend->m_OverLapped, NULL);

		//�������
		if (nResultCode == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			CloseSocket(m_wRountID);
			return false;
		}

		//���ñ���
		m_bSendIng = true;		
	}
	
	return true;
}
//�������
bool CTCPNetworkItem::OnRecvCompleted(COverLappedRecv * pOverLappedRecv, DWORD dwThancferred)
{
	//У�����
	ASSERT(m_bRecvIng == true);

	//���ñ���
	m_bRecvIng = false;

	//�жϹر�
	if (m_hSocketHandle == INVALID_SOCKET)
	{
		CloseSocket(m_wRountID);
		return true;
	}

	//��������
	INT nResultCode = recv(m_hSocketHandle, (char*)m_cbRecvBuf + m_wRecvSize, sizeof(m_cbRecvBuf) - m_wRecvSize, 0);

	//�ر��ж�
	if (nResultCode <= 0)
	{
		CloseSocket(m_wRountID);
		return true;
	}

	//�ж��ж�
	if (m_bShutDown == true) return true;

	//���ñ���
	m_wRecvSize += nResultCode;
	m_wSurvivalTime = SAFETY_QUOTIETY;
	m_dwRecvPacketCount = GetTickCount();

	//�������
	BYTE cbBuffer[SOCKET_TCP_BUFFER];
	TCP_Head * pHead = (TCP_Head *)m_cbRecvBuf;

	//��������
	try
	{
		while (m_wRecvSize >= sizeof(TCP_Head))
		{
			//У������
			WORD wPacketSize = pHead->TCPInfo.wPacketSize;

			//�����ж�
			if (wPacketSize > SOCKET_TCP_BUFFER) throw TEXT("���ݰ�����̫��");
			if (wPacketSize < sizeof(TCP_Head))
			{
				TCHAR szBuffer[512];
				_sntprintf(szBuffer, CountArray(szBuffer), TEXT("���ݰ�����̫�� %d,%d,%d,%d,%d,%d,%d,%d"), m_cbRecvBuf[0],
					m_cbRecvBuf[1],
					m_cbRecvBuf[2],
					m_cbRecvBuf[3],
					m_cbRecvBuf[4],
					m_cbRecvBuf[5],
					m_cbRecvBuf[6],
					m_cbRecvBuf[7]
					);
				g_TraceServiceManager.TraceString(szBuffer, TraceLevel_Exception);

				_sntprintf(szBuffer, CountArray(szBuffer), TEXT("���� %d, �汾 %d, Ч���� %d"), pHead->TCPInfo.wPacketSize,
					pHead->TCPInfo.cbDataKind, pHead->TCPInfo.cbCheckCode);

				g_TraceServiceManager.TraceString(szBuffer, TraceLevel_Exception);
				throw TEXT("���ݰ�����̫��");
			}
			if (pHead->TCPInfo.cbDataKind != DK_MAPPED && pHead->TCPInfo.cbDataKind != 0x05) {
				CString aa;
				aa.Format(TEXT("0x%x"), pHead->TCPInfo.cbDataKind);
				g_TraceServiceManager.TraceString(aa, TraceLevel_Exception);
				throw TEXT("���ݰ��汾��ƥ��");
			}
			//����ж�
			if (m_wRecvSize < wPacketSize) break;

			//��ȡ����
			CopyMemory(cbBuffer, m_cbRecvBuf, wPacketSize);
			WORD wRealySize = CrevasseBuffer(cbBuffer, wPacketSize);

			//���ñ���
			m_dwRecvPacketCount++;

			//��������
			LPVOID pData = cbBuffer + sizeof(TCP_Head);
			WORD wDataSize = wRealySize + sizeof(TCP_Head);
			TCP_Command Command = ((TCP_Head *)cbBuffer)->CommandInfo;

			//��Ϣ����
			if (Command.wMainCmdID != MDM_KN_COMMAND)
				m_pITCPNetworkItemSink->OnEventSocketRead(Command, pData, wDataSize, this);

			//ɾ������
			m_wRecvSize -= wPacketSize;
			if (m_wRecvSize > 0)
				MoveMemory(m_cbRecvBuf, m_cbRecvBuf + wPacketSize, m_wRecvSize);
		}
	}
	catch (LPCTSTR pszMessage)
	{
		//������Ϣ
		TCHAR szString[512] = TEXT("");
		_sntprintf(szString, CountArray(szString), TEXT("SocketEngine Index=%ld��RountID=%ld��OnRecvCompleted ������%s���쳣"), m_wIndex, m_wRountID, pszMessage);
		g_TraceServiceManager.TraceString(szString, TraceLevel_Exception);

		//�ر�����
		CloseSocket(m_wRountID);

		return false;
	}
	catch (...)
	{
		//������Ϣ
		TCHAR szString[512] = TEXT("");
		_sntprintf(szString, CountArray(szString), TEXT("SocketEngine Index=%ld��RountID=%ld��OnRecvCompleted �������Ƿ����쳣"), m_wIndex, m_wRountID);
		g_TraceServiceManager.TraceString(szString, TraceLevel_Exception);

		//�ر�����
		CloseSocket(m_wRountID);

		return false;
	}

	return RecvData();
}
//�ر����
bool CTCPNetworkItem::OnCloseCompleted()
{
	//У��״̬
	ASSERT(m_hSocketHandle == INVALID_SOCKET);
	ASSERT(m_bSendIng == false && m_bRecvIng == false);

	//�ر��¼�
	m_pITCPNetworkItemSink->OnEventSocketShut(this);

	//�ָ�����
	ResumeData();

	return true;
}
//��������
WORD CTCPNetworkItem::EncryptBuffer(BYTE pcbDataBuffer[], WORD wDataSize, WORD wBufferSize)
{
	WORD i = 0;
	//У�����
	ASSERT(wDataSize >= sizeof(TCP_Head));
	ASSERT(wDataSize <= sizeof(TCP_Head) + SOCKET_TCP_BUFFER);
	ASSERT(wBufferSize >= (wDataSize + sizeof(DWORD) * 2));

	//��д��Ϣͷ
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

	//���ñ���
	m_dwSendPacketCount++;

	return wDataSize;
}
//��������
WORD CTCPNetworkItem::CrevasseBuffer(BYTE pcbDataBuffer[], WORD wDataSize)
{
	WORD i = 0;
	//У�����
	ASSERT(wDataSize >= sizeof(TCP_Head));
	ASSERT(((TCP_Head*)pcbDataBuffer)->TCPInfo.wPacketSize == wDataSize);

	//У�������ֽ�ӳ��
	TCP_Head * pHead = (TCP_Head*)pcbDataBuffer;
	for (i = sizeof(TCP_Head); i < wDataSize; ++i)
	{
		pcbDataBuffer[i] = MapRecvByte(pcbDataBuffer[i]);
	}

	return wDataSize;
}
//���ӳ��
WORD CTCPNetworkItem::SeedRandMap(WORD wSeed)
{
	DWORD dwHold = wSeed;
	return (WORD)((dwHold = dwHold * 241103L + 2533101L) >> 16);
}
//����ӳ������
BYTE CTCPNetworkItem::MapSendByte(BYTE cbData)
{
	BYTE cbMap;
	cbMap = g_SendByteMap[cbData];
	return cbMap;
}
//����ӳ������
BYTE CTCPNetworkItem::MapRecvByte(BYTE cbData)
{
	BYTE cbMap;
	cbMap = g_RecvByteMap[cbData];
	return cbMap;
}
//�����ж�
bool CTCPNetworkItem::SendVerdict(WORD wRountID)
{
	if (m_wRountID != wRountID || m_bShutDown == true) return false;
	if (m_hSocketHandle == INVALID_SOCKET || m_dwRecvPacketCount == 0) return false;

	return true;
}
//��ȡ�����ص��ṹ 
COverLappedSend * CTCPNetworkItem::GetSendOverLapped(WORD wPacketSize)
{
	//�����ж� �ϴλ�û�������Ҫ��������
	if (m_OverLappedSendActive.GetCount() > 1)
	{
		INT_PTR nActiveCount = m_OverLappedSendActive.GetCount();
		COverLappedSend * pOverLappedSend = m_OverLappedSendActive[nActiveCount - 1];
		if (sizeof(pOverLappedSend->m_cbBuffer) >= (pOverLappedSend->m_WSABuffer.len + wPacketSize + sizeof(DWORD) * 2))
			return pOverLappedSend;
	}

	//���ж���
	if (m_OverLappedSendActive.GetCount() > 0)
	{
		//��ȡ����
		INT_PTR nFreeCount = m_OverLappedSendBuffer.GetCount();
		COverLappedSend * pOverLappedSend = m_OverLappedSendBuffer[nFreeCount - 1];

		//���ñ���
		pOverLappedSend->m_WSABuffer.len = 0;
		m_OverLappedSendActive.Add(pOverLappedSend);
		m_OverLappedSendBuffer.RemoveAt(nFreeCount - 1);

		return pOverLappedSend;
	}

	try
	{
		//��������
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

//��д�߳���
//���캯��
CTCPNetworkThreadReadWrite::CTCPNetworkThreadReadWrite()
{
	m_hCompletionPort = NULL;
	return;
}
//��������
CTCPNetworkThreadReadWrite::~CTCPNetworkThreadReadWrite()
{

}
//���ú���
bool CTCPNetworkThreadReadWrite::InitThread(HANDLE hCompletionPort)
{
	ASSERT(hCompletionPort != NULL);

	m_hCompletionPort = hCompletionPort;

	return true;
}
//���к���
bool CTCPNetworkThreadReadWrite::OnEventThreadRun()
{
	//У�����
	ASSERT(m_hCompletionPort != NULL);

	//��������
	DWORD dwThancferred = 0;
	OVERLAPPED * pOverLapped = NULL;
	CTCPNetworkItem * pTCPNetworkItem = NULL;

	//��ɶ˿�
	BOOL bSuccess = GetQueuedCompletionStatus(m_hCompletionPort, &dwThancferred, 
											 (PULONG_PTR)&pTCPNetworkItem, &pOverLapped, INFINITE);

	if (bSuccess == FALSE && GetLastError() != ERROR_NETNAME_DELETED) return false;

	//�˳��ж�
	if (pTCPNetworkItem == NULL && pOverLapped == NULL) return false;

	//��������
	COverLapped * pSocketLapped = CONTAINING_RECORD(pOverLapped, COverLapped, m_OverLapped);

	//��������
	switch (pSocketLapped->GetOperationType())
	{
		case enOperationType_Send: //���ݷ���
			{
				CWHDataLocker ThreadLock(pTCPNetworkItem->GetCriticalSection());
				pTCPNetworkItem->OnSendCompleted((COverLappedSend *)pSocketLapped, dwThancferred);
				break;
			}
		case enOperationType_Recv: //���ݶ�ȡ
			{
				CWHDataLocker ThreadLock(pTCPNetworkItem->GetCriticalSection());
				pTCPNetworkItem->OnRecvCompleted((COverLappedRecv *)pSocketLapped, dwThancferred);
				break;
			}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//Ӧ���̶߳���
//���캯��
CTCPNetworkThreadAccept::CTCPNetworkThreadAccept()
{
	m_hListenSocket = INVALID_SOCKET;
	m_hCompletionPort = NULL;
	m_pTCPNetworkEngine = NULL;
}
//��������
CTCPNetworkThreadAccept::~CTCPNetworkThreadAccept()
{

}
//���ú���
bool CTCPNetworkThreadAccept::InitThread(HANDLE hCompletionPort, SOCKET hListenSocket, CTCPNetworkEngine * pNetworkEngine)
{
	//У�����
	ASSERT(hCompletionPort != NULL);
	ASSERT(hListenSocket != INVALID_SOCKET);
	ASSERT(pNetworkEngine != NULL);
	//���ñ���
	m_hListenSocket = hListenSocket;
	m_hCompletionPort = hCompletionPort;
	m_pTCPNetworkEngine = pNetworkEngine;

	return true;
}
//���к���
bool CTCPNetworkThreadAccept::OnEventThreadRun()
{
	//У�����
	ASSERT(m_hCompletionPort != NULL);
	ASSERT(m_pTCPNetworkEngine != NULL);

	//��������
	SOCKET hConnectSocket = INVALID_SOCKET;
	CTCPNetworkItem * pTCPNetworkItem = NULL;

	try
	{
		//��������
		SOCKADDR_IN SocketAddr;
		INT nBufferSize = sizeof(SocketAddr);
		hConnectSocket = WSAAccept(m_hListenSocket, (SOCKADDR*)&SocketAddr, &nBufferSize, NULL, NULL);

		//�˳��ж�
		if (hConnectSocket == INVALID_SOCKET) return false;

		//��ȡ����
		pTCPNetworkItem = m_pTCPNetworkEngine->ActiveNetworkItem();

		//ʧ���ж�
		if (pTCPNetworkItem == NULL)
		{
			ASSERT(FALSE);
			throw TEXT("�������Ӷ���ʧ��");
		}

		//��������
		CWHDataLocker ThreadLock(pTCPNetworkItem->GetCriticalSection());

		//�󶨶���
		pTCPNetworkItem->Attach(hConnectSocket, SocketAddr.sin_addr.S_un.S_addr);
		CreateIoCompletionPort((HANDLE)hConnectSocket, m_hCompletionPort, (ULONG_PTR)pTCPNetworkItem, 0);

		//�������
		pTCPNetworkItem->RecvData();
	}
	catch (...)
	{
		//�������
		ASSERT(pTCPNetworkItem == NULL);

		//�ر�����
		if (hConnectSocket != INVALID_SOCKET)
		{
			closesocket(hConnectSocket);
		}
	}

	return true;
}

//����߳���

//���캯��
CTCPNetworkThreadDetect::CTCPNetworkThreadDetect()
{
	m_dwPileTime = 0L;
	m_dwDetectTime = 10000L;
	m_pTCPNetworkEngine = NULL;
}
//��������
CTCPNetworkThreadDetect::~CTCPNetworkThreadDetect()
{

}
//���ú���
bool CTCPNetworkThreadDetect::InitThread(CTCPNetworkEngine * pNetworkEngine, DWORD dwDetectTime)
{
	//У�����
	ASSERT(pNetworkEngine != NULL);

	//���ñ���
	m_dwPileTime = 0L;
	m_dwDetectTime = dwDetectTime;
	m_pTCPNetworkEngine = pNetworkEngine;

	return true;
}
//���к���
bool CTCPNetworkThreadDetect::OnEventThreadRun()
{
	//У�����
	ASSERT(m_pTCPNetworkEngine != NULL);

	//���ü��
	Sleep(200);
	m_dwPileTime += 200L;

	//�������
	if (m_dwPileTime >= m_dwDetectTime)
	{
		m_dwPileTime = 0L;
		m_pTCPNetworkEngine->DetectSocket();
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////

//��������

//���캯��
CTCPNetworkEngine::CTCPNetworkEngine()
{
	//��֤����
	m_bValidate = false;
	m_bNormalRun = true;

	//��������
	m_bService = false;
	ZeroMemory(m_cbBuffer, sizeof(m_cbBuffer));


	//���ñ���
	m_wMaxConnect = 0;
	m_wServicePort = 0;
	m_dwDetectTime = 10000L;

	//�ں˱���
	m_hServerSocket = INVALID_SOCKET;
	m_hCompletionPort = NULL;
	m_pITCPNetworkEngineEvent = NULL;
}
//��������
CTCPNetworkEngine::~CTCPNetworkEngine()
{

}

//�ӿڲ�ѯ
VOID * CTCPNetworkEngine::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(IServiceModule, Guid, dwQueryVer);
	QUERYINTERFACE(ITCPNetworkEngine, Guid, dwQueryVer);
	QUERYINTERFACE(IAsynchronismEngineSink, Guid, dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(ITCPNetworkEngine, Guid, dwQueryVer);

	return NULL;
}

//��������
bool CTCPNetworkEngine::StartService()
{
	//״̬У��
	ASSERT(m_bService == false);
	if (m_bService == true) return false;

	//�ж϶˿�
	if (m_wServicePort == 0)
	{
		m_wServicePort = 3000;
	}

	//ϵͳ��Ϣ
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);
	DWORD dwThreadCount = SystemInfo.dwNumberOfProcessors;

	//��ɶ˿�
	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, SystemInfo.dwNumberOfProcessors);
	if (m_hCompletionPort == NULL) return false;

	//��������
	SOCKADDR_IN SocketAddr;
	ZeroMemory(&SocketAddr, sizeof(SocketAddr));

	SocketAddr.sin_family = AF_INET;
	SocketAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	SocketAddr.sin_port = htons(m_wServicePort);
	m_hServerSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	//�����ж�
	if (m_hServerSocket == INVALID_SOCKET)
	{
		LPCTSTR pszString = TEXT("ϵͳ��Դ������� TCP/IP Э��û�а�װ����������ʧ��");
		g_TraceServiceManager.TraceString(pszString, TraceLevel_Exception);
		return false;
	}
	//����ɶ˿���socket�� ���ڼ���
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
		//��ʧ���ٴγ���
		m_hServerSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		//�����ж�
		if (m_hServerSocket == INVALID_SOCKET)
		{
			LPCTSTR pszString = TEXT("ϵͳ��Դ������� TCP/IP Э��û�а�װ����������ʧ��");
			g_TraceServiceManager.TraceString(pszString, TraceLevel_Exception);
			return false;
		}
		m_wServicePort++;
		SocketAddr.sin_family = AF_INET;
		SocketAddr.sin_addr.S_un.S_addr = INADDR_ANY;
		SocketAddr.sin_port = htons(m_wServicePort);
	}

	//�����˿�
	if (listen(m_hServerSocket, 200) == SOCKET_ERROR)
	{
		TCHAR szString[512] = TEXT("");
		_sntprintf(szString, CountArray(szString), TEXT("�˿�������������ռ�ã����� %ld �˿�ʧ��"), m_wServicePort);
		g_TraceServiceManager.TraceString(szString, TraceLevel_Exception);
		return false;
	}

	//�첽����
	IUnknownEx * pIUnknownEx = QUERY_ME_INTERFACE(IUnknownEx);
	if (m_AsynchronismEngine.SetAsynchronismSink(pIUnknownEx) == false)
	{
		ASSERT(FALSE);
		return false;
	}

	//��ҳ��֤
	WebAttestation();

	//��������
	if (m_AsynchronismEngine.StartService() == false)
	{
		ASSERT(FALSE);
		return false;
	}

	//������д�߳�
	for (DWORD i = 0; i < dwThreadCount; ++i)
	{
		CTCPNetworkThreadReadWrite * pNetworkRSThread = new CTCPNetworkThreadReadWrite();
		if (pNetworkRSThread->InitThread(m_hCompletionPort) == false) return false;
		m_SocketRWThreadArray.Add(pNetworkRSThread);
	}

	//Ӧ���߳�
	if (m_SocketAcceptThread.InitThread(m_hCompletionPort, m_hServerSocket, this) == false) return false;
	if (m_SocketAcceptThread.StartThread() == false) return false;

	//������д�߳�
	for (DWORD i = 0; i < dwThreadCount; ++i)
	{
		CTCPNetworkThreadReadWrite * pNetworkRWThread = m_SocketRWThreadArray[i];
		ASSERT(pNetworkRWThread != NULL);
		if (pNetworkRWThread->StartThread() == false) return false;
	}

	//����߳�
	m_SocketDetectThread.InitThread(this, m_dwDetectTime);
	if (m_SocketDetectThread.StartThread() == false) return false;

	//���ñ���
	m_bService = true;

	return true;
}
//ֹͣ����
bool CTCPNetworkEngine::ConcludeService()
{
	//���ñ���
	m_bService = false;

	//����߳�
	m_SocketDetectThread.ConcludeThread(INFINITE);

	//Ӧ���߳�
	if (m_hServerSocket != INVALID_SOCKET)
	{
		closesocket(m_hServerSocket);
		m_hServerSocket = INVALID_SOCKET;
	}

	m_SocketAcceptThread.ConcludeThread(INFINITE);

	//�첽����
	m_AsynchronismEngine.ConcludeService();

	//��д�߳�
	INT_PTR nCount = m_SocketRWThreadArray.GetCount();
	if (m_hCompletionPort != NULL)
	{
		for (INT_PTR i = 0; i < nCount; ++i)
			//ָʾ�߳������������˳�
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

	//��ɶ˿�
	if (m_hCompletionPort != NULL)
	{
		CloseHandle(m_hCompletionPort);
		m_hCompletionPort = NULL;
	}

	//�ر�����
	CTCPNetworkItem * pTCPNetworkItem = NULL;
	for (INT_PTR i = 0; i < nCount; ++i)
	{
		pTCPNetworkItem = m_NetworkItemActive[i];
		pTCPNetworkItem->CloseSocket(pTCPNetworkItem->GetRountID());
		pTCPNetworkItem->ResumeData();
	}

	//��������
	m_NetworkItemBuffer.Append(m_NetworkItemActive);
	m_NetworkItemActive.RemoveAll();
	m_TempNetworkItemArray.RemoveAll();

	return true;
}
//���ö˿�
WORD CTCPNetworkEngine::GetServicePort()
{
	return m_wServicePort;
}
//��ǰ�˿�
WORD CTCPNetworkEngine::GetCurrentPort()
{
	return m_wServicePort;
}
//���ýӿ�
bool CTCPNetworkEngine::SetTCPNetworkEngineEvent(IUnknownEx * pIUnknownEx)
{
	//״̬У��
	ASSERT(m_bService == false);
	if (m_bService == true) return false;

	//��ѯ�ӿ�
	m_pITCPNetworkEngineEvent = QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx, ITCPNetworkEngineEvent);

	//�����ж�
	if (m_pITCPNetworkEngineEvent == NULL)
	{
		ASSERT(FALSE);
		return false;
	}

	return true;
}
//���ò���
bool CTCPNetworkEngine::SetServiceParameter(WORD wServicePort, WORD wMaxConnect, LPCTSTR  pszCompilation)
{
	//״̬У��
	ASSERT(m_bService == false);
	if (m_bService == true) return false;

	//���ñ���
	ASSERT(wServicePort != 0);
	m_wMaxConnect = wMaxConnect;
	m_wServicePort = wServicePort;

	return true;
}
//���ͺ���
bool CTCPNetworkEngine::SendData(DWORD dwSocketID, WORD wMainCmdID, WORD wSubCmdID)
{
	//��������
	CWHDataLocker ThreadLock(m_BufferLocked);
	tagSendDataRequest * pSendDataRequest = (tagSendDataRequest *)m_cbBuffer;

	//��������
	pSendDataRequest->wDataSize = 0;
	pSendDataRequest->wSubCmdID = wSubCmdID;
	pSendDataRequest->wMainCmdID = wMainCmdID;
	pSendDataRequest->wIndex = SOCKET_INDEX(dwSocketID);
	pSendDataRequest->wRountID = SOCKET_ROUNTID(dwSocketID);

	//��������
	WORD wSendSize = sizeof(tagSendDataRequest) - sizeof(pSendDataRequest->cbSendBuffer);
	return m_AsynchronismEngine.PostAsynchronismData(ASYNCHRONISM_SEND_DATA, m_cbBuffer, wSendSize);
}
//���ͺ���
bool CTCPNetworkEngine::SendData(DWORD dwSocketID, WORD wMainCmdID, WORD wSubCmdID, VOID * pData, WORD wDataSize)
{
	//У������
}
//��������
bool CTCPNetworkEngine::SendDataBatch(WORD wMainCmdID, WORD wSubCmdID, VOID * pData, WORD wDataSize, BYTE cbBatchMask)
{

}