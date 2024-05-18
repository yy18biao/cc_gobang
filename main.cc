#include "server.hpp"

#define HOST "127.0.0.1"
#define PORT 3306
#define USER "root"
#define PASS "123456"
#define DBNAME "c_gobang"

int main()
{
    server _server(HOST, USER, PASS, DBNAME, PORT);
    _server.start(8000);
    return 0;
}