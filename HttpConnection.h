#pragma once
#include "const.h"

#include <atomic>
class LogicSystem;

class HttpConnection :public std::enable_shared_from_this<HttpConnection>
{
	friend class LogicSystem;
public:
	HttpConnection(tcp::socket&& socket);
	HttpConnection(boost::asio::io_context& ioc);
	void Start();
	tcp::socket& GetSocket() { return socket_;}

	http::response<http::dynamic_body> GetResponse() { return response_; }
	std::string GetRequest() { return boost::beast::buffers_to_string(request_.body().data()); }

	// 工作队列发送response接口
	void SendJsonResponse(
		http::status status,
		std::string jsonBody);

	void DeferResponse();
	bool IsResponseDeferred() const;
private:
	void CheckDeadline();
	void WriteResponse(bool responseStarted = false);
	void HandleRequest();
	void PreParseGetRequest();//预解析Get请求，提取参数
	std::string GetFullUrl() const;//拼接Host请求头和target，用于调试时输出完整URL
private:
	tcp::socket socket_;
	beast::flat_buffer buffer_;//缓冲区
	http::request<http::dynamic_body> request_;//接受动态请求
	http::response<http::dynamic_body> response_;//响应动态请求
	std::string get_url_;//存储Get请求的url
	std::unordered_map<std::string, std::string> get_params_;//存储Get请求的参数

	//定时器 
	boost::asio::steady_timer deadline_{socket_.get_executor(),std::chrono::seconds(30)};

	std::atomic_bool response_started_{ false };
	std::atomic_bool response_deferred_{ false };

};


