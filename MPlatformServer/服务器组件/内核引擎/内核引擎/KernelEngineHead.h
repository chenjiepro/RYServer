#ifndef KERNERL_ENGINE_HEAD_FILE
#define KERNERL_ENGINE_HEAD_FILE
//////////////////////////////////////////////////////////////////////////////////
//包含文件

#include <Afxmt.h>
#include <Comutil.h>
#include <ICrsint.h>
#include <Process.h>
#include <Wininet.h>
#include <WinSock2.h>

//平台文件

#include "..\..\..\服务器组件\服务核心\ServiceCoreDll\ServiceCoreDll.h"

//////////////////////////////////////////////////////////////////////////////////
//ADO 定义

#import "MSADO15.DLL" rename_namespace("ADOCG") rename("EOF","EndOfFile")
using namespace ADOCG;

typedef _com_error						CComError;						//COM 错误
typedef _variant_t						CDBVarValue;					//数据库数值

//////////////////////////////////////////////////////////////////////////////////
//导出定义

//导出定义
#ifndef KERNEL_ENGINE_CLASS
	#ifdef  KERNEL_ENGINE_DLL
	#define KERNEL_ENGINE_CLASS _declspec(dllexport)
	#else
	#define KERNEL_ENGINE_CLASS _declspec(dllimport)
	#endif
#endif

//模块定义
#ifndef _DEBUG
#define KERNEL_ENGINE_DLL_NAME	TEXT("KernelEngine.dll")			//组件 DLL 名字
#else
#define KERNEL_ENGINE_DLL_NAME	TEXT("KernelEngineD.dll")			//组件 DLL 名字
#endif

//////////////////////////////////////////////////////////////////////////////////
//系统常量

//常量定义
#define TIME_CELL					200									//时间单元
#define TIMES_INFINITY				DWORD(-1)							//无限次数
#define MAX_ASYNCHRONISM_DATA		16384								//异步数据

//////////////////////////////////////////////////////////////////////////////////
//网络定义

//连接错误
#define CONNECT_SUCCESS				0									//连接成功
#define CONNECT_FAILURE				1									//连接失败
#define CONNECT_EXCEPTION			2									//参数异常

//网络状态
#define SOCKET_STATUS_IDLE			0									//空闲状态
#define SOCKET_STATUS_WAIT			1									//等待状态
#define SOCKET_STATUS_CONNECT		2									//连接状态

//关闭原因
#define SHUT_REASON_INSIDE			0									//内部原因
#define SHUT_REASON_NORMAL			1									//正常关闭
#define SHUT_REASON_REMOTE			2									//远程关闭
#define SHUT_REASON_TIME_OUT		3									//网络超时
#define SHUT_REASON_EXCEPTION		4									//异常关闭

//////////////////////////////////////////////////////////////////////////////////
//枚举定义

//输出等级
enum enTraceLevel
{
	TraceLevel_Info = 0,									//信息消息
	TraceLevel_Normal = 1,									//普通消息
	TraceLevel_Warning = 2,									//警告消息
	TraceLevel_Exception = 3,								//异常消息
	TraceLevel_Debug = 4,									//调试消息
};

//SQL 异常类型
enum enSQLException
{
	SQLException_None = 0,									//没有异常
	SQLException_Connect = 1,									//连接错误
	SQLException_Syntax = 2,									//语法错误
};

//////////////////////////////////////////////////////////////////////////////////
//事件定义

//事件标识
#define EVENT_TIMER					0x0001								//时间事件
#define EVENT_CONTROL				0x0002								//控制事件
#define EVENT_DATABASE				0x0003								//数据库事件

//网络事件
#define EVENT_TCP_SOCKET_READ		0x0004								//读取事件
#define EVENT_TCP_SOCKET_SHUT		0x0005								//关闭事件
#define EVENT_TCP_SOCKET_LINK		0x0006								//连接事件

//网络事件
#define EVENT_TCP_NETWORK_ACCEPT	0x0007								//应答事件
#define EVENT_TCP_NETWORK_READ		0x0008								//读取事件
#define EVENT_TCP_NETWORK_SHUT		0x0009								//关闭事件

//事件掩码
#define EVENT_MASK_KERNEL			0x00FF								//内核事件
#define EVENT_MASK_CUSTOM			0xFF00								//自定义事件

//////////////////////////////////////////////////////////////////////////////////


//控制事件
struct NTY_ControlEvent
{
	WORD							wControlID;							//控制标识
};

//定时器事件
struct NTY_TimerEvent
{
	DWORD							dwTimerID;							//时间标识
	WPARAM							dwBindParameter;					//绑定参数
};

