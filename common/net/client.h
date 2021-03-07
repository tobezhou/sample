#ifndef __CLIENT_H
#define __CLIENT_H

#include <mutex>

#include "net.h"

#define IPADDRESS   "192.168.111.128"
#define PORT        5555
#define MAXSIZE     1024

namespace myclient
{
	class myClient
	{
	public:
		myClient();
		~myClient();

		void init();
		void initSocket();
		void initThread();

		//���÷�����
		bool setNonblock();

		void connectSrv();
		void disconnectSrv();

		void doThreadRun();
		void createData();
		void doRountine();
		void doSendData();

		void readPackage();
		void handCommand(int nCmd);
		void createPackage(int nCmd, void* pData, int nLen);
	private:
		int  m_Clientfd;	//�ͻ���socket

		std::mutex m_MutexWrite;		//д��

		char m_BuffWrite[MAXSIZE];
		char m_BuffRead[MAXSIZE];

		common::CONNECT_DATA m_ConnectData;
	};
}

#endif