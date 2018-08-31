#ifndef TCP_SOCKET_SERVICE_HEAD_FILE
#define TCP_SOCKET_SERVICE_HEAD_FILE

#pragma once

#include "KernelEngineHead.h"

//////////////////////////////////////////////////////////////////////////

//�����߳�
class CTCPSocketServiceThread : public CWHThread
{
	//�ں˱���
protected:
	HWND						m_hWnd;							//���ھ��
	SOCKET						m_hSocket;						//���Ӿ��
	BYTE						m_TCPSocketStatus;				//����״̬

	//�������
protected:
	CWHDataQueue				m_DataQueue;					//�������
	CCriticalSection			m_CriticalSection;				//��������

	//���ձ���
protected:
	WORD						m_wRecvSize;					//���ճ���
	BYTE						m_cbRecvBuf[SOCKET_TCP_BUFFER * 10];	//���ջ���

	//�������
protected:
	bool						m_bNeedBuffer;					//����״̬
	DWORD						m_dwBufferData;					//��������ʵ�ʴ�С
	DWORD						m_dwBufferSize;					//�����С�ܳ���
	LPBYTE						m_pcbDataBuffer;				//��������

	//��������
protected:
	BYTE						m_cbSendRound;					//�ֽ�ӳ��
	BYTE						m_cbRecvRound;					//�ֽ�ӳ��
	DWORD							m_dwSendXorKey;						//������Կ
	DWORD							m_dwRecvXorKey;						//������Կ

	//��������
protected:
	DWORD							m_dwSendTickCount;					//����ʱ��
	DWORD							m_dwRecvTickCount;					//����ʱ��
	DWORD							m_dwSendPacketCount;				//���ͼ���
	DWORD							m_dwRecvPacketCount;				//���ܼ���

	//��������
public:
	//���캯��
	CTCPSocketServiceThread();
	//��������
	virtual ~CTCPSocketServiceThread();

	//���غ���
public:
	//�����¼�
	virtual bool OnEventThreadRun();
	//��ʼ�¼�
	virtual bool OnEventThreadStart();
	//��ֹ�¼�
	virtual bool OnEventThreadConclude();
	//�����߳�
	virtual bool ConcludeThread(DWORD dwMillSeconds);

	//������
public:
	//Ͷ������
	bool PostThreadRequest(WORD wIdentifier, VOID * const pBuffer, WORD wDataSize);

	//���ƺ���
private:
	//�ر�����
	VOID PerformCloseSocket(BYTE cbShutReason);
	//���ӷ�����
	DWORD PerformConnect(DWORD dwServerIP, WORD wPort);
	//���ͺ���
	DWORD PerformSendData(WORD wMainCmdID, WORD wSubCmdID);
	//���ͺ���
	DWORD PerformSendData(WORD wMainCmdID, WORD wSubCmdID, VOID * pData, WORD wDataSize);

	//��������
private:
	//���ͺ���
	DWORD SendBuffer(VOID * pBuffer, WORD wSendSize);
	//��������
	VOID AmortizeBuffer(VOID * pData, WORD wDataSize);
	//��������
	WORD CrevasseBuffer(BYTE cbDataBuffer[], WORD wDataSize);
	//��������
	WORD EncryptBuffer(BYTE cbDataBuffer[], WORD wDataSize, WORD wBufferSize);

	//������
private:
	//������Ϣ
	LRESULT OnSocketNotify(WPARAM wParam, LPARAM lParam);
	//������Ϣ
	LRESULT OnServiceRequest(WPARAM wParam, LPARAM lParam);

	//��������
private:
	//�����ȡ
	LRESULT OnSocketNotifyRead(WPARAM wParam, LPARAM lParam);
	//���緢��
	LRESULT OnSocketNotifyWrite(WPARAM wParam, LPARAM lParam);
	//����ر�
	LRESULT OnSocketNotifyClose(WPARAM wParam, LPARAM lParam);
	//��������
	LRESULT OnSocketNotifyConnect(WPARAM wParam, LPARAM lParam);

	//��������
private:
	//���ӳ��
	inline WORD SeedRandMap(WORD wSeed);
	//����ӳ��
	inline BYTE MapSendByte(BYTE cbData);
	//����ӳ��
	inline BYTE MapRecvByte(BYTE cbData);
};


//////////////////////////////////////////////////////////////////////////

#endif