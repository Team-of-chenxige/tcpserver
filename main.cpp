#include <QCoreApplication>
#include "tcpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    TcpServer server;
    if (!server.startServer(1234)) {  // 监听端口 1234
        return -1;
    }

    return a.exec();
}
