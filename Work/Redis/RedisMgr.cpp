#include "RedisMgr.h"
#include "ConfigMgr.h"

RedisMgr::RedisMgr()
{
}

RedisMgr::~RedisMgr()
{
    Close();
}

void RedisMgr::Close()
{
    RedisPool::GetInstance()->Close();
}

VerifyCodeResult RedisMgr::ConsumeVerifyCode(
    const std::string& key,
    const std::string& inputCode
)
{
    static const std::string luaScript = R"lua(
local storedCode = redis.call("GET", KEYS[1])

if not storedCode then
    return 0
end

if storedCode ~= ARGV[1] then
    return -1
end

redis.call("DEL", KEYS[1])

return 1
)lua";

    RedisConGuard guard(
        RedisPool::GetInstance()->BorrowConnect()
    );

    auto* connection = guard.get();

    if (!connection)
    {
        std::cerr
            << "[ConsumeVerifyCode] no available Redis connection"
            << std::endl;

        return VerifyCodeResult::RedisError;
    }

    RedisReplyMgr reply;

    /*  EVAL
    *   Lua脚本
    *   1个Redis key
    *   key
    *    用户输入的验证码
    */
    reply = reinterpret_cast<redisReply*>(
        redisCommand(
            connection,
            "EVAL %b 1 %b %b",

            luaScript.data(),
            static_cast<size_t>(luaScript.size()),

            key.data(),
            static_cast<size_t>(key.size()),

            inputCode.data(),
            static_cast<size_t>(inputCode.size())
        )
        );

    if (!reply)
    {
        std::cerr
            << "[ConsumeVerifyCode] Redis returned no reply"
            << std::endl;

        return VerifyCodeResult::RedisError;
    }

    if (reply->type == REDIS_REPLY_ERROR)
    {
        std::cerr
            << "[ConsumeVerifyCode] Lua error: "
            << (reply->str ? reply->str : "unknown error")
            << std::endl;

        return VerifyCodeResult::RedisError;
    }

    if (reply->type != REDIS_REPLY_INTEGER)
    {
        std::cerr
            << "[ConsumeVerifyCode] unexpected reply type: "
            << reply->type
            << std::endl;

        return VerifyCodeResult::RedisError;
    }

    switch (reply->integer)
    {
    case 1:
        return VerifyCodeResult::Success;

    case 0:
        return VerifyCodeResult::CodeMissing;

    case -1:
        return VerifyCodeResult::CodeMismatch;

    default:
        return VerifyCodeResult::RedisError;
    }
}

bool RedisMgr::Connect(const std::string& host, int port)
{
    auto* context = RedisPool::GetInstance()->BorrowConnect();
    if (!context)
    {
        std::cout << "RedisPool is empty, connect failed" << std::endl;
        return false;
    }
    RedisPool::GetInstance()->ReturnConnect(context);
    return true;
}

bool RedisMgr::Get(const std::string& key, std::string& value)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "[Get " << key << "] failed, no available connection" << std::endl;
        return false;
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "GET %s", key.c_str());
    if (!reply)
    {
        std::cout << "[Get " << key << "] failed" << std::endl;
        return false;
    }
    if (reply->type != REDIS_REPLY_STRING)
    {
        std::cout << "[Get " << key << "] failed, type is not string" << std::endl;
        return false;
    }
    value = reply->str;
    return true;
}

bool RedisMgr::Set(const std::string& key, const std::string& value)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "[Set " << key << " " << value << "] failed, no available connection" << std::endl;
        return false;
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "SET %s %s", key.c_str(), value.c_str());
    if (!reply)
    {
        std::cout << "Execut command [ SET " << key << " " << value << " ] failure " << std::endl;
        return false;
    }
    if (!(reply->type == REDIS_REPLY_STATUS &&
          (std::strcmp(reply->str, "OK") == 0 || std::strcmp(reply->str, "ok") == 0)))
    {
        std::cout << "Execut command [ SET " << key << " " << value << " ] failure " << std::endl;
        return false;
    }
    std::cout << "Execut command [ SET " << key << "  " << value << " ] success ! " << std::endl;
    return true;
}

