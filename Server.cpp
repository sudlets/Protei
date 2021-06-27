#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <vector>
#include <algorithm>
using namespace std;

#define BACKLOG 100 //  число входящих соединений

void *get_in_addr(sockaddr *sa)
{
    if(sa->sa_family == AF_INET)
    {
        return &(((sockaddr_in *)sa)->sin_addr);
    }
    else
    {
        return &(((sockaddr_in6 *)sa)->sin6_addr);
    }
}

int sendall(int sockfd, const char *buf, int len, int flags)
{
    int total = 0;
    int bytesleft = len;
    int sendbytes = -1;

    if(send(sockfd, &len, 4, 0) == -1)
        return -1;

    while(total < len)
    {
        sendbytes = send(sockfd, buf + total, bytesleft, flags);
        if(sendbytes == -1) {break;}
        total += sendbytes;
        bytesleft -= sendbytes;
    }

    return sendbytes == -1 ? -1 : 0;
}

int recvall(int sockfd, string &msg, int flags)
{
    unsigned int inputbytes = 0;
    unsigned int totalbytes = 0;
    int status = -1;
    char buf[1500];

    if((status = recv(sockfd, &inputbytes, 4, 0)) != -1)
    {
        if(status == 0)     // Клиент закрывает соединение
            return 0;

        while(totalbytes < inputbytes)
        {
            if((status = recv(sockfd, buf, sizeof(buf), flags)) == -1)
                break;

            msg.append(buf, status);
            totalbytes += status;
        }
    }

    return status == -1 ? -1 : 1;
}

int recvall_udp(int sockfd, string &msg, int flags, sockaddr_storage &ther_addr, socklen_t &addr_size)
{
    unsigned int inputbytes = 0;
    unsigned int totalbytes = 0;
    int status = -1;
    char *buf = nullptr;

    if((status = recvfrom(sockfd, &inputbytes, 4, 0, (sockaddr *)&ther_addr, &addr_size)) != -1)
    {
        if(status == 0)
            return 0;

        if(buf != nullptr)
            delete[] buf;

        buf = new char[inputbytes];

        while(totalbytes < inputbytes)
        {
            if((status = recvfrom(sockfd, buf, inputbytes, flags, (sockaddr *)&ther_addr, &addr_size)) == -1)
                break;

            msg.append(buf, status);
            totalbytes += status;
        }
    }

    if(buf != nullptr)
        delete[] buf;
    return status == -1 ? -1 : 1;
}

int sendall_udp(int sockfd, const char *buf, int len, int flags, sockaddr_storage &ther_addr, socklen_t &addr_size)
{
    int total = 0;
    int bytesleft = len;
    int sendbytes = -1;

    if(sendto(sockfd, &len, 4, 0, (sockaddr *)&ther_addr, addr_size) == -1)
        return -1;

    while(total < len)
    {
        sendbytes = sendto(sockfd, buf + total, bytesleft, flags, (sockaddr *)&ther_addr, addr_size);
        if(sendbytes == -1) {break;}
        total += sendbytes;
        bytesleft -= sendbytes;
    }

    return sendbytes == -1 ? -1 : 0;
}

int countdigits(const string &msg, string &answer)
{
    stringstream x;
    string word;
    vector<int> digits;
    int count = 0;
    int sum = 0;

    x << msg;

    while (x >> word)
    {
        if(isdigit(*word.c_str()))
        {
            digits.push_back(atoi(word.c_str()));
            count++;
        }
    }

    if(!count)
        return 0;

    else
    {
        x.str(std::string());
        x.clear();

        sort(digits.begin(), digits.end());

        for(int digit:digits)
        {
            sum += digit;
            x << digit << " ";
        }

        x << endl << endl << sum << endl;

        answer = x.str();

        return 1;
    }
}

