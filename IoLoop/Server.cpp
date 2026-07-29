#include "Server.h"
#include "HttpConnection.h"
#include "IOSPool.h"

void Server::start()
{
	auto self = shared_from_this();
	auto& io_context = IOSPool::GetInstance()->GetIOService();
	auto new_connection = std::make_shared<HttpConnection>(io_context);
	acceptor_.async_accept(new_connection->GetSocket(), [self,new_connection](beast::error_code ec)
		{
			try
			{
				if (ec)
				{
					self->start();
					return;
				}
				//创建新连接
				std::cout << "socket accept" << std::endl;
				new_connection->Start();
				
				//继续监听
				self->start();
			}
			catch (std::exception& ec)
			{
			}
		});

}
