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

		//���÷�����
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
		int  m_Listenfd;	//����socket
		int  m_Epollfd;		//epoll����

		std::list<int> m_Readfd;	//���Զ�������socketfd

		std::mutex m_Mutex_Accept;//����������
		std::mutex m_Mutex_Read;  //��ȡ������

		std::condition_variable m_Accept_Cond;	//����������
		std::condition_variable m_Read_Cond;	//��������

		char m_Buff[MAXSIZE];	//������

		hashList<CONNECT_DATA>*		m_Connect_Data;		//����������������
		hashList<CONNECT_INDEX>*	m_Connect_Idx;		//�������ݵ��±꣬��������±꣬������ٷ��� m_Connect_Data
	};
}

#endif