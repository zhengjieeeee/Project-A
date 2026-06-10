#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "asio.hpp"

using asio::ip::tcp;
using error_code = asio::error_code;

enum class ConnectionState : uint8_t
{
    UNCONNECTED,
    CONNECTING,
    CONNECTED,
    ERRORCONNECT
};

enum class TCPERROR : uint8_t
{
    CONNECT_ERROR,
    SEND_ERROR,
    RECV_ERROR,
    RECONNECT_ERROR
};

enum class CallBackType : uint8_t
{
    READ,
    CONNECTION,
    TCPERROR
};

struct CallBackPackage
{
    CallBackType type;
    uint8_t code;
    std::vector<char> data;
};

using TcpCallback = std::function<void(const CallBackPackage& package)>;

class TCPConnection : public std::enable_shared_from_this<TCPConnection>
{
public:
    TCPConnection(const std::string& ip, int port);
    ~TCPConnection();

    void registerTcpCallback(TcpCallback cb);

    bool connect();
    void close();
    void send(const std::string& data);
    void reConnect();

private:
    void doRead();
    void doWrite();

private:
    std::string _ip;
    int _port;

    asio::io_context _io_context;
    std::unique_ptr<tcp::socket> _socket;
    std::thread _io_thread;

    ConnectionState _state;
    bool _is_running;

    TcpCallback _callback;

    std::mutex _mutex;
    std::condition_variable _io_cv;
    std::queue<std::vector<char>> _send_queue;
};