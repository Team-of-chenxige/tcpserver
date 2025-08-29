#include "clientthread.h"
#include <QTcpSocket>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

ClientThread::ClientThread(qintptr socketDescriptor, QObject *parent)
    : QThread(parent), socket(new QTcpSocket())
{
    socket->setSocketDescriptor(socketDescriptor);
    connect(socket, &QTcpSocket::readyRead, this, &ClientThread::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &ClientThread::onDisconnected);
}

void ClientThread::run()
{
    if (!initDatabase()) {
        socket->write("数据库初始化失败");
        socket->disconnectFromHost();
        return;
    }
    exec();  // 进入事件循环
}

bool ClientThread::initDatabase()
{
    QString connName = QString("conn_%1").arg((quintptr)QThread::currentThreadId());
    db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName("users.db");

    if (!db.open()) {
        qDebug() << "数据库连接失败:" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);

    // 创建用户表
    bool tableExists = query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='users'");
    if (!tableExists || !query.next()) {
        QString createTableSQL = R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT NOT NULL,
                password TEXT NOT NULL,
                role BOOLEAN NOT NULL,
                gender BOOLEAN NOT NULL,
                age INTEGER NOT NULL,
                phone TEXT NOT NULL DEFAULT '',
                UNIQUE(username, role)
            );
        )";

        if (!query.exec(createTableSQL)) {
            qDebug() << "创建表失败:" << query.lastError().text();
            return false;
        }
    }

    // 创建挂号表 (不使用外键约束)
    tableExists = query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='guahao'");
    if (!tableExists || !query.next()) {
        QString createGuahaoSQL = R"(
            CREATE TABLE IF NOT EXISTS guahao (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                doctor_username TEXT NOT NULL,
                patient_username TEXT NOT NULL
            );
        )";

        if (!query.exec(createGuahaoSQL)) {
            qDebug() << "创建挂号表失败:" << query.lastError().text();
            return false;
        }
    }

    return true;
}


void ClientThread::onReadyRead()
{
    QByteArray data = socket->readAll();
    QString msg = QString::fromUtf8(data).trimmed();

    QStringList parts = msg.split(",");
    parts.removeAll("");

    if (parts.isEmpty()) {
        socket->write("格式错误");
        return;
    }

    QString command = parts[0];
    QString parameters = msg.mid(command.length() + 1);

    QString result = handleRequest(command, parameters);
    socket->write(result.toUtf8());
}

QString ClientThread::handleRequest(const QString &command, const QString &parameters)
{
    if (command == "register") {
        return registerUser(parameters);
    } else if (command == "login") {
        return loginUser(parameters);
    } else if (command == "viewguahao") {
        return viewGuahao(parameters);  // 查看挂号
    } else if (command == "guahao") {
        QStringList args = parameters.split(",");
        if (args.size() < 2) {
            return "挂号参数不完整";
        }
        return guahao(args[0], args[1]);  // 挂号
    } else {
        return "未知命令";
    }
}


QString ClientThread::registerUser(const QString &parameters)
{
    QStringList args = parameters.split(",");
    if (args.size() < 6) {  // 用户名、密码、角色、性别、年龄、手机号
        return "注册参数不完整";
    }

    QString username = args[0];
    QString password = args[1];
    bool role = (args[2] == "1");  // 1 表示医生，0 表示患者
    bool gender = (args[3] == "1");  // 1 表示男性，0 表示女性
    int age = args[4].toInt();
    QString phone = args[5];

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM users WHERE username = :username AND role = :role");
    query.bindValue(":username", username);
    query.bindValue(":role", role);
    if (!query.exec()) {
        qDebug() << "查询失败:" << query.lastError().text();
        return "数据库错误";
    }
    query.next();
    if (query.value(0).toInt() > 0) {
        return "该用户已注册";
    }

    query.prepare("INSERT INTO users (username, password, role, gender, age, phone) "
                  "VALUES (:username, :password, :role, :gender, :age, :phone)");
    query.bindValue(":username", username);
    query.bindValue(":password", password);
    query.bindValue(":role", role);
    query.bindValue(":gender", gender);
    query.bindValue(":age", age);
    query.bindValue(":phone", phone);
    if (!query.exec()) {
        qDebug() << "插入失败:" << query.lastError().text();
        return "数据库错误";
    }

    return "注册成功";
}

