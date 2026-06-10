#include "HttpClient.h"
#include <sstream>

// 已经去掉 using namespace std;

HttpClient::HttpClient(const std::string& ip, int port)
    : _ip(ip), _port(port)
{
}

void HttpClient::get(const std::string& url, HttpCallback cb)
{
    sendRequest("GET", url, "", std::move(cb));
}

void HttpClient::post(const std::string& url, const std::string& body, HttpCallback cb)
{
    sendRequest("POST", url, body, std::move(cb));
}

void HttpClient::sendRequest(const std::string& method, 
                             const std::string& url, 
                             const std::string& body, 
                             HttpCallback cb)
{
    auto tcp = std::make_shared<TCPConnection>(_ip, _port);
    auto recv_buffer = std::make_shared<std::string>();

    tcp->registerReadCallback([this, recv_buffer](const auto& data) {
        onRead(data, recv_buffer);
    });

    tcp->registerConnectCallback([this, method, url, body, tcp, recv_buffer, cb](ConnectionState state) {
        onConnect(state, method, url, body, tcp, recv_buffer, cb);
    });

    tcp->registerErrorCallback([this, cb](TCPERROR err) {
        onError(err, cb);
    });

    tcp->connect();
}

void HttpClient::onRead(const std::shared_ptr<std::vector<char>>& data, std::shared_ptr<std::string> recv_buffer)
{
    recv_buffer->append(data->begin(), data->end());
}

void HttpClient::onConnect(ConnectionState state,
                           const std::string& method,
                           const std::string& url,
                           const std::string& body,
                           std::shared_ptr<TCPConnection> tcp,
                           std::shared_ptr<std::string> recv_buffer,
                           HttpCallback cb)
{
    if (state == ConnectionState::CONNECTED)
    {
        std::stringstream req;
        req << method << " " << url << " HTTP/1.1\r\n";
        req << "Host: " << _ip << "\r\n";
        req << "Connection: close\r\n";

        if (method == "POST")
        {
            req << "Content-Length: " << body.size() << "\r\n";
            req << "Content-Type: application/x-www-form-urlencoded\r\n";
        }

        req << "\r\n";
        if (!body.empty())
        {
            req << body;
        }

        tcp->send(req.str());
    }
    else if (state == ConnectionState::UNCONNECTED)
    {
        HttpResult res;

        std::size_t pos = recv_buffer->find("\r\n\r\n");
        if (pos == std::string::npos)
        {
            res = { false, 0, "", "recv_error" };
        }
        else
        {
            std::string header = recv_buffer->substr(0, pos);
            std::string resp_body = recv_buffer->substr(pos + 4);

            std::istringstream iss(header);
            std::string line;
            std::getline(iss, line);

            std::string ver, code_str;
            std::istringstream line_iss(line);
            line_iss >> ver >> code_str;

            res.code = std::stoi(code_str);
            res.success = (res.code >= 200 && res.code < 300);
            res.body = resp_body;
        }

        if (cb) cb(res);
    }
}

void HttpClient::onError(TCPERROR err, HttpCallback cb)
{
    if (cb) cb({ false, 0, "", "network_error" });
}