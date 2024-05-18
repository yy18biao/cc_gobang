#pragma once

#include "util.hpp"
#include <mutex>
#include <cassert>
#include <cmath>

class user_table
{
private:
    MYSQL *_mysql;
    std::mutex _mutex;

public:
    user_table(const std::string &host,
               const std::string &username,
               const std::string &password,
               const std::string &dbname,
               uint16_t port = 3306)
    {
        _mysql = u_mysql::create(host, username, password, dbname, port);
        assert(_mysql != nullptr);
    }

    ~user_table()
    {
        u_mysql::mysql_destroy(_mysql);
        _mysql = NULL;
    }

public:
    // 新增用户
    bool add_user(Json::Value &user)
    {
#define ADD_USER "insert user values(null, '%s', md5('%s'), 0, 0, 0, 'waiting.jpg');"
        if (user["password"].isNull() || user["username"].isNull())
        {
            LOG(DEG, "注册时用户名/密码为空");
            return false;
        }

        char sql[4096] = {0};
        sprintf(sql, ADD_USER, user["username"].asCString(), user["password"].asCString());
        if (!(u_mysql::mysql_exec(_mysql, sql)))
        {
            LOG(DEG, "注册用户失败");
            return false;
        }

        return true;
    }

    // 登录验证，并返回用户信息
    bool login(Json::Value &user)
    {
#define LOGIN_USER "select id, score, total_count, win_count, photo from user where username='%s' and password=md5('%s');"
        if (user["password"].isNull() || user["username"].isNull())
        {
            LOG(DEG, "登录时用户名/密码为空");
            return false;
        }

        char sql[4096] = {0};
        sprintf(sql, LOGIN_USER, user["username"].asCString(), user["password"].asCString());

        MYSQL_RES *res; // 用于以后获取sql语句查询到的信息

        {
            // 加锁
            std::unique_lock<std::mutex> lock(_mutex);

            // 执行语句
            if (!(u_mysql::mysql_exec(_mysql, sql)))
            {
                LOG(DEG, "验证用户登录失败，执行sql语句失败");
                return false;
            }

            // 接受sql查询数据
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                LOG(DEG, "验证用户登录失败，没有该用户数据!!");
                return false;
            }
        }

        // 提取用户数据各字段
        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            LOG(DEG, "该用户数据不是唯一的!!");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        user["photo"] = row[4];
        mysql_free_result(res); // 释放

