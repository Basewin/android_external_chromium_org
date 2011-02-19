/*
This is a UDT self-testing program.
The code in the program may not be cleaned up yet.

for performance testing, use appserver/appclient and sendfile/recvfile.
*/


#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
   #include <signal.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif
#include <iostream>
#include <algorithm>
#include <udt.h>

using namespace std;

int g_IP_Version = AF_INET;
int g_Socket_Type = SOCK_STREAM;


int createUDTSocket(UDTSOCKET& usock, int version = AF_INET, int type = SOCK_STREAM, int port = 0, bool rendezvous = false)
{
   addrinfo hints;
   addrinfo* res;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = version;
   hints.ai_socktype = type;

   char service[16];
   sprintf(service, "%d", port);

   if (0 != getaddrinfo(NULL, service, &hints, &res))
   {
      cout << "illegal port number or port is busy.\n" << endl;
      return -1;
   }

   usock = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

   int snd_buf = 64000;
   int rcv_buf = 100000;
   UDT::setsockopt(usock, 0, UDT_SNDBUF, &snd_buf, sizeof(int));
   UDT::setsockopt(usock, 0, UDT_RCVBUF, &rcv_buf, sizeof(int));
   snd_buf = 64000;
   rcv_buf = 100000;
   UDT::setsockopt(usock, 0, UDP_SNDBUF, &snd_buf, sizeof(int));
   UDT::setsockopt(usock, 0, UDP_RCVBUF, &rcv_buf, sizeof(int));
   int fc = 256;
   UDT::setsockopt(usock, 0, UDT_FC, &fc, sizeof(int));
   bool reuse = true;
   UDT::setsockopt(usock, 0, UDT_REUSEADDR, &reuse, sizeof(bool));
   UDT::setsockopt(usock, 0, UDT_RENDEZVOUS, &rendezvous, sizeof(bool));

   if (UDT::ERROR == UDT::bind(usock, res->ai_addr, res->ai_addrlen))
   {
      cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
      return -1;
   }

   freeaddrinfo(res);
   return 0;
}

int createTCPSocket(SYSSOCKET& ssock, int version = AF_INET, int type = SOCK_STREAM, int port = 0, bool rendezvous = false)
{
   addrinfo hints;
   addrinfo* res;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = version;
   hints.ai_socktype = type;

   char service[16];
   sprintf(service, "%d", port);

   if (0 != getaddrinfo(NULL, service, &hints, &res))
   {
      cout << "illegal port number or port is busy.\n" << endl;
      return -1;
   }

   ssock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

   if (bind(ssock, res->ai_addr, res->ai_addrlen) != 0)
   {
      return -1;
   }

   freeaddrinfo(res);
   return 0;
}

int connect(UDTSOCKET& usock, int port, int version, int type)
{
   addrinfo hints, *peer;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = version;
   hints.ai_socktype = type;

   char buffer[16];
   sprintf(buffer, "%d", port);

   if (0 != getaddrinfo("127.0.0.1", buffer, &hints, &peer))
   {
      return 0;
   }

   UDT::connect(usock, peer->ai_addr, peer->ai_addrlen);

   freeaddrinfo(peer);

   return 0;
}

int tcp_connect(SYSSOCKET& ssock, int port, int version, int type)
{
   addrinfo hints, *peer;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = version;
   hints.ai_socktype = type;

   char buffer[16];
   sprintf(buffer, "%d", port);

   if (0 != getaddrinfo("127.0.0.1", buffer, &hints, &peer))
   {
      return 0;
   }

   connect(ssock, peer->ai_addr, peer->ai_addrlen);

   freeaddrinfo(peer);

   return 0;
}


