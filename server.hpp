#pragma once

#include "match.hpp"
#include "session.hpp"
#include <uuid/uuid.h>

#define WWWROOT "./wwwroot/"

class server
{
private:
    std::string _web_root; // 静态资源根目录 ./wwwroot/
    ws_t _wssrv;
    user_table _table;
    online _online;
    room_group _rooms;
    matcher _mm;
    session_group _session;
    bool _file_blag = false;
    std::string _filename;
    uint64_t _file_size;

private:
    // http响应返回
    void http_resp(ws_t::connection_ptr &conn, bool result,
                   websocketpp::http::status_code::value code, const std::string &reason)
    {
        Json::Value resp_json;
        resp_json["result"] = result;
        resp_json["reason"] = reason;
        std::string resp_body;
        u_json::serialize(resp_json, resp_body);

        conn->set_status(code);
        conn->set_body(resp_body);
        conn->append_header("Content-Type", "application/json");
        return;
    }

    // websocket响应返回
    void ws_resp(ws_t::connection_ptr conn, Json::Value &resp)
    {
        std::string body;
        u_json::serialize(resp, body);
        conn->send(body);
    }

    // 从cookie中获取当前ssid
    bool get_cookie_ssid(const std::string &cookie_str, const std::string &key, std::string &val)
    {
        // Cookie: SSID=XXX; path=/;
        std::string sep = "; ";
        std::vector<std::string> cookie_arr;
        u_string::split(cookie_str, sep, cookie_arr);
        for (auto str : cookie_arr)
        {
            // 对单个cookie字符串，以 = 为间隔进行分割，得到key和val
            std::vector<std::string> tmp_arr;
            u_string::split(str, "=", tmp_arr);
            if (tmp_arr.size() != 2)
                continue;

            if (tmp_arr[0] == key)
            {
                val = tmp_arr[1];
                return true;
            }
        }
        return false;
    }

    // 根据cookie信息获取当前session指针对象
    session_ptr get_session_by_cookie(ws_t::connection_ptr conn)
    {
        Json::Value err_resp;
        // =获取请求信息中的Cookie，从Cookie中获取ssid
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty())
        {
            // 如果没有cookie，返回错误：没有cookie信息，让客户端重新登录
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "没有找到cookie信息，需要重新登录";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }

        // 从cookie中取出ssid
        std::string ssid_str;
        bool ret = get_cookie_ssid(cookie_str, "SSID", ssid_str);
        if (ret == false)
        {
            // cookie中没有ssid，返回错误：没有ssid信息，让客户端重新登录
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "没有找到SSID信息，需要重新登录";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }

        // 在session管理中查找对应的会话信息
        session_ptr ssp = _session.get_session_by_ssid(std::stol(ssid_str));
        if (ssp.get() == nullptr)
        {
            // 没有找到session，则认为登录已经过期，需要重新登录
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "没有找到session信息，需要重新登录";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }

        return ssp;
    }