QString ClientThread::loginUser(const QString &parameters)
{
    QStringList args = parameters.split(",");
    if (args.size() < 3) {
        return "登录参数不完整";
    }

    QString username = args[0];
    QString password = args[1];
    bool role = (args[2] == "1");

    QSqlQuery query(db);

    query.prepare("SELECT COUNT(*) FROM users WHERE username = :username AND role = :role");
    query.bindValue(":username", username);
    query.bindValue(":role", role);
    if (!query.exec()) {
        qDebug() << "查询失败:" << query.lastError().text();
        return "数据库错误";
    }
    query.next();

    if (query.value(0).toInt() == 0) {
        return "用户名不存在";
    }

    query.prepare("SELECT COUNT(*) FROM users WHERE username = :username AND password = :password AND role = :role");
    query.bindValue(":username", username);
    query.bindValue(":password", password);
    query.bindValue(":role", role);

    if (!query.exec()) {
        qDebug() << "查询失败:" << query.lastError().text();
        return "数据库错误";
    }

    query.next();

    if (query.value(0).toInt() == 0) {
        return "密码错误";
    }

    return "登录成功";
}


QString ClientThread::viewGuahao(const QString &doctorUsername)
{
    QSqlQuery query(db);
    query.prepare("SELECT patient_username FROM guahao WHERE doctor_username = :doctorUsername");
    query.bindValue(":doctorUsername", doctorUsername);

    if (!query.exec()) {
        qDebug() << "查询失败:" << query.lastError().text();
        return "数据库错误";
    }

    QStringList patients;
    while (query.next()) {
        patients.append(query.value(0).toString());
    }

    if (patients.isEmpty()) {
        return "没有挂号记录";
    }

    QString result = "挂号患者：\n";
    for (const QString &patient : patients) {
        result += patient + "\n";
    }
    result += "总共 " + QString::number(patients.size()) + " 名患者";
    return result;
}




QString ClientThread::guahao(const QString &doctorUsername, const QString &patientUsername)
{
    QSqlQuery query(db);

    // 检查医生是否存在
    query.prepare("SELECT COUNT(*) FROM users WHERE username = :doctorUsername AND role = 1"); // 医生角色为 1
    query.bindValue(":doctorUsername", doctorUsername);
    if (!query.exec()) {
        qDebug() << "查询失败:" << query.lastError().text();
        return "数据库错误";
    }
    query.next();
    if (query.value(0).toInt() == 0) {
        return "医生不存在";
    }

    // 检查患者是否存在
    query.prepare("SELECT COUNT(*) FROM users WHERE username = :patientUsername AND role = 0"); // 患者角色为 0
    query.bindValue(":patientUsername", patientUsername);
    if (!query.exec()) {
        qDebug() << "查询失败:" << query.lastError().text();
        return "数据库错误";
    }
    query.next();
    if (query.value(0).toInt() == 0) {
        return "患者不存在";
    }

    // 插入挂号信息
    query.prepare("INSERT INTO guahao (doctor_username, patient_username) VALUES (:doctorUsername, :patientUsername)");
    query.bindValue(":doctorUsername", doctorUsername);
    query.bindValue(":patientUsername", patientUsername);

    if (!query.exec()) {
        qDebug() << "插入失败:" << query.lastError().text();
        return "挂号失败";
    }

    return "挂号成功";
}


void ClientThread::onDisconnected()
{
    socket->deleteLater();
    quit();
}