#ifndef WIN32
void* Test_1_Srv(void* param)
#else
DWORD WINAPI Test_1_Srv(LPVOID param)
#endif
{
   UDTSOCKET serv;
   if (createUDTSocket(serv, AF_INET, SOCK_STREAM, 9000) < 0)
      return NULL;

   UDT::listen(serv, 10);

   sockaddr_storage clientaddr;
   int addrlen = sizeof(clientaddr);
   UDTSOCKET new_sock = UDT::accept(serv, (sockaddr*)&clientaddr, &addrlen);

   UDT::close(serv);

   if (new_sock == UDT::INVALID_SOCK)
   {
      return NULL;
   }

   const int size = 10000;
   int32_t buffer[size];
   fill_n(buffer, 0, size);

   int torecv = size * sizeof(int32_t);
   while (torecv > 0)
   {
      int rcvd = UDT::recv(new_sock, (char*)buffer + size * sizeof(int32_t) - torecv, torecv, 0);
      if (rcvd < 0)
      {
         cout << "recv: " << UDT::getlasterror().getErrorMessage() << endl;
         return NULL;
      }

      torecv -= rcvd;
   }

   // check data
   for (int i = 0; i < size; ++ i)
   {
      if (buffer[i] != i)
      {
         cout << "DATA ERROR " << i << " " << buffer[i] << endl;
         break;
      }
   }

   UDT::close(new_sock);

   return NULL;
}

#ifndef WIN32
void* Test_1_Cli(void* param)
#else
DWORD WINAPI Test_1_Cli(LPVOID param)
#endif
{
   UDTSOCKET client;
   if (createUDTSocket(client, AF_INET, SOCK_STREAM, 0) < 0)
      return NULL;

   connect(client, 9000, AF_INET, SOCK_STREAM);

   const int size = 10000;
   int32_t buffer[size];
   for (int i = 0; i < size; ++ i)
      buffer[i] = i;

   int tosend = size * sizeof(int32_t);
   while (tosend > 0)
   {
      int sent = UDT::send(client, (char*)buffer + size * sizeof(int32_t) - tosend, tosend, 0);
      if (sent < 0)
      {
         cout << "send: " << UDT::getlasterror().getErrorMessage() << endl;
         return NULL;
      }

      tosend -= sent;
   }

   UDT::close(client);
   return NULL;
}

#ifndef WIN32
void* Test_2_Srv(void* param)
#else
DWORD WINAPI Test_2_Srv(LPVOID param)
#endif
{
#ifndef WIN32
   //ignore SIGPIPE
   sigset_t ps;
   sigemptyset(&ps);
   sigaddset(&ps, SIGPIPE);
   pthread_sigmask(SIG_BLOCK, &ps, NULL);
#endif

   // create 1000 UDT sockets
   UDTSOCKET serv;
   if (createUDTSocket(serv, AF_INET, SOCK_STREAM, 9000) < 0)
      return NULL;

   UDT::listen(serv, 10);

   vector<UDTSOCKET> new_socks;
   new_socks.resize(1000);

   int eid = UDT::epoll_create();

   for (int i = 0; i < 1000; ++ i)
   {
      sockaddr_storage clientaddr;
      int addrlen = sizeof(clientaddr);
      new_socks[i] = UDT::accept(serv, (sockaddr*)&clientaddr, &addrlen);

      if (new_socks[i] == UDT::INVALID_SOCK)
      {
         cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
         return NULL;
      }

      UDT::epoll_add_usock(eid, new_socks[i]);
   }


   // create 10 TCP sockets
   SYSSOCKET tcp_serv;
   if (createTCPSocket(tcp_serv, AF_INET, SOCK_STREAM, 9001) < 0)
      return NULL;

   listen(tcp_serv, 10);

   vector<SYSSOCKET> tcp_socks;
   tcp_socks.resize(10);

   for (int i = 0; i < 10; ++ i)
   {
      sockaddr_storage clientaddr;
      socklen_t addrlen = sizeof(clientaddr);
      tcp_socks[i] = accept(tcp_serv, (sockaddr*)&clientaddr, &addrlen);

      UDT::epoll_add_ssock(eid, tcp_socks[i]);
   }


   // using epoll to retrieve both UDT and TCP sockets
   set<UDTSOCKET> readfds;
   set<SYSSOCKET> tcpread;
   int count = 1000 + 10;
   while (count > 0)
   {
      UDT::epoll_wait(eid, &readfds, NULL, -1, &tcpread);
      for (set<UDTSOCKET>::iterator i = readfds.begin(); i != readfds.end(); ++ i)
      {
         int32_t data;
         UDT::recv(*i, (char*)&data, 4, 0);

         -- count;
      }

      for (set<SYSSOCKET>::iterator i = tcpread.begin(); i != tcpread.end(); ++ i)
      {
         int32_t data;
         recv(*i, (char*)&data, 4, 0);

         -- count;
      }
   }

   for (vector<UDTSOCKET>::iterator i = new_socks.begin(); i != new_socks.end(); ++ i)
   {
      UDT::close(*i);
   }

   for (vector<SYSSOCKET>::iterator i = tcp_socks.begin(); i != tcp_socks.end(); ++ i)
   {
#ifndef WIN32
      close(*i);
#else
      closesocket(*i);
#endif
   }

   UDT::close(serv);
#ifndef WIN32
   close(tcp_serv);
#else
   closesocket(tcp_serv);
#endif

   return NULL;
}

