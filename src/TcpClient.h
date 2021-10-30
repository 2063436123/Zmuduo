//
// Created by Hello Peter on 2021/10/22.
//

#ifndef TESTLINUX_TCPCLIENT_H
#define TESTLINUX_TCPCLIENT_H

#include <utility>
#include <sys/timerfd.h>

#include "InAddr.h"
#include "Codec.h"
#include "Logger.h"
#include "Socket.h"
#include "Buffer.h"
#include "Event.h"
#include "TcpConnection.h"
#include "TcpSource.h"
#include "EventLoopPool.h"

class TcpClient : public TcpSource {
public:
    // 一个实例对应一个客户
    TcpClient(Socket clientfd, EventLoop *loop, Codec codec) : clientfd_(std::move(clientfd)), loop_(loop), codec_(std::move(codec)) {
        loop_->init();
    }

    void setConnMsgCallback(std::function<void(TcpConnection *)> callback) {
        connMsgCallback_ = callback;
    }

    void setConnCloseCallback(std::function<void(TcpConnection *)> callback) {
        connCloseCallback_ = callback;
    }

    void setConnErrorCallback(std::function<void(TcpConnection*)> callback) {
        connErrorCallback_ = callback;
    }

    TcpConnection* connect(InAddr addr) {
        // todo 暂时用阻塞式connect连接
        // clientfd.setNonblock();
        clientfd_.connect(addr);

        auto event = Event::make(clientfd_.fd(), loop_->epoller());
        auto conn = TcpConnection::makeHeapObject(clientfd_.fd(), this, loop_);
        auto preConnMsgCallback = [conn, this]() {
            ssize_t n = conn->readBuffer().readFromSocket(conn->socket());
            if (n == 0) {
                // 对端关闭
                closeConnection(conn, nullptr);
                return;
            } else if (n < 0) {
                if (connErrorCallback_)
                    connErrorCallback_(conn);
                Logger::sys("read error");
                closeConnection(conn, nullptr);
                return;
            }
            // todo: 添加codec处理
            if (codec_.check(conn))
                connMsgCallback_(conn);
        };
        event->setReadCallback(preConnMsgCallback);
        event->setReadable(true);

        backThread_ = std::thread(&EventLoop::start, loop_);
        return conn;
    }

    // 关闭连接，在 read返回0 或 客户调用activeClose 时被调用
    void closeConnection(TcpConnection *connection, std::function<void(TcpConnection*)> destoryCallback) override {
        assert(destoryCallback == nullptr);
        if (connection->writeBuffer().readableBytes() > 0) {
            auto event = connection->eventLoop()->epoller()->getEvent(connection->socket().fd());
            event->setReadable(false);
            connection->send(true);
            return;
        }
        // 销毁前先调用CloseCallback
        if (connCloseCallback_)
            connCloseCallback_(connection);
        connection->destroy();
        // TcpConnection析构时调用Socket成员的析构函数，其中会close(sockfd)完成连接的关闭
        delete connection;

        // FIXME: 假设唯一的一条连接断开时自动终止eventLoop
        loop_->stop();
        return;
    }

    void addTimedTask(uint32_t nextOccurTime, uint32_t intervalTime, std::function<void()> task) {
        timespec nxtTime{.tv_sec = nextOccurTime / 1000, .tv_nsec = (nextOccurTime % 1000) * 1000000};
        timespec interTime{.tv_sec = intervalTime / 1000, .tv_nsec = (intervalTime % 1000) * 1000000};
        struct itimerspec spec{.it_interval = interTime, .it_value = nxtTime};
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd_settime(timerfd, 0, &spec, nullptr) < 0)
            Logger::sys("timerfd_settime error");

        auto timerEvent = Event::make(timerfd, loop_->epoller());
        auto readCallback = [timerfd = timerfd, task]() {
            uint64_t tmp;
            read(timerfd, &tmp, sizeof(tmp));
            task();
        };
        timerEvent->setReadCallback(readCallback);
        timerEvent->setReadable(true);
    }

    void join() {
        backThread_.join();
    }

    ~TcpClient() {
        if (backThread_.joinable())
            backThread_.join();
    }

private:
    Socket clientfd_;
    EventLoop *loop_;
    std::thread backThread_; // 副线程运行EventLoop
    Codec codec_;
    std::function<void(TcpConnection *)> connMsgCallback_, connCloseCallback_, connErrorCallback_;
};

#endif //TESTLINUX_TCPCLIENT_H
