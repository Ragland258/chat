#include "HttpConnection.h"
#include <iostream>
#include "LogicSystem.h"
HttpConnection::HttpConnection(tcp::socket&& socket) :socket_(std::move(socket))
{
}

HttpConnection::HttpConnection(boost::asio::io_context& ioc):socket_(ioc)
{
}

static std::string BeastViewToString(boost::beast::string_view view)
{
	// Beast string_view does not own memory, so copy it before logging or concatenating.
	return std::string(view.data(), view.size());
}

static unsigned char ToHex(unsigned char x)
{
	return x > 9 ? x + 55 : x + 48;
}

static unsigned char FromHex(unsigned char x)
{
	unsigned char y;
	if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
	else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
	else if (x >= '0' && x <= '9') y = x - '0';
	else assert(0);
	return y;
}

static std::string UrlEncode(const std::string& str)//对url进行编码
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		if (isalnum((unsigned char)str[i]) ||
			(str[i] == '-') ||
			(str[i] == '_') ||
			(str[i] == '.') ||
			(str[i] == '~'))
			strTemp += str[i];
		else if (str[i] == ' ')
			strTemp += "+";
		else
		{
			strTemp += '%';
			strTemp += ToHex((unsigned char)str[i] >> 4);
			strTemp += ToHex((unsigned char)str[i] % 16);
		}
	}
	return strTemp;
}

static std::string UrlDecode(const std::string& str)//对url进行解码
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		if (str[i] == '+') strTemp += ' ';
		else if (str[i] == '%')
		{
			assert(i + 2 < length);
			unsigned char high = FromHex((unsigned char)str[++i]);
			unsigned char low = FromHex((unsigned char)str[++i]);
			strTemp += high * 16 + low;
		}
		else strTemp += str[i];
	}
	return strTemp;
}

std::string HttpConnection::GetFullUrl() const
{
	// HTTP request target only contains path and query, such as /get_test?a=1.
	std::string target = BeastViewToString(request_.target());

	std::string host;
	auto host_iter = request_.base().find(http::field::host);
	if (host_iter != request_.base().end())
	{
		// Host header carries domain or host:port, such as localhost:9999.
		host = BeastViewToString(request_[http::field::host]);
	}

	if (host.empty())
	{
		host = "unknown-host";
	}

	return "http://" + host + target;
}
void HttpConnection::Start()
{
	auto self = shared_from_this();
	http::async_read(socket_,buffer_,request_,
		[self](::boost::beast::error_code ec, ::std::size_t bytes_transferred)
		{
			try
			{
				if (ec)
				{
					std::cout << "read error:" << ec.message() << std::endl;
					return;
				}
				boost::ignore_unused(bytes_transferred);
				self->HandleRequest();
				self->CheckDeadline();

			}
			catch(std::exception& ec)
			{
				std::cout << "read error:" << ec.what() << std::endl;
			}
		});	
}

void HttpConnection::CheckDeadline()
{
	auto self = shared_from_this();

	deadline_.async_wait([self](boost::beast::error_code ec)
		{
			try
			{
				if (ec == boost::asio::error::operation_aborted)
				{
					return;
				}

				if (ec)
				{
					std::cout << "deadline error: " << ec.message() << std::endl;
					return;
				}

				self->socket_.close(ec);
			}
			catch (std::exception& e)
			{
				std::cout << "deadline error: " << e.what() << std::endl;
			}
		});
}

void HttpConnection::WriteResponse(bool responseStarted)
{
	auto self = shared_from_this();
	if (!responseStarted)
	{
		bool expected = false;
		if (!response_started_.compare_exchange_strong(expected, true))
		{
			std::cerr
				<< "[response] duplicate response ignored"
				<< std::endl;
			return;
		}
	}

	response_.content_length(response_.body().size());
	http::async_write(socket_,response_,
		[self](::boost::beast::error_code ec, ::std::size_t bytes_transferred)
		{
			try
			{
				if (ec)
				{
					std::cout << "write error:" << ec.message() << std::endl;
					return;
				}
				boost::ignore_unused(bytes_transferred);
				//关闭socket
				self->socket_.shutdown(tcp::socket::shutdown_send, ec);//关闭发送端
				self->deadline_.cancel();
			}
			catch (std::exception& ec)
			{
				std::cout << "write error:" << ec.what() << std::endl;
			}
		});
}

