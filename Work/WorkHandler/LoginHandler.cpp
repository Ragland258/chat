#include "LoginHandler.h"
#include "IoLoop/HttpConnection.h"
#include "PasswordHasher.h"
void LoginHandler::Handler(std::shared_ptr<HttpConnection> connection)
{
	namespace http = boost::beast::http;
	std::weak_ptr<HttpConnection> weakConn(connection);
	auto body = connection->GetRequest();

	try
	{
        ThreadPool::GetInstance()->commit(
            [body = std::move(body), weakConn, this]()
            {
                //设置response的响应值
                http::status responseStatus =
                    http::status::ok;

                std::string responseJson;
                try
                {
					Json::Value src_root;
					Json::Reader reader;
					bool parse_success = reader.parse(body, src_root);
                    if (!parse_success
                        || !src_root.isObject())
                    {
                        responseJson =
                            this->BuildJsonResponse(
                                ErrorCode::Error_Json,
                                "invalid json"
                            );
                    }
                    else
                    {
						auto email = src_root["email"].asString(); 
						auto password = src_root["password"].asString();
						if (email.empty() || password.empty())// 数据丢失
                        {
                            responseJson =
                                this->BuildJsonResponse(
                                    ErrorCode::Error_Json,
                                    "email or password is empty"
                                );
                        }
                        else
                        {
                            // mysql查询
							auto pwdHash = MysqlMgr::GetInstance()->GetPasswordHash(email);

							if (pwdHash.empty()) // 用户不存在
                            {
                                responseJson =
                                    this->BuildJsonResponse(
                                        ErrorCode::User_Not_Exist,
                                        "user not found"
                                    );
                            }
                            else  
                            {
                                if (PasswordHasher::VerifyPassword(password, pwdHash)) // 密码正确
                                {
                                    // 创建token id
									// 调用redis创建token

                                    auto TTL = std::stoi((*ConfigMgr::GetInstance())["Redis"]["TTL"]);
                                    if(TTL <= 0)
                                    {
                                        TTL = 30 * 60; // 默认30分钟
									}
									auto token = RedisMgr::GetInstance()->CreateLoginToken(email, TTL);

                                    if(token.empty())
                                    {
                                        responseJson =
                                            this->BuildJsonResponse(
                                                ErrorCode::Redis_Error,
                                                "redis error"
                                            );
									}
									else // 生成token成功
                                    {
										// 调用grpc查找可用的服务器

                                        responseJson =
                                            this->BuildJsonResponse(
                                                ErrorCode::Success,
                                                "login success",
                                                token
                                            );
                                    }
                                }
                                else // 密码错误
                                {
                                    responseJson =
                                        this->BuildJsonResponse(
                                            ErrorCode::PassWord_Error,
                                            "password error"
                                        );
								}
                            }
                        }
                    }
                }
                catch (...)
                {
                    std::cerr
                        << "[register] unknown business exception"
                        << std::endl;

                    responseStatus =
                        http::status::internal_server_error;

                    responseJson =
                        this->BuildJsonResponse(
                            ErrorCode::Unknown_Error,
                            "internal server error"
                        );
                }

                auto lockedConnection =
                    weakConn.lock();

                if (!lockedConnection)
                {
                    /*
                     * 客户端已经断开，
                     * 不需要再发送响应。
                     */
                    return;
                }

                /*
                 * SendJsonResponse 内部会 post 回
                 * HttpConnection 的 Asio executor。
                 *
                 * 工作线程不直接修改 response_。
                 */
                lockedConnection->SendJsonResponse(
                    responseStatus,
                    std::move(responseJson)
                );
            }
        );
	}
    catch (const std::exception& exception)
    {
        /*
         * commit 本身可能因为：
         *
         * 1. 线程池正在停止；
         * 2. 后续实现有界队列且队列已满；
         *
         * 而抛出异常。
         */
        std::cerr
            << "[register] submit task failed: "
            << exception.what()
            << std::endl;

        connection->SendJsonResponse(
            http::status::service_unavailable,
            BuildJsonResponse(
                ErrorCode::Mysql_Pool_Timeout,
                "server busy"
            )
        );
    }

}
