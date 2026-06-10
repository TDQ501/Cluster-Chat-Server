#include "chatservice.h"
#include "public.h"
#include <muduo/base/Logging.h>
#include <vector>
#include "friendmodel.h"
using namespace muduo;
using namespace std;

//获取单例对象的接口
ChatService* ChatService::instance()
{
	static ChatService service;
	return &service;
}

//注册消息以及Hanlder回调操作
ChatService::ChatService()
{
	//登陆业务  msgid=1
	_msgHandlerMap.insert({ LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3) });

	//注册业务  msgid=2
	_msgHandlerMap.insert({ REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3) });

	//一对一聊天业务
	_msgHandlerMap.insert({ ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3) });

	//添加好友业务 msgid=6
	_msgHandlerMap.insert({ ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3) });

	//创建群组业务
	_msgHandlerMap.insert({ CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3) });

	//加入群组业务
	_msgHandlerMap.insert({ ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3) });

	//群聊天业务
	_msgHandlerMap.insert({ GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3) });

	//注销业务
	_msgHandlerMap.insert({ LOGINOUT_MSG,std::bind(&ChatService::loginout,this,_1,_2,_3) });

	//连接redis服务器
	if (_redis.connect())
	{
		//设置上报消息的回调
		_redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
	}
}

//服务器异常，业务重置方法
void ChatService::reset()
{
	_usermodel.resetState();
}

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
	auto it = _msgHandlerMap.find(msgid);
	if (it == _msgHandlerMap.end())
	{
		return [=](const TcpConnectionPtr& conn, json &js, Timestamp time)
			{
				LOG_ERROR << "msgid:" << msgid << "can not find hanler!";
			}; 
	}
	else
	{
		return _msgHandlerMap[msgid];
	}
}

//处理登陆业务  ORM（对象关系映射）业务层操作的都是对象   DAO(数据层)  登录用的数据：id  password
void ChatService::login(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
	int id = js["id"].get<int>();
	string pwd = js["password"];

	User user = _usermodel.query(id); 
	if (user.getId() == id && user.getPwd() == pwd)
	{
		if (user.getState() == "online")
		{
			//用户已经登陆不允许重复登录
			json response;
			response["msgid"] = LOGIN_MSG_ACK;
			response["errno"] = 2;
			response["errmsg"] = "该账号已经登录,请勿重复登录";
			conn->send(response.dump());
		}
		else
		{
			//登录成功，记录用户连接信息
			{
			lock_guard<mutex> lock(_connMutex);//一把_connMutex的锁
			_userConnMap.insert({ id,conn });
			}

			//id用户登陆成功后，向redis订阅channel（id）
			_redis.subscribe(id);

			//登陆成功,更新用户状态信息
			user.setState("online");
			_usermodel.updateState(user);
			json response;
			response["msgid"] = LOGIN_MSG_ACK;
			response["errno"] = 0;
			response["id"] = user.getId();
			response["name"] = user.getName();

			//查询该用户是否有离线消息
			vector<string> vec = _offlineMsModel.query(id);
			if (!vec.empty())
			{
				response["offlinemsg"] = vec;//查询到之后，将容器中的信息中的offlinemsg字段，给response

				//读取该用户的离线消息后，把该用户的所有离线消息删除掉
				_offlineMsModel.remove(id);
			}
			//查询该用户的好友信息并返回
			vector<User> userVec = _friendModel.query(id);
			if (!userVec.empty())
			{
				vector<string> vec2;
				for (User& user : userVec)//定义了一个引用变量 user，每次迭代时绑定到 userVec 中的当前元素,vector<User> userVec
				{
					json js;
					js["id"] = user.getId();
					js["name"] = user.getName();
					js["state"] = user.getState();
					vec2.push_back(js.dump());
				}
				response["friends"] = vec2;
			}
			conn->send(response.dump());//将信息序列化之后发送
		}
	}
	else
	{
		//登录失败
		json response;
		response["msgid"] = LOGIN_MSG_ACK;
		response["errno"] = 1;
		response["errmsg"] = "用户名或密码错误！";
		conn->send(response.dump());//将信息序列化之后发送
	}
}