int main()
{
    int status;
    int sockfd;
    int sockfd_udp = -1;
    addrinfo hints;
    addrinfo *results;
    addrinfo hints_udp;
    addrinfo *results_udp;

    int new_fd;
    sockaddr_storage ther_addr;
    socklen_t addr_size = sizeof ther_addr;

    fd_set master;
    fd_set readfds;
    int fdmax;

    char ipstr[INET6_ADDRSTRLEN];

    string msg;
    string answer;

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    memset(&hints_udp, 0, sizeof hints);
    hints_udp.ai_family = AF_UNSPEC;
    hints_udp.ai_socktype = SOCK_DGRAM;
    hints_udp.ai_flags = AI_PASSIVE;

    if((status = getaddrinfo(NULL, "8086", &hints, &results)) != 0)
    {
        cout << "getaddrinfo error: " << gai_strerror(status);
        exit(1);
    }

    if((status = getaddrinfo(NULL, "8086", &hints_udp, &results_udp)) != 0)
    {
        cout << "getaddrinfo error: " << gai_strerror(status);
        exit(1);
    }

    if(!results)
        exit(2);

    if(!results_udp)
        exit(2);

    if((sockfd = socket(results->ai_family, results->ai_socktype, results->ai_protocol)) == -1)
    {
        perror("socket error");
        exit(3);
    }

    if((sockfd_udp = socket(results_udp->ai_family, results_udp->ai_socktype, results_udp->ai_protocol)) == -1)
    {
        perror("socket error");
        exit(3);
    }

    if(bind(sockfd, results->ai_addr, results->ai_addrlen) == -1)
    {
        perror("bind error");
        exit(4);
    }
    if(bind(sockfd_udp, results_udp->ai_addr, results_udp->ai_addrlen) == -1)
    {
        perror("bind error");
        exit(4);
    }

    inet_ntop(results->ai_family, get_in_addr(results->ai_addr), ipstr, sizeof ipstr);
    cout << "Сервер отслеживает входящие соединения tcp на " << ipstr << endl;

    setsockopt(sockfd_udp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    inet_ntop(results_udp->ai_family, get_in_addr(results_udp->ai_addr), ipstr, sizeof ipstr);
    cout << "Сервер отслеживает входящие соединения udp на " << ipstr << endl;

    freeaddrinfo(results_udp);
    freeaddrinfo(results);

    if(listen(sockfd, BACKLOG) == -1)
    {
        perror("listen error");
        exit(5);
    }

    FD_ZERO(&readfds);
    FD_ZERO(&master);
    FD_SET(sockfd, &master);
    FD_SET(sockfd_udp, &master);
    fdmax = sockfd <= sockfd_udp ? sockfd_udp : sockfd;

    while(true)
    {
        readfds = master;
        if(select(fdmax + 1, &readfds, nullptr, nullptr, nullptr) == -1)
        {
            perror("select error");
            exit(6);
        }

        for(int thissocket = 0; thissocket <= fdmax; thissocket++)
        {
            if(FD_ISSET(thissocket, &readfds))
            {
                if(thissocket == sockfd)
                {
                    new_fd = accept(sockfd, (sockaddr *)&ther_addr, &addr_size);
                    if(new_fd == -1)
                        perror("accept");
                    else
                    {
                        FD_SET(new_fd, &master);
                        if(new_fd > fdmax)
                            fdmax = new_fd;

                        inet_ntop(ther_addr.ss_family, get_in_addr((sockaddr *)&ther_addr), ipstr, sizeof ipstr);
                        cout << "New connection from: " << ipstr << " socket: " << new_fd << endl;
                    }
                }
                else if(thissocket == sockfd_udp) // обработка данных от клиента udp
                {
                    status = recvall_udp(thissocket, msg, 0, ther_addr, addr_size);
                    if(status == -1)
                        perror("recvfrom");
                    else
                    {
                        inet_ntop(ther_addr.ss_family, get_in_addr((sockaddr *)&ther_addr), ipstr, sizeof ipstr);
                        cout << "Got packet from: " << ipstr << endl;

                        if(countdigits(msg, answer))
                            sendall_udp(thissocket, answer.c_str(), answer.length(), 0, ther_addr, addr_size);
                        else
                            sendall_udp(thissocket, msg.c_str(), msg.length(), 0, ther_addr, addr_size);
                    }
                }
                else // обработка данных от клиента tcp
                {
                    if((status = recvall(thissocket, msg, 0)) <= 0)
                    {
                        if(status == -1)
                            perror("recv error");
                        else
                            cout << "Closed connection socket: " << thissocket << endl;

                        shutdown(thissocket, 2);
                        close(thissocket);
                        FD_CLR(thissocket, &master);
                    }
                    else
                    {
                        if(countdigits(msg, answer))
                            sendall(thissocket, answer.c_str(), answer.length(), 0);
                        else
                            sendall(thissocket, msg.c_str(), msg.length(), 0);
                    }
                }
                FD_CLR(thissocket, &readfds);
                msg.clear();
                answer.clear();
            }
        }
    }

    shutdown(sockfd, 2); // 0 -> recv, 1 -> send, 2 -> all
    close(sockfd);

    return 0;
}

/*
 * FD_SET(int fd, fd_set *set);         // Добавляет fd в set
 * FD_CLR(int fd, fd_set *set);         // Удаляет fd из set
 * FD_ISSET(int fd, fd_set *set);       // Возвращает true если fd есть в set
 * FD_ZERO(fd_set *set);                // Очищает set
*/
