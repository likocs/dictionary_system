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
#include <sstream>
#include <cstdint>

using namespace std;
// 主程序循环
void DictClient::run()
{
    while (true)
    {
        showMainMenu(); // 展示主菜单
        int choice;     // 选择功能
        cin >> choice;
        cin.ignore(); // 清除缓冲区

        // 对选则的内容进行多分枝
        switch (choice)
        {
        case R:           // 注册功能
            doRegister(); // 调用注册函数
            break;

        case L:            // 登录功能
            if (doLogin()) // 调用登录函数
            {
                showUserMenu(); // 调用用户展示函数
            }
            break;

        case Q:       // 退出功能
            doQuit(); // 调用退出函数
            break;

        default:
            cout << "无效的选择" << endl;
            break;
        }

        cout << "按回车继续....";
        cin.get(); // 吸收回车
    }
}

// 主菜单函数
void DictClient::showMainMenu()
{
    system("clear"); // 用于清屏
    cout << "***************************" << endl;
    cout << "*********1、注册***********" << endl;
    cout << "*********2、登录***********" << endl;
    cout << "*********3、退出***********" << endl;
    cout << "***************************" << endl;
    cout << "请选择：";
}

// 显示用户菜单
void DictClient::showUserMenu()
{
    while (is_logged_in_)
    {
        system("clear"); // 清屏
        cout << "当前用户：" << username_ << endl;
        cout << "*********************************" << endl;
        cout << "**********1、查单词***************" << endl;
        cout << "**********2、历史记录***************" << endl;
        cout << "**********3、背单词***************" << endl;
        cout << "**********4、等级/经验***************" << endl;
        cout << "**********5、返回上一级***************" << endl;
        cout << "*********************************" << endl;
        cout << "请选择：";

        int choice;
        cin >> choice;
        cin.ignore();

        // 对选择内容进行多分枝
        switch (choice)
        {
        case 1:
            doQuerry(); // 调用查单词函数
            break;

        case 2:
            doHistory(); // 调用查单词函数
            break;

        case 3:
            doLearn();
            break;

        case 4:
            doPetInfo();
            break;

        case 5:
            doQuit(); // 调用查单词函数
            break;

        default:
            cout << "无效的选择" << endl;
            break;
        }

        cout << "按回车继续...." << endl;
        cin.get();
    }
}

void DictClient::doLearn()
{
    while (true)
    {
        Msg req;
        req.type = B;
        strcpy(req.name, username_.c_str());
        req.text[0] = '\0';

        req.networkByteOrder();
        if (send(sockfd_, &req, sizeof(req), 0) < 0)
        {
            perror("发送背单词请求失败");
            return;
        }

        Msg resp;
        if (recv(sockfd_, &resp, sizeof(resp), 0) <= 0)
        {
            perror("接收背单词响应失败");
            return;
        }
        resp.hostByteOrder();

        if (resp.type == D)
        {
            string payload(resp.text);
            istringstream iss(payload);
            uint32_t quiz_id = 0;
            iss >> quiz_id;
            string meaning;
            getline(iss, meaning);
            if (!meaning.empty() && meaning[0] == ' ')
            {
                meaning.erase(meaning.begin());
            }

            cout << "默写模式（输入#退出）" << endl;
            cout << "释义：" << meaning << endl;
            cout << "请输入单词：";
            string spelling;
            getline(cin, spelling);
            if (spelling == "#")
            {
                return;
            }

            Msg submit;
            submit.type = N;
            strcpy(submit.name, username_.c_str());
            string text = to_string(quiz_id) + " " + spelling;
            strncpy(submit.text, text.c_str(), sizeof(submit.text));
            submit.text[sizeof(submit.text) - 1] = '\0';

            submit.networkByteOrder();
            if (send(sockfd_, &submit, sizeof(submit), 0) < 0)
            {
                perror("发送默写提交失败");
                return;
            }

            if (recv(sockfd_, &resp, sizeof(resp), 0) <= 0)
            {
                perror("接收默写结果失败");
                return;
            }
            resp.hostByteOrder();
            cout << resp.text << endl;
            cout << "按回车继续...." << endl;
            cin.get();
            continue;
        }

        if (strcmp(resp.text, "**EMPTY**") == 0)
        {
            cout << "暂无可学习单词" << endl;
            return;
        }

        string payload(resp.text);
        istringstream iss(payload);
        string word;
        iss >> word;
        string meaning;
        getline(iss, meaning);
        if (!meaning.empty() && meaning[0] == ' ')
        {
            meaning.erase(meaning.begin());
        }

        cout << "背单词模式（输入#退出）" << endl;
        cout << "单词：" << word << endl;
        cout << "释义：" << meaning << endl;
        cout << "是否记住？(1/0/#):";
        string input;
        getline(cin, input);
        if (input == "#")
        {
            return;
        }

        int remembered = (input == "1" || input == "y" || input == "Y") ? 1 : 0;
        Msg submit;
        submit.type = M;
        strcpy(submit.name, username_.c_str());
        string text = word + " " + to_string(remembered);
        strncpy(submit.text, text.c_str(), sizeof(submit.text));
        submit.text[sizeof(submit.text) - 1] = '\0';

        submit.networkByteOrder();
        if (send(sockfd_, &submit, sizeof(submit), 0) < 0)
        {
            perror("发送背单词提交失败");
            return;
        }

        if (recv(sockfd_, &resp, sizeof(resp), 0) <= 0)
        {
            perror("接收背单词提交响应失败");
            return;
        }
        resp.hostByteOrder();
        cout << resp.text << endl;
        cout << "按回车继续...." << endl;
        cin.get();
    }
}

