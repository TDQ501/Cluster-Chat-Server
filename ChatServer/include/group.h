#pragma once
#pragma once
#include <string>
#include <vector>
#include "groupuser.h"
using namespace std;

class Group
{
public:
	Group(int id = -1, string name = "", string desc = "")
	{
		this->name = name;
		this->id = id;
		this->desc = desc;
	}

	//一些接口可以用来赋值
	void setId(int id) { this->id = id; }
	void setName(string name) { this->name = name; }
	void setDesc(string desc) { this->desc = desc; }

	int getId() { return this->id; }
	string getName() { return this->name; }
	string getDesc() { return this ->desc; }
	vector<GroupUser>& getUsers() { return this->users;}

private:
	int id;
	string name;
	string desc;
	vector<GroupUser> users;
};