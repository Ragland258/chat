#pragma once
#include <memory>

class HttpConnection;

/** 
* 请求处理基类,具体请求的方法实现部分
**/
class RequestHandler
{
public:
	virtual ~RequestHandler() = default;

	RequestHandler(
		const RequestHandler&) = delete;

	RequestHandler& operator=(
		const RequestHandler&) = delete;

	virtual void Handler(
		std::shared_ptr<HttpConnection> connection
	) = 0;
	
protected:
	RequestHandler() = default;
};

