#pragma once

#include "util.hpp"
#include "online.hpp"
#include "table.hpp"

#define BOARD_ROW 15
#define BOARD_COL 15
#define WHITE_CHESS 1
#define BLACK_CHESS 2

// 房间状态
typedef enum
{
    GAME_START,
    GAME_OVER
} room_statu;

class room
{
private:
    uint64_t _room_id;                    // 房间id
    room_statu _statu;                    // 房间状态
    int _user_count;                      // 房间里的用户数量
    uint64_t _white_id;                   // 白棋id
    uint64_t _black_id;                   // 黑棋id
    user_table *_table;                   // 数据库管理
    online *_online;                      // 在线用户管理
    std::vector<std::vector<int>> _board; // 棋盘

private:
    // 单路获胜方法
    bool Once_win(int row, int col, int row_off, int col_off, int color)
    {
        int count = 1;
        // 变化后的位置
        int search_row = row + row_off;
        int search_col = col + col_off;
        // 正向判断
        while (search_row >= 0 && search_row < BOARD_ROW &&
               search_col >= 0 && search_col < BOARD_COL &&
               _board[search_row][search_col] == color)
        {
            // 同色棋子数量++
            count++;
            // 检索位置继续向后偏移
            search_row += row_off;
            search_col += col_off;
        }

        // 反向判断
        search_row = row - row_off;
        search_col = col - col_off;
        while (search_row >= 0 && search_row < BOARD_ROW &&
               search_col >= 0 && search_col < BOARD_COL &&
               _board[search_row][search_col] == color)
        {
            // 同色棋子数量++
            count++;
            // 检索位置继续向后偏移
            search_row -= row_off;
            search_col -= col_off;
        }

        return (count >= 5);
    }

    // 获胜条件方法
    int check_win(int row, int col, int color)
    {
        if (Once_win(row, col, 0, 1, color) ||
            Once_win(row, col, 1, 0, color) ||
            Once_win(row, col, -1, 1, color) ||
            Once_win(row, col, -1, -1, color))
            return color == WHITE_CHESS ? _white_id : _black_id;

        // 没有玩家胜利返回0
        return 0;
    }

public:
    room(uint64_t room_id, user_table *tb_user, online *online_user)
        : _room_id(room_id), _statu(GAME_START), _user_count(0),
          _table(tb_user), _online(online_user),
          _board(BOARD_ROW, std::vector<int>(BOARD_COL, 0))
    {
        LOG(DEG, "%lu号房间创建成功", _room_id);
    }

    ~room()
    {
        LOG(DEG, "%lu号房间销毁成功", _room_id);
    }

    // 获取房间id
    uint64_t id() { return _room_id; }
    // 获取房间状态
    room_statu statu() { return _statu; }
    // 获取房间里的用户数量
    int player_count() { return _user_count; }
    // 添加白棋用户
    void add_white_user(uint64_t uid)
    {
        _white_id = uid;
        _user_count++;
    }
    // 添加黑棋用户
    void add_black_user(uint64_t uid)
    {
        _black_id = uid;
        _user_count++;
    }
    // 获取白棋id
    uint64_t get_white_user() { return _white_id; }
    // 获取黑棋id
    uint64_t get_black_user() { return _black_id; }

public:
    /*处理下棋动作*/
    Json::Value chess_handle(Json::Value &req)
    {
        // 响应
        Json::Value json_resp = req;
        // 判断两个玩家是否都在线，一个不在线，另一方胜利
        uint64_t uid = req["uid"].asUInt64(); // 获取当前操作的用户id
        if (!(_online->is_in_room_game(_white_id)))
        {
            // 如果白棋玩家掉线，黑棋胜利
            json_resp["result"] = true;
            json_resp["reason"] = "对手掉线，本局获胜";
            json_resp["winner"] = (Json::UInt64)_black_id;
            return json_resp;
        }

        if (!(_online->is_in_room_game(_black_id)))
        {
            // 如果黑棋玩家掉线，白棋胜利
            json_resp["result"] = true;
            json_resp["reason"] = "对手掉线，本局获胜";
            json_resp["winner"] = (Json::UInt64)_white_id;
            return json_resp;
        }

        // 获取走棋位置，判断当前走棋是否合理
        int row = req["row"].asInt();
        int col = req["col"].asInt();
        if (_board[row][col] != 0)
        {
            json_resp["result"] = false;
            json_resp["reason"] = "该位置已被占用";
            return json_resp;
        }
        // 若该位置合理则改变该位置数值，首先判断当前用户是白棋或黑棋
        int cur = uid == _white_id ? WHITE_CHESS : BLACK_CHESS;
        _board[row][col] = cur;

        // 判断是否有玩家胜利
        uint64_t winner_id = check_win(row, col, cur);
        if (winner_id != 0)
            json_resp["reason"] = "恭喜获胜";
        json_resp["result"] = true;
        json_resp["winner"] = (Json::UInt64)winner_id;
        return json_resp;
    }

