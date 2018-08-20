#ifndef KERNERL_ENGINE_HEAD_FILE
#define KERNERL_ENGINE_HEAD_FILE
//////////////////////////////////////////////////////////////////////////////////
//�����ļ�

#include <Afxmt.h>
#include <Comutil.h>
#include <ICrsint.h>
#include <Process.h>
#include <Wininet.h>
#include <WinSock2.h>

//ƽ̨�ļ�

#include "..\..\..\���������\�������\ServiceCoreDll\ServiceCoreDll.h"

//////////////////////////////////////////////////////////////////////////////////
//ADO ����

#import "MSADO15.DLL" rename_namespace("ADOCG") rename("EOF","EndOfFile")
using namespace ADOCG;

typedef _com_error						CComError;						//COM ����
typedef _variant_t						CDBVarValue;					//���ݿ���ֵ

//////////////////////////////////////////////////////////////////////////////////
//��������

//��������
#ifndef KERNEL_ENGINE_CLASS
	#ifdef  KERNEL_ENGINE_DLL
	#define KERNEL_ENGINE_CLASS _declspec(dllexport)
	#else
	#define KERNEL_ENGINE_CLASS _declspec(dllimport)
	#endif
#endif

//ģ�鶨��
#ifndef _DEBUG
#define KERNEL_ENGINE_DLL_NAME	TEXT("KernelEngine.dll")			//��� DLL ����
#else
#define KERNEL_ENGINE_DLL_NAME	TEXT("KernelEngineD.dll")			//��� DLL ����
#endif

//////////////////////////////////////////////////////////////////////////////////
//ϵͳ����

//��������
#define TIME_CELL					200									//ʱ�䵥Ԫ
#define TIMES_INFINITY				DWORD(-1)							//���޴���
#define MAX_ASYNCHRONISM_DATA		16384								//�첽����

//////////////////////////////////////////////////////////////////////////////////
//���綨��

//���Ӵ���
#define CONNECT_SUCCESS				0									//���ӳɹ�
#define CONNECT_FAILURE				1									//����ʧ��
#define CONNECT_EXCEPTION			2									//�����쳣

//����״̬
#define SOCKET_STATUS_IDLE			0									//����״̬
#define SOCKET_STATUS_WAIT			1									//�ȴ�״̬
#define SOCKET_STATUS_CONNECT		2									//����״̬

//�ر�ԭ��
#define SHUT_REASON_INSIDE			0									//�ڲ�ԭ��
#define SHUT_REASON_NORMAL			1									//�����ر�
#define SHUT_REASON_REMOTE			2									//Զ�̹ر�
#define SHUT_REASON_TIME_OUT		3									//���糬ʱ
#define SHUT_REASON_EXCEPTION		4									//�쳣�ر�

//////////////////////////////////////////////////////////////////////////////////
//ö�ٶ���

//����ȼ�
enum enTraceLevel
{
	TraceLevel_Info = 0,									//��Ϣ��Ϣ
	TraceLevel_Normal = 1,									//��ͨ��Ϣ
	TraceLevel_Warning = 2,									//������Ϣ
	TraceLevel_Exception = 3,								//�쳣��Ϣ
	TraceLevel_Debug = 4,									//������Ϣ
};

//SQL �쳣����
enum enSQLException
{
	SQLException_None = 0,									//û���쳣
	SQLException_Connect = 1,									//���Ӵ���
	SQLException_Syntax = 2,									//�﷨����
};

//////////////////////////////////////////////////////////////////////////////////
//�¼�����

//�¼���ʶ
#define EVENT_TIMER					0x0001								//ʱ���¼�
#define EVENT_CONTROL				0x0002								//�����¼�
#define EVENT_DATABASE				0x0003								//���ݿ��¼�

//�����¼�
#define EVENT_TCP_SOCKET_READ		0x0004								//��ȡ�¼�
#define EVENT_TCP_SOCKET_SHUT		0x0005								//�ر��¼�
#define EVENT_TCP_SOCKET_LINK		0x0006								//�����¼�

//�����¼�
#define EVENT_TCP_NETWORK_ACCEPT	0x0007								//Ӧ���¼�
#define EVENT_TCP_NETWORK_READ		0x0008								//��ȡ�¼�
#define EVENT_TCP_NETWORK_SHUT		0x0009								//�ر��¼�

//�¼�����
#define EVENT_MASK_KERNEL			0x00FF								//�ں��¼�
#define EVENT_MASK_CUSTOM			0xFF00								//�Զ����¼�

//////////////////////////////////////////////////////////////////////////////////


//�����¼�
struct NTY_ControlEvent
{
	WORD							wControlID;							//���Ʊ�ʶ
};

//��ʱ���¼�
struct NTY_TimerEvent
{
	DWORD							dwTimerID;							//ʱ���ʶ
	WPARAM							dwBindParameter;					//�󶨲���
};

