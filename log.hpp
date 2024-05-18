#pragma once

#include <iostream>
#include <cstdio>
#include <ctime>
#include <cstdarg>

#define DEG 0             // 调试级别
#define ERR 1             // 错误级别
#define DEAFULT_LEVEL DEG // 默认级别

void LOG(int level, const char *format, ...)
{
    do
    {
        // 如果该日志等级比默认等级要小则不需要输出
        if (level < DEAFULT_LEVEL)
            break;

        // 获取时间戳
        time_t t = time(nullptr);
        // 将时间戳转换格式 %H:%M:%S
        struct tm *tt = localtime(&t);
        char time_buf[32];
        strftime(time_buf, 31, "%H:%M:%S", tt);

        // 将可变参数转换为可输出字符串
        va_list age;
        va_start(age, format);
        char line[1024];
        vsnprintf(line, sizeof(line), format, age);

        FILE *DEG_log = fopen("DEG_log.txt", "a");
        FILE *ERR_log = fopen("ERR_log.txt", "a");

        if (ERR_log && DEG_log)
        {
            FILE *tmp;
            if (level == DEG)
                tmp = DEG_log;
            if (level == ERR)
                tmp = ERR_log;

            if (tmp)
                fprintf(tmp, "[%s %s:%d] %s\n", time_buf, __FILE__, __LINE__, line);

            fclose(DEG_log);
            fclose(ERR_log);
        }
    } while (0);
}