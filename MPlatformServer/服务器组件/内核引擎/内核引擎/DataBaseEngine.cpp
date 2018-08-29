#include "stdafx.h"
#include "DataBaseEngine.h"
#include "Math.h"
#include "TraceServiceManager.h"

//////////////////////////////////////////////////////////////////////////

//�궨��
_COM_SMARTPTR_TYPEDEF(IADORecordBinding, __uuidof(IADORecordBinding));

//���Ч��
#define EfficacyResult(hResult) { if (FAILED(hResult)) _com_issue_error(hResult); }

//////////////////////////////////////////////////////////////////////////

//ADO������

//���캯��
CDataBaseException::CDataBaseException()
{
	//��������
	m_hResult = S_OK;
	m_SQLException = SQLException_None;

	return;
}
//��������
CDataBaseException::~CDataBaseException()
{

}
//�ӿڲ�ѯ
VOID * CDataBaseException::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(IDataBaseException, Guid, dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(IDataBaseException, Guid, dwQueryVer);

	return NULL;
}
//���ô���
VOID CDataBaseException::SetExceptionInfo(enSQLException SQLException, CComError * pComError)
{
	//У�����
	ASSERT(pComError != NULL);
	if (pComError == NULL) return;

	//������Ϣ
	m_SQLException = SQLException;
	m_hResult = pComError->Error();
	m_strDescribe.Format(TEXT("���ݿ��쳣��%s [ 0x%8x ]"),(LPCTSTR)pComError->Description(), pComError->Error());

	return;
}


//////////////////////////////////////////////////////////////////////////

//���ݿ����

//���캯��
CDataBase::CDataBase() : m_dwResumeConnectTime(10L), m_dwResumeConnectCount(30L)
{
	//״̬����
	m_dwConnectCount = 0;
	m_dwConnectErrorTime = 0;

	//��������
	m_DBCommand.CreateInstance(__uuidof(Command));
	m_DBRecordset.CreateInstance(__uuidof(Recordset));
	m_DBConnection.CreateInstance(__uuidof(Connection));

	//Ч������
	if (m_DBCommand == NULL) throw TEXT("���� m_DBCommand ����ʧ��");
	if (m_DBRecordset == NULL) throw TEXT("���� m_DBRecordset ����ʧ��");
	if (m_DBConnection == NULL) throw TEXT("���� m_DBConnection ����ʧ��");

	//���ñ���
	m_DBCommand->CommandType = adCmdStoredProc;

	return;
}
//��������
CDataBase::~CDataBase()
{
	//�ر�����
	CloseConnection();

	m_DBCommand.Release();
	m_DBRecordset.Release();
	m_DBConnection.Release();

	return;
}
//�ӿڲ�ѯ
VOID * CDataBase::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(IDataBase, Guid, dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(IDataBase, Guid, dwQueryVer);

	return NULL;
}
//������
VOID CDataBase::OpenConnection()
{
	try
	{
		//�ر�����
		CloseConnection();

		//������
		EfficacyResult(m_DBConnection->Open(_bstr_t(m_strConnect), L"", L"", adConnectUnspecified));

		//���ö���
		m_DBConnection->CursorLocation = adUseClient;
		m_DBCommand->ActiveConnection = m_DBConnection;
	}
	catch (CComError & ComError)
	{
		//���ñ���
		m_dwConnectCount = 0;
		m_dwConnectErrorTime = (DWORD)time(NULL);

		OnSQLException(SQLException_Connect, &ComError);
	}

	return;
}
//�ر�����
VOID CDataBase::CloseConnection()
{
	try
	{
		//���ñ���
		m_dwConnectCount = 0;
		m_dwConnectErrorTime = 0;

		//�ر�����
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
//������Ϣ
bool CDataBase::SetConnectionInfo(DWORD dwDBAddr, WORD wPort, LPCTSTR szDBName, LPCTSTR szUser, LPCTSTR szPassword)
{
	//�������
	ASSERT(szUser != NULL);
	ASSERT(szDBName != NULL);
	ASSERT(szPassword != NULL);

	//��������
	CString strUser;
	CString strDBName;
	CString strPassword;

	//�ַ���ת��
	ConvertToSQLSyntax(szUser, strUser);
	ConvertToSQLSyntax(szDBName, strDBName);
	ConvertToSQLSyntax(szPassword, strPassword);

	//��������
	BYTE * pcbDBAddr = (BYTE *)&dwDBAddr;
	m_strConnect.Format(TEXT("Provider=SQLOLEDB.1;Password=%s;Persist Security Info=True;User ID=%s;Initial Catalog=%s;Data Source=%d.%d.%d.%d,%ld;"),
		strPassword, strUser, strDBName, pcbDBAddr[0], pcbDBAddr[1], pcbDBAddr[2], pcbDBAddr[3], wPort);

	return true;
}
//������Ϣ
bool CDataBase::SetConnectionInfo(LPCTSTR dwDBAddr, WORD wPort, LPCTSTR szDBName, LPCTSTR szUser, LPCTSTR szPassword)
{

}
//�������
VOID CDataBase::ClearParameters()
{

}
//��ȡ����
VOID CDataBase::GetParameters(LPCTSTR pszParamName, CDBVarValue & DBVarValue)
{

}
//�������
VOID CDataBase::AddParameters(LPCTSTR pszName, DataTypeEnum Type, ParameterDirectionEnum Direction, LONG Size, CDBVarValue & DBVarValue)
{

}
//�л���¼
VOID CDataBase::NextRecordset()
{

}
//�رռ�¼
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
//�󶨶���
VOID CDataBase::BindToRecordset(CADORecordBinding * pBind)
{

}
//�����ƶ�
VOID CDataBase::MoveToNext()
{

}
//�Ƶ���ͷ
VOID CDataBase::MoveToFirst()
{

}
//�Ƿ����
bool CDataBase::IsRecordsetEnd()
{

}
//��ȡ��Ŀ
LONG CDataBase::GetRecordCount()
{

}
//������ֵ
LONG CDataBase::GetReturnValue()
{

}
//��ȡ����
VOID CDataBase::GetRecordsetValue(LPCTSTR pszItem, CDBVarValue & DBVarValue)
{

}
//�洢����
VOID CDataBase::ExecuteProcess(LPCTSTR pszSPName, bool bRecordset)
{

}
//ִ�����
VOID CDataBase::ExecuteSentence(LPCTSTR pszCommand, bool bRecordset)
{

}
//���Ӵ���
bool CDataBase::IsConnectError()
{

}
//�Ƿ��
bool CDataBase::IsRecordsetOpen()
{
	//״̬�ж�
	if (m_DBRecordset == NULL) return false;
	if (m_DBRecordset->GetState() == adStateClosed) return false;

	return true;
}
//��������
bool CDataBase::TryConnectAgain(CComError * pComError)
{

}
//ת���ַ�
VOID CDataBase::ConvertToSQLSyntax(LPCTSTR pszString, CString & strResult)
{

}
//������
VOID CDataBase::OnSQLException(enSQLException SQLException, CComError * pComError)
{

}

