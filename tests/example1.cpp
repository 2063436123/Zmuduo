//
// Created by Hello Peter on 2021/9/7.
//

#include "../TcpServer.h"
#include "../TcpConnection.h"

using namespace std;

void whenNewConnectionEstablished(TcpConnection *conn) {
    Socket &s = conn->socket();
//    cout << "conn connected! from " << s.peerInAddr().ipPortStr() << " to " << s.localInAddr().ipPortStr() << " ";
//    cout << "fd: " << s.fd() << endl;
}

void whenMsgArrived(TcpConnection *conn) {
    Socket &s = conn->socket();
    auto &buf = conn->readBuffer();
//    cout << "read: " << std::string(buf.peek(), buf.readableBytes());

    conn->send(buf.peek(), buf.readableBytes());

    buf.retrieveAll();
    conn->activeClose();
}

void whenClose(TcpConnection *conn) {
    Socket &s = conn->socket();
//    cout << "conn terminated! from " << endl;
    // fixed 此时不该调用peerInAddr()，因为可能对端已经关闭了（当对端而非我端主动关闭时）
    // s.peerInAddr().ipPortStr() << " to " << s.localInAddr().ipPortStr() << endl;
}
TcpServer *server_copy;
void whenStdinMsgArrived(shared_ptr<TcpConnection> conn) {
    auto& buf = conn->readBuffer();
    Logger::info("stopping...");
    Logger::fatal("stdin:{}", std::string(buf.peek(), buf.readableBytes()));
    if (conn->readBuffer().findStr("bye"))
        server_copy->stop();
    conn-> // todo关闭stdin的readable监听，从而顺利退出程序
}

int main() {
    if (Options::setMaxFiles(1048576) < 0)
        Logger::sys("getMaxFiles error");
    Options::getMaxFiles();

    auto s = Socket::makeListened();
    EventLoop loop;
    TcpServer server(std::move(s), InAddr("12345"), &loop);
    server_copy = &server;

    // for stop tcpServer
    server.addOnlyMsgChannel(STDIN_FILENO, whenStdinMsgArrived);

    server.setConnEstaCallback(whenNewConnectionEstablished);
    server.setConnMsgCallback(whenMsgArrived);
    server.setConnCloseCallback(whenClose);
//    server.start();
    server.start(4);
}