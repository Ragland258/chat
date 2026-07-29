#pragma once
#include <memory>
#include "const.h"

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

    std::string BuildJsonResponse(
        ErrorCode error,
        const std::string& message,
        const std::string& email = " ",
        const std::string& name = " "
    )
    {
        Json::Value root;
        root["error"] = static_cast<int>(error);
        root["message"] = message;
        if (!email.empty())
            root["email"] = email;
        if (!name.empty())
            root["name"] = name;

        //让字节变得紧凑
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";

        return Json::writeString(writer, root);
    }
};

