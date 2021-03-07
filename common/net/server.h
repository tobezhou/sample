#ifndef __SERVER_H
#define __SERVER_H

#include <list>
#include <condition_variable>

#include "net.h"
using namespace common;

#define IPADDRESS   "192.168.111.128"
#define PORT        5555
#define MAXSIZE     1024
#define LISTENQ     5
#define FDSIZE      1000
#define EPOLLEVENTS 100
#define MAX_PLAYER_CNT		10000
#define MAX_SOCKETFD_LEN	1000000

namespace myserver
{
	class mySrv
	{
	public:
		mySrv();
		~mySrv();

		void startUp();
		void initSocket();
		void initThread();

		//设置非阻塞
		bool setNonblock();
		void shutDown(int nSockfd);

		void doEpoll();
		void doAccept();
		void doRead();
		void doRountine();

		void readSockfd(int nSockfd);
		void writeSockfd(CONNECT_DATA* c);

		void readPackage(CONNECT_DATA* c);
		void handCommand(CONNECT_DATA* c, int nCmd);
		void createPackage(CONNECT_DATA* c, int nCmd, void* pData, int nLen);

		void epollCtl(int nEpfd, int nOp, int nFd, unsigned int nEvent);

		CONNECT_DATA* findFreeData();
		CONNECT_INDEX* findConnectIndex(const int nSocketfd);

		CONNECT_DATA* findConnectData(const int nSocketfd);
		CONNECT_DATA* findConnectDataIdx(const int nIndex);
	private:
		int  m_Listenfd;	//监听socket
		int  m_Epollfd;		//epoll对象

		std::list<int> m_Readfd;	//可以读操作的socketfd

		std::mutex m_Mutex_Accept;//监听互拆锁
		std::mutex m_Mutex_Read;  //读取互拆锁

		std::condition_variable m_Accept_Cond;	//监听条件锁
		std::condition_variable m_Read_Cond;	//读条件锁

		char m_Buff[MAXSIZE];	//缓冲区

		hashList<CONNECT_DATA>*		m_Connect_Data;		//管理所有连接数据
		hashList<CONNECT_INDEX>*	m_Connect_Idx;		//连接数据的下标，保存的是下标，方便快速访问 m_Connect_Data
	};
}

#endif