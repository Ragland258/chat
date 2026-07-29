#include "VerifyGrpcClient.h"

VarifyGrpcClient::VarifyGrpcClient() {}

GetVarifyRsp VarifyGrpcClient::GetVerify(std::string email)
{
	ClientContext context;// Create a new client context for the RPC
	GetVarifyRsp reply;// Create a new GetVarifyRsp object to hold the response �ذ�
	GetVarifyReq request;// Create a new GetVarifyReq object to hold the request ����
	request.set_email(email);//���ó�Ա
	auto pool = RPCPool::GetInstance();
	auto stub = pool->Borrow();
	if (!stub) {
		reply.set_error(static_cast<int>(ErrorCode::RPC_Error));
		reply.set_email(email);
		return reply;
	}

	Status status = stub->GetVarifyCode(&context, request, &reply);// Call the GetVarifyCode RPC method
	if (status.ok())
	{
		pool->Return(std::move(stub));
		return reply;
	}
	else
	{
		std::cout << "[RPC] GetVarifyCode failed, code: " << status.error_code()
			<< ", message: " << status.error_message() << std::endl;
		reply.set_error(static_cast<int>(ErrorCode::RPC_Error));
		reply.set_email(email);
		pool->Return(std::move(stub));
		return reply;
	}
}



RPCPool::RPCPool():b_stop_(false)
{
	auto config = ConfigMgr::GetInstance();

	host_ = (*config)["VarifyServer"]["Host"];
	port_ = (*config)["VarifyServer"]["Port"];
	pool_size_ = atoi((*config)["VarifyServer"]["PoolSize"].c_str());
	std::cout << "[RPCPool] target: " << host_ << ":" << port_
		<< ", pool size: " << pool_size_ << std::endl;

	for (auto i = 0; i < pool_size_; i++)
	{
		std::shared_ptr<Channel> channel = grpc::CreateChannel(host_ + ":" + port_,
			grpc::InsecureChannelCredentials());
		connections_.push( VarifyService::NewStub(channel));
	}
}

std::unique_ptr<VarifyService::Stub> RPCPool::Borrow()
{
	std::unique_lock<std::mutex> lk(mutex_);
	cv_.wait(lk, [this]()
		{
			if (b_stop_)
				return true;
			return !connections_.empty();
		});

	if (b_stop_)
		return nullptr;
	auto stub = std::move(connections_.front());
	connections_.pop();
	return stub;
}

void RPCPool::Return(std::unique_ptr<VarifyService::Stub>&& stub)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (b_stop_)
		return;
	connections_.push(std::move(stub));
	cv_.notify_one();

}


void RPCPool::Stop()
{
	{
		std::lock_guard<std::mutex> lk(mutex_);
		if (b_stop_)
			return;
		else
			b_stop_ = true;
	}
	cv_.notify_all();
}

RPCPool::~RPCPool()
{

	Stop();
	while (!connections_.empty())
		connections_.pop();
}