//���ݿ��¼�
struct NTY_DataBaseEvent
{
	WORD							wRequestID;							//�����ʶ
	DWORD							dwContextID;						//�����ʶ
};

//��ȡ�¼�
struct NTY_TCPSocketReadEvent
{
	WORD							wDataSize;							//���ݴ�С
	WORD							wServiceID;							//�����ʶ
	TCP_Command						Command;							//������Ϣ
};

//�ر��¼�
struct NTY_TCPSocketShutEvent
{
	WORD							wServiceID;							//�����ʶ
	BYTE							cbShutReason;						//�ر�ԭ��
};

//�����¼�
struct NTY_TCPSocketLinkEvent
{
	INT								nErrorCode;							//�������
	WORD							wServiceID;							//�����ʶ
};

//Ӧ���¼�
struct NTY_TCPNetworkAcceptEvent
{
	DWORD							dwSocketID;							//�����ʶ
	DWORD							dwClientAddr;						//���ӵ�ַ
};

//��ȡ�¼�
struct NTY_TCPNetworkReadEvent
{
	WORD							wDataSize;							//���ݴ�С
	DWORD							dwSocketID;							//�����ʶ
	TCP_Command						Command;							//������Ϣ
};

//�ر��¼�
struct NTY_TCPNetworkShutEvent
{
	DWORD							dwSocketID;							//�����ʶ
	DWORD							dwClientAddr;						//���ӵ�ַ
	DWORD							dwActiveTime;						//����ʱ��
};

//////////////////////////////////////////////////////////////////////////////////

#ifdef _UNICODE
#define VER_IDataBaseException INTERFACE_VERSION(1,1)
static const GUID IID_IDataBaseException = { 0x008be9d3, 0x2305, 0x40da, 0x00ae, 0xd1, 0x61, 0x7a, 0xd2, 0x2a, 0x47, 0xfc };
#else
#define VER_IDataBaseException INTERFACE_VERSION(1,1)
static const GUID IID_IDataBaseException = { 0x428361ed, 0x9dfa, 0x43d7, 0x008f, 0x26, 0x17, 0x06, 0x47, 0x6b, 0x2a, 0x51 };
#endif

//���ݿ��쳣
interface IDataBaseException : public IUnknownEx
{
	//�쳣����
	virtual HRESULT GetExceptionResult() = NULL;
	//�쳣����
	virtual LPCTSTR GetExceptionDescribe() = NULL;
	//�쳣����
	virtual enSQLException GetExceptionType() = NULL;
};


#ifdef _UNICODE
#define VER_ITraceService INTERFACE_VERSION(1,1)
static const GUID IID_ITraceService = { 0xe4096162, 0x8134, 0x4d2c, 0x00b6, 0x4f, 0x09, 0x5d, 0xcc, 0xca, 0xe0, 0x81 };
#else
#define VER_ITraceService INTERFACE_VERSION(1,1)
static const GUID IID_ITraceService = { 0xe5f636c6, 0xabb5, 0x4752, 0x00bb, 0xc8, 0xcd, 0xb1, 0x76, 0x58, 0xf5, 0x2d };
#endif


//�¼����
interface ITraceService : public IUnknownEx
{
	//�ִ����
	virtual bool TraceString(LPCTSTR pszString, enTraceLevel TraceLevel) = NULL;
};

#ifdef _UNICODE
#define VER_ITraceServiceManager INTERFACE_VERSION(1,1)
static const GUID IID_ITraceServiceManager = { 0x6d14efe6, 0x892a, 0x4a48, 0x0092, 0xc9, 0xdb, 0xea, 0x92, 0xdd, 0xd5, 0x13 };
#else
#define VER_ITraceServiceManager INTERFACE_VERSION(1,1)
static const GUID IID_ITraceServiceManager = { 0x8bfc36db, 0x5ba2, 0x42ba, 0x0081, 0xb0, 0x87, 0xb0, 0x1c, 0x9e, 0xaf, 0xfe };
#endif

//�������
interface ITraceServiceManager : public IUnknownEx
{
	//״̬����
public:
	//׷��״̬
	virtual bool IsEnableTrace(enTraceLevel TraceLevel) = NULL;
	//׷�ٿ���
	virtual bool EnableTrace(enTraceLevel TraceLevel, bool bEnableTrace) = NULL;

	//��������
public:
	//���ýӿ�
	virtual bool SetTraceService(IUnknownEx * pIUnknownEx) = NULL;
	//��ȡ�ӿ�
	virtual VOID *GetTraceService(REFGUID Guid, DWORD dwQueryVer) = NULL;

	//����ӿ�
public:
	//�ִ����
	virtual bool TraceString(LPCTSTR pszString, enTraceLevel TraceLevel) = NULL;
};
#endif