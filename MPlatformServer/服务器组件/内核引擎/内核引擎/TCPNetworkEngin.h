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


#endif