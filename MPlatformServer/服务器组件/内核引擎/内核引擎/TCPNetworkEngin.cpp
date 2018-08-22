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

}
//���ͺ���
bool CTCPNetworkItem::SendData(VOID * pData, WORD wDataSize, WORD wMainCmdID, WORD wSubCmdID, WORD wRountID)
{

}
//���ղ���
bool CTCPNetworkItem::RecvData()
{

}
//�ر�����
bool CTCPNetworkItem::CloseSocket(WORD wRountID)
{

}
//���ùر�
bool CTCPNetworkItem::ShutDownSocket(WORD wRountID)
{

}
//����Ⱥ��
bool CTCPNetworkItem::AllowBatchSend(WORD wRountID, bool bAllowBatch, BYTE cbBatchMask)
{

}
//�������
bool CTCPNetworkItem::OnSendCompleted(COverLappedSend * pOverLappedSend, DWORD dwThancferred)
{

}
//�������
bool CTCPNetworkItem::OnrRecvCompleted(COverLappedSend * pOverLappedRecv, DWORD dwThancferred)
{

}
//�ر����
bool CTCPNetworkItem::OnCloseCompleted()
{

}
//��������
WORD CTCPNetworkItem::EncryptBuffer(BYTE pcbDataBuffer[], WORD wDataSize, WORD wBufferSize)
{

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

}