#ifndef WIN32
void* Test_2_Cli(void* param)
#else
DWORD WINAPI Test_2_Cli(LPVOID param)
#endif
{
#ifndef WIN32
   //ignore SIGPIPE
   sigset_t ps;
   sigemptyset(&ps);
   sigaddset(&ps, SIGPIPE);
   pthread_sigmask(SIG_BLOCK, &ps, NULL);
#endif

   // create 1000 UDT clients
   vector<UDTSOCKET> cli_socks;
   cli_socks.resize(1000);

   // 100 individual ports
   for (int i = 0; i < 100; ++ i)
   {
      if (createUDTSocket(cli_socks[i], AF_INET, SOCK_STREAM, 0) < 0)
      {
         cout << "socket: " << UDT::getlasterror().getErrorMessage() << endl;
         return NULL;
      }
   }

   // 900 shared port

   if (createUDTSocket(cli_socks[100], AF_INET, SOCK_STREAM, 0) < 0)
   {
      cout << "socket: " << UDT::getlasterror().getErrorMessage() << endl;
      return NULL;
   }

   sockaddr* addr = NULL;
   int size = 0;

   addr = (sockaddr*)new sockaddr_in;
   size = sizeof(sockaddr_in);

   UDT::getsockname(cli_socks[100], addr, &size);
   char sharedport[NI_MAXSERV];
   getnameinfo(addr, size, NULL, 0, sharedport, sizeof(sharedport), NI_NUMERICSERV);

   for (int i = 101; i < 1000; ++ i)
   {
      if (createUDTSocket(cli_socks[i], AF_INET, SOCK_STREAM, atoi(sharedport)) < 0)
      {
         cout << "socket: " << UDT::getlasterror().getErrorMessage() << endl;
         return NULL;
      }
   }

   for (vector<UDTSOCKET>::iterator i = cli_socks.begin(); i != cli_socks.end(); ++ i)
   {
      if (connect(*i, 9000, AF_INET, SOCK_STREAM) < 0)
      {
         cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
         return NULL;
      }
   }


   // create 10 TCP clients
   vector<SYSSOCKET> tcp_socks;
   tcp_socks.resize(10);

   for (int i = 0; i < 10; ++ i)
   {
      if (createTCPSocket(tcp_socks[i], AF_INET, SOCK_STREAM, 0) < 0)
      {
         return NULL;
      }

      tcp_connect(tcp_socks[i], 9001, AF_INET, SOCK_STREAM);
   }


   // send data from both UDT and TCP clients
   int32_t data = 0;
   for (vector<UDTSOCKET>::iterator i = cli_socks.begin(); i != cli_socks.end(); ++ i)
   {
      UDT::send(*i, (char*)&data, 4, 0);
      ++ data;
   }

   for (vector<SYSSOCKET>::iterator i = tcp_socks.begin(); i != tcp_socks.end(); ++ i)
   {
      send(*i, (char*)&data, 4, 0);
      ++ data;
   }


   // close all client sockets
   for (vector<UDTSOCKET>::iterator i = cli_socks.begin(); i != cli_socks.end(); ++ i)
   {
      UDT::close(*i);
   }

   for (vector<SYSSOCKET>::iterator i = tcp_socks.begin(); i != tcp_socks.end(); ++ i)
   {
#ifndef WIN32
      close(*i);
#else
      closesocket(*i);
#endif
   }

   return NULL;
}

