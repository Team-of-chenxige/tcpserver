#include "tcpserver.h"
#include <QTcpSocket>
#include <QDebug>

TcpServer::TcpServer(QObject *parent)
    : QTcpServer(parent)
{
}

bool TcpServer::startServer(quint16 port)
{
    if (!this->listen(QHostAddress::Any, port)) {
        qDebug() << "服务器启动失败!";
        return false;
    }
    qDebug() << "服务器已启动，端口:" << port;
    return true;
}

void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    ClientThread *thread = new ClientThread(socketDescriptor, this);
    threads.append(thread);
    thread->start();
}