private:
    // 静态资源请求的处理
    void file_handler(ws_t::connection_ptr &conn, const std::string &uri)
    {
        // 获取到请求uri资源路径
        std::string body;
        Json::Value resp_json;
        // 组合出文件的实际路径   相对根目录 + uri
        std::string realpath = _web_root + uri;
        // 如果请求的是个目录，设置默认路径
        if (realpath.back() == '/')
            realpath += "login.html";

        // 文件不存在，读取文件内容失败，返回404
        if (!u_file::read(realpath, body))
        {
            realpath = _web_root + "/404.html";
            u_file::read(realpath, body);
            conn->set_status(websocketpp::http::status_code::not_found);
            conn->set_body(body);
            return;
        }

        // 设置响应正文
        conn->set_status(websocketpp::http::status_code::ok);
        conn->set_body(body);
    }

    // 注册功能
    void reg(ws_t::connection_ptr &conn)
    {
        websocketpp::http::parser::request req = conn->get_request();

        // 获取到请求正文
        std::string req_body = conn->get_request_body();

        // 对正文进行json反序列化，得到用户名和密码
        Json::Value login_info;
        bool ret = u_json::unserialize(req_body, login_info);
        if (ret == false)
        {
            LOG(DEG, "反序列化注册信息失败");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请求的正文格式错误");
        }

        // 进行数据库的用户新增操作
        if (login_info["username"].isNull() || login_info["password"].isNull())
        {
            LOG(DEG, "注册时用户名密码不完整");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请输入用户名/密码");
        }

        ret = _table.add_user(login_info);
        if (ret == false)
        {
            LOG(DEG, "注册时向数据库插入数据失败");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "用户名已经被占用!");
        }

        //  如果成功了，则返回200
        return http_resp(conn, true, websocketpp::http::status_code::ok, "注册用户成功");
    }

    // 登录功能
    void login(ws_t::connection_ptr &conn)
    {
        // 获取请求正文，并进行json反序列化，得到用户名和密码
        std::string req_body = conn->get_request_body();

        Json::Value login_info;

        if (!u_json::unserialize(req_body, login_info))
        {
            std::cout << 1 << std::endl;
            LOG(DEG, "登录时反序列化登录信息失败");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请求的正文格式错误");
        }

        // 校验正文完整性，进行数据库的用户信息验证
        if (login_info["username"].isNull() || login_info["password"].isNull())
        {
            std::cout << 2 << std::endl;
            LOG(DEG, "登录时用户名密码不完整");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请输入用户名/密码");
        }

        if (!_table.login(login_info))
        {
            std::cout << 3 << std::endl;
            // 如果验证失败，则返回400
            LOG(DEG, "登录时用户名密码错误");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "用户名密码错误");
        }

        // 如果验证成功，给客户端创建session
        uint64_t uid = login_info["id"].asUInt64();
        session_ptr ssp = _session.create_session(uid, LOGIN);
        if (ssp.get() == nullptr)
        {
            std::cout << 4 << std::endl;
            LOG(DEG, "登录创建会话失败");
            return http_resp(conn, false, websocketpp::http::status_code::internal_server_error, "创建会话失败");
        }
        _session.set_session_time(ssp->ssid(), TIME_OVER);

        // 设置响应头部：Set-Cookie,将sessionid通过cookie返回
        std::string cookie_ssid = "SSID=" + std::to_string(ssp->ssid());
        conn->append_header("Set-Cookie", cookie_ssid);
        return http_resp(conn, true, websocketpp::http::status_code::ok, "登录成功");
    }

    // 用户信息获取功能
    void info(ws_t::connection_ptr &conn)
    {
        // 获取请求信息中的Cookie，从Cookie中获取ssid
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty())
        {
            // 如果没有cookie，返回错误：没有cookie信息，让客户端重新登录
            return http_resp(conn, true, websocketpp::http::status_code::bad_request, "找不到cookie信息，请重新登录");
        }

        // 从cookie中取出ssid
        std::string ssid_str;
        if (!get_cookie_ssid(cookie_str, "SSID", ssid_str))
        {
            // cookie中没有ssid，返回错误：没有ssid信息，让客户端重新登录
            return http_resp(conn, true, websocketpp::http::status_code::bad_request, "找不到ssid信息，请重新登录");
        }

        // 在session管理中查找对应的会话信息
        session_ptr ssp = _session.get_session_by_ssid(std::stol(ssid_str));
        if (ssp.get() == nullptr)
        {
            // 没有找到session，则认为登录已经过期，需要重新登录
            return http_resp(conn, true, websocketpp::http::status_code::bad_request, "登录过期，请重新登录");
        }

        // 从数据库中取出用户信息，进行序列化发送给客户端
        uint64_t uid = ssp->get_user();
        Json::Value user_info;
        if (!_table.select_by_userid(uid, user_info))
        {
            // 获取用户信息失败，返回错误：找不到用户信息
            return http_resp(conn, true, websocketpp::http::status_code::bad_request, "找不到用户信息，请重新登录");
        }

        std::string body;
        u_json::serialize(user_info, body);
        conn->set_body(body);
        conn->append_header("Content-Type", "application/json");
        conn->set_status(websocketpp::http::status_code::ok);

        // 刷新session的过期时间
        _session.set_session_time(ssp->ssid(), TIME_OVER);
    }

    // websocket连接http回调函数入口
    void http_callback(websocketpp::connection_hdl hdl)
    {
        ws_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string method = req.get_method();
        std::string uri = req.get_uri();

        // 根据方法和uri执行相对应的方法
        if (method == "POST" && uri == "/reg")
            return reg(conn);
        else if (method == "POST" && uri == "/login")
            return login(conn);
        else if (method == "GET" && uri == "/info")
            return info(conn);
        else
            return file_handler(conn, uri);
    }

