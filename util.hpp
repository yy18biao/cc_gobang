#pragma once

#include "log.hpp"
#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>
#include <string>
#include <sstream>
#include <fstream>
#include <memory>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> ws_t;

class u_mysql
{
public:
    // 创建mysql句柄、连接、设置字符集
    static MYSQL *create(const std::string &host,
                         const std::string &username,
                         const std::string &password,
                         const std::string &dbname,
                         uint16_t port = 3306)
    {
        // 创建句柄
        MYSQL *mysql = mysql_init(nullptr);
        if (mysql == nullptr)
        {
            LOG(ERR, "mysql句柄创建失败");
            return nullptr;
        }

        // 连接
        if (mysql_real_connect(mysql, host.c_str(), username.c_str(), password.c_str(), dbname.c_str(), port, nullptr, 0) == nullptr)
        {
            LOG(ERR, "连接mysql服务器失败 : %s", mysql_error(mysql));
            mysql_close(mysql);
            return nullptr;
        }

        // 设置字符集
        if (mysql_set_character_set(mysql, "utf8") != 0)
        {
            LOG(ERR, "设置mysql字符集失败 : %s", mysql_error(mysql));
            mysql_close(mysql);
            return NULL;
        }

        return mysql;
    }

    // 执行sql语句
    static bool mysql_exec(MYSQL *mysql, const std::string &sql)
    {
        int ret = mysql_query(mysql, sql.c_str());
        if (ret != 0)
        {
            LOG(DEG, "执行sql语句(%s)失败: %s\n", sql.c_str(), mysql_error(mysql));
            return false;
        }
        return true;
    }

    // 销毁mysql句柄
    static void mysql_destroy(MYSQL *mysql)
    {
        if (mysql != nullptr)
            mysql_close(mysql);

        return;
    }
};

class u_json
{
public:
    // 序列化
    // 传入value对象，序列化后赋给输入输出参数str
    static bool serialize(Json::Value root, std::string &str)
    {
        // 实例化一个 StreamWriteBuilder 对象
        Json::StreamWriterBuilder jsb;

        // 通过 StreamWriteBuilder 类对象生产一个 StreamWrite 对象
        std::unique_ptr<Json::StreamWriter> jsw(jsb.newStreamWriter());

        // 使用 StreamWrite 对象，对Json::Values中的数据序列化
        // 创建字符串流对象接收反序列化后的结果
        std::stringstream ss;
        int res = jsw->write(root, &ss);
        if (res != 0)
        {
            LOG(ERR, "序列化失败");
            return false;
        }

        str = ss.str();
        return true;
    }

    // 反序列化
    // 传入原序列化的数据，将反序列化后的数据赋给输入输出型参数value对象
    static bool unserialize(const std::string &str, Json::Value &root)
    {
        // 实例化一个CharReaderBuilder工厂类对象
        Json::CharReaderBuilder jcr;

        // 使用CharReaderBuilder工厂类生产一个CharReader对象
        std::unique_ptr<Json::CharReader> jcb(jcr.newCharReader());

        // 使用CharReader对象进行json格式字符串str的反序列化
        bool res = jcb->parse(str.c_str(), str.c_str() + str.size(), &root, nullptr);
        if (!res)
        {
            LOG(ERR, "反序列化失败");
            return false;
        }

        return true;
    }
};

class u_file
{
public:
    static bool read(const std::string &filename, std::string &body)
    {
        // 打开文件
        std::ifstream ifs(filename, std::ios::binary);
        if (ifs.is_open() == false)
        {
            LOG(ERR, "%s 打开文件失败!!", filename.c_str());
            return false;
        }

        // 获取文件大小 利用偏移量计算
        size_t fsize = 0;
        ifs.seekg(0, std::ios::end);
        fsize = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        // 改变body大小以便存储所有的文件数据
        body.resize(fsize);

        // 将文件所有数据读取出来
        ifs.read(&body[0], fsize);
        if (ifs.good() == false)
        {
            LOG(ERR, "读取 %s 该文件失败!", filename.c_str());
            ifs.close();
            return false;
        }

        // 关闭文件
        ifs.close();
        return true;
    }
};

class u_string
{
public:
    static int split(const std::string &in, const std::string &sep, std::vector<std::string> &arr)
    {
        arr.clear();
        int pos = 0;
        int index = 0;
        while (index < in.size())
        {
            pos = in.find(sep, index);

            // 不存在将整个字符串提取
            if (pos == std::string::npos)
            {
                arr.push_back(in.substr(index));
                break;
            }

            // 存在即提取到分隔符的字符串
            if (pos == index)
            {
                index = pos + sep.size();
                continue;
            }

            arr.push_back(in.substr(index, pos - index));
            index = pos + sep.size();
        }

        return arr.size();
    }
};