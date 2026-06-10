#pragma once

#include <string>
#include <functional>
#include <memory>

#include "asio.hpp"

struct HttpResult
{
    bool success = false;
    int code = 0;
    std::string body;
    std::string error;
};

using HttpCallback = std::function<void(const HttpResult&)>;

class HttpClient
{
public:
    HttpClient(const std::string& ip, int port);
    ~HttpClient();

    void get(const std::string& url, HttpCallback cb);
    void post(const std::string& url, const std::string& body, HttpCallback cb);

private:
    void sendRequest(const std::string& method,
        const std::string& url,
        const std::string& body,
        HttpCallback cb);

    void onRead(std::shared_ptr<std::string> recv_buffer,
        asio::ip::tcp::socket& socket,
        HttpCallback cb,
        const std::error_code& ec,
        std::size_t bytes);

    void onConnected(const std::string& method,
        const std::string& url,
        const std::string& body,
        std::shared_ptr<asio::ip::tcp::socket> socket,
        std::shared_ptr<std::string> recv_buffer,
        HttpCallback cb,
        const std::error_code& ec);

private:
    std::string _ip;
    int _port;

    asio::io_context _io_ctx;
    std::thread _io_thread;
};