void HttpConnection::HandleRequest()
{
	auto self = shared_from_this();
	//设置版本
	response_.version(request_.version());
	//http短链接
	response_.keep_alive(false);

	// Print the raw target and reconstructed full URL before route dispatch.
	std::string raw_target = BeastViewToString(request_.target());
	std::string full_url = GetFullUrl();
	std::cout << "[request] method: " << request_.method_string()
		<< ", target: " << raw_target
		<< ", full url: " << full_url << std::endl;

	if (request_.method() == http::verb::get)
	{
		//预解析Get请求，提取参数
		PreParseGetRequest();
		std::cout << "[dispatch] GET url: " << get_url_
			<< ", full url: " << full_url << std::endl;
		//使用逻辑单例处理请求
		bool success = LogicSystem::GetInstance()->HandleGet(get_url_, self);//get_url_是预解析后的url，self是当前连接的shared_ptr
		if (!success)//如果处理失败，返回404
		{
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");//
			beast::ostream(response_.body()) << "The resource '" << raw_target << "' was not found.";//
			std::cout << "[404] GET dispatch url: " << get_url_
				<< ", raw target: " << raw_target
				<< ", full url: " << full_url << std::endl;
			WriteResponse();
			return;
		}
		//处理成功，返回200
		
		response_.result(http::status::ok);
		response_.set(http::field::server, "GateServer");
		WriteResponse();
		return;
	}
	else if(request_.method() == http::verb::post)
	{
		// POST routes are matched by the original request target.
		std::string post_url = raw_target;
		std::cout << "[dispatch] POST url: " << post_url
			<< ", full url: " << full_url << std::endl;
		//使用逻辑单例处理请求
		bool success = LogicSystem::GetInstance()->HandlePost(post_url, self);//target()返回请求的url，self是当前连接的shared_ptr
		if (!success)//如果处理失败，返回404
		{
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");//
			beast::ostream(response_.body()) << "The resource '" << post_url << "' was not found.";//
			std::cout << "[404] POST url: " << post_url
				<< ", full url: " << full_url << std::endl;
			WriteResponse();
			return;
		}
		//处理成功，返回200
		if (IsResponseDeferred())
		{
			return;
		}

		response_.result(http::status::ok);
		response_.set(http::field::server, "GateServer");
		WriteResponse();
		return;
	}
	else
	{
		// 不支持的请求方法返回405 Method Not Allowed
		response_.result(http::status::method_not_allowed);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "Only GET and POST methods are supported";
	}
	WriteResponse();
}
void HttpConnection::PreParseGetRequest()
{
	//提取请求的url
	auto url = request_.target();
	//先找到?位置
	auto pos = url.find('?');
	if(pos == std::string::npos)
	{
		get_url_ = std::string(url);
		return;
	}

	//赋值get_url_为?前的部分
	get_url_ = url.substr(0, pos);

	auto query = url.substr(pos + 1);
	std::istringstream query_stream(query);
	std::string key_value;
	while (std::getline(query_stream, key_value, '&'))
	{
		auto equal_pos = key_value.find('=');
		if (equal_pos != std::string::npos)
		{
			auto key = UrlDecode(key_value.substr(0, equal_pos));
			auto value = UrlDecode(key_value.substr(equal_pos + 1));
			get_params_[key] = value;
		}
	}
}



void HttpConnection::SendJsonResponse(
	http::status status,
	std::string jsonBody)
{
	/*
	 * 确保一个请求只发送一次响应。
	 */
	bool expected = false;

	if (!response_started_.compare_exchange_strong(
		expected,
		true))
	{
		std::cerr
			<< "[response] duplicate response ignored"
			<< std::endl;

		return;
	}

	auto self = shared_from_this();

	/*
	 * 无论此函数在哪个线程调用，
	 * 都将真正的响应操作投递回 socket executor。
	 */
	boost::asio::post(
		socket_.get_executor(),
		[
			self,
			status,
			jsonBody = std::move(jsonBody)
		]() mutable
		{
			self->response_ = {};

			self->response_.version(
				self->request_.version()
			);

			self->response_.keep_alive(false);

			self->response_.result(status);

			self->response_.set(
				http::field::server,
				"GateServer"
			);

			self->response_.set(
				http::field::content_type,
				"application/json; charset=utf-8"
			);

			beast::ostream(
				self->response_.body()
			) << jsonBody;

			self->response_.prepare_payload();

			self->WriteResponse(true);
		}
	);
}

void HttpConnection::DeferResponse()
{
	response_deferred_.store(true);
}

bool HttpConnection::IsResponseDeferred() const
{
	return response_deferred_.load();
}
