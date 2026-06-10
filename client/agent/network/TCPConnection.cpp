#include "TCPConnection.h"
#include <sstream>
#include <chrono>

using namespace asio;

TCPConnection::TCPConnection(const std::string& ip, int port)
    : _ip(ip), _port(port),
      _socket(std::make_unique<tcp::socket>(_io_context)),
      _state(ConnectionState::UNCONNECTED),
      _is_running(false)
{
}

TCPConnection::~TCPConnection()
{
    close();
}

void TCPConnection::registerTcpCallback(TcpCallback cb)
{
    _callback = std::move(cb);
}

bool TCPConnection::connect()
{
    if (_state == ConnectionState::CONNECTED)
        return true;

    _state = ConnectionState::CONNECTING;

    tcp::resolver resolver(_io_context);
    auto ep = resolver.resolve(_ip, std::to_string(_port));

    auto self = shared_from_this();
    async_connect(*_socket, ep, [this, self](const error_code& ec, const tcp::endpoint&) {
        if (!ec)
        {
            _state = ConnectionState::CONNECTED;
            _is_running = true;

            if (_callback) {
                GlobalThreadPool::getInstance().enqueue_serial([this]() {
                    CallBackPackage pkg{};
                    pkg.type = CallBackType::CONNECTION;
                    pkg.code = static_cast<uint8_t>(_state);
                    _callback(pkg);
                });
            }

            doRead();
        }
        else
        {
            _state = ConnectionState::ERRORCONNECT;

            if (_callback) {
                GlobalThreadPool::getInstance().enqueue_serial([this]() {
                    CallBackPackage pkg{};
                    pkg.type = CallBackType::TCPERROR;
                    pkg.code = static_cast<uint8_t>(TCPERROR::CONNECT_ERROR);
                    _callback(pkg);
                });
            }
            if (_callback) {
                GlobalThreadPool::getInstance().enqueue_serial([this]() {
                    CallBackPackage pkg{};
                    pkg.type = CallBackType::CONNECTION;
                    pkg.code = static_cast<uint8_t>(_state);
                    _callback(pkg);
                });
            }

            reConnect();
        }
    });

    if (!_io_thread.joinable())
    {
        _io_thread = std::thread([this]() {
            _io_context.run();
        });
    }

    return true;
}

void TCPConnection::reConnect()
{
    if (_state != ConnectionState::UNCONNECTED && _state != ConnectionState::ERRORCONNECT)
        return;

    std::thread([this, self = shared_from_this()]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        connect();
    }).detach();
}

void TCPConnection::close()
{
    _is_running = false;
    _io_context.stop();

    if (_socket && _socket->is_open())
    {
        error_code ec;
        _socket->close(ec);
    }

    if (_io_thread.joinable())
    {
        _io_thread.join();
    }

    _state = ConnectionState::UNCONNECTED;
}

void TCPConnection::send(const std::string& data)
{
    if (!_socket->is_open() || !_is_running)
        return;

    std::lock_guard<std::mutex> lock(_mutex);
    _send_queue.push(std::vector<char>(data.begin(), data.end()));
    if (_send_queue.size() == 1)
        doWrite();
}

void TCPConnection::doWrite()
{
    if (_send_queue.empty())
        return;

    auto self = shared_from_this();
    auto& buf = _send_queue.front();

    async_write(*_socket, buffer(buf), [this, self](error_code ec, size_t) {
        if (ec || !_is_running)
        {
            if (_callback) {
                GlobalThreadPool::getInstance().enqueue_serial([this]() {
                    CallBackPackage pkg{};
                    pkg.type = CallBackType::TCPERROR;
                    pkg.code = static_cast<uint8_t>(TCPERROR::SEND_ERROR);
                    _callback(pkg);
                });
            }
            close();
            return;
        }

        std::lock_guard<std::mutex> lock(_mutex);
        _send_queue.pop();
        if (!_send_queue.empty())
            doWrite();
    });
}

void TCPConnection::doRead()
{
    auto buf = std::make_shared<std::vector<char>>(4096);
    auto self = shared_from_this();

    _socket->async_read_some(buffer(*buf), [this, self, buf](error_code ec, size_t len) {
        if (ec || !_is_running)
        {
            if (_callback) {
                GlobalThreadPool::getInstance().enqueue_serial([this]() {
                    CallBackPackage pkg{};
                    pkg.type = CallBackType::TCPERROR;
                    pkg.code = static_cast<uint8_t>(TCPERROR::RECV_ERROR);
                    _callback(pkg);
                });
            }
            close();
            return;
        }

        buf->resize(len);
        if (_callback) {
            GlobalThreadPool::getInstance().enqueue_serial([this, buf]() {
                CallBackPackage pkg{};
                pkg.type = CallBackType::READ;
                pkg.data = *buf;
                _callback(pkg);
            });
        }

        doRead();
    });
}