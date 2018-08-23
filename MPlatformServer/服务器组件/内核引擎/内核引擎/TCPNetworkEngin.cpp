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
	WORD							wRountid;							//ѭ������
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

}
//�������
bool CTCPNetworkItem::OnrRecvCompleted(COverLappedSend * pOverLappedRecv, DWORD dwThancferred)
{

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

}
//���ӳ��
WORD CTCPNetworkItem::SeedRandMap(WORD wSeed)
{

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