        return true;
    }

    // 通过用户名获取用户信息
    bool select_by_username(const std::string &name, Json::Value &user)
    {
#define USER_BY_NAME "select id, score, total_count, win_count, photo from user where username='%s';"
        char sql[4096] = {0};
        sprintf(sql, USER_BY_NAME, name.c_str());
        MYSQL_RES *res; // 用于以后获取sql语句查询到的信息

        {
            // 加锁
            std::unique_lock<std::mutex> lock(_mutex);

            // 执行语句
            if (!(u_mysql::mysql_exec(_mysql, sql)))
            {
                LOG(DEG, "查询用户信息失败，执行sql语句失败");
                return false;
            }

            // 接受sql查询数据
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                LOG(DEG, "查询用户信息失败，没有该用户数据!!");
                return false;
            }
        }

        // 提取用户数据各字段
        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            LOG(DEG, "该用户数据不是唯一的!!");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        user["photo"] = row[4];
        mysql_free_result(res); // 释放

        return true;
    }

    // 通过用户id获取用户信息
    bool select_by_userid(uint64_t id, Json::Value &user)
    {
#define USER_BY_ID "select username, score, total_count, win_count, photo from user where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, USER_BY_ID, id);
        MYSQL_RES *res; // 用于以后获取sql语句查询到的信息

        {
            // 加锁
            std::unique_lock<std::mutex> lock(_mutex);

            // 执行语句
            if (!(u_mysql::mysql_exec(_mysql, sql)))
            {
                LOG(DEG, "查询用户信息失败，执行sql语句失败");
                return false;
            }

            // 接受sql查询数据
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                LOG(DEG, "查询用户信息失败，没有该用户数据!!");
                return false;
            }
        }

        // 提取用户数据各字段
        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            LOG(DEG, "该用户数据不是唯一的!!");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)id;
        user["username"] = row[0];
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        user["photo"] = row[4];
        if (user["total_count"].asInt64() == 0)
            user["win_rate"] = "0%";
        else
        {
            uint64_t a = user["total_count"].asInt64();
            uint64_t b = user["win_count"].asInt64();
            double res = static_cast<double>(b) / a * 100.0;
            uint64_t r = static_cast<uint64_t>(std::llround(res));
            user["win_rate"] = std::to_string(r) + "%";
        }
        mysql_free_result(res); // 释放

        return true;
    }

    // 通过用户名模糊查询获取用户id
    bool select_user_by_like_name(const std::string &username, std::vector<std::string> &user_id)
    {
#define LIKE_NAME "select id from user where username like '%s%s%s';"
        char sql[4096] = {0};
        sprintf(sql, LIKE_NAME, "%", username.c_str(), "%");

        MYSQL_RES *res; // 用于以后获取sql语句查询到的信息

        {
            // 加锁
            std::unique_lock<std::mutex> lock(_mutex);

            // 执行语句
            if (!(u_mysql::mysql_exec(_mysql, sql)))
            {
                LOG(DEG, "查询用户信息失败，执行sql语句失败");
                return false;
            }

            // 接受sql查询数据
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                LOG(DEG, "查询用户信息失败，没有该用户数据!!");
                return false;
            }
        }

        // 提取用户数据各字段
        MYSQL_ROW rows;
        while ((rows = mysql_fetch_row(res)))
        {
            const char *field_value = rows[0] ? rows[0] : "NULL";
            user_id.push_back(std::string(field_value));
        }

        return true;
    }

    // 通过用户id获取用户好友
    bool select_friends_by_userid(uint64_t id, std::vector<std::string> &friends_id)
    {
#define FRIENDS "select friend_id from friends where uid=%d and statu=1;"
        char sql[4096] = {0};
        sprintf(sql, FRIENDS, id);

        MYSQL_RES *res; // 用于以后获取sql语句查询到的信息

        {
            // 加锁
            std::unique_lock<std::mutex> lock(_mutex);

            // 执行语句
            if (!(u_mysql::mysql_exec(_mysql, sql)))
            {
                LOG(DEG, "查询用户信息失败，执行sql语句失败");
                return false;
            }

            // 接受sql查询数据
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                LOG(DEG, "查询用户信息失败，没有该用户数据!!");
                return false;
            }
        }

        // 提取用户数据各字段
        MYSQL_ROW rows;
        while ((rows = mysql_fetch_row(res)))
        {
            const char *field_value = rows[0] ? rows[0] : "NULL";
            friends_id.push_back(std::string(field_value));
        }

        return true;
    }

    // 查询好友申请
    bool select_apply_friends(uint64_t id, std::vector<std::string> &applyid)
    {
#define SELECT_APPLY "select friend_id from friends where uid=%d and statu=2;"
        char sql[4096] = {0};
        sprintf(sql, SELECT_APPLY, id);

        MYSQL_RES *res; // 用于以后获取sql语句查询到的信息

        {
            // 加锁
            std::unique_lock<std::mutex> lock(_mutex);

            // 执行语句
            if (!(u_mysql::mysql_exec(_mysql, sql)))
            {
                LOG(DEG, "查询用户信息失败，执行sql语句失败");
                return false;
            }

            // 接受sql查询数据
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                LOG(DEG, "查询用户信息失败，没有该用户数据!!");
                return false;
            }
        }

        // 提取用户数据各字段
        MYSQL_ROW rows;
        while ((rows = mysql_fetch_row(res)))
        {
            const char *field_value = rows[0] ? rows[0] : "NULL";
            applyid.push_back(std::string(field_value));
        }

        return true;
    }

    // 查看当前两个id是否已经绑定好友
    int is_friend(uint64_t id1, uint64_t id2)
    {
#define IS_FRIEND "select count(friend_id) from friends where uid=%d and friend_id=%d;"
        char sql[4096] = {0};
        sprintf(sql, IS_FRIEND, id1, id2);

        MYSQL_RES *res; // 用于以后获取sql语句查询到的信息

        {
            // 加锁
            std::unique_lock<std::mutex> lock(_mutex);

            // 执行语句
            if (!(u_mysql::mysql_exec(_mysql, sql)))
            {
                LOG(DEG, "查询用户信息失败，执行sql语句失败");
                return -1;
            }

            // 接受sql查询数据
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                LOG(DEG, "查询用户信息失败，没有该用户数据!!");
                return -1;
            }
        }

        // 提取用户数据各字段
        MYSQL_ROW rows = mysql_fetch_row(res);
        const char *field_value = rows[0] ? rows[0] : "NULL";
        std::string resu = std::string(field_value);
        if (resu == "NULL")
            return -1;
        return std::stoi(resu);
    }

    // 好友申请
    bool apply_friends(uint64_t id1, uint64_t id2)
    {
#define APPLY_FRIENDS "insert into friends values(%d, %d, %d);"

        char sql[4096] = {0};
        sprintf(sql, APPLY_FRIENDS, id1, id2, 0);
        char sql2[4096] = {0};
        sprintf(sql2, APPLY_FRIENDS, id2, id1, 2);

        if (!u_mysql::mysql_exec(_mysql, sql) || !u_mysql::mysql_exec(_mysql, sql2))
        {
            LOG(ERR, "添加好友失败!!\n");
            return false;
        }

        return true;
    }

    // 好友通过
    bool pass_friends(uint64_t uid, uint64_t hid)
    {
#define PASS_FRIENDS "update friends set statu=1 where uid=%d and friend_id=%d;"
        char sql[4096] = {0};
        sprintf(sql, PASS_FRIENDS, uid, hid);
        char sql2[4096] = {0};
        sprintf(sql2, PASS_FRIENDS, hid, uid);

        if (!u_mysql::mysql_exec(_mysql, sql) || !u_mysql::mysql_exec(_mysql, sql2))
        {
            LOG(ERR, "同意好友申请失败!!\n");
            return false;
        }

        return true;
    }

    // 好友拒绝
    bool refuse_friends(uint64_t uid, uint64_t hid)
    {
#define REFUSE_FRIENDS "delete from friends where uid=%d and friend_id=%d;"
        char sql[4096] = {0};
        sprintf(sql, REFUSE_FRIENDS, uid, hid);
        char sql2[4096] = {0};
        sprintf(sql2, REFUSE_FRIENDS, hid, uid);

        if (!u_mysql::mysql_exec(_mysql, sql) || !u_mysql::mysql_exec(_mysql, sql2))
        {
            LOG(ERR, "拒绝好友申请失败!!\n");
            return false;
        }

        return true;
    }

    // 修改头像
    bool update_photo(const std::string &photo, uint64_t id)
    {
#define UPDATE_PHOTO "update user set photo='%s' where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, UPDATE_PHOTO, photo.c_str(), id);
        if (!u_mysql::mysql_exec(_mysql, sql))
        {
            LOG(ERR, "修改头像失败!!\n");
            return false;
        }

        return true;
    }

    // 获胜用户加积分 加胜场 加总场次
    bool win(uint64_t id)
    {
#define USER_WIN "update user set score=score+100, total_count=total_count+1, win_count=win_count+1 where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, USER_WIN, id);
        if (!u_mysql::mysql_exec(_mysql, sql))
        {
            LOG(DEG, "修改胜者数据失败!!\n");
            return false;
        }
        return true;
    }

    // 失败用户减积分 加总场次
    bool lose(uint64_t id)
    {
        Json::Value user;
        int upscore = 90;
        if (!select_by_userid(id, user))
            return false;
        if (user["score"].asInt() < 90)
            upscore = user["score"].asInt();

#define USER_LOSE "update user set score=score-%d, total_count=total_count+1 where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, USER_LOSE, upscore, id);
        if (!u_mysql::mysql_exec(_mysql, sql))
        {
            LOG(DEG, "修改败者数据失败!!\n");
            return false;
        }
        return true;
    }
};