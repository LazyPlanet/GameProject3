﻿#include "stdafx.h"
#include "CommandDef.h"
#include "AccountMsgHandler.h"
#include "CommonFunc.h"
#include "CommonEvent.h"
#include "PacketHeader.h"
#include "GameService.h"
#include "CommonSocket.h"
#include "../Message/Msg_ID.pb.h"
#include "../Message/Msg_RetCode.pb.h"
#include "../Message/Msg_Game.pb.h"
#include "Log.h"




CAccountMsgHandler::CAccountMsgHandler()
{

}

CAccountMsgHandler::~CAccountMsgHandler()
{

}

BOOL CAccountMsgHandler::Init(UINT32 dwReserved)
{
	m_DBManager.Init();

	m_AccountManager.InitManager();

	return TRUE;
}

BOOL CAccountMsgHandler::Uninit()
{
	m_DBManager.Uninit();

	return TRUE;
}


BOOL CAccountMsgHandler::DispatchPacket(NetPacket* pNetPacket)
{
	switch(pNetPacket->m_dwMsgID)
	{
		default:
		{
			PROCESS_MESSAGE_ITEM(MSG_ACCOUNT_REG_REQ,	    OnMsgAccountRegReq);
			PROCESS_MESSAGE_ITEM(MSG_ACCOUNT_LOGIN_REQ,		OnMsgAccontLoginReq);
		}
		break;
	}


	return TRUE;
}

BOOL CAccountMsgHandler::OnUpdate(UINT32 dwTick)
{
	return TRUE;
}

BOOL CAccountMsgHandler::OnMsgAccountRegReq(NetPacket* pPacket)
{
	AccountRegReq Req;

	Req.ParsePartialFromArray(pPacket->m_pDataBuffer->GetData(), pPacket->m_pDataBuffer->GetBodyLenth());

	PacketHeader* pHeader = (PacketHeader*) pPacket->m_pDataBuffer->GetBuffer();

	ERROR_RETURN_TRUE(pHeader->dwUserData != 0);

	AccountRegAck Ack;

	CAccountObject* pAccount = m_AccountManager.GetAccountObjectByName(Req.accountname());
	if(pAccount != NULL)
	{
		Ack.set_retcode(MRC_ACCOUNT_EXIST);
		ServiceBase::GetInstancePtr()->SendMsgProtoBuf(pPacket->m_dwConnID, MSG_ACCOUNT_REG_ACK, 0, pHeader->dwUserData, Ack);
		CLog::GetInstancePtr()->LogError("Error CAccountMsgHandler::OnMsgAccountRegReq RetCode:MRC_ACCOUNT_EXIST");
		return TRUE;
	}



	UINT64 u64ID = 0;
	UINT32 dwChannel = 0;
	std::string strPwd;
	BOOL bRet = m_DBManager.GetAccountData(Req.accountname().c_str(), u64ID, strPwd, dwChannel);
	if (bRet)
	{
		Ack.set_retcode(MRC_ACCOUNT_EXIST);
		CLog::GetInstancePtr()->LogError("Error CAccountMsgHandler::OnMsgAccountRegReq RetCode:MRC_ACCOUNT_EXIST2");
		m_AccountManager.AddAccountObject(u64ID, Req.accountname(), strPwd, dwChannel);
		ServiceBase::GetInstancePtr()->SendMsgProtoBuf(pPacket->m_dwConnID, MSG_ACCOUNT_REG_ACK, 0, pHeader->dwUserData, Ack);
		return TRUE;
	}

	pAccount = m_AccountManager.CreateAccountObject(Req.accountname().c_str(), Req.password().c_str(), Req.channel());
	if(pAccount == NULL)
	{
		Ack.set_retcode(MRC_FAILED);
		CLog::GetInstancePtr()->LogError("Error CAccountMsgHandler::OnMsgAccountRegReq RetCode:MRC_FAILED");
		ServiceBase::GetInstancePtr()->SendMsgProtoBuf(pPacket->m_dwConnID, MSG_ACCOUNT_REG_ACK, 0, pHeader->dwUserData, Ack);
		return FALSE;
	}

	if(!m_DBManager.CreateAccount(pAccount->m_ID, Req.accountname().c_str(), Req.password().c_str(), pAccount->m_dwChannel, pAccount->m_dwCreateTime))
	{
		Ack.set_retcode(MRC_FAILED);
		CLog::GetInstancePtr()->LogError("Error CAccountMsgHandler::OnMsgAccountRegReq RetCode:MRC_FAILED2");
	}
	else
	{
		Ack.set_retcode(MRC_SUCCESSED);
	}

	Ack.set_accountid(pAccount->m_ID);

	ServiceBase::GetInstancePtr()->SendMsgProtoBuf(pPacket->m_dwConnID, MSG_ACCOUNT_REG_ACK, 0, pHeader->dwUserData, Ack);

	return TRUE;
}

BOOL CAccountMsgHandler::OnMsgAccontLoginReq(NetPacket* pPacket)
{
	AccountLoginReq Req;
	Req.ParsePartialFromArray(pPacket->m_pDataBuffer->GetData(), pPacket->m_pDataBuffer->GetBodyLenth());

	PacketHeader* pHeader = (PacketHeader*) pPacket->m_pDataBuffer->GetBuffer();
	ERROR_RETURN_TRUE(pHeader->dwUserData != 0);

	AccountLoginAck Ack;
	CAccountObject* pAccObj = m_AccountManager.GetAccountObjectByName(Req.accountname());
	if(pAccObj != NULL)
	{
		ERROR_RETURN_FALSE(pAccObj->m_ID != 0);
		if(Req.password() == pAccObj->m_strPassword)
		{
			Ack.set_lastsvrid(pAccObj->m_dwLastSvrID);
			Ack.set_accountid(pAccObj->m_ID);
			Ack.set_retcode(MRC_SUCCESSED);
			ServiceBase::GetInstancePtr()->SendMsgProtoBuf(pPacket->m_dwConnID, MSG_ACCOUNT_LOGIN_ACK, 0, pHeader->dwUserData, Ack);
			return TRUE;
		}
	}

	UINT64 u64AccountID = m_DBManager.VerifyAccount(Req.accountname().c_str(), Req.password().c_str());
	if(u64AccountID == 0)
	{
		Ack.set_retcode(MRC_FAILED);
		CLog::GetInstancePtr()->LogError("Error CAccountMsgHandler::OnMsgAccontLoginReq RetCode:MRC_FAILED");
	}
	else
	{
		m_AccountManager.AddAccountObject(u64AccountID, Req.accountname().c_str(), Req.password().c_str(), Req.channel());
		Ack.set_retcode(MRC_SUCCESSED);
	}

	Ack.set_lastsvrid(0);
	Ack.set_accountid(u64AccountID);
	ServiceBase::GetInstancePtr()->SendMsgProtoBuf(pPacket->m_dwConnID, MSG_ACCOUNT_LOGIN_ACK, 0, pHeader->dwUserData, Ack);
	return TRUE;
}
