#pragma once

#include "util.hpp"
#include <unordered_map>
#include <mutex>

class online
{
private:
    std::mutex _mutex;
    // 建立游戏大厅用户的用户ID与通信连接的关系
    std::unordered_map<uint64_t, ws_t::connection_ptr> _hall_user;
    // 建立游戏房间用户的用户ID与通信连接的关系
    std::unordered_map<uint64_t, ws_t::connection_ptr> _room_user;

public:
    // websocket连接建立的时候加入游戏大厅在线用户管理
    void add_hall_game(uint64_t uid, ws_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_user.insert(std::make_pair(uid, conn));
    }

    // websocket连接建立的时候加入游戏房间在线用户管理
    void add_room_game(uint64_t uid, ws_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _room_user.insert(std::make_pair(uid, conn));
    }

    // websocket连接断开的时候移除游戏大厅在线用户管理
    void erase_hall_games(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_user.erase(uid);
    }

    // websocket连接建立的时候移除游戏房间在线用户管理
    void erase_room_games(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _room_user.erase(uid);
    }

    // 判断当前指定用户是否在游戏大厅
    bool is_in_hall_game(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _hall_user.find(uid);
        if (it == _hall_user.end())
            return false;
        return true;
    }

    // 判断当前指定用户是否在游戏房间
    bool is_in_room_game(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _room_user.find(uid);
        if (it == _room_user.end())
            return false;
        return true;
    }

    // 通过用户ID在游戏大厅用户管理中获取对应的通信连接
    ws_t::connection_ptr get_conn_from_hall(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _hall_user.find(uid);
        if (it == _hall_user.end())
            return ws_t::connection_ptr();

        return it->second;
    }

    // 通过用户ID在游戏房间用户管理中获取对应的通信连接
    ws_t::connection_ptr get_conn_from_room(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _room_user.find(uid);
        if (it == _room_user.end())
            return ws_t::connection_ptr();

        return it->second;
    }
};