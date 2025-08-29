#ifndef CLIENTTHREAD_H
#define CLIENTTHREAD_H

#include <QThread>
#include <QTcpSocket>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QStringList>

class ClientThread : public QThread
{
    Q_OBJECT
public:
    explicit ClientThread(qintptr socketDescriptor, QObject *parent = nullptr);
    void run() override;

private:
    bool initDatabase();  // 初始化数据库
    QString viewGuahao(const QString &doctorUsername);  // 查看挂号
    QString guahao(const QString &doctorUsername, const QString &patientUsername);  // 挂号
    QString registerUser(const QString &parameters);  // 注册用户
    QString loginUser(const QString &parameters);  // 用户登录

    void onReadyRead();  // 处理客户端发送数据的函数
    QString handleRequest(const QString &command, const QString &parameters);  // 处理命令

    void onDisconnected();  // 断开连接时的处理

private:
    QTcpSocket *socket;  // 用于客户端通信的 socket
    QSqlDatabase db;  // 数据库连接
};

#endif // CLIENTTHREAD_H