    /*处理聊天动作*/
    Json::Value handle_chat(Json::Value &req)
    {
        Json::Value json_resp = req;

        // 广播消息---返回消息
        json_resp["result"] = true;
        return json_resp;
    }

    /*处理玩家退出房间动作*/
    void handle_exit(uint64_t uid)
    {
        // 如果是下棋中退出，则对方胜利，否则下棋结束了退出，则是正常退出
        Json::Value json_resp;
        if (_statu == GAME_START)
        {
            uint64_t winner_id = (Json::UInt64)(uid == _white_id ? _black_id : _white_id);
            json_resp["optype"] = "put_chess";
            json_resp["result"] = true;
            json_resp["reason"] = "对方掉线，恭喜获胜！";
            json_resp["room_id"] = (Json::UInt64)_room_id;
            json_resp["uid"] = (Json::UInt64)uid;
            json_resp["row"] = -1;
            json_resp["col"] = -1;
            json_resp["winner"] = (Json::UInt64)winner_id;
            uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
            _table->win(winner_id);
            _table->lose(loser_id);
            _statu = GAME_OVER;
            broadcast(json_resp);
        }
        // 房间中玩家数量--
        --_user_count;
        return;
    }

    /*总调用函数，根据请求的头部类型执行对应方法*/
    void handle_request(Json::Value &req)
    {
        // 校验房间号是否匹配
        Json::Value json_resp;
        uint64_t room_id = req["room_id"].asUInt64();
        if (room_id != _room_id)
        {
            json_resp["optype"] = req["optype"].asString();
            json_resp["result"] = false;
            json_resp["reason"] = "房间号不匹配！";
            return broadcast(json_resp);
        }
        // 根据不同的请求类型调用不同的处理函数
        if (req["optype"].asString() == "put_chess")
        {
            json_resp = chess_handle(req);
            if (json_resp["winner"].asUInt64() != 0)
            {
                uint64_t winner_id = json_resp["winner"].asUInt64();
                uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
                _table->win(winner_id);
                _table->lose(loser_id);
                _statu = GAME_OVER;
            }
        }
        else if (req["optype"].asString() == "chat")
        {
            json_resp = handle_chat(req);
        }
        else
        {
            json_resp["optype"] = req["optype"].asString();
            json_resp["result"] = false;
            json_resp["reason"] = "未知请求类型";
        }
        std::string body;
        u_json::serialize(json_resp, body);

        return broadcast(json_resp);
    }

    /*将指定的数据广播给房间中所有玩家*/
    void broadcast(Json::Value &rsp)
    {
        // 对要响应的信息进行序列化，将Json::Value中的数据序列化成为json格式字符串
        std::string body;
        u_json::serialize(rsp, body);

        // 获取房间中所有用户的通信连接并发送响应信息
        ws_t::connection_ptr wconn = _online->get_conn_from_room(_white_id);
        if (wconn.get() != nullptr)
            wconn->send(body);
        else
            LOG(DEG, "房间-白棋玩家连接获取失败");

        ws_t::connection_ptr bconn = _online->get_conn_from_room(_black_id);
        if (bconn.get() != nullptr)
            bconn->send(body);
        else
            LOG(DEG, "房间-黑棋玩家连接获取失败");

        return;
    }
};

