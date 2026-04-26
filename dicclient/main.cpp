#include "client.hpp"

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
int main(int argc, const char *argv[])
{
    // 判断传入的数据是否足够
    if (argc != 3)
    {
        cerr << "用法：" << argv[0] << " <服务器ip> <端口号>" << endl;
        return -1;
    }

    // 实例化客户端对象
    try
    {
        DictClient client(argv[1], atoi(argv[2])); // 实例化客户端对象
        client.run();                              // 运行主要逻辑
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    return 0;
}