//处理注册业务
void ChatService::reg(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
	string name = js["name"];
	string pwd = js["password"];

	User user;
	user.setName(name);
	user.setPwd(pwd);
	bool state = _usermodel.insert(user);
	if (state)
	{
		//注册成功
		json response;
		response["msgid"] = REG_MSG_ACK;
		response["errno"] = 0;
		response["id"] = user.getId();
		conn->send(response.dump());//将信息序列化之后发送
	}
	else
	{
		//注册失败
		json response;
		response["msgid"] = REG_MSG_ACK;
		response["errno"] = 1;
		conn->send(response.dump());//将信息序列化之后发送
	}
}

//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{
	User user;
	{
		lock_guard<mutex> lock(_connMutex);
		for (auto it = _userConnMap.begin();it != _userConnMap.end();it++)
		{
			if (it->second == conn)
			{
				user.setId(it->first);
				_userConnMap.erase(it);
				break;
			}
		}
	}

	//用户异常退出，相当于下线，在redis中取消订阅通道
	_redis.unsubscribe(user.getId());

	//更新用户的状态信息
	if (user.getId() != -1)
	{
		user.setState("offline");
		_usermodel.updateState(user);
	}
}

//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
	int toid = js["to"].get<int>();
	{
		lock_guard<mutex> lock(_connMutex);
		auto it = _userConnMap.find(toid);
		if (it != _userConnMap.end())
		{
			//toid 在线 转发消息  服务器主动推送消息给toid用户
			it->second->send(js.dump());
			return;
		}
	}

	//查询toid是否在线
	User user = _usermodel.query(toid);
	if (user.getState() == "online")
	{
		_redis.publish(toid, js.dump());
		return;
	}

	//toid不在线，存储离线消息
	_offlineMsModel.insert(toid, js.dump());
}

//添加好友业务
void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
	int userid = js["id"].get<int>();
	int friendid = js["friendid"].get<int>();

	_friendModel.insert(userid, friendid);
}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
	int userid = js["id"].get<int>();
	string name = js["groupname"];
	string desc = js["groupdesc"];

	//存储新创建的群组信息
	Group group(-1, name, desc);
	if (_groupModel.creatGroup(group))
	{
		//存储群组创建人信息
		_groupModel.addGroup(userid, group.getId(), "creator");
	}
}

//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
	int userid = js["id"].get<int>();
	int groupid = js["groupid"].get<int>();
	_groupModel.addGroup(userid, groupid, "normal");
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
	int userid = js["id"].get<int>();
	int groupid = js["groupid"].get<int>();

	vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
	for (int id : useridVec)
	{
		auto it = _userConnMap.find(id);
		if (it != _userConnMap.end())
		{
			//转发消息
			it->second->send(js.dump());
		}
		else
		{
			User user = _usermodel.query(id);
			if (user.getState() == "online")
			{
				_redis.publish(id, js.dump());
			}
			else
			{
				//存储离线消息
				_offlineMsModel.insert(id, js.dump());
			}
			//储存离线消息
			_offlineMsModel.insert(id, js.dump());
		}
	}
}

//注销业务
void ChatService::loginout(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
	int userid = js["id"].get<int>();

	{
		lock_guard<mutex> lock(_connMutex);
		auto it = _userConnMap.find(userid);
		if (it != _userConnMap.end())
		{
			_userConnMap.erase(it);
		}
	}

	//用户注销，相当于就是下线，在redis中取消订阅通道
	_redis.unsubscribe(userid);

	//更新用户的状态信息
	User user(userid, "", "", "offline");
	_usermodel.updateState(user);
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
	lock_guard<mutex> lock(_connMutex);
	auto it = _userConnMap.find(userid);
	if (it != _userConnMap.end())
	{
		it->second->send(msg);
		return;
	}

	// 存储该用户的离线消息
	_offlineMsModel.insert(userid, msg);
}