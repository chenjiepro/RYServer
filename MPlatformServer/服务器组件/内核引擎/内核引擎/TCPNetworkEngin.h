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


#endif