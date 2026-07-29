#include "LogicSystem.h"

#include "HttpConnection.h"
#include "RegisterHandler.h"
#include "RequestHandler.h"
#include "VerifyGrpcClient.h"

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/http.hpp>
#include <json/json.h>

#include <iostream>
#include <utility>

namespace beast = boost::beast;
namespace http = boost::beast::http;

LogicSystem::~LogicSystem() = default;

void LogicSystem::RegisterGet(
    const std::string& url,
    ConnectionCallback callback)
{
    get_handlers_.insert_or_assign(
        url,
        std::move(callback));
}

bool LogicSystem::HandleGet(
    const std::string& url,
    std::shared_ptr<HttpConnection> connection)
{
    const auto iter = get_handlers_.find(url);

    if (iter == get_handlers_.end())
    {
        return false;
    }

    iter->second(std::move(connection));
    return true;
}

void LogicSystem::RegisterPost(
    const std::string& url,
    ConnectionCallback callback)
{
    post_handlers_.insert_or_assign(
        url,
        std::move(callback));
}

void LogicSystem::RegisterPostHandler(
    const std::string& url,
    std::unique_ptr<RequestHandler> handler)
{
    if (!handler)
    {
        return;
    }

    post_request_handlers_.insert_or_assign(
        url,
        std::move(handler));
}

bool LogicSystem::HandlePost(
    const std::string& url,
    std::shared_ptr<HttpConnection> connection)
{
    // 新的类式 Handler 优先。
    const auto handlerIter =
        post_request_handlers_.find(url);

    if (handlerIter != post_request_handlers_.end())
    {
        handlerIter->second->Handler(
            std::move(connection));

        return true;
    }

    // 暂时兼容旧的 Lambda 回调路由。
    const auto callbackIter = post_handlers_.find(url);

    if (callbackIter == post_handlers_.end())
    {
        return false;
    }

    callbackIter->second(std::move(connection));
    return true;
}

LogicSystem::LogicSystem()
{
    RegisterGet(
        "/get_test",
        [](std::shared_ptr<HttpConnection> connection)
        {
            beast::ostream(connection->response_.body())
                << "recv get_test request";

            std::cout
                << "recv get_test request"
                << std::endl;

            int index = 0;

            for (const auto& element : connection->get_params_)
            {
                ++index;

                beast::ostream(connection->response_.body())
                    << "param "
                    << index
                    << ": "
                    << element.first
                    << " = "
                    << element.second
                    << std::endl;
            }
        });

    RegisterPost(
        "/get_verify",
        [](std::shared_ptr<HttpConnection> connection)
        {
            const std::string body =
                boost::beast::buffers_to_string(
                    connection->request_.body().data());

            connection->response_.body().clear();
            connection->response_.set(
                http::field::content_type,
                "application/json; charset=utf-8");

            Json::Value requestJson;
            Json::Reader reader;

            if (!reader.parse(body, requestJson)
                || !requestJson.isObject())
            {
                Json::Value errorJson;
                errorJson["error"] =
                    static_cast<int>(ErrorCode::Error_Json);
                errorJson["msg"] = "invalid json";

                beast::ostream(connection->response_.body())
                    << errorJson.toStyledString();

                return;
            }

            if (!requestJson.isMember("email")
                || requestJson["email"].asString().empty())
            {
                Json::Value errorJson;
                errorJson["error"] =
                    static_cast<int>(ErrorCode::Error_Json);
                errorJson["msg"] = "lack email field";

                beast::ostream(connection->response_.body())
                    << errorJson.toStyledString();

                return;
            }

            const std::string email =
                requestJson["email"].asString();

            const GetVarifyRsp verifyResponse =
                VarifyGrpcClient::GetInstance()
                ->GetVerify(email);

            Json::Value responseJson;
            responseJson["code"] = verifyResponse.error();
            responseJson["email"] =
                verifyResponse.email().empty()
                ? email
                : verifyResponse.email();
            responseJson["msg"] =
                verifyResponse.error()
                == static_cast<int>(ErrorCode::Success)
                ? "success"
                : "send email failed";

            beast::ostream(connection->response_.body())
                << responseJson.toStyledString();
        });

    // POST /register 交给 RegisterHandler 处理。
    RegisterPostHandler(
        "/register",
        std::make_unique<RegisterHandler>());
}