//数据库事件
struct NTY_DataBaseEvent
{
	WORD							wRequestID;							//请求标识
	DWORD							dwContextID;						//对象标识
};

//读取事件
struct NTY_TCPSocketReadEvent
{
	WORD							wDataSize;							//数据大小
	WORD							wServiceID;							//服务标识
	TCP_Command						Command;							//命令信息
};

//关闭事件
struct NTY_TCPSocketShutEvent
{
	WORD							wServiceID;							//服务标识
	BYTE							cbShutReason;						//关闭原因
};

//连接事件
struct NTY_TCPSocketLinkEvent
{
	INT								nErrorCode;							//错误代码
	WORD							wServiceID;							//服务标识
};

//应答事件
struct NTY_TCPNetworkAcceptEvent
{
	DWORD							dwSocketID;							//网络标识
	DWORD							dwClientAddr;						//连接地址
};

//读取事件
struct NTY_TCPNetworkReadEvent
{
	WORD							wDataSize;							//数据大小
	DWORD							dwSocketID;							//网络标识
	TCP_Command						Command;							//命令信息
};

//关闭事件
struct NTY_TCPNetworkShutEvent
{
	DWORD							dwSocketID;							//网络标识
	DWORD							dwClientAddr;						//连接地址
	DWORD							dwActiveTime;						//连接时间
};

//////////////////////////////////////////////////////////////////////////////////

#ifdef _UNICODE
#define VER_IDataBaseException INTERFACE_VERSION(1,1)
static const GUID IID_IDataBaseException = { 0x008be9d3, 0x2305, 0x40da, 0x00ae, 0xd1, 0x61, 0x7a, 0xd2, 0x2a, 0x47, 0xfc };
#else
#define VER_IDataBaseException INTERFACE_VERSION(1,1)
static const GUID IID_IDataBaseException = { 0x428361ed, 0x9dfa, 0x43d7, 0x008f, 0x26, 0x17, 0x06, 0x47, 0x6b, 0x2a, 0x51 };
#endif

//数据库异常
interface IDataBaseException : public IUnknownEx
{
	//异常代码
	virtual HRESULT GetExceptionResult() = NULL;
	//异常描述
	virtual LPCTSTR GetExceptionDescribe() = NULL;
	//异常类型
	virtual enSQLException GetExceptionType() = NULL;
};


#ifdef _UNICODE
#define VER_ITraceService INTERFACE_VERSION(1,1)
static const GUID IID_ITraceService = { 0xe4096162, 0x8134, 0x4d2c, 0x00b6, 0x4f, 0x09, 0x5d, 0xcc, 0xca, 0xe0, 0x81 };
#else
#define VER_ITraceService INTERFACE_VERSION(1,1)
static const GUID IID_ITraceService = { 0xe5f636c6, 0xabb5, 0x4752, 0x00bb, 0xc8, 0xcd, 0xb1, 0x76, 0x58, 0xf5, 0x2d };
#endif


//事件输出
interface ITraceService : public IUnknownEx
{
	//字串输出
	virtual bool TraceString(LPCTSTR pszString, enTraceLevel TraceLevel) = NULL;
};

#ifdef _UNICODE
#define VER_ITraceServiceManager INTERFACE_VERSION(1,1)
static const GUID IID_ITraceServiceManager = { 0x6d14efe6, 0x892a, 0x4a48, 0x0092, 0xc9, 0xdb, 0xea, 0x92, 0xdd, 0xd5, 0x13 };
#else
#define VER_ITraceServiceManager INTERFACE_VERSION(1,1)
static const GUID IID_ITraceServiceManager = { 0x8bfc36db, 0x5ba2, 0x42ba, 0x0081, 0xb0, 0x87, 0xb0, 0x1c, 0x9e, 0xaf, 0xfe };
#endif

//输出管理
interface ITraceServiceManager : public IUnknownEx
{
	//状态管理
public:
	//追踪状态
	virtual bool IsEnableTrace(enTraceLevel TraceLevel) = NULL;
	//追踪控制
	virtual bool EnableTrace(enTraceLevel TraceLevel, bool bEnableTrace) = NULL;

	//服务配置
public:
	//设置接口
	virtual bool SetTraceService(IUnknownEx * pIUnknownEx) = NULL;
	//获取接口
	virtual VOID *GetTraceService(REFGUID Guid, DWORD dwQueryVer) = NULL;

	//服务接口
public:
	//字串输出
	virtual bool TraceString(LPCTSTR pszString, enTraceLevel TraceLevel) = NULL;
};
#endif