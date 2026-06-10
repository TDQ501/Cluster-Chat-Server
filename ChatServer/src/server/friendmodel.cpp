#include "friendmodel.h"
#include "db.h"

//添加好友关系
void FriendModel::insert(int userid, int friendid)
{
	//组装MySQl语句
	char sql[1024] = { 0 };
	sprintf(sql, "insert into Friend value(%d,%d)", userid, friendid);

	MySQL mysql;
	if (mysql.connect())
	{
		mysql.update(sql);
	}
}

//返回用户好友列表
vector<User> FriendModel::query(int userid)
{
	char sql[1024] = { 0 };
	//使用联合主键方式查询的SQL语句
	sprintf(sql, "select a.id, a.name, a.state from user a inner join Friend b on b.friendid = a.id where b.userid = %d", userid);

	vector<User> vec;
	MySQL mysql;
	if (mysql.connect())
	{
		MYSQL_RES* res = mysql.query(sql);//接受查询结果
		if (res != nullptr)
		{
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(res)) != nullptr)
			{
				User user;
				user.setId(atoi(row[0]));
				user.setName(row[1]);
				user.setState(row[2]);
				vec.push_back(user);
			}
			mysql_free_result(res);
			return vec;
		}
	}
	return vec;//没找到返回一个默认值
}