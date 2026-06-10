#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <chrono>
#include <ctime>
#include <semaphore.h>
#include <atomic>
using namespace std;
using nlohmann::json;

//Linux 系统网络编程头文件
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "group.h"
#include <user.hpp>
#include <public.h>

//记录当前系统登陆的用户信息
User g_currentUser;

//记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;

//记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

//控制主菜单页面程序
bool isMainMenuRunning = false;
 
//记录当前登录成功用户的基本信息
void showCurrentUserData();

//接收线程
void readTaskHandler(int clientid);

//获取系统时间
string getCurrentTime();

//主聊天程序页面
void mainMenu(int);
void doLoginResponse(json& js);
void doRegResponse(json& js);

// 用于读写线程之间的通信
sem_t rwsem;

// 记录登录状态, 用于 循环走 哪一步
atomic_bool g_isLogin_Success(false);

//聊天客户端实现，main线程用来发送线程，子线程用来接收
int main(int argc, char** argv)//argc(至少为1):命令行参数个数  char** argv 参数字符串数组
{
	if (argc < 3) //需要服务器IP和端口号两个额外的参数
	{
		cerr << "command invalid! please check! example:./CMakeTarget 127.0.0.1 6000" << endl;
		exit(-1);
	}
	
	//解析通过命令行参数传递的IP和Port
	char* ip = argv[1];//第一个参数（IP地址字符串）
	uint16_t port = atoi(argv[2]);//第二个参数（端口号字符串）
	
	//创建client端的socket
	int clientfd = socket(AF_INET, SOCK_STREAM, 0);//创建一个 IPv4 的 TCP 套接字。AF_INET：地址族，IPv4。SOCK_STREAM：提供可靠的、面向连接的字节流（TCP）。0：自动选择协议（这里就是 TCP）。
	if (-1 == clientfd)
	{
		cerr << "socket create error" << endl;
		exit(-1);
	}

	// 填写client需要连接的server信息ip+port
	sockaddr_in server;	//用于存储 IPv4 地址和端口的结构体
	memset(&server, 0, sizeof(sockaddr_in));//将整个结构体清零，确保所有字段初始为0，避免残留数据。
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = inet_addr(ip);

	// client和server进行连接
	if (-1 == connect(clientfd, (sockaddr*)&server, sizeof(sockaddr_in)))
	{
		cerr << "connect server error" << endl;
		close(clientfd);
		exit(-1);
	}

	// 连接服务器成功，启动接收子线程
	std::thread readTask(readTaskHandler, clientfd); // pthread_create
	readTask.detach();

	// 初始化读写线程通信用的信号量
	sem_init(&rwsem, 0, 0);

	// main线程用于接收用户输入，负责发送数据
	for(;;)
	{
		// 显示首页面菜单 登录、注册、退出
		cout << "========================" << endl;
		cout << "1. login" << endl;
		cout << "2. register" << endl;
		cout << "3. quit" << endl;
		cout << "========================" << endl;
		cout << "choice:";
		int choice = 0;
		cin >> choice;
		cin.get(); // 读掉缓冲区残留的回车

		switch (choice)
		{
		case 1:
			//登录业务
		{
			int id = 0;
			char pwd[20] = { 0 };
			cout << "userid:";
			cin >> id;
			cin.get();
			cout << "userpassword:";
			cin.getline(pwd, 50);

			json js;
			js["msgid"] = LOGIN_MSG;
			js["id"] = id;
			js["password"] = pwd;
			string request = js.dump();

			g_isLogin_Success = false;

			int len = send(clientfd, request.c_str(), strlen(request.c_str())+1, 0);
			if (len == -1)
			{
				cerr << "send login msg error:" << request << endl;
			}

			sem_wait(&rwsem); // 等待信号量，由子线程处理完登录的响应消息后，通知这里
			if (g_isLogin_Success == true)
			{
				// 主线程继续执行, 进入聊天菜单页面
				isMainMenuRunning = true;
				mainMenu(clientfd);
			}
		}
			break;
		case 2: //注册业务
		{
			char name[50] = { 0 };
			char pwd[50] = { 0 };
			cout << "username:";
			cin.getline(name, 50);
			cout << "userpassword:";
			cin.getline(pwd, 50);

			json js;
			js["msgid"] = REG_MSG;
			js["name"] = name;
			js["password"] = pwd;
			string request = js.dump();

			int len = send(clientfd, request.c_str(), strlen(request.c_str()), 0);
			if (len == -1)
			{
				cerr << "send reg msg error:" << request << endl;
			}
			sem_wait(&rwsem); // 注册完, 会通知
		}
			break;
		case 3:
		{
			cout << "exit system" << endl;
			// 销毁信号量
			sem_destroy(&rwsem);
			close(clientfd);
			exit(0);
		}
		default:
			cerr << "invalid input!" << endl;
			break;
		}

	}
}

