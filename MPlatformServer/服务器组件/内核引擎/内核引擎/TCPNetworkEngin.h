#ifndef TCP_NETWORK_ENGINE_HEAD_FILE
#define TCP_NETWORK_ENGINE_HEAD_FILE

#include "KernelEngineHead.h"

//////////////////////////////////////////////////////////////////////////
//ö�ٶ���

//��������
enum enOperationType
{
	enOperationType_Send,		//��������
	enOperationType_Recv,		//��������
};

//////////////////////////////////////////////////////////////////////////

//��˵��
class COverLappedSend;
class COverLappedRecv;
class CTCPNetworkItem;
class CTCPNetworkEngine;

//���ֶ���
typedef class CWHArray<COverLappedSend *>		COverLappedSendPtr;
typedef class CWHArray<COverLappedRecv *>		COverLappedRecvPtr;

//////////////////////////////////////////////////////////////////////////
//�ӿڶ���

//���Ӷ���ص��ӿ�
interface ITCPNetworkItemSink
{
	//���¼�
	virtual bool OnEventSocketBind(CTCPNetworkItem * pCTCPNetworkItem) = NULL;
	//�ر��¼�
	virtual bool OnEventSocketShut(CTCPNetworkItem * pCTCPNetworkItem) = NULL;
	//��ȡ�¼�
	virtual bool OnEventSocketRead(TCP_Command Command, VOID *pData, WORD wDataSize, CTCPNetworkItem * pCTCPNetworkItem) = NULL;
};

//////////////////////////////////////////////////////////////////////////

//�ص��ṹ����
class COverLapped
{
	//��������
public:
	WSABUF						m_WSABuffer;						//����ָ��
	OVERLAPPED					m_OverLapped;						//�ص��ṹ
	const enOperationType		m_OperationType;					//��������

	//��������
public:
	//���캯��
	COverLapped(enOperationType OperationType);
	//��������
	virtual ~COverLapped();

	//��Ϣ����
public:
	//��ȡ����
	enOperationType GetOperationType() { return m_OperationType; }
};

//�����ص��ṹ
class COverLappedSend : public COverLapped
{
	//��������
public:
	BYTE						m_cbBuffer[SOCKET_TCP_BUFFER];						//���ݻ���

	//��������
public:
	//���캯��
	COverLappedSend();
	//��������
	virtual ~COverLappedSend();
};

//�����ص��ṹ
class COverLappedRecv : public COverLapped
{
	//��������
public:
	//���캯��
	COverLappedRecv();
	//��������
	virtual ~COverLappedRecv();
};


//////////////////////////////////////////////////////////////////////////

//��������
class CTCPNetworkItem
{
	//��Ԫ����
	friend class CTCPNetworkEngine;

	//��������
protected:
	DWORD						m_dwClientIP;						//���ӵ�ַ
	DWORD						m_dwActiveTime;						//����ʱ��

	//�ں˱���
protected:
	WORD						m_wIndex;							//��������
	WORD						m_wRountID;							//ѭ������
	WORD						m_wSurvivalTime;					//����ʱ��
	SOCKET						m_hSocketHandle;					//���Ӿ��
	CCriticalSection			m_CriticalSection;					//��������
	ITCPNetworkItemSink *		m_pITCPNetworkItemSink;				//�ص��ӿ�

	//״̬����
protected:
	bool						m_bSendIng;							//���ͱ�־
	bool						m_bRecvIng;							//���ձ�־
	bool						m_bShutDown;						//�رձ�־
	bool						m_bAllowBatch;						//����Ⱥ��
	BYTE						m_bBatchMask;						//Ⱥ����ʾ

	//���ܱ���
protected:
	WORD						m_wRecvSize;						//���ճ���
	BYTE						m_cbRecvBuf[SOCKET_TCP_BUFFER * 5];	//���ջ���

	//��������
protected:
	DWORD						m_dwSendTickCount;					//����ʱ��
	DWORD						m_dwRecvTickCount;					//����ʱ��
	DWORD						m_dwSendPacketCount;				//���ͼ���
	DWORD						m_dwRecvPacketCount;				//���ܼ���

