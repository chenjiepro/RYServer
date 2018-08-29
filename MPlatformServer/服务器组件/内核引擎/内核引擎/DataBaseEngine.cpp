#include "stdafx.h"
#include "DataBaseEngine.h"
#include "Math.h"
#include "TraceServiceManager.h"

//////////////////////////////////////////////////////////////////////////

//宏定义
_COM_SMARTPTR_TYPEDEF(IADORecordBinding, __uuidof(IADORecordBinding));

//结果效验
#define EfficacyResult(hResult) { if (FAILED(hResult)) _com_issue_error(hResult); }

//////////////////////////////////////////////////////////////////////////

//ADO错误类

//构造函数
CDataBaseException::CDataBaseException()
{
	//错误类型
	m_hResult = S_OK;
	m_SQLException = SQLException_None;

	return;
}
//析构函数
CDataBaseException::~CDataBaseException()
{

}
//接口查询
VOID * CDataBaseException::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(IDataBaseException, Guid, dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(IDataBaseException, Guid, dwQueryVer);

	return NULL;
}
//设置错误
VOID CDataBaseException::SetExceptionInfo(enSQLException SQLException, CComError * pComError)
{
	//校验参数
	ASSERT(pComError != NULL);
	if (pComError == NULL) return;

	//设置信息
	m_SQLException = SQLException;
	m_hResult = pComError->Error();
	m_strDescribe.Format(TEXT("数据库异常：%s [ 0x%8x ]"),(LPCTSTR)pComError->Description(), pComError->Error());

	return;
}


//////////////////////////////////////////////////////////////////////////

//数据库对象

//构造函数
CDataBase::CDataBase() : m_dwResumeConnectTime(10L), m_dwResumeConnectCount(30L)
{
	//状态变量
	m_dwConnectCount = 0;
	m_dwConnectErrorTime = 0;

	//创建对象
	m_DBCommand.CreateInstance(__uuidof(Command));
	m_DBRecordset.CreateInstance(__uuidof(Recordset));
	m_DBConnection.CreateInstance(__uuidof(Connection));

	//效验数据
	if (m_DBCommand == NULL) throw TEXT("创建 m_DBCommand 对象失败");
	if (m_DBRecordset == NULL) throw TEXT("创建 m_DBRecordset 对象失败");
	if (m_DBConnection == NULL) throw TEXT("创建 m_DBConnection 对象失败");

	//设置变量
	m_DBCommand->CommandType = adCmdStoredProc;

	return;
}
//析构函数
CDataBase::~CDataBase()
{
	//关闭连接
	CloseConnection();

	m_DBCommand.Release();
	m_DBRecordset.Release();
	m_DBConnection.Release();

	return;
}
//接口查询
VOID * CDataBase::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(IDataBase, Guid, dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(IDataBase, Guid, dwQueryVer);

	return NULL;
}
//打开连接
VOID CDataBase::OpenConnection()
{
	try
	{
		//关闭连接
		CloseConnection();

		//打开连接
		EfficacyResult(m_DBConnection->Open(_bstr_t(m_strConnect), L"", L"", adConnectUnspecified));

		//设置对象
		m_DBConnection->CursorLocation = adUseClient;
		m_DBCommand->ActiveConnection = m_DBConnection;
	}
	catch (CComError & ComError)
	{
		//设置变量
		m_dwConnectCount = 0;
		m_dwConnectErrorTime = (DWORD)time(NULL);

		OnSQLException(SQLException_Connect, &ComError);
	}

	return;
}
//关闭连接
VOID CDataBase::CloseConnection()
{
	try
	{
		//设置变量
		m_dwConnectCount = 0;
		m_dwConnectErrorTime = 0;

		//关闭连接
		CloseRecordset();

		if (m_DBConnection != NULL && m_DBConnection->GetState() != adStateClosed)
		{
			EfficacyResult(m_DBConnection->Close());
		}
	}
	catch (CComError & ComError)
	{
		OnSQLException(SQLException_Syntax, &ComError);
	}

	return;
}
//连接信息
bool CDataBase::SetConnectionInfo(DWORD dwDBAddr, WORD wPort, LPCTSTR szDBName, LPCTSTR szUser, LPCTSTR szPassword)
{
	//检验参数
	ASSERT(szUser != NULL);
	ASSERT(szDBName != NULL);
	ASSERT(szPassword != NULL);

	//变量定义
	CString strUser;
	CString strDBName;
	CString strPassword;

	//字符串转换
	ConvertToSQLSyntax(szUser, strUser);
	ConvertToSQLSyntax(szDBName, strDBName);
	ConvertToSQLSyntax(szPassword, strPassword);

	//构造连接
	BYTE * pcbDBAddr = (BYTE *)&dwDBAddr;
	m_strConnect.Format(TEXT("Provider=SQLOLEDB.1;Password=%s;Persist Security Info=True;User ID=%s;Initial Catalog=%s;Data Source=%d.%d.%d.%d,%ld;"),
		strPassword, strUser, strDBName, pcbDBAddr[0], pcbDBAddr[1], pcbDBAddr[2], pcbDBAddr[3], wPort);

	return true;
}
//连接信息
bool CDataBase::SetConnectionInfo(LPCTSTR dwDBAddr, WORD wPort, LPCTSTR szDBName, LPCTSTR szUser, LPCTSTR szPassword)
{

}
//清除参数
VOID CDataBase::ClearParameters()
{

}
//获取参数
VOID CDataBase::GetParameters(LPCTSTR pszParamName, CDBVarValue & DBVarValue)
{

}
//插入参数
VOID CDataBase::AddParameters(LPCTSTR pszName, DataTypeEnum Type, ParameterDirectionEnum Direction, LONG Size, CDBVarValue & DBVarValue)
{

}
//切换记录
VOID CDataBase::NextRecordset()
{

}
//关闭记录
VOID CDataBase::CloseRecordset()
{
	try
	{
		if (IsRecordsetOpen() == true)
			EfficacyResult(m_DBRecordset->Close());
	}
	catch (CComError & ComError)
	{
		OnSQLException(SQLException_Syntax, &ComError);
	}

	return;
}
//绑定对象
VOID CDataBase::BindToRecordset(CADORecordBinding * pBind)
{

}
//往下移动
VOID CDataBase::MoveToNext()
{

}
//移到开头
VOID CDataBase::MoveToFirst()
{

}
//是否结束
bool CDataBase::IsRecordsetEnd()
{

}
//获取数目
LONG CDataBase::GetRecordCount()
{

}
//返回数值
LONG CDataBase::GetReturnValue()
{

}
//获取数据
VOID CDataBase::GetRecordsetValue(LPCTSTR pszItem, CDBVarValue & DBVarValue)
{

}
//存储过程
VOID CDataBase::ExecuteProcess(LPCTSTR pszSPName, bool bRecordset)
{

}
//执行语句
VOID CDataBase::ExecuteSentence(LPCTSTR pszCommand, bool bRecordset)
{

}
//连接错误
bool CDataBase::IsConnectError()
{

}
//是否打开
bool CDataBase::IsRecordsetOpen()
{
	//状态判断
	if (m_DBRecordset == NULL) return false;
	if (m_DBRecordset->GetState() == adStateClosed) return false;

	return true;
}
//重新连接
bool CDataBase::TryConnectAgain(CComError * pComError)
{

}
//转换字符
VOID CDataBase::ConvertToSQLSyntax(LPCTSTR pszString, CString & strResult)
{

}
//错误处理
VOID CDataBase::OnSQLException(enSQLException SQLException, CComError * pComError)
{

}