private:
    // 游戏大厅长连接成功
    void wsopen_game_hall(ws_t::connection_ptr conn)
    {
        // 游戏大厅长连接建立成功
        Json::Value resp_json;
        // 登录验证--判断当前客户端是否已经成功登录
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;

        // 判断当前客户端是否是重复登录
        if (_online.is_in_hall_game(ssp->get_user()) || _online.is_in_room_game(ssp->get_user()))
        {
            resp_json["optype"] = "hall_ready";
            resp_json["reason"] = "玩家重复登录！";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }

        // 将当前客户端以及连接加入到游戏大厅
        _online.add_hall_game(ssp->get_user(), conn);

        // 给客户端响应游戏大厅连接建立成功
        resp_json["optype"] = "hall_ready";
        resp_json["result"] = true;
        ws_resp(conn, resp_json);

        // 将session设置为永久存在
        _session.set_session_time(ssp->ssid(), TIME_FOREVER);
    }

    // 游戏房间长连接成功
    void wsopen_game_room(ws_t::connection_ptr conn)
    {
        Json::Value resp_json;
        // 获取当前客户端的session
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;

        // 当前用户是否已经在在线用户管理的游戏房间或者游戏大厅中
        if (_online.is_in_hall_game(ssp->get_user()) || _online.is_in_room_game(ssp->get_user()))
        {
            resp_json["optype"] = "room_ready";
            resp_json["reason"] = "玩家重复登录！";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }

        // 判断当前用户是否已经创建好了房间 --- 房间管理
        room_ptr rp = _rooms.get_room_by_uid(ssp->get_user());
        if (rp.get() == nullptr)
        {
            resp_json["optype"] = "room_ready";
            resp_json["reason"] = "没有找到玩家的房间信息";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }

        // 将当前用户添加到在线用户管理的游戏房间中
        _online.add_room_game(ssp->get_user(), conn);

        // 将会话设置为永久存在
        _session.set_session_time(ssp->ssid(), TIME_FOREVER);

        // 获取当前用户信息
        Json::Value now_user;
        _table.select_by_userid(ssp->get_user(), now_user);

        //  编写返回数据
        resp_json["optype"] = "room_ready";
        resp_json["result"] = true;
        resp_json["room_id"] = (Json::UInt64)rp->id();
        resp_json["uid"] = (Json::UInt64)ssp->get_user();
        resp_json["white_id"] = (Json::UInt64)rp->get_white_user();
        resp_json["black_id"] = (Json::UInt64)rp->get_black_user();
        resp_json["username"] = now_user["username"];
        resp_json["score"] = now_user["score"];
        resp_json["win_count"] = now_user["win_count"];
        resp_json["mphoto"] = now_user["photo"];
        resp_json["win_rate"] = now_user["win_rate"];

        // 获取对手信息
        uint64_t hid = ssp->get_user() == rp->get_white_user() ? rp->get_black_user() : rp->get_white_user();
        _table.select_by_userid(hid, now_user);
        resp_json["hname"] = now_user["username"];
        resp_json["hscore"] = now_user["score"];
        resp_json["hwin_count"] = now_user["win_count"];
        resp_json["hphoto"] = now_user["photo"];
        resp_json["hwin_rate"] = now_user["win_rate"];

        return ws_resp(conn, resp_json);
    }

    void wsopen_callback(websocketpp::connection_hdl hdl)
    {
        // websocket长连接建立成功之后的处理函数
        ws_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();

        if (uri == "/hall")
        {
            // 建立了游戏大厅的长连接
            return wsopen_game_hall(conn);
        }
        else if (uri == "/room")
        {
            // 建立了游戏房间的长连接
            return wsopen_game_room(conn);
        }
    }

private:
    // 游戏大厅长连接关闭
    void wsclose_game_hall(ws_t::connection_ptr conn)
    {
        // 判断当前客户端是否已经成功登录
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;

        // 将玩家从游戏大厅中移除
        _online.erase_hall_games(ssp->get_user());

        // 将session恢复生命周期的管理，设置定时销毁
        _session.set_session_time(ssp->ssid(), TIME_OVER);
    }

    // 游戏房间长连接关闭
    void wsclose_game_room(ws_t::connection_ptr conn)
    {
        // 获取会话信息，识别客户端
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;

        // 将玩家从在线用户管理中移除
        _online.erase_room_games(ssp->get_user());

        // 将session回复生命周期的管理，设置定时销毁
        _session.set_session_time(ssp->ssid(), TIME_OVER);

        // 将玩家从游戏房间中移除
        _rooms.remove_room_user(ssp->get_user());
    }

    // websocket连接关闭回调函数
    void wsclose_callback(websocketpp::connection_hdl hdl)
    {
        // websocket连接断开前的处理
        ws_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();

        if (uri == "/hall")
        {
            // 关闭了游戏大厅的长连接
            return wsclose_game_hall(conn);
        }
        else if (uri == "/room")
        {
            // 关闭了游戏房间的长连接
            return wsclose_game_room(conn);
        }
    }

