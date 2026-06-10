#include "usermodel.hpp"
#include "db.h"

//不实现业务行为，只负责增删改查

//User表的增加方法
bool UserModel::insert(User& user)
{
	char sql[1024] = { 0 };
	//扫描格式字符串，遇到第一个 %s 时，取出第一个额外参数（user.getName().c_str()）的字符串内容。遇到第二个% s 时，取第二个参数（user.getPwd().c_str()）的内容。第三个% s 同理。
	sprintf(sql, "insert into user(name,password,state) value('%s','%s','%s')", user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());

	MySQL mysql;
	if (mysql.connect())
	{
		if (mysql.update(sql))
		{
			//获取插入成功的用户数据生成的主键id
			user.setId(mysql_insert_id(mysql.getConnection()));//mysql_insert_id(mysql.getConnection())这是 MySQL C API 函数，返回上一次即新插入记录的自增ID。
			return true;
		}
	}
	return false;
}

//根据用户号码查询信息
User UserModel::query(int id)
{
	char sql[1024] = { 0 };
	sprintf(sql, "select * from user where id =%d", id);
	MySQL mysql;
	if (mysql.connect())
	{
		MYSQL_RES* res = mysql.query(sql);//接受查询结果
		if (res!=nullptr)
		{
			MYSQL_ROW row = mysql_fetch_row(res);
			if (row != nullptr)
			{
				User user;
				user.setId(atoi(row[0]));
				user.setName(row[1]);
				user.setPwd(row[2]);
				user.setState(row[3]);
				mysql_free_result(res);//释放掉避免内存泄漏
				return user;
			}
		}
	}
	return User();//没找到返回一个默认值
}

//更新用户的状态信息
bool UserModel::updateState(User user)
{
	char sql[1024] = { 0 };
	sprintf(sql, "update user set state= '%s' where id = %d", user.getState().c_str(), user.getId());

	MySQL mysql;
	if (mysql.connect())
	{
		if (mysql.update(sql))
		{
			return true;
		}
	}
	return false;
}

//重置用户的状态信息
void UserModel::resetState()
{
	char sql[1024] = "update user set state= 'offline' where state = 'online'";

	MySQL mysql;
	if (mysql.connect())
	{
		mysql.update(sql);
	}
}