#pragma once

#include "util.hpp"
#include "online.hpp"
#include "table.hpp"
#include "room.hpp"
#include <list>
#include <mutex>
#include <condition_variable>

template <class T>
class match_queue
{
private:
    std::mutex _mutex;             // 互斥锁
    std::list<T> _list;            // 链表
    std::condition_variable _cond; // 条件变量

public:
    /*获取元素个数*/
    int size()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.size();
    }

    /*判断是否为空*/
    bool empty()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.empty();
    }

    /*阻塞线程*/
    void wait()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock);
    }

    /*入队数据，并唤醒线程*/
    void push(const T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.push_back(data);
        _cond.notify_all();
    }

    /*出队数据*/
    bool pop(T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_list.empty() == true)
            return false;

        data = _list.front();
        _list.pop_front();
        return true;
    }

    /*移除指定的数据*/
    void remove(T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.remove(data);
    }
};

class matcher
{
private:
    /*四个等级，四条队列*/
    match_queue<uint64_t> _bronze_queue;
    match_queue<uint64_t> _silver_queue;
    match_queue<uint64_t> _gold_queue;
    match_queue<uint64_t> _king_queue;
    /*约战队列*/
    std::unordered_map<uint64_t, uint64_t> _friends;
    /*四条队列，四个线程分别管理*/
    std::thread _th_bronze;
    std::thread _th_silver;
    std::thread _th_gold;
    std::thread _th_king;
    /*房间管理类对象*/
    room_group *_room;
    /*用户数据库管理对象*/
    user_table *_table;
    /*在线用户管理对象*/
    online *_online;

private:
    /*线程总回调方法*/
    void handle_match(match_queue<uint64_t> &mq)
    {
        while (1)
        {
            // 判断队列人数是否大于2，<2则阻塞等待
            while (mq.size() < 2)
                mq.wait();

            // 人数够了，出队两个玩家
            uint64_t uid1, uid2;
            bool ret = mq.pop(uid1);
            if (ret == false)
                continue;

            ret = mq.pop(uid2);
            if (ret == false)
            {
                this->add(uid1);
                continue;
            }

            // 校验两个玩家是否在线，如果有人掉线，则另一个人重新添加入队列
            ws_t::connection_ptr conn1 = _online->get_conn_from_hall(uid1);
            if (conn1.get() == nullptr)
            {
                this->add(uid2);
                continue;
            }
            ws_t::connection_ptr conn2 = _online->get_conn_from_hall(uid2);
            if (conn2.get() == nullptr)
            {
                this->add(uid1);
                continue;
            }

            // 为两个玩家创建房间，并将玩家加入房间中
            room_ptr rp = _room->create_room(uid1, uid2);
            if (rp.get() == nullptr)
            {
                this->add(uid1);
                this->add(uid2);
                continue;
            }

            // 对两个玩家进行响应
            Json::Value resp;
            resp["optype"] = "match_success";
            resp["result"] = true;
            std::string body;
            u_json::serialize(resp, body);
            conn1->send(body);
            conn2->send(body);
        }
    }

    /*四个线程分别封装的线程回调方法*/
    void th_bronze_entry() { return handle_match(_bronze_queue); }
    void th_silver_entry() { return handle_match(_silver_queue); }
    void th_gold_entry() { return handle_match(_gold_queue); }
    void th_king_entry() { return handle_match(_king_queue); }

public:
    matcher(room_group *rm, user_table *ut, online *om)
        : _room(rm), _table(ut), _online(om),
          _th_bronze(std::thread(&matcher::th_bronze_entry, this)),
          _th_silver(std::thread(&matcher::th_silver_entry, this)),
          _th_gold(std::thread(&matcher::th_gold_entry, this)),
          _th_king(std::thread(&matcher::th_king_entry, this))
    {
        LOG(DEG, "游戏匹配模块初始化完毕");
    }

    bool add(uint64_t uid, uint64_t hid)
    {
        _friends.insert(std::make_pair(uid, hid));
    }

    bool add(uint64_t uid)
    {
        // 根据玩家的天梯分数，来判定玩家档次，添加到不同的匹配队列
        // 根据用户ID，获取玩家信息
        Json::Value user;
        bool ret = _table->select_by_userid(uid, user);
        if (ret == false)
        {
            LOG(DEG, "获取玩家:%d 信息失败！！", uid);
            return false;
        }
        int score = user["score"].asInt();
        // 添加到指定的队列中
        if (score < 1000)
            _bronze_queue.push(uid);
        else if (score >= 1000 && score < 2000)
            _silver_queue.push(uid);
        else if (score >= 2000 && score < 3000)
            _gold_queue.push(uid);
        else
            _king_queue.push(uid);

        return true;
    }

    bool del_friends(uint64_t uid)
    {
        _friends.erase(uid);
    }

    bool del(uint64_t uid)
    {
        Json::Value user;
        bool ret = _table->select_by_userid(uid, user);
        if (ret == false)
        {
            LOG(DEG, "获取玩家:%d 信息失败！！", uid);
            return false;
        }
        int score = user["score"].asInt();

        // 移除出指定的队列中
        if (score < 1000)
            _bronze_queue.remove(uid);
        else if (score >= 1000 && score < 2000)
            _silver_queue.remove(uid);
        else if (score >= 2000 && score < 3000)
            _gold_queue.remove(uid);
        else
            _king_queue.remove(uid);

        return true;
    }

    void handle_friends(uint64_t id)
    {
        // 对两个玩家进行响应
        Json::Value resp;

        auto it = _friends.find(id);
        if (it == _friends.end())
            return;

        ws_t::connection_ptr conn1 = _online->get_conn_from_hall(id);
        if (conn1.get() == nullptr)
            return;

        ws_t::connection_ptr conn2 = _online->get_conn_from_hall(it->second);
        if (conn2.get() == nullptr)
        {
            resp["optype"] = "invite_fail";
            resp["result"] = true;
            del_friends(id);
            std::string body;
            u_json::serialize(resp, body);
            conn1->send(body);
            return;
        }

        // 为两个玩家创建房间，并将玩家加入房间中
        room_ptr rp = _room->create_room(id, it->second);
        if (rp.get() == nullptr)
        {
            resp["optype"] = "invite_fail";
            resp["result"] = true;
            del_friends(id);
            std::string body;
            u_json::serialize(resp, body);
            conn1->send(body);
            return;
        }

        resp["optype"] = "match_success";
        resp["result"] = true;
        std::string body;
        u_json::serialize(resp, body);
        conn1->send(body);
        conn2->send(body);
    }
};