private:
    //  游戏大厅消息回调函数
    void wsmsg_game_hall(ws_t::connection_ptr conn, ws_t::message_ptr msg)
    {
        Json::Value resp_json;
        std::string resp_body;

        // 身份验证，当前客户端到底是哪个玩家
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;

        // 获取请求信息
        std::string req_body = msg->get_payload();

        // 判断是否为文件传输
        if (_file_blag)
        {
            // 打开文件
            std::ofstream outFile("./wwwroot/img/" + _filename, std::ios::out | std::ios::binary);

            if (outFile.is_open())
            {
                // 写入数据
                outFile.write(req_body.c_str(), req_body.size());
                // 关闭文件
                outFile.close();
                // 更新用户数据库信息
                _table.update_photo(_filename, ssp->get_user());
                // 编写响应
                resp_json["optype"] = "file_success";
                resp_json["result"] = true;
                // 复原标志
                _file_blag = false;
                _filename.clear();
                LOG(DEG, "文件保存成功");
                // 发送响应
                return ws_resp(conn, resp_json);
            }
            else
                return LOG(ERR, "文件保存失败");
        }

        // 反序列化请求数据
        Json::Value req_json;
        if (!u_json::unserialize(req_body, req_json))
        {
            resp_json["result"] = false;
            resp_json["reason"] = "请求信息解析失败";
            return ws_resp(conn, resp_json);
        }

        // 对于请求进行处理
        if (!req_json["optype"].isNull() && req_json["optype"].asString() == "file")
        {
            // 如果为file说明此时的请求正文里的是头像文件的信息
            _file_blag = true;
            _filename = req_json["filename"].asString() + req_json["fileExtension"].asString();
            return;
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "friends")
        {
            uint64_t uid = ssp->get_user();

            Json::Value root;
            root = Json::arrayValue;
            Json::Value one_value;
            one_value["optype"] = "friends";
            root.append(one_value);
            std::vector<std::string> friends;
            _table.select_friends_by_userid(uid, friends);
            for (auto &id : friends)
            {
                Json::Value user_info;
                uid = std::stoi(id);
                _table.select_by_userid(uid, user_info);
                root.append(user_info);
            }

            Json::StreamWriterBuilder writer;
            std::string json_str;
            std::unique_ptr<Json::StreamWriter> streamWriter(writer.newStreamWriter());
            std::stringstream ss;
            streamWriter->write(root, &ss);
            json_str = ss.str();
            conn->send(json_str);

            return;
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "user_search")
        {
            std::string username = req_json["username"].asString();

            Json::Value root;
            root = Json::arrayValue;
            Json::Value one_value;
            one_value["optype"] = "friends_search";
            root.append(one_value);
            std::vector<std::string> users;
            _table.select_user_by_like_name(username, users);
            for (auto &id : users)
            {
                Json::Value user_info;
                uint64_t uid = std::stoi(id);
                _table.select_by_userid(uid, user_info);
                root.append(user_info);
            }

            Json::StreamWriterBuilder writer;
            std::string json_str;
            std::unique_ptr<Json::StreamWriter> streamWriter(writer.newStreamWriter());
            std::stringstream ss;
            streamWriter->write(root, &ss);
            json_str = ss.str();
            conn->send(json_str);

            return;
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "apply_select")
        {
            uint64_t uid = ssp->get_user();

            Json::Value root;
            root = Json::arrayValue;
            Json::Value one_value;
            one_value["optype"] = "apply_select";
            root.append(one_value);
            std::vector<std::string> users;
            _table.select_apply_friends(uid, users);
            for (auto &id : users)
            {
                Json::Value user_info;
                uint64_t uid = std::stoi(id);
                _table.select_by_userid(uid, user_info);
                root.append(user_info);
            }

            Json::StreamWriterBuilder writer;
            std::string json_str;
            std::unique_ptr<Json::StreamWriter> streamWriter(writer.newStreamWriter());
            std::stringstream ss;
            streamWriter->write(root, &ss);
            json_str = ss.str();
            conn->send(json_str);

            return;
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "apply_friend")
        {
            uint64_t uid = ssp->get_user();
            if (!_table.select_by_username(req_json["username"].asCString(), resp_json))
            {
                resp_json["optype"] = "apply_error";
                resp_json["result"] = true;
                resp_json["reason"] = "获取用户信息失败";
                return ws_resp(conn, resp_json);
            }
            uint64_t hid = resp_json["id"].asInt64();

            if (_table.is_friend(uid, hid) > 0)
            {
                resp_json["optype"] = "apply_error";
                resp_json["result"] = true;
                resp_json["reason"] = "该用户已是好友/已发送申请";
                return ws_resp(conn, resp_json);
            }

            if (_table.is_friend(uid, hid) == -1)
            {
                resp_json["optype"] = "apply_error";
                resp_json["result"] = true;
                resp_json["reason"] = "获取用户信息失败";
                return ws_resp(conn, resp_json);
            }

            if (!_table.apply_friends(uid, hid))
            {
                resp_json["optype"] = "apply_error";
                resp_json["result"] = true;
                resp_json["reason"] = "申请添加好友失败";
                return ws_resp(conn, resp_json);
            }

            resp_json["optype"] = "apply_success";
            resp_json["result"] = true;
            return ws_resp(conn, resp_json);
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "pass_friend")
        {
            uint64_t uid = ssp->get_user();

            if (!_table.select_by_username(req_json["username"].asCString(), resp_json))
            {
                resp_json["optype"] = "pass_error";
                resp_json["result"] = true;
                resp_json["reason"] = "获取用户信息失败";
                return ws_resp(conn, resp_json);
            }
            uint64_t hid = resp_json["id"].asInt64();

            if (!_table.pass_friends(uid, hid))
            {
                resp_json["optype"] = "pass_error";
                resp_json["result"] = true;
                resp_json["reason"] = "同意好友申请失败";
                return ws_resp(conn, resp_json);
            }

            resp_json["optype"] = "pass_success";
            resp_json["result"] = true;
            return ws_resp(conn, resp_json);
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "refuse_friend")
        {
            uint64_t uid = ssp->get_user();

            if (!_table.select_by_username(req_json["username"].asCString(), resp_json))
            {
                resp_json["optype"] = "refuse_error";
                resp_json["result"] = true;
                resp_json["reason"] = "获取用户信息失败";
                return ws_resp(conn, resp_json);
            }
            uint64_t hid = resp_json["id"].asInt64();

            if (!_table.refuse_friends(uid, hid))
            {
                resp_json["optype"] = "refuse_error";
                resp_json["result"] = true;
                resp_json["reason"] = "拒绝好友申请失败";
                return ws_resp(conn, resp_json);
            }

            resp_json["optype"] = "refuse_success";
            resp_json["result"] = true;
            return ws_resp(conn, resp_json);
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_start")
        {
            //  开始对战匹配：通过匹配模块，将用户添加到匹配队列中
            _mm.add(ssp->get_user());
            resp_json["optype"] = "match_start";
            resp_json["result"] = true;
            return ws_resp(conn, resp_json);
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_stop")
        {
            //  停止对战匹配：通过匹配模块，将用户从匹配队列中移除
            _mm.del(ssp->get_user());
            resp_json["optype"] = "match_stop";
            resp_json["result"] = true;
            return ws_resp(conn, resp_json);
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "invite_select")
        {
            // 查询用户是否在线
            if (!_table.select_by_username(req_json["username"].asCString(), resp_json))
            {
                resp_json["optype"] = "invite_ol";
                resp_json["result"] = false;
                return ws_resp(conn, resp_json);
            }

            if (!_online.is_in_hall_game(resp_json["id"].asInt64()))
            {
                resp_json["optype"] = "invite_ol";
                resp_json["result"] = false;
                return ws_resp(conn, resp_json);
            }

            resp_json["optype"] = "invite_ol";
            resp_json["result"] = true;
            resp_json["hid"] = resp_json["id"];
            resp_json["uid"] = (Json::UInt64)get_session_by_cookie(conn)->get_user();
            return ws_resp(conn, resp_json);
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "invite_start")
        {
            if (!_online.is_in_hall_game(req_json["uid"].asInt64()) || !_online.is_in_hall_game(req_json["hid"].asInt64()))
            {
                resp_json["result"] = false;
                resp_json["reason"] = "有用户未在线";
                return ws_resp(conn, resp_json);
            }

            // 加入匹配队列
            _mm.add(req_json["uid"].asUInt64(), req_json["hid"].asUInt64());
            // 获取对方连接
            ws_t::connection_ptr wconn = _online.get_conn_from_hall(req_json["hid"].asInt64());
            // 获取当前用户信息
            _table.select_by_userid(req_json["uid"].asInt64(), resp_json);
            resp_json["optype"] = "invite_start";
            resp_json["result"] = true;
            ws_resp(wconn, resp_json);
            resp_json["optype"] = "match_start";
            resp_json["result"] = true;
            return ws_resp(conn, resp_json);
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "invite_success")
        {
            uint64_t id = req_json["id"].asUInt64();
            _mm.handle_friends(id);
            return;
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "invite_fail")
        {
            ws_t::connection_ptr wconn = _online.get_conn_from_hall(req_json["hid"].asInt64());
            resp_json["optype"] = "invite_fail";
            resp_json["result"] = true;
            // ws_resp(conn ,resp_json);
            return ws_resp(wconn, resp_json);
        }

        resp_json["optype"] = "unknow";
        resp_json["reason"] = "请求类型未知";
        resp_json["result"] = false;
        return ws_resp(conn, resp_json);
    }

    // 游戏房间消息回调函数
    void wsmsg_game_room(ws_t::connection_ptr conn, ws_t::message_ptr msg)
    {
        Json::Value resp_json;

        // 获取客户端session，识别客户端身份
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            LOG(DEG, "房间没有找到会话信息");
            return;
        }

        // 获取客户端房间信息
        room_ptr rp = _rooms.get_room_by_uid(ssp->get_user());
        if (rp.get() == nullptr)
        {
            resp_json["optype"] = "unknow";
            resp_json["reason"] = "没有找到玩家的房间信息";
            resp_json["result"] = false;
            LOG(DEG, "房间没有找到玩家房间信息");
            return ws_resp(conn, resp_json);
        }

        // 对消息进行反序列化
        Json::Value req_json;
        std::string req_body = msg->get_payload();
        if (!u_json::unserialize(req_body, req_json))
        {
            resp_json["optype"] = "unknow";
            resp_json["reason"] = "请求解析失败";
            resp_json["result"] = false;
            LOG(DEG, "房间反序列化请求失败");
            return ws_resp(conn, resp_json);
        }
        LOG(DEG, "收到房间请求，开始处理");

        // 根据房间获取到当前用户头像和对手头像
        uint64_t uid = req_json["uid"].asInt64();
        Json::Value now_resp;
        if (!_table.select_by_userid(uid, now_resp))
        {
            resp_json["optype"] = "unknow";
            resp_json["reason"] = "请求解析失败";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }
        req_json["username"] = now_resp["username"];
        req_json["photo"] = now_resp["photo"];

        // 通过房间模块进行消息请求的处理
        return rp->handle_request(req_json);
    }

    void wsmsg_callback(websocketpp::connection_hdl hdl, ws_t::message_ptr msg)
    {
        // websocket长连接通信处理
        ws_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();

        if (uri == "/hall")
            return wsmsg_game_hall(conn, msg);

        else if (uri == "/room")
            return wsmsg_game_room(conn, msg);
    }

public:
    /*进行成员初始化，以及服务器回调函数的设置*/
    server(const std::string &host,
           const std::string &user,
           const std::string &pass,
           const std::string &dbname,
           uint16_t port = 3306,
           const std::string &wwwroot = WWWROOT)
        : _web_root(wwwroot), _table(host, user, pass, dbname, port),
          _rooms(&_table, &_online), _session(&_wssrv), _mm(&_rooms, &_table, &_online)
    {
        _wssrv.set_access_channels(websocketpp::log::alevel::none);
        _wssrv.init_asio();
        _wssrv.set_reuse_addr(true);
        _wssrv.set_http_handler(std::bind(&server::http_callback, this, std::placeholders::_1));
        _wssrv.set_open_handler(std::bind(&server::wsopen_callback, this, std::placeholders::_1));
        _wssrv.set_close_handler(std::bind(&server::wsclose_callback, this, std::placeholders::_1));
        _wssrv.set_message_handler(std::bind(&server::wsmsg_callback, this, std::placeholders::_1, std::placeholders::_2));
    }

    /*启动服务器*/
    void start(int port)
    {
        _wssrv.listen(port);
        _wssrv.start_accept();
        _wssrv.run();
    }
};