	//��������
protected:
	BYTE						m_cbSendRound;						//�ֽ�ӳ��
	BYTE						m_cbRecvRound;						//�ֽ�ӳ��
	DWORD						m_dwSendXorKey;						//������Կ
	DWORD						m_dwRecvXorKey;						//������Կ

	//�ڲ�����
protected:
	COverLappedRecv				m_OverLappedRecv;					//�ص��ṹ
	COverLappedSendPtr			m_OverLappedSendActive;				//�ص��ṹ

	//�������
protected:
	CCriticalSection			m_SendBufferSection;				//��������
	COverLappedSendPtr			m_OverLappedSendBuffer;				//�ص��ṹ

	//��������
public:
	//���캯��
	CTCPNetworkItem(WORD wIndex, ITCPNetworkItemSink * pITCPNetworkItemSink);
	//��������
	virtual ~CTCPNetworkItem();

	//��ȡ��ʶ����
public:
	//��ȡ����
	inline WORD GetIndex(){ return m_wIndex; }
	//��ȡ����
	inline WORD GetRountID(){ return m_wRountID; }
	//��ȡ��ʶ
	inline DWORD GetIdentifierID(){ return MAKELONG(m_wIndex, m_wRountID); }

	//��ȡ���Ժ���
public:
	//��ȡ��ַ
	inline DWORD GetClientIP() { return m_dwClientIP; }
	//����ʱ��
	inline DWORD GetActiveTime() { return m_dwActiveTime; }
	//����ʱ��
	inline DWORD GetSendTickCount() { return m_dwSendTickCount; }
	//����ʱ��
	inline DWORD GetRecvTickCount() { return m_dwRecvTickCount; }
	//���Ͱ���
	inline DWORD GetSendPacketCount() { return m_dwSendPacketCount; }
	//���հ���
	inline DWORD GetRecvPacketCount() { return m_dwRecvPacketCount; }
	//��������
	inline CCriticalSection & GetCriticalSection() { return m_CriticalSection; }

	//��ȡ״̬����
public:
	//Ⱥ������
	inline bool IsAllowBatch() { return m_bAllowBatch; }
	//��������
	inline bool IsAllowSendData() { return m_dwRecvPacketCount > 0L; }
	//�ж�����
	inline bool IsValidSocket() { return (m_hSocketHandle != INVALID_SOCKET); }
	//Ⱥ����ʾ
	inline BYTE GetBatchMask() { return m_bBatchMask; }

	//������
public:
	//�󶨶���
	DWORD Attach(SOCKET hSocket, DWORD dwClientIP);
	//�ָ�����
	DWORD ResumeData();

	//���ƺ���
public:
	//���ͺ���
	bool SendData(WORD wMainCmdID, WORD wSubCmdID, WORD wRountID);
	//���ͺ���
	bool SendData(VOID * pData, WORD wDataSize, WORD wMainCmdID, WORD wSubCmdID, WORD wRountID);
	//���ղ���
	bool RecvData();
	//�ر�����
	bool CloseSocket(WORD wRountID);

	//״̬����
public:
	//���ùر�
	bool ShutDownSocket(WORD wRountID);
	//����Ⱥ��
	bool AllowBatchSend(WORD wRountID, bool bAllowBatch, BYTE cbBatchMask);

	//֪ͨ�ӿ�
public:
	//�������
	bool OnSendCompleted(COverLappedSend * pOverLappedSend, DWORD dwThancferred);
	//�������
	bool OnrRecvCompleted(COverLappedSend * pOverLappedRecv, DWORD dwThancferred);
	//�ر����
	bool OnCloseCompleted();

	//���ܺ���
private:
	//��������
	WORD EncryptBuffer(BYTE pcbDataBuffer[], WORD wDataSize, WORD wBufferSize);
	//��������
	WORD CrevasseBuffer(BYTE pcbDataBuffer[], WORD wDataSize);

	//��������
private:
	//���ӳ��
	inline WORD SeedRandMap(WORD wSeed);
	//����ӳ������
	inline BYTE MapSendByte(BYTE cbData);
	//����ӳ������
	inline BYTE MapRecvByte(BYTE cbData);

	//��������
private:
	//�����ж�
	inline bool SendVerdict(WORD wRountID);
	//��ȡ�����ص��ṹ
	inline COverLappedSend * GetSendOverLapped(WORD wPacketSize);
};


#endif