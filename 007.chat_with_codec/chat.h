#pragma once

#include <stdint.h>
#include <unordered_map>

enum class MsgType : uint32_t {
	// 系统消息
	msg,
	// 登录
	login,
	// 登录结果
	login_result,
	// 查询所有活跃（成员数大于0）的房间
	query_all_live_rooms,
	// 房间列表
	rooms,
	// 查询房间信息
	get_room_info,
	// 房间信息
	room,
	// 加入房间
	join,
	// 加入成功
	joined,
	// 离开房间
	leave,
	// 离开成功
	leaved,
	// 全系统广播
	broadcast,
	// 房间内广播
	groupcast,
};

struct MsgHeader {
	// Msg.data 长度
	uint32_t len = 0;
	MsgType type = MsgType::join;
};

static constexpr int MSG_HEADER_LEN = sizeof(MsgHeader);

struct Msg {
	MsgHeader header = {};
	char* data = nullptr;
};

struct Client {
	int fd = -1;
	bufferevent* bev = nullptr;

	// name 为空表示未注册，否则已注册
	std::string name = {};
	// room 为-1表示未加入房间，否则已加入
	int room = -1;
};

struct Room {
	int id = -1;
	// key:fd value:client
	std::unordered_map<int, Client*> clients = {};
};