bool RedisMgr::Auth(const std::string& password)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "Auth failed, no available connection" << std::endl;
        return false;
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "AUTH %s", password.c_str());
    if (!reply)
    {
        std::cout << "Auth failed" << std::endl;
        return false;
    }
    if (reply->type != REDIS_REPLY_STATUS || std::strcmp(reply->str, "OK") != 0)
    {
        std::cout << "Auth failed: " << (reply->str ? reply->str : "") << std::endl;
        return false;
    }
    std::cout << "Auth success" << std::endl;
    return true;
}

bool RedisMgr::LPush(const std::string& key, const std::string& value)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "[LPush " << key << " " << value << "] failed, no available connection" << std::endl;
        return false;
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "LPUSH %s %s", key.c_str(), value.c_str());
    if (!reply || reply->type != REDIS_REPLY_INTEGER)
    {
        std::cout << "Execut command [ LPUSH " << key << " " << value << " ] failure " << std::endl;
        return false;
    }
    std::cout << "Execut command [ LPUSH " << key << " " << value << " ] success ! " << std::endl;
    return true;
}

bool RedisMgr::LPop(const std::string& key, std::string& value)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "[LPop " << key << "] failed, no available connection" << std::endl;
        return false;
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "LPOP %s", key.c_str());
    if (!reply || reply->type != REDIS_REPLY_STRING)
    {
        std::cout << "Execut command [ LPOP " << key << " ] failure " << std::endl;
        return false;
    }
    value = reply->str;
    return true;
}

bool RedisMgr::RPush(const std::string& key, const std::string& value)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "[RPush " << key << " " << value << "] failed, no available connection" << std::endl;
        return false;
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "RPUSH %s %s", key.c_str(), value.c_str());
    if (!reply || reply->type != REDIS_REPLY_INTEGER)
    {
        std::cout << "Execut command [ RPUSH " << key << " " << value << " ] failure " << std::endl;
        return false;
    }
    std::cout << "Execut command [ RPUSH " << key << " " << value << " ] success ! " << std::endl;
    return true;
}

bool RedisMgr::RPop(const std::string& key, std::string& value)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "[RPop " << key << "] failed, no available connection" << std::endl;
        return false;
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "RPOP %s", key.c_str());
    if (!reply || reply->type != REDIS_REPLY_STRING)
    {
        std::cout << "Execut command [ RPOP " << key << " ] failure " << std::endl;
        return false;
    }
    value = reply->str;
    return true;
}

bool RedisMgr::HSet(const std::string& key, const std::string& hkey, const std::string& value)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "[HSet " << key << " " << hkey << " " << value << "] failed, no available connection" << std::endl;
        return false;
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "HSET %s %s %s",
                                      key.c_str(), hkey.c_str(), value.c_str());
    if (!reply || reply->type != REDIS_REPLY_INTEGER)
    {
        std::cout << "Execut command [ HSET " << key << " " << hkey << " " << value << " ] failure " << std::endl;
        return false;
    }
    std::cout << "Execut command [ HSET " << key << " " << hkey << " " << value << " ] success ! " << std::endl;
    return true;
}

bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "[HSet binary] failed, no available connection" << std::endl;
        return false;
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "HSET %s %s %b",
                                      key, hkey, hvalue, hvaluelen);
    if (!reply || reply->type != REDIS_REPLY_INTEGER)
    {
        std::cout << "Execut command [ HSET binary ] failure " << std::endl;
        return false;
    }
    std::cout << "Execut command [ HSET binary ] success ! " << std::endl;
    return true;
}

std::string RedisMgr::HGet(const std::string& key, const std::string& hkey)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "[HGet " << key << " " << hkey << "] failed, no available connection" << std::endl;
        return "";
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "HGET %s %s", key.c_str(), hkey.c_str());
    if (!reply || reply->type != REDIS_REPLY_STRING)
    {
        std::cout << "Execut command [ HGET " << key << " " << hkey << " ] failure " << std::endl;
        return "";
    }
    return reply->str;
}

bool RedisMgr::Del(const std::string& key)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "[Del " << key << "] failed, no available connection" << std::endl;
        return false;
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "DEL %s", key.c_str());
    if (!reply || reply->type != REDIS_REPLY_INTEGER)
    {
        std::cout << "Execut command [ DEL " << key << " ] failure " << std::endl;
        return false;
    }
    std::cout << "Execut command [ DEL " << key << " ] success ! " << std::endl;
    return true;
}

