#pragma once
#include "const.h"
#include <grpcpp/grpcpp.h>
#include "generated/message.grpc.pb.h"
#include "Singleton.h"
#include "ConfigMgr.h"


using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::VarifyService;
using message::GetVarifyReq;
using message::GetVarifyRsp;

/*
 * @brief RPC连接池
 * @details 用于管理RPC连接,包括创建、回收、获取连接等操作
*/
class RPCPool:public Singleton<RPCPool>
{
	friend class Singleton<RPCPool>;
public:
	~RPCPool();
	RPCPool(const RPCPool&) = delete;
	RPCPool& operator = (const RPCPool&) = delete;

	/*
	 * @brief 停止RPC连接池
	 * @details 停止连接池,将所有连接放回连接池,等待被其他线程获取
	 */
	void Stop();

	/*
		 * @brief 获取RPC连接
		 * @details 从连接池中获取一个RPC连接,如果连接池为空,则等待连接池有连接
		 * @return std::unique_ptr<VarifyService::Stub> RPC连接
		 */
	std::unique_ptr<VarifyService::Stub> Borrow();

	/*
	 * @brief 回收RPC连接
	 * @details 将连接池中的连接放回连接池,等待被其他线程获取
	 * @param stub RPC连接
	 */
	void Return(std::unique_ptr<VarifyService::Stub>&& stub);
		
private:
	RPCPool();
		
private:
	std::atomic<bool> b_stop_;//是否回收
	std::size_t pool_size_;
	std::string host_;
	std::string port_;
	std::queue<std::unique_ptr<VarifyService::Stub>> connections_;//使用互斥锁的队列,性能不高,后续修改
	std::mutex mutex_;
	std::condition_variable cv_;

};


class VarifyGrpcClient :public Singleton<VarifyGrpcClient>
{
	friend class Singleton<VarifyGrpcClient>;

public:
	GetVarifyRsp GetVerify(std::string email);
	~VarifyGrpcClient(){}
	VarifyGrpcClient(const VarifyGrpcClient&) = delete;
	VarifyGrpcClient& operator=(const VarifyGrpcClient&) = delete;
private:
	VarifyGrpcClient();
	
};

