# Cluster-Chat-Server
集群聊天服务器 （服务器+客户端）C++ 实现
基于 Muduo 网络库 + MySQL + Redis 的 C++ 集群聊天服务器，支持用户注册、登录、一对一聊天、群组聊天、好友管理、跨服务器消息转发等功能。
## 功能特性
1.用户注册 / 登录
2.好友添加与管理
3.一对一聊天（支持离线消息）
4.群组创建 / 加入 / 群聊（支持离线消息）
5.跨服务器消息转发（基于 Redis 发布/订阅）
6.用户状态管理（在线/离线）

**IDE**: Visual Studio 2022（使用 **Linux 远程开发** 功能）
- **远程编译环境**: CentOS 7
- **编译器**: g++ 11 (devtoolset-11)
- **C++ 标准**: C++17
- **依赖库**:
  - [Muduo](https://github.com/chenshuo/muduo) (网络库)
  - [MySQL C API](https://dev.mysql.com/downloads/connector/c/) (数据库)
  - [hiredis](https://github.com/redis/hiredis) (Redis 客户端)
  - [nlohmann/json](https://github.com/nlohmann/json) (JSON 解析)
 
  