#ifndef WIN32
void* Test_3_Srv(void* param)
#else
DWORD WINAPI Test_3_Srv(LPVOID param)
#endif
{
   vector<UDTSOCKET> srv_socks;
   srv_socks.resize(50);

   int port = 61000;

   for (int i = 0; i < 50; ++ i)
   {
      if (createUDTSocket(srv_socks[i], AF_INET, SOCK_STREAM, port ++, true) < 0)
      {
        cout << "error\n";
      }
   }

   int peer_port = 51000;

   for (vector<UDTSOCKET>::iterator i = srv_socks.begin(); i != srv_socks.end(); ++ i)
   {
      connect(*i, peer_port ++, AF_INET, SOCK_STREAM);
   }

   for (vector<UDTSOCKET>::iterator i = srv_socks.begin(); i != srv_socks.end(); ++ i)
   {
      int32_t data = 0;
      UDT::recv(*i, (char*)&data, 4, 0);
   }

   for (vector<UDTSOCKET>::iterator i = srv_socks.begin(); i != srv_socks.end(); ++ i)
   {
      UDT::close(*i);
   }

   return NULL;
}

#ifndef WIN32
void* Test_3_Cli(void* param)
#else
DWORD WINAPI Test_3_Cli(LPVOID param)
#endif
{
   vector<UDTSOCKET> cli_socks;
   cli_socks.resize(50);

   int port = 51000;

   for (int i = 0; i < 50; ++ i)
   {
      if (createUDTSocket(cli_socks[i], AF_INET, SOCK_STREAM, port ++, true) < 0)
      {
        cout << "error\n";
      }
   }

   int peer_port = 61000;

   for (vector<UDTSOCKET>::iterator i = cli_socks.begin(); i != cli_socks.end(); ++ i)
   {
      connect(*i, peer_port ++, AF_INET, SOCK_STREAM);
   }

   int32_t data = 0;
   for (vector<UDTSOCKET>::iterator i = cli_socks.begin(); i != cli_socks.end(); ++ i)
   {
      UDT::send(*i, (char*)&data, 4, 0);
      ++ data;
   }

   for (vector<UDTSOCKET>::iterator i = cli_socks.begin(); i != cli_socks.end(); ++ i)
   {
      UDT::close(*i);
   }

   return NULL;
}

#ifndef WIN32
void* Test_4_Srv(void* param)
#else
DWORD WINAPI Test_4_Srv(LPVOID param)
#endif
{
   UDTSOCKET serv;
   if (createUDTSocket(serv, AF_INET, SOCK_STREAM, 9000) < 0)
      return NULL;

   UDT::listen(serv, 10);

   const int total = 1000;

   vector<UDTSOCKET> new_socks;
   new_socks.resize(total);

   for (int i = 0; i < total; ++ i)
   {
      sockaddr_storage clientaddr;
      int addrlen = sizeof(clientaddr);
      new_socks[i] = UDT::accept(serv, (sockaddr*)&clientaddr, &addrlen);

      if (new_socks[i] == UDT::INVALID_SOCK)
      {
         cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
         return NULL;
      }
   }

   for (vector<UDTSOCKET>::iterator i = new_socks.begin(); i != new_socks.end(); ++ i)
   {
      UDT::close(*i);
   }

   UDT::close(serv);

   return NULL;

}

