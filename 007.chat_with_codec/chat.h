#pragma once

#include <stdint.h>
#include <unordered_map>

enum class MsgType : uint32_t {
	// ϵͳ��Ϣ
	msg,
	// ��¼
	login,
	// ��¼���
	login_result,
	// ��ѯ���л�Ծ����Ա������0���ķ���
	query_all_live_rooms,
	// �����б�
	rooms,
	// ��ѯ������Ϣ
	get_room_info,
	// ������Ϣ
	room,
	// ���뷿��
	join,
	// ����ɹ�
	joined,
	// �뿪����
	leave,
	// �뿪�ɹ�
	leaved,
	// ȫϵͳ�㲥
	broadcast,
	// �����ڹ㲥
	groupcast,
};

struct MsgHeader {
	// Msg.data ����
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

	// name Ϊ�ձ�ʾδע�ᣬ������ע��
	std::string name = {};
	// room Ϊ-1��ʾδ���뷿�䣬�����Ѽ���
	int room = -1;
};

struct Room {
	int id = -1;
	// key:fd value:client
	std::unordered_map<int, Client*> clients = {};
};