//显示当前登录成功用户的基本信息
void showCurrentUserData()
{
	cout << "=================================login user============================" << endl;
	cout << "current login user id:" << g_currentUser.getId() << "name:" << g_currentUser.getName() << endl;
	cout << "-------------------------friend list-------------------------" << endl;
	if (!g_currentUserFriendList.empty())
	{
		for (User& user : g_currentUserFriendList)
		{
			cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
		}
	}
	cout << "----------------------------group list-------------------------" << endl;
	if (!g_currentUserGroupList.empty())
	{
		for (Group& group : g_currentUserGroupList)
		{
			cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
			for (GroupUser& groupuser : group.getUsers())
			{
				cout << groupuser.getId() << " " << groupuser.getName() << " " << groupuser.getState() << " " << groupuser.getRole() << " " << endl;
			}
		}
	}
	cout << "====================================================================" << endl;
}

//接收线程
void readTaskHandler(int clientfd)
{
	for (;;)
	{
		char buffer[1024] = { 0 };
		int len = recv(clientfd, buffer, 1024, 0);
		if (len < 0)                               // ==-1
		{
			cerr << "recv error" << endl;
			close(clientfd);
			exit(-1);
		}
		else if (len == 0) // 服务器关闭连接
		{
			cout << "server close" << endl;
			close(clientfd);
			exit(-1);
		}

		//接收ChatServer转发的数据，反序列化生成json对象
		json js = json::parse(buffer);
		int msgtype = js["msgid"].get<int>();
		if (ONE_CHAT_MSG == msgtype)
		{
			cout << js["time"].get<string>() << "[" << js["id"] << "]" 
				<< js["name"].get<string>() << "said:" << js["msg"].get<string>() << endl;
			continue;
		}
		if(GROUP_CHAT_MSG==msgtype)
		{
			cout << "群消息：["<<js["groupid"]<<"]:"<<js["time"].get<string>() << "[" 
				<< js["id"] << "]" << js["name"].get<string>() << "said:" << js["msg"].get<string>() << endl;
			continue;
		}
		// 登录成功
		if (LOGIN_MSG_ACK == msgtype)
		{
			doLoginResponse(js); // 处理登陆成功的 业务逻辑
			sem_post(&rwsem);          // 通知主线程, 登录结果是什么
			continue;
		}

		// 注册
		if (REG_MSG_ACK == msgtype)
		{
			doRegResponse(js);
			sem_post(&rwsem);
			continue;
		}
	}
}

