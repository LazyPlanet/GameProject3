﻿#include "stdafx.h"
#include "AccountManager.h"
#include "CommonFunc.h"
#include "Log.h"
#include "../ServerData/ServerDefine.h"
#include "CommonConvert.h"

Th_RetName _SaveAccountThread( void* pParam )
{
	CAccountObjectMgr* pAccountManager = (CAccountObjectMgr*)pParam;
	if(pAccountManager != NULL)
	{
		pAccountManager->SaveAccountChange();
	}

	CommonThreadFunc::ExitThread();

	return Th_RetValue;

}

CAccountObjectMgr::CAccountObjectMgr()
{
	m_IsRun			= FALSE;
	m_hThread		= NULL;
	m_bCrossChannel = FALSE;
}

CAccountObjectMgr::~CAccountObjectMgr()
{
	m_IsRun		= FALSE;
	m_hThread	= NULL;
}

BOOL CAccountObjectMgr::LoadCacheAccount()
{
	std::string strHost = CConfigFile::GetInstancePtr()->GetStringValue("mysql_acc_svr_ip");
	UINT32 nPort = CConfigFile::GetInstancePtr()->GetIntValue("mysql_acc_svr_port");
	std::string strUser = CConfigFile::GetInstancePtr()->GetStringValue("mysql_acc_svr_user");
	std::string strPwd = CConfigFile::GetInstancePtr()->GetStringValue("mysql_acc_svr_pwd");
	std::string strDb = CConfigFile::GetInstancePtr()->GetStringValue("mysql_acc_svr_db_name");
	m_bCrossChannel = CConfigFile::GetInstancePtr()->GetIntValue("account_cross_channel");

	if(!m_DBConnection.open(strHost.c_str(), strUser.c_str(), strPwd.c_str(), strDb.c_str(), nPort))
	{
		CLog::GetInstancePtr()->LogError("LoadCacheAccount Error: Can not open database!!!");
		return FALSE;
	}

	CppMySQLQuery QueryResult = m_DBConnection.querySQL("select * from account");
	while(!QueryResult.eof())
	{
		AddAccountObject(QueryResult.getInt64Field("id"), QueryResult.getStringField("name"),
		                 QueryResult.getStringField("password"),
		                 QueryResult.getIntField("channel"));

		if(m_u64MaxID < QueryResult.getInt64Field("id"))
		{
			m_u64MaxID = QueryResult.getInt64Field("id");
		}

		QueryResult.nextRow();
	}

	m_IsRun = TRUE;
	m_hThread = CommonThreadFunc::CreateThread(_SaveAccountThread, this);
	ERROR_RETURN_FALSE(m_hThread != NULL);


	return TRUE;
}

CAccountObject* CAccountObjectMgr::GetAccountObjectByID( UINT64 AccountID )
{
	return GetByKey(AccountID);
}

CAccountObject* CAccountObjectMgr::CreateAccountObject(std::string strName, std::string strPwd, UINT32 dwChannel)
{
	m_u64MaxID += 1;

	CAccountObject* pObj = InsertAlloc(m_u64MaxID);
	ERROR_RETURN_NULL(pObj != NULL);

	pObj->m_SealStatue		= SS_OK;
	pObj->m_strName			= strName;
	pObj->m_strPassword		= strPwd;
	pObj->m_ID				= m_u64MaxID;
	pObj->m_dwChannel		= dwChannel;
	pObj->m_uCreateTime	= CommonFunc::GetCurrTime();

	if (m_bCrossChannel)
	{
		m_mapNameObj.insert(std::make_pair(strName, pObj));
	}
	else
	{
		m_mapNameObj.insert(std::make_pair(strName + CommonConvert::IntToString(dwChannel), pObj));
	}

	m_ArrChangedAccount.push(pObj);

	return pObj;
}

BOOL CAccountObjectMgr::ReleaseAccountObject(UINT64 AccountID )
{
	return Delete(AccountID);
}

BOOL CAccountObjectMgr::AddAccountObject(UINT64 u64ID, std::string strName, std::string strPwd, UINT32 dwChannel)
{
	CAccountObject* pObj = InsertAlloc(u64ID);
	ERROR_RETURN_FALSE(pObj != NULL);

	pObj->m_SealStatue = SS_OK;
	pObj->m_strName = strName;
	pObj->m_strPassword = strPwd;
	pObj->m_ID = u64ID;
	pObj->m_dwChannel = dwChannel;

	if (m_bCrossChannel)
	{
		m_mapNameObj.insert(std::make_pair(strName, pObj));
	}
	else
	{
		m_mapNameObj.insert(std::make_pair(strName + CommonConvert::IntToString(dwChannel), pObj));
	}

	return TRUE;
}

BOOL CAccountObjectMgr::SaveAccountChange()
{
	while(IsRun())
	{
		CAccountObject* pAccount = NULL;

		CHAR szSql[SQL_BUFF_LEN];

		while(m_ArrChangedAccount.pop(pAccount) && (pAccount != NULL))
		{
			snprintf(szSql, SQL_BUFF_LEN, "replace into account(id, name, password, channel, create_time) values('%lld','%s','%s', '%d', '%lld')",
			         pAccount->m_ID, pAccount->m_strName.c_str(), pAccount->m_strPassword.c_str(), pAccount->m_dwChannel, pAccount->m_uCreateTime);

			if(m_DBConnection.execSQL(szSql) > 0)
			{
				continue;
			}

			CLog::GetInstancePtr()->LogError("CAccountMsgHandler::SaveAccountChange Failed, DB Lose Connection!");

			int nTimes = 0;
			while (!m_DBConnection.reconnect())
			{
				nTimes++;
				if (nTimes > 3)
				{
					break;
				}
				CommonFunc::Sleep(1000);
			}

			if(m_DBConnection.execSQL(szSql) < 0)
			{
				CLog::GetInstancePtr()->LogError("CAccountMsgHandler::SaveAccountChange Failed, execSQL Error!");
			}
		}

		CommonFunc::Sleep(10);
	}

	return TRUE;
}

BOOL CAccountObjectMgr::Init()
{
	ERROR_RETURN_FALSE(LoadCacheAccount());

	return TRUE;
}

BOOL CAccountObjectMgr::Uninit()
{
	m_IsRun = FALSE;

	CommonThreadFunc::WaitThreadExit(m_hThread);

	m_mapNameObj.clear();

	m_DBConnection.close();

	Clear();

	return TRUE;
}

BOOL CAccountObjectMgr::IsRun()
{
	return m_IsRun;
}

CAccountObject* CAccountObjectMgr::GetAccountObject( std::string name, UINT32 dwChannel )
{
	if (m_bCrossChannel)
	{
		auto itor = m_mapNameObj.find(name);
		if (itor != m_mapNameObj.end())
		{
			return itor->second;
		}
	}
	else
	{
		auto itor = m_mapNameObj.find(name + CommonConvert::IntToString(dwChannel));
		if (itor != m_mapNameObj.end())
		{
			return itor->second;
		}
	}

	return NULL;
}
