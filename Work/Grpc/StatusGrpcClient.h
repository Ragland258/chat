#pragma once
#include "ConfigMgr.h"
#include "RpcPool.h"
#include "Singleton.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;
using message::StatusService;


class StatusGrpcClient :public Singleton<StatusGrpcClient>
{
	using Pool = RpcPool<StatusService>;
	friend class Singleton<StatusGrpcClient>;
public:
	~StatusGrpcClient() = default;
	GetChatServerRsp GetChatServer(int uid);


private:
	Pool pool_;
};