// "help" command handler
void help(int fd = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void loginout(int, string);

// 系统支持的客户端命令列表（帮助页面）
unordered_map<string, string> commandMap = 
{
	{"help", "Show all commands, format: help"},
	{"chat", "One-to-one chat, format: chat:friendid:message"},
	{"addfriend", "Add friend, format: addfriend:friendid"},
	{"creategroup", "Create group, format: creategroup:groupname:groupdesc"},
	{"addgroup", "Join group, format: addgroup:groupid"},
	{"groupchat", "Group chat, format: groupchat:groupid:message"},
	{"loginout", "Logout, format: loginout"}
};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = 
{
	{"help", help},
	{"chat", chat},
	{"addfriend", addfriend},
	{"creategroup", creategroup},
	{"addgroup", addgroup},
	{"groupchat", groupchat},
	{"loginout", loginout} 
};

//主聊天页面程序
void mainMenu(int clientfd)
{
	help();

	char buffer[1024] = { 0 };
	while(isMainMenuRunning)
	{
		cin.getline(buffer, 1024);
		string commandbuf(buffer);
		string command;//存储命令
		int index = commandbuf.find(":");
		if (index == -1)
		{
			command = commandbuf;//表示没有：的指令（help和logout或者错误的输入）
		}
		else
		{
			command = commandbuf.substr(0, index);
		}
		auto it = commandHandlerMap.find(command);
		if (it == commandHandlerMap.end())
		{
			cerr << "invalid input command!" << endl;
			continue;
		}

		//调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要需改该函数
		it->second(clientfd, commandbuf.substr(index + 1, commandbuf.size() - index));
	}
}

void help(int, string)
{
	cout << "show command list" << endl;
	for (auto& p : commandMap)
	{
		cout << p.first << ":" << p.second << endl;
	}
	cout << endl;
}

// "chat" command handler
void chat(int clientfd, string str)
{
	int idx = str.find(":"); // friendid:message
	if (-1 == idx)
	{
		cerr << "chat command invalid!" << endl;
		return;
	}

	int friendid = atoi(str.substr(0, idx).c_str());//先从json形式的字符串转为c语言字符串然后使用atoi函数转为int形式
	string message = str.substr(idx + 1, str.size() - idx);

	json js;
	js["msgid"] = ONE_CHAT_MSG;
	js["id"] = g_currentUser.getId();
	js["name"] = g_currentUser.getName();
	js["to"] = friendid;
	js["msg"] = message;
	js["time"] = getCurrentTime();
	string buffer = js.dump();

	int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()), 0);
	if (-1 == len)
	{
		cerr << "send chat msg error -> " << buffer << endl;
	}
}

// "addfriend" command handler
void addfriend(int clientfd, string str)
{
	int friendid = atoi(str.c_str());
	json js;
	js["msgid"] = ADD_FRIEND_MSG;
	js["id"] = g_currentUser.getId();
	js["friendid"] = friendid;
	string buffer = js.dump();

	int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()), 0);
	if (-1 == len)
	{
		cerr << "send addfriend msg error -> " << buffer << endl;
	}
}

// "creategroup" command handler
void creategroup(int clientfd, string str)
{
	int idx = str.find(":");//groupname:groupdesc
	if (-1 == idx)
	{
		cerr << "creategroup command invalid!" << endl;
		return;
	}

	string groupname = str.substr(0, idx);
	string groupdesc = str.substr(idx + 1, str.size() - idx);

	json js;
	js["msgid"] = CREATE_GROUP_MSG;
	js["id"] = g_currentUser.getId();
	js["groupname"] = groupname;
	js["groupdesc"] = groupdesc;
	string buffer = js.dump();

	int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()), 0);
	if (-1 == len)
	{
		cerr << "send creategroup msg error -> " << buffer << endl;
	}
}

// "addgroup" command handler
void addgroup(int clientfd, string str)
{
	int groupid = atoi(str.c_str());
	json js;
	js["msgid"] = ADD_GROUP_MSG;
	js["id"] = g_currentUser.getId();
	js["groupid"] = groupid;
	string buffer = js.dump();

	int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()), 0);
	if (-1 == len)
	{
		cerr << "send addgroup msg error -> " << buffer << endl;
	}
}

// "groupchat" command handler
void groupchat(int clientfd, string str)
{
	int idx = str.find(":");
	if (-1 == idx)
	{
		cerr << "groupchat command invalid!" << endl;
		return;
	}

	int groupid = atoi(str.substr(0, idx).c_str());
	string message = str.substr(idx + 1, str.size() - idx);

	json js;
	js["msgid"] = GROUP_CHAT_MSG;
	js["id"] = g_currentUser.getId();
	js["name"] = g_currentUser.getName();
	js["groupid"] = groupid;
	js["msg"] = message;
	js["time"] = getCurrentTime();
	string buffer = js.dump();

	int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()), 0);
	if (-1 == len)
	{
		cerr << "send groupchat msg error -> " << buffer << endl;
	}
}

// "loginout" command handler
void loginout(int clientfd, string str)
{
	json js;
	js["msgid"] = LOGINOUT_MSG;
	js["id"] = g_currentUser.getId();
	string buffer = js.dump();

	int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()), 0);
	if (-1 == len)
	{
		cerr << "send loginout msg error -> " << buffer << endl;
	}
	else
	{
		isMainMenuRunning = false;
	}
}