bool RedisMgr::ExistsKey(const std::string& key)
{
    RedisConGuard guard(RedisPool::GetInstance()->BorrowConnect());
    auto* connect_ = guard.get();
    if (!connect_)
    {
        std::cout << "[ExistsKey " << key << "] failed, no available connection" << std::endl;
        return false;
    }

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(connect_, "EXISTS %s", key.c_str());
    if (!reply || reply->type != REDIS_REPLY_INTEGER)
    {
        std::cout << "Execut command [ EXISTS " << key << " ] failure " << std::endl;
        return false;
    }
    return reply->integer != 0;
}

RedisPool::~RedisPool()
{
    Close();
    std::lock_guard<std::mutex> lock(mutex_);
    while (!connections_.empty())
    {
        redisFree(connections_.front());
        connections_.pop();
    }
}

redisContext* RedisPool::BorrowConnect()
{
    while (true)
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

        auto* context = connections_.front();
        connections_.pop();
        //std::cout << "[RedisPool] borrow, remaining: " << connections_.size() << std::endl;
        lk.unlock();

        if (CheckConnection(context))
            return context;

        //std::cout << "[RedisPool] borrowed connection is not alive, reconnect" << std::endl;
        redisFree(context);
        auto* new_context = CreateConnection();
        if (new_context)
            return new_context;

        std::lock_guard<std::mutex> lock(mutex_);
        cv_.notify_one();
    }
}

void RedisPool::ReturnConnect(redisContext* context)
{
    if (!context)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (b_stop_)
    {
        redisFree(context);
        return;
    }

    if (!CheckConnection(context))
    {
        std::cout << "[RedisPool] return bad connection, reconnect" << std::endl;
        redisFree(context);
        auto* new_context = CreateConnection();
        if (new_context)
            connections_.push(new_context);
        cv_.notify_one();
        return;
    }

    connections_.push(context);
    //std::cout << "[RedisPool] return, remaining: " << connections_.size() << std::endl;
    cv_.notify_one();
}

void RedisPool::Close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    b_stop_ = true;
    std::cout << "[RedisPool] closing, destroy " << connections_.size() << " connections" << std::endl;
    while (!connections_.empty())
    {
        redisFree(connections_.front());
        connections_.pop();
    }
    cv_.notify_all();
}

RedisPool::RedisPool()
    : b_stop_(false), pool_size_(0)
{
    auto config = ConfigMgr::GetInstance();

    host_ = (*config)["RedisServer"]["Host"];
    port_ = (*config)["RedisServer"]["Port"];
    pool_size_ = atoi((*config)["RedisServer"]["size"].c_str());
    password_ = (*config)["RedisServer"]["password"];
    std::cout << "[RedisPool] config: host=" << host_ << ", port=" << port_
              << ", pwd_len=" << password_.length() << std::endl;

    for (int i = 0; i < pool_size_; ++i)
    {
        auto* context = CreateConnection();
        if (context)
        {
            connections_.push(context);
            //std::cout << "[RedisPool] connection " << connections_.size() << "/" << pool_size_ << " created" << std::endl;
        }
    }
    std::cout << "[RedisPool] init " << connections_.size() << "/" << pool_size_ << " connections" << std::endl;
}

redisContext* RedisPool::CreateConnection()
{
    auto* context = redisConnect(host_.c_str(), atoi(port_.c_str()));
    if (context == nullptr || context->err != 0)
    {
        std::cout << "[RedisPool] connect failed: "
                  << (context ? context->errstr : "null context") << std::endl;
        if (context)
            redisFree(context);
        return nullptr;
    }

    if (!password_.empty())
    {
        RedisReplyMgr reply;
        reply = (redisReply*)redisCommand(context, "AUTH %s", password_.c_str());
        if (!reply || reply->type == REDIS_REPLY_ERROR)
        {
            std::cout << "[RedisPool] auth failed: "
                      << (reply ? reply->str : "no reply") << std::endl;
            redisFree(context);
            return nullptr;
        }
    }

    return context;
}

bool RedisPool::CheckConnection(redisContext* context)
{
    if (!context || context->err != 0)
        return false;

    RedisReplyMgr reply;
    reply = (redisReply*)redisCommand(context, "PING");
    if (!reply)
        return false;

    return reply->type == REDIS_REPLY_STATUS &&
           reply->str &&
           std::strcmp(reply->str, "PONG") == 0;
}
