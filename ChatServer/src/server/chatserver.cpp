#include "chatserver.hpp"
#include <functional>
#include "json.hpp"
#include "chatservice.h"
using nlohmann::json;

//三个数据分别是：事件循环 IP+Port 服务器名称
ChatServer::ChatServer(EventLoop* loop, const InetAddress& listenAddr, const string& nameArg) :_server(loop, listenAddr, nameArg), _loop(loop)//初始化成员列表
{
	//给服务器注册用户连接的创建和断开回调

	//回调函数是一种被传递给其他函数，并在特定时间发生时被对方调用的函数
	//在这里_server下的setConnectionCallback希望收到一个参数为const TcpConnectionPtr&的可调用对象（回调函数），因此我们定义了void onConnection(const TcpConnectionPtr&)这个成员函数
	//但是成员函数的调用需要用到this指针，指向所调用的对象，因此使用bind进行绑定，this指向当前对象，bind返回一个可调用的对象（该对象符合setConnectionCallback签名）
	_server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));//_1是参数占位符

	//给服务器注册用户读写事件回调
	_server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

	//设置服务器端的线程数量
	_server.setThreadNum(4); //一个IO线程，3个worker线程
}

	//开启事件循环
void ChatServer::start()
{
	_server.start();
}

//专门处理用户的连接创建和断开
void ChatServer::onConnection(const TcpConnectionPtr& conn)//conn是一个智能指针 ->被重载了 conn->返回一个原始TcpConnection对象
{
	if (!conn->connected())
	{
		ChatService::instance()->clientCloseException(conn);
		conn->shutdown();
	}
}

//专门处理用户的读写事件 连接 缓冲区 接收到数据的时间
void ChatServer::onMessage(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp time)
{
	string buf = buffer->retrieveAllAsString();
	//数据的反序列化
	json js = json::parse(buf);//目的：完全解耦网络模块的代码和业务模块的代码 通过js["msgid"]获取业务hanler

	auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());//这里json反序列化得到的不是一个C++形式的整数，应该调用get<>()这个模板进行转化
	msgHandler(conn, js, time);
}