#ifndef WIN32
void* start_and_destroy_clients(void* param)
#else
DWORD WINAPI start_and_destroy_clients(LPVOID param)
#endif
{
   const int total = 25;

   vector<UDTSOCKET> cli_socks;
   cli_socks.resize(total);

   if (createUDTSocket(cli_socks[0], AF_INET, SOCK_STREAM, 0) < 0)
   {
      cout << "socket: " << UDT::getlasterror().getErrorMessage() << endl;
      return NULL;
   }

   sockaddr* addr = NULL;
   int size = 0;

   addr = (sockaddr*)new sockaddr_in;
   size = sizeof(sockaddr_in);

   UDT::getsockname(cli_socks[0], addr, &size);
   char sharedport[NI_MAXSERV];
   getnameinfo(addr, size, NULL, 0, sharedport, sizeof(sharedport), NI_NUMERICSERV);

   for (int i = 1; i < total; ++ i)
   {
      if (createUDTSocket(cli_socks[i], AF_INET, SOCK_STREAM, atoi(sharedport)) < 0)
      {
         cout << "socket: " << UDT::getlasterror().getErrorMessage() << endl;
         return NULL;
      }
   }

   for (vector<UDTSOCKET>::iterator i = cli_socks.begin(); i != cli_socks.end(); ++ i)
   {
      if (connect(*i, 9000, AF_INET, SOCK_STREAM) < 0)
      {
         cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
         return NULL;
      }
   }

   for (vector<UDTSOCKET>::iterator i = cli_socks.begin(); i != cli_socks.end(); ++ i)
   {
      UDT::close(*i);
   }   

   return NULL;
}

#ifndef WIN32
void* Test_4_Cli(void*)
#else
DWORD WINAPI Test_4_Cli(LPVOID)
#endif
{
   const int total_threads = 40;  // 40 * 25 = 1000

#ifndef WIN32
   vector<pthread_t> cli_threads;
   cli_threads.resize(total_threads);

   for (vector<pthread_t>::iterator i = cli_threads.begin(); i != cli_threads.end(); ++ i)
   {
      pthread_create(&(*i), NULL, start_and_destroy_clients, NULL);
   }

   for (vector<pthread_t>::iterator i = cli_threads.begin(); i != cli_threads.end(); ++ i)
   {
      pthread_join(*i, NULL);
   }
#else
   vector<HANDLE> cli_threads;
   cli_threads.resize(total_threads);

   for (vector<HANDLE>::iterator i = cli_threads.begin(); i != cli_threads.end(); ++ i)
   {
      *i = CreateThread(NULL, 0, NULL, start_and_destroy_clients, 0, NULL);
   }

   for (vector<HANDLE>::iterator i = cli_threads.begin(); i != cli_threads.end(); ++ i)
   {
      WaitForSingleObject(*i, INFINITE);
   }
#endif

   return NULL;
}


int main()
{
   const int test_case = 4;

#ifndef WIN32
   void* (*Test_Srv[test_case])(void*);
   void* (*Test_Cli[test_case])(void*);
#else
   DWORD (WINAPI *Test_Srv[test_case])(LPVOID);
   DWORD (WINAPI *Test_Cli[test_case])(LPVOID);
#endif

   Test_Srv[0] = Test_1_Srv;
   Test_Cli[0] = Test_1_Cli;
   Test_Srv[1] = Test_2_Srv;
   Test_Cli[1] = Test_2_Cli;
   Test_Srv[2] = Test_3_Srv;
   Test_Cli[2] = Test_3_Cli;
   Test_Srv[3] = Test_4_Srv;
   Test_Cli[3] = Test_4_Cli;

   for (int i = 0; i < test_case; ++ i)
   {
      cout << "Start Test # " << i + 1 << endl;

      UDT::startup();

#ifndef WIN32
      pthread_t srv, cli;
      pthread_create(&srv, NULL, Test_Srv[i], NULL);
      pthread_create(&cli, NULL, Test_Cli[i], NULL);

      pthread_join(srv, NULL);
      pthread_join(cli, NULL);
#else
      HANDLE srv, cli;
      srv = CreateThread(NULL, 0, Test_Srv[i], NULL, 0, NULL);
      cli = CreateThread(NULL, 0, Test_Cli[i], NULL, 0, NULL);

      WaitForSingleObject(srv, INFINITE);
      WaitForSingleObject(cli, INFINITE);
#endif

      UDT::cleanup();

      cout << "Test # " << i + 1 << " completed." << endl;
   }

   return 0;
}
