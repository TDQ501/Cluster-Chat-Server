#include <offlinemessagemodel.h>
#include "db.h"

//存储用户的离线消息
void OfflineMessageModel::insert(int userid, string msg)
{
	char sql[1024] = { 0 };
	sprintf(sql, "insert into OfflineMessage value(%d,'%s')", userid, msg.c_str());

	MySQL mysql;
	if (mysql.connect())
	{
		mysql.update(sql);
	}
}

//删除用户的离线消息
void OfflineMessageModel::remove(int userid)
{
	char sql[1024] = { 0 };
	sprintf(sql, "delete from OfflineMessage where userId=%d", userid);

	MySQL mysql;
	if (mysql.connect())
	{
		mysql.update(sql);
	}
}

//查询用户的离线消息
vector<string> OfflineMessageModel::query(int userid)
{
	char sql[1024] = { 0 };
	sprintf(sql, "select message from OfflineMessage where userId =%d", userid);//只需要message信息 所以不使用select * from

	vector<string> vec;
	MySQL mysql;
	if (mysql.connect())
	{
		MYSQL_RES* res = mysql.query(sql);//接受查询结果
		if (res != nullptr)
		{
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(res)) != nullptr)
			{
				vec.push_back(row[0]);
			}
			mysql_free_result(res);
			return vec;
		}
	}
	return vec;//没找到返回一个默认值
}