//获取系统时间
string getCurrentTime()
{
	auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	struct tm* ptm = localtime(&tt);
	char date[60] = { 0 };
	sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
		(int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
		(int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
	return std::string(date);
}

void doLoginResponse(json& js)
{
	json response = js;
	if (response["errno"] == 0)
	{
		// 设置登录状态
		g_isLogin_Success = true;

		// 根据业务代码处理 1.登录成功返回 2.好友列表 3.群组列表 4.离线消息
		cout << "login success" << endl;

		// 客户端记录登录用户信息
		g_currentUser.setId(response["id"]);
		g_currentUser.setName(response["name"]);

	
		if (response.contains("friends"))
		{
			vector<string> friends = response["friends"]; // 类型是vector<string>, 不是vector<User>,  根据服务器业务,存的是js.dump() 字符串
			g_currentUserFriendList.clear();
			for (auto& friendUser : friends)
			{
				json js = json::parse(friendUser); // 反序列化
				User user;
				user.setId(js["id"]);
				user.setName(js["name"]);
				user.setState(js["state"]);
				g_currentUserFriendList.push_back(user);
			}
			for (auto& friendUser : g_currentUserFriendList)
			{
				cout << "friendid: " << friendUser.getId() << " name: " << friendUser.getName() << " state: " << friendUser.getState() << endl;
			}
		}
		else
		{
			cout << "friends list is empty" << endl;
		}

		// 处理群组列表
		if (response.contains("groups")) // 判断是否包含字段, 跟好点,  而不是看 是不是空
		{
			vector<string> groups = response["groups"]; // 类型是vector<string>, 不是vector<User>,  根据服务器业务,存的是js.dump() 字符串
			g_currentUserGroupList.clear();
			for (auto& groupl : groups)
			{
				json js = json::parse(groupl); // 反序列化
				string tmp = js.dump();
				Group group;
				group.setId(js["id"]);
				group.setName(js["groupname"]);
				group.setDesc(js["groupdesc"]);

				// 处理群组成员列表
				vector<string> users = js["users"];
				for (auto& userl : users)
				{
					json js = json::parse(userl); // 反序列化
					GroupUser user;
					user.setId(js["id"]);
					user.setName(js["name"]);
					user.setState(js["state"]);
					user.setRole(js["role"]);
					group.getUsers().push_back(user);
				}
				g_currentUserGroupList.push_back(group);
			}
			for (auto& group : g_currentUserGroupList)
			{
				cout << "groupid: " << group.getId() << " name: " << group.getName() << " desc: " << group.getDesc() << endl;
				for (auto& groupUser : group.getUsers())
				{
					cout << "group user id: " << groupUser.getId() << " name: " << groupUser.getName() << " state: " << groupUser.getState() << " role: " << groupUser.getRole() << endl;
				}
			}
		}
		else
		{
			cout << "groups list is empty" << endl;
		}

		// 显示当前登录用户的基本信息---包含好友列表和群组列表
		showCurrentUserData();

		// 处理离线消息
		if (response.contains("offlinemsg")) // 判断是否包含字段, 跟好点,  而不是看 是不是空
		{
			vector<string> offlinemsg = response["offlinemsg"]; // 类型是vector<string>, 不是vector<User>,  根据服务器业务,存的是js.dump() 字符串
			for (auto& msg : offlinemsg)
			{
				json js = json::parse(msg); // 反序列化
				// 时间+fromid+fromname+msg-----详看笔记 一对一聊天发送的格式

				// 分一下 个人离线和群组离线
				if (js["msgid"] == ONE_CHAT_MSG)
				{
					cout << js["time"].get<string>() << "[" << js["id"].get<int>() << "] " << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
				}
				if (js["msgid"] == GROUP_CHAT_MSG) // 群组聊天消息
				{
					cout << "群消息-->[" << js["groupid"] << "] " << js["time"].get<string>() << "[" << js["id"] << "] " << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
				}
			}
		}
		else
		{
			cout << "offlinemsg list is empty" << endl;
		}
	}
	else // 不分那么细, 服务器已经确定错误信息了
	{
		g_isLogin_Success = false; // 登录失败

		// 登录失败
		cout << "login failed, error: " << response["errmsg"] << endl;
	}
}

void doRegResponse(json& js)
{
	json response = js;
	if (response["errno"] == 0)
	{ // 根据业务代码处理
		cout << "register success, userid: " << response["id"] << " do not forget it!" << endl;
	}
	else
	{
		// 注册失败
		cout << "register failed, error: name is already exit!" << endl;
	}
}