using room_ptr = std::shared_ptr<room>;
class room_group
{
private:
    uint64_t _rid;                                 // 房间id 依次增长
    std::mutex _mutex;                             // 互斥锁
    user_table *_table;                            // 数据库管理
    online *_online;                               // 在线用户管理
    std::unordered_map<uint64_t, room_ptr> _rooms; // 房间id与房间关联管理
    std::unordered_map<uint64_t, uint64_t> _users; // 用户id和房间id关联管理

public:
    room_group(user_table *ut, online *om)
        : _rid(1), _table(ut), _online(om)
    {
        LOG(DEG, "房间管理模块初始化成功");
    }

    ~room_group()
    {
        LOG(DEG, "房间管理模块销毁成功");
    }

public:
    /*为两个用户创建房间 返回房间管理指针对象*/
    room_ptr create_room(uint64_t uid1, uint64_t uid2)
    {
        // 判断两个用户是否都在游戏大厅 两个都在才需要创建房间
        if (_online->is_in_hall_game(uid1) == false)
        {
            LOG(DEG, "%lu 该用户不在游戏大厅", uid1);
            return room_ptr();
        }

        if (_online->is_in_hall_game(uid2) == false)
        {
            LOG(DEG, "%lu 该用户不在游戏大厅", uid2);
            return room_ptr();
        }

        // 创建房间
        std::unique_lock<std::mutex> lock(_mutex);
        room_ptr rp(new room(_rid, _table, _online));
        // 添加黑白棋用户
        rp->add_white_user(uid1);
        rp->add_black_user(uid2);

        // 将房间和房间id和用户id相关联并管理
        _rooms.insert(std::make_pair(_rid, rp));
        _users.insert(std::make_pair(uid1, _rid));
        _users.insert(std::make_pair(uid2, _rid));
        _rid++;

        return rp;
    }

    /*通过房间id获取房间信息*/
    room_ptr get_room_by_rid(uint64_t rid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _rooms.find(rid);
        if (it == _rooms.end())
            return room_ptr();

        return it->second;
    }

    /*通过用户ID获取房间信息*/
    room_ptr get_room_by_uid(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        // 通过用户ID获取房间ID
        auto uit = _users.find(uid);
        if (uit == _users.end())
            return room_ptr();

        uint64_t rid = uit->second;
        // 通过房间ID获取房间信息
        auto rit = _rooms.find(rid);
        if (rit == _rooms.end())
            return room_ptr();

        return rit->second;
    }

    /*通过房间ID销毁房间*/
    void remove_room(uint64_t rid)
    {
        // 通过房间ID，获取房间信息
        room_ptr rp = get_room_by_rid(rid);
        if (rp.get() == nullptr)
            return;

        // 通过房间信息，获取房间中所有用户的ID
        uint64_t uid1 = rp->get_white_user();
        uint64_t uid2 = rp->get_black_user();

        // 移除房间管理中的用户信息
        std::unique_lock<std::mutex> lock(_mutex);
        _users.erase(uid1);
        _users.erase(uid2);

        // 移除房间管理信息
        _rooms.erase(rid);
    }

    /*删除房间中指定用户，如果房间中没有用户了，则销毁房间*/
    void remove_room_user(uint64_t uid)
    {
        room_ptr rp = get_room_by_uid(uid);
        if (rp.get() == nullptr)
            return;

        // 处理房间中玩家退出动作
        rp->handle_exit(uid);

        // 房间中没有玩家了，则销毁房间
        if (rp->player_count() == 0)
            remove_room(rp->id());
            
        return;
    }
};