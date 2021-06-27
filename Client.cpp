#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

#define UDP_PROTOCOL 1
#define TCP_PROTOCOL 2
using namespace std;

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
        if(status == 0)     // Отправитель закрывает соединение
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

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        cout << "usage: ./program {-t | -u} hostname" << endl;
        exit(1);
    }

    cout << "argc: " << argc << endl;
    cout << "argv[2]: " << argv[2] << endl;

    char c;
    int protocol = -1;

    while (--argc > 0 && (*++argv)[0] == '-')
        while(c = *++argv[0])
            switch(c)
            {
            case 't': protocol = TCP_PROTOCOL; break;
            case 'u': protocol = UDP_PROTOCOL; break;
            default:
                cout << "Illegal option \"" << c <<"\"" << endl;
                exit(20);
            }

    int status;
    int sockfd;
    addrinfo hints;
    addrinfo *results;

    char ipstr[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;

    if(protocol == TCP_PROTOCOL)
        hints.ai_socktype = SOCK_STREAM;
    else if(protocol == UDP_PROTOCOL)
        hints.ai_socktype = SOCK_DGRAM;
    else
        exit(20);

    fd_set readfds;
    timeval tv;
    tv.tv_sec = 60;
    tv.tv_usec = 500000;

    if((status = getaddrinfo(argv[0], "8086", &hints, &results)) != 0)
    {
        cout << "getaddrinfo error: " << gai_strerror(status);
        exit(2);
    }

    if((sockfd = socket(results->ai_family, results->ai_socktype, results->ai_protocol)) == -1)
    {
        perror("socket error");
        exit(3);
    }

    if(connect(sockfd, results->ai_addr, results->ai_addrlen) == -1)
    {
        perror("connect error");
        exit(4);
    }

    inet_ntop(results->ai_family, get_in_addr(results->ai_addr), ipstr, sizeof ipstr);
    cout << "Успешное соединение с сервером: " << ipstr << endl;

    freeaddrinfo(results);

    //==
    string msg;
    string word;
    stringstream x;
    string answer;

    while(true)
    {
        cout << "Введите сообщение или \"exit\" для выхода\n" << "<< ";

        getline(cin, msg);
        x << msg;
        x >> word;

        if(!word.compare("exit"))
            break;

        if(sendall(sockfd, msg.c_str(), msg.length(), 0) == -1)
        {
            perror("send error");
            exit(5);
        }
        //=====
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        if(select(sockfd + 1, &readfds, nullptr, nullptr, &tv) == -1)
        {
            perror("select error");
            exit(6);
        }

        if(FD_ISSET(sockfd, &readfds))
        {
            status = recvall(sockfd, answer, 0);
            if(status == -1)
            {
                perror("recvfrom error");
                exit(6);
            }
            if(status == 0)
            {
                cout << "Closed connection from " << ipstr << endl;
                break;
            }

            cout << "Message: " << answer << endl;
        }
        else
            cout << "Сервер не отвечает" << endl;

        //=====

        answer.clear();
        msg.clear();
        x.str(std::string());
        x.clear();
    }
    //==


    shutdown(sockfd, 2); // 0 -> recv, 1 -> send, 2 -> all
    close(sockfd);

    return 0;
}


/*
 * struct addrinfo {
 *  int                 ai_flags;           // AI_PASSICE, AI_CANONNAME, ETC...
 *  int                 ai_family           // AF_INET, AF_INET6, AF_UNSPEC
 *  int                 ai_socktype;        // SOCK_STREAM, SOCK_DGRAM
 *  int                 ai_protocol;        // используйте 0 для "any"
 *  size_t              ai_addrlen;         // размер ai_addr в байтах
 *  struct sockaddr     *ai_addr;           // struct sockaddr_in or _in6
 *  char                *ai_canonname;      // полное каноническое имя хоста
 *  struct addrinfo     *ai_next;           // связанный список, следующий
 * };
 */

/*
 * struct sockaddr {
 *  unsigned short      sa_family;          // семейство адресов, AF_INET or AF_INET6
 *  char                sa_data[14];        // 14 байт адреса протокола и порт
 * };
*/

/* Для работы со структурой sockaddr программисты создают параллельную структуру _in _in6
 * Эта структура позволяет обращаться к элементам адреса сокета.
 * struct sockaddr_in {
 *  short int           sin_family;         // Семейство адресов, AF_INET
 *  unsigned short int  sin_port;           // Номер порта
 *  struct in_addr      sin_addr;           // Интернет адрес
 *  unsigned char       sin_zero[8];        // Размер как у struct sockaddr - должен быть обнулён memset()
 * };
*/

/*
 * struct in_addr {
 *  uint32_t            s_addr;             // Это 32-битный int (4 байта)
 * };
*/

/*
* struct sockaddr_in6 {
*  u_int16_t            sin6_family;        // Семейство адресов, AF_INET
*  u_int16_t            sin6_port;          // Номер порта
*  u_int32_t            sin6_flowinfo;      // потоковая информация IPv6
*  struct in6_addr      sin6_addr;          // Интернет адрес
*  u_int32_t            sin6_scope_id;      // Scope ID
* };
*/

/*
* struct in6_addr {
*  unsigned char        s6_addr[16];        // адрес IPv6
* };
*/

/* Структура, которая может содержать обе версии протоколов IP
 * struct sockaddr_storage{
 *  sa_family_t         ss_family;          // семейство адресов
 * // это выравнивание длины, зависит от реализации
 *  char                __ss_pad1[SS_PAD1SIZE];
 *  int64_t             __ss_align;
 *  char                __ss_pad2[SS_PAD2SIZE];
*/

/*
 * getaddrinfo(
 *  const char *node,                    // например, "www.example.com" или IP
 *  const char *servКлиентice,                 // например, "http" или номер порта
 *  const struct addrinfo *hints         // заполненная структура addrinfo
 *  struct addrinfo **res);              // возвращает связанный список результатов
 * };
 */

/* Поместить в структура ip адрес
 * struct sockaddr_in sa;
 * struct sockaddr_in6 sa6;
 * inet_pton(AF_INET, "192.0.2.1", &(sa.sin_addr));         // возвращает -1 при ошибке, 0 что-то, 1 успех
 * inet_pton(AF_INET6, "2001::3490", &(sa6.sin6_addr));     // возвращает -1 при ошибке, 0 что-то, 1 успех
 * ВАЖНО! старый способ inet_addr() и inet_aton() устарел и не работает с IPv6
 */

/* Вывести на экран ip
 * char ip4[INET_ADDRSTRLEN];
 * struct sockaddr_in sa; // предположительно чем-то загружено
 * inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN);
 * orintf("The ipv4 address is: %s\n", ip4);
 */
