#pragma once

#include "util.hpp"
#include <unordered_map>

typedef enum
{
    UNLOGIN,
    LOGIN
} s_statu;

class session
{
private:
    uint64_t _ssid;      // session标识符
    uint64_t _uid;       // session对应的用户ID
    s_statu _statu;      // 用户状态
    ws_t::timer_ptr _tp; // session关联的定时器

public:
    session(uint64_t ssid)
        : _ssid(ssid)
    {
        LOG(DEG, "会话 %p 被创建", this);
    }
    ~session()
    {
        LOG(DEG, "会话 %p 被释放", this);
    }
    uint64_t ssid() { return _ssid; }                       // 返回session id
    void set_statu(s_statu statu) { _statu = statu; }       // 设置用户状态
    void set_user(uint64_t uid) { _uid = uid; }             // 设置用户id
    uint64_t get_user() { return _uid; }                    // 返回用户id
    bool is_login() { return (_statu == LOGIN); }           // 判断用户是否登录
    void set_timer(const ws_t::timer_ptr &tp) { _tp = tp; } // 设置定时器
    ws_t::timer_ptr &get_timer() { return _tp; }            // 返回定时器
};

#define TIME_OVER 30000
#define TIME_FOREVER -1
using session_ptr = std::shared_ptr<session>;
class session_group
{
private:
    uint64_t _ssid;                                     // session id
    std::mutex _mutex;                                  // 互斥锁
    std::unordered_map<uint64_t, session_ptr> _session; // ssid和session关联管理
    ws_t *_server;                                      // websocket服务器

public:
    session_group(ws_t *srv) : _ssid(1), _server(srv)
    {
        LOG(DEG, "session管理器初始化完毕");
    }
    ~session_group() { LOG(DEG, "session管理器销毁"); }

public:
    // 创建session
    session_ptr create_session(uint64_t uid, s_statu statu)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        session_ptr ssp(new session(_ssid));
        ssp->set_statu(statu);
        ssp->set_user(uid);

        // 添加到管理
        _session.insert(std::make_pair(_ssid, ssp));
        _ssid++;
        return ssp;
    }

    // 新增session到管理
    void add_session(const session_ptr &ssp)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _session.insert(std::make_pair(ssp->ssid(), ssp));
    }

    // 通过ssid获取session信息
    session_ptr get_session_by_ssid(uint64_t ssid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _session.find(ssid);
        if (it == _session.end())
            return session_ptr();

        return it->second;
    }

    // 移除session出管理器
    void remove_session(uint64_t ssid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _session.erase(ssid);
    }

    // 设置session定时器
    void set_session_time(uint64_t ssid, int ms)
    {
        /*
        依赖于websocketpp的定时器
        登录之后 需要在一定时间内销毁session
        在游戏大厅或者游戏房间时 session应该永久保存
        退出游戏大厅或者游戏房间时又应该为一定时间销毁
        */

        /*获取指定session*/
        session_ptr ssp = get_session_by_ssid(ssid);
        if (ssp == nullptr)
            return;

        /*获取当前session的定时器*/
        ws_t::timer_ptr tp = ssp->get_timer();

        /*分情况讨论*/
        if (tp.get() == nullptr && ms == TIME_FOREVER)
        {
            // 当前定时器为永久，设置为永久
            return;
        }
        else if (tp.get() == nullptr && ms != TIME_FOREVER)
        {
            // 当前定时器为永久，设置为指定时间

            // 先定义好定时器，再添加进session
            ws_t::timer_ptr ttp = _server->set_timer(ms, std::bind(&session_group::remove_session, this, ssid));
            ssp->set_timer(ttp);
        }
        else if (tp.get() != nullptr && ms != TIME_FOREVER)
        {
            // 当前定时器不为永久，需要重新设置不为永久的定时器

            // 需要先删除原定时器，定义好新的定时器再添加进去
            // 需要注意的是，定时器的删除函数调用后定时任务并不是立即取消的
            // 而是会执行定时器函数一次，由于执行定时器函数会导致session被删除
            // 因此最后还得添加session回到管理结构中
            // 另外因为cancel虽然会执行一次定时器，但并不是立即执行的
            // 所以如果直接添加session会出现先执行添加再执行定时器的情况
            // 因此并不能直接添加session，而是通过调用定时器的方式去添加才能保证执行添加在后

            // 取消原定时器
            tp->cancel();

            // 添加session回
            _server->set_timer(0, std::bind(&session_group::add_session, this, ssp));

            // 重新定义并添加定时器
            ws_t::timer_ptr newtp = _server->set_timer(ms, std::bind(&session_group::remove_session, this, ssp->ssid()));
            ssp->set_timer(newtp);
        }
        else if (tp.get() != nullptr && ms == TIME_FOREVER)
        {
            // 当前定时器不为永久，需要设置为永久

            // 取消原定时器
            tp->cancel();

            // 添加session回
            _server->set_timer(0, std::bind(&session_group::add_session, this, ssp));

            // 永久就代表着没有定时器即可，因此可以设置定时器为空
            ssp->set_timer(ws_t::timer_ptr());
        }
    }
};