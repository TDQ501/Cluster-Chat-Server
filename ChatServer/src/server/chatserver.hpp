#pragma once
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <functional>
using namespace std;
using namespace muduo;
using namespace muduo::net;

/*
基于muduo网络库开发服务器程序
1、组合TcpServer对象
2、创建EventLoop事件循环对象的指针
3、明确TcpServer构造函数需要什么参数，输出ChatServer的构造函数
4、在当前服务器类的构造函数中，注册处理连接的回调函数和处理读写时间的回调函数
5、设置合适的服务端线程数量
*/
class ChatServer
{
public:
	//三个数据分别是：事件循环 IP+Port 服务器名称
	ChatServer(EventLoop* loop, const InetAddress& listenAddr, const string& nameArg);

	//开启事件循环
	void start();
private:
	//专门处理用户的连接创建和断开
	void onConnection(const TcpConnectionPtr& conn);//conn是一个智能指针 ->被重载了 conn->返回一个原始TcpConnection对象
	

	//专门处理用户的读写事件 连接 缓冲区 接收到数据的时间
	void onMessage(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp time);

	TcpServer _server;

	EventLoop* _loop;
};
