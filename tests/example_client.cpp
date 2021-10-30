//
// Created by Hello Peter on 2021/10/22.
//

#include "../src/TcpClient.h"

void whenMsgArrived(TcpConnection *connection) {
    auto buf = connection->readBuffer();
    Logger::info("new msg: {}\n", std::string(buf.peek(), buf.readableBytes()));
    std::string msg;
    Logger::info("send msg? ");
    // todo 如何实现连接关闭时通知用户不要阻塞在getline上？

    connection->socket().resetClose();
    return;
    // BUG: 如果在等待用户输入的过程中，此连接被对端关闭，那么connection对象将会被自动delete，
    // 导致后面的connection->send中的段错误
    std::getline(std::cin, msg);
    connection->send(msg.c_str(), msg.size());
}

void whenErrorOccur(TcpConnection *conn) {
    Logger::info("error occur, in{}\n", conn->socket().fd());
}


void whenConnClose(TcpConnection *connection) {
    Logger::info("connection close, info: {}\n", connection->socket().fd());
}

int main() {
    EventLoop loop;
    TcpClient client(Socket::makeNewSocket(), &loop, Codec(Codec::UNLIMITED_MODEL, 0));
    client.setConnMsgCallback(whenMsgArrived);
    client.setConnCloseCallback(whenConnClose);
    client.setConnErrorCallback(whenErrorOccur);

    auto conn = client.connect(InAddr("12345", "127.0.0.1"));

    std::string msg;
    std::getline(std::cin, msg);
    conn->send(msg.c_str(), msg.size());
    client.join();
}