void DictClient::doPetInfo()
{
    Msg msg;
    msg.type = P;
    strcpy(msg.name, username_.c_str());
    msg.text[0] = '\0';

    msg.networkByteOrder();
    if (send(sockfd_, &msg, sizeof(msg), 0) < 0)
    {
        perror("发送等级/经验请求失败");
        return;
    }

    if (recv(sockfd_, &msg, sizeof(msg), 0) <= 0)
    {
        perror("接收等级/经验响应失败");
        return;
    }
    msg.hostByteOrder();
    cout << msg.text << endl;
}

// 用户注册函数
bool DictClient::doRegister()
{
    // 定义相关数据信息
    Msg msg;
    msg.type = R; // 表示是注册类型

    // 提示并输入用户名和密码
    cout << "请输入用户名称(最长19个字符):";
    cin.getline(msg.name, sizeof(msg.name));
    cout << "请输入密码(最长127个字符):";
    cin.getline(msg.text, sizeof(msg.text));

    // 将上面的信息转换成网络字节序
    msg.networkByteOrder();
    // 发送给服务器
    if (send(sockfd_, &msg, sizeof(msg), 0) < 0)
    {
        perror("注册请求失败");
        return false;
    }

    // 程序执行至此，表示注册成功
    if (recv(sockfd_, &msg, sizeof(msg), 0) <= 0)
    {
        perror("接收注册信息响应失败");
        return false;
    }
    // 说明收到服务器的消息
    msg.hostByteOrder(); // 将收到的服务器消息转换为本机字节序

    // 处理服务器给出的响应
    if (strcmp(msg.text, "**OK**") == 0)
    {
        cout << "注册成功" << endl;
        return true;
    }
    else if (strcmp(msg.text, "**EXISTS**") == 0)
    {
        cout << "注册失败，用户名已经存在" << endl;
    }
    else
    {
        cout << "注册失败，错误未知" << endl;
    }

    return false;
}

// 定义登录函数
bool DictClient::doLogin()
{
    // 定义相关数据信息
    Msg msg;
    msg.type = L; // 表示是登录类型

    cout << "请输入用户名:";
    cin.getline(msg.name, sizeof(msg.name));
    cout << "请输入密码：";
    cin.getline(msg.text, sizeof(msg.text));

    // 转换为网络字节序
    msg.networkByteOrder();
    // 发送给服务器
    if (send(sockfd_, &msg, sizeof(msg), 0) < 0)
    {
        perror("发送登录请求失败");
        return false;
    }

    // 程序执行至此，表示向服务器发送数据成功，接收服务器反馈的结果
    if (recv(sockfd_, &msg, sizeof(msg), 0) <= 0)
    {
        perror("接收登录响应失败");
        return false;
    }

    // 将接收的反馈信息转换为本机字节序
    msg.hostByteOrder();

    // 对接收的结果进行处理
    if (strcmp(msg.text, "**OK**") == 0)
    {
        cout << "登录成功" << endl;
        username_ = msg.name;
        is_logged_in_ = true; // 表示在线

        return true;
    }
    else if (strcmp(msg.text, "**EXISTS**") == 0)
    {
        cout << "登录失败，用户已经在线" << endl;
    }
    else
    {
        cout << "登录失败，原因未知" << endl;
    }

    return false;
}

// 查单词功能
void DictClient::doQuerry()
{
    Msg msg;                             // 信息传递体
    msg.type = S;                        // 表示查询功能
    strcpy(msg.name, username_.c_str()); // 要查单词的用户名

    // 循环进行查询单词
    while (true)
    {
        cout << "请输入要查询的单词(输入#后结束查询):";
        cin.getline(msg.text, sizeof(msg.text));

        // 判断是否结束查询
        if (strcmp(msg.text, "#") == 0)
        {
            cout << "*********";
            break;
        }

        // 将消息发送给服务器，进行检索
        msg.networkByteOrder();
        if (send(sockfd_, &msg, sizeof(msg), 0) < 0)
        {
            perror("发送查询消息失败");
            return;
        }

        // 函数执行至此，表示插叙成功，并收到服务器反馈的结果
        if (recv(sockfd_, &msg, sizeof(msg), 0) <= 0)
        {
            perror("接收查询结果失败");
            return;
        }
        msg.hostByteOrder(); // 转化为主机字节序

        cout << "释义：" << msg.text << endl;
    }
}

// 查看历史记录
void DictClient::doHistory()
{
    Msg msg;                             // 信息传递体
    msg.type = H;                        // 表示查询功能
    strcpy(msg.name, username_.c_str()); // 要查单词的用户名

    // 转换为网络字节序
    msg.networkByteOrder();
    if (send(sockfd_, &msg, sizeof(msg), 0) < 0)
    {
        perror("发送历史记录请求失败");
        return;
    }

    cout << "查询历史记录：" << endl;
    while (true)
    {
        if (recv(sockfd_, &msg, sizeof(msg), 0) <= 0)
        {
            perror("接收历史数据失败");
            return;
        }
        msg.hostByteOrder(); // 转换为主机字节序

        // 判断是否查询完毕
        if (strcmp(msg.text, "**OVER**") == 0)
        {
            break;
        }

        // 将消息展示出来
        cout << msg.text << endl;
    }
}

// 退出登录功能
void DictClient::doQuit()
{
    if (is_logged_in_) // 判断是否在线
    {
        Msg msg;                             // 信息传递体
        msg.type = Q;                        // 表示查询功能
        strcpy(msg.name, username_.c_str()); // 要查单词的用户名

        // 转换为网络字节序
        msg.networkByteOrder();

        if (send(sockfd_, &msg, sizeof(msg), 0) < 0)
        {
            perror("发送突出请求失败");
            return;
        }
        else
        {
            is_logged_in_ = false; // 表示下线
            username_.clear();
            cout << "用户已经退出" << endl;
        }
    }
}
