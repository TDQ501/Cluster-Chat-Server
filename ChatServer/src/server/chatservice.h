#pragma once
#include <unordered_map>
#include <muduo/net/TcpConnection.h>
#include <functional>
#include <mutex>
#include "usermodel.hpp"
#include "json.hpp"
#include "offlinemessagemodel.h"
#include "friendmodel.h"
#include "groupmodel.h"
#include "redis.h"
using namespace std;
using namespace muduo;
using namespace muduo::net;
using nlohmann::json;

//表示处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr& conn, json &js, Timestamp time)>;//自己定义一个回调函数

//业务类
class ChatService
{
public:
	static ChatService* instance();//单例对象
	//处理登陆业务
	void login(const TcpConnectionPtr& conn, json &js, Timestamp time);

	//处理注册业务
	void reg(const TcpConnectionPtr& conn, json &js, Timestamp time);

	//一对一聊天业务
	void oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time);

	//获取消息对应的处理器
	MsgHandler getHandler(int msgid);

	//添加好友业务
	void addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time);

	//处理客户端异常退出
	void clientCloseException(const TcpConnectionPtr& conn);

	//服务器异常，业务重置方法
	void reset();

	//创建群组业务
	void createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);

	//加入群组业务
	void addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);

	//群组聊天业务
	void groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time);

	//注销业务
	void loginout(const TcpConnectionPtr& conn, json& js, Timestamp time);

	// 从redis消息队列中获取订阅的消息
	void handleRedisSubscribeMessage(int, string);
private:
	ChatService();

	//存储消息id和其对应的业务处理方法
	unordered_map<int, MsgHandler> _msgHandlerMap;

	//数据操作类对象 不做具体的数据库相关的操作，数据库相关的操作被封装到User里面
	UserModel _usermodel;

	//数据操作类对象
	unordered_map<int, TcpConnectionPtr> _userConnMap;
	OfflineMessageModel _offlineMsModel;
	FriendModel _friendModel;
	GroupModel _groupModel;

	//定义互斥锁，保证_userConnMap的线程安全
	mutex _connMutex;

	//redis操作对象
	Redis _redis;
};
