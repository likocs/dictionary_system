#ifndef MESSAGE_HPP
#define MESSAGE_HPP

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

using namespace std;
// 操作类型的定义
#define R 1 // 用户注册
#define L 2 // 用户登录
#define Q 3 // 用户退出
#define S 4 // 单词查询
#define H 5 // 历史记录
#define P 6
#define B 7
#define M 8
#define D 9
#define N 10

// 定义客户端与服务器进行数据通信的协议
struct Msg
{
    int type;       // 操作类型
    char name[20];  // 用户名
    char text[128]; // 文本内容  （密码/单词）

    // 定义相关字节序的转换函数
    // 主机字节序转网络字节序
    void networkByteOrder()
    {
        type = htonl(type);
    }

    // 网络字节序转主机字节序
    void hostByteOrder()
    {
        type = ntohl(type);
    }
};

#endif
