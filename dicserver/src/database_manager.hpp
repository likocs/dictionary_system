#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <iostream>
#include <string>
#include <iomanip>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <sys/sendfile.h>

#include <sqlite3.h> //数据库的头文件
#include <string>    //字符串头文件
#include <mutex>     //互斥锁头文件
#include <memory>    //智能指针所在的头文件
using namespace std;
class DatabaseManager
{
public:
    // 构造函数：需要外部传入用户库的路径以及单词库的路径
    DatabaseManager(const string &usr_db_path, const string &dict_db_path);
    ~DatabaseManager();

    // 初始化数据表
    bool initalizeDatabase();

    // 用户相关的操作
    bool registerUser(const string &name, const string &password);               // 用户注册操作
    bool loginUser(const string &name, const string &password, bool &is_online); // 用户登录操作
    bool logoutUser(const string &name);                                         // 用户退出操作

    // 单词表的操作
    bool querryWord(const string &word, string &meaning);                                                  // 查询单词
    bool recordHistory(const string &name, const string &word, const string &meaning, const string &time); // 历史记录
    bool getHistory(const string name, string &history);
    bool isWordMastered(const string &name, const string &word, bool &mastered);
    bool setWordMastery(const string &name, const string &word, int mastery);
    bool addUserExpAndLevel(const string &name, int exp_gain, int &level, int &exp, bool &level_up);
    bool getPetInfo(const string &name, int &level, int &exp);
    bool getLearnWord(const string &name, string &word, string &meaning);

private:
    sqlite3 *usr_db_;  // 用户数据库指针
    sqlite3 *dict_db_; // 单词数据库指针
    mutex usr_mutex_;  // 用户库互斥锁
    mutex dict_mutex_; // 单词库互斥锁

    // 定义私有函数
    bool initializeUserDB(); // 初始化用户表
    bool initializeDictDB(); // 初始化单词表
    bool dictToDatabase();   // 导入词典数据

    // 执行sql语句的函数
    bool executeSQL(sqlite3 *db, const string &sql, char **errmsg = NULL);

    bool ensureColumnExists(sqlite3 *db, const string &table, const string &column, const string &ddl);
};

#endif
