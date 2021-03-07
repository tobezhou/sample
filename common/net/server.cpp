#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <thread>
#include <fcntl.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/types.h>

#include "server.h"

namespace myserver
{
	mySrv::mySrv()
	{
		m_Listenfd = -1;
		m_Epollfd = -1;
		memset(m_Buff, 0, sizeof(m_Buff));

		m_Connect_Data = nullptr;
		m_Connect_Idx = nullptr;
	}

	mySrv::~mySrv()
	{
		if (m_Listenfd != -1)
		{
			close(m_Listenfd);
			m_Listenfd = -1;
		}

		if (m_Epollfd != -1)
		{
			close(m_Epollfd);
			m_Epollfd = -1;
		}

		if (m_Connect_Data)
		{
			free(m_Connect_Data);
			m_Connect_Data = nullptr;
		}

		if (m_Connect_Idx)
		{
			free(m_Connect_Idx);
			m_Connect_Idx = nullptr;
		}
	}

	void mySrv::startUp()
	{
		m_Connect_Data = new hashList<CONNECT_DATA>(MAX_PLAYER_CNT);
		for (int i = 0; i < MAX_PLAYER_CNT; i++)
		{
			CONNECT_DATA* data = m_Connect_Data->Value(i);
			data->init();
		}

		m_Connect_Idx = new hashList<CONNECT_INDEX>(MAX_SOCKETFD_LEN);
		for (int i = 0; i < MAX_SOCKETFD_LEN; i++)
		{
			CONNECT_INDEX* idx = m_Connect_Idx->Value(i);
			idx->Reset();
		}

		initSocket();
		initThread();
	}

	void mySrv::initSocket()
	{
		//创建套接字
		m_Listenfd = socket(AF_INET, SOCK_STREAM, 0);
		if (m_Listenfd == -1)
		{
			perror("socket error:\n");
			exit(1);
		}

		//设置非阻塞模式
		if (!setNonblock())
		{
			perror("setNonblock error:\n");
			exit(1);
		}
		
		//设置端口号重复绑定
		int flag = 1;
		int nRet = setsockopt(m_Listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
		if (nRet == -1)
		{
			perror("setsockopt error:\n");
			exit(1);
		}

		//绑定IP端口
		struct sockaddr_in servaddr;
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		inet_pton(AF_INET, IPADDRESS, &servaddr.sin_addr);
		servaddr.sin_port = htons(PORT);
		nRet = bind(m_Listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
		if (nRet == -1)
		{
			perror("bind error: \n");
			exit(1);
		}

		//开始监听socket
		nRet = listen(m_Listenfd, LISTENQ);
		if (nRet == -1)
		{
			perror("listen error: \n");
			exit(1);
		}

		//创建一个描述符
		m_Epollfd = epoll_create(FDSIZE);

		//添加监听描述符事件
		epollCtl(m_Epollfd, EPOLL_CTL_ADD, m_Listenfd, EPOLLIN);
	}

	bool mySrv::setNonblock()
	{
		int flags = fcntl(m_Listenfd, F_GETFL);
		if (flags < 0)
			return false;

		flags |= O_NONBLOCK;
		if (fcntl(m_Listenfd, F_SETFL, flags) < 0)
			return false;

		return true;
	}

	void mySrv::shutDown(int nSockfd)
	{
		auto c = findConnectData(nSockfd);
		if (nullptr == c)
			return;

		epollCtl(m_Epollfd, EPOLL_CTL_DEL, nSockfd, EPOLLIN | EPOLLET);
		c->reset();
		close(nSockfd);
	}

	void mySrv::initThread()
	{
		//创建epoll处理线程
		std::thread te(&mySrv::doEpoll, this);
		//创建监听线程
		std::thread ta(&mySrv::doAccept, this);
		//创建读线程
		std::thread tr(&mySrv::doRead, this);
// 		//创建写线程
// 		std::thread tw(&mySrv::doWrite, this);
		//创建业务线程
		std::thread t(&mySrv::doRountine, this);

		te.detach();
		ta.detach();
		tr.detach();
		//tw.detach();
		t.detach();
	}

	void mySrv::doEpoll()
	{
		//处理epoll事件
		struct epoll_event events[EPOLLEVENTS];
		for ( ; ; )
		{
			//获取已经准备好的描述符事件
			int nEventCnt = epoll_wait(m_Epollfd, events, EPOLLEVENTS, -1);
			for (int i = 0; i < nEventCnt; i++)
			{
				int fd = events[i].data.fd;
				//处理新的连接，如果未处理该连接，这里会重复触发，直到该连接被 accept 内核才不会返回
				if (fd == m_Listenfd)
				{
					m_Accept_Cond.notify_one();
				}
				//处理读事件
				else if (events[i].events & EPOLLIN)
				{
					{
						std::unique_lock<std::mutex> l_read(m_Mutex_Read);
						m_Readfd.push_back(fd);
					}

					m_Read_Cond.notify_one();
				}

				//写事件单独一个线程处理
			}
		}
	}

	void mySrv::doAccept()
	{
		while (true)
		{
			{
				//在作用域上锁，出了作用域释放锁
				std::unique_lock<std::mutex> l_accept(m_Mutex_Accept);
				m_Accept_Cond.wait(l_accept);
			}

			struct sockaddr_in cliaddr;
			socklen_t  cliaddrlen;
			int clifd = accept(m_Listenfd, (struct sockaddr*)&cliaddr, &cliaddrlen);
			if (clifd == -1) 
			{
				if (errno != EAGAIN &&
					errno != ECONNABORTED &&
					errno != EPROTO &&
					errno != EINTR)
				{
					perror("accept error:");
				}
				continue;
			}

			auto c = findFreeData();
			auto idx = findConnectIndex(clifd);
			if (nullptr == c || nullptr == idx)
			{
				printf("doAccept error: clifd = %d\n", clifd);
				continue;
			}

			idx->nIndex = c->nIdx;
			c->nSockfd = clifd;

			//设置非阻塞模式
			fcntl(clifd, F_SETFL, O_NONBLOCK);

			//添加一个客户描述符和事件
			epollCtl(m_Epollfd, EPOLL_CTL_ADD, clifd, EPOLLIN | EPOLLET); //设置边缘触发，之后内核缓冲区有数据就不会一直触发读写事件

			printf("\naccept a new client:fd:%d, %s:%d\n", clifd, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
			
			int nData = 0;
			createPackage(c, 1, &nData, sizeof(nData));
		}
	}

	void mySrv::doRead()
	{
		while (true)
		{
			int fd = -1;
			{
				std::unique_lock<std::mutex> l_read(m_Mutex_Read);
				while (m_Readfd.empty())
				{
					m_Read_Cond.wait(l_read);
				}

				fd = m_Readfd.front();
				m_Readfd.pop_front();
			}
			if (fd != -1)
			{
				readSockfd(fd);
			}
		}
	}

	void mySrv::doRountine()
	{
		while (true)
		{
			for (int i = 0; i < m_Connect_Data->Count(); i++)
			{
				auto c = m_Connect_Data->Value(i);
				if (c->nSockfd < 0)
					continue;

				readPackage(c);

				//发送数据处理
				writeSockfd(c);
			}

			usleep(500);
		}
	}

	void mySrv::readSockfd(int nSockfd)
	{
		auto c = findConnectData(nSockfd);
		if (nullptr == c)
			return;

		//ET模式，一直读到没有数据为止
		while (true)
		{
			memset(&m_Buff, 0, sizeof(m_Buff));
			ssize_t nRead = recv(nSockfd, m_Buff, MAXSIZE, 0);
			if (nRead < 0)
			{
				if (errno == EINTR)
					continue;
				else if (errno == EAGAIN)
					break;
				else
				{
					c->recvs.isCompleted = true;
					perror("read error:\n");
					shutDown(nSockfd);
					return;
				}
			}
			else if (nRead == 0)
			{
				shutDown(nSockfd);
				return;
			}
			
			if (c->recvs.head == c->recvs.tail)
			{
				c->recvs.tail = 0;
				c->recvs.head = 0;
			}

			if (c->recvs.tail + nRead > MAX_RECV_LEN)
			{
				printf("err: recv buff max, fd = %d", nSockfd);
				return;
			}

			memcpy(&c->recvs.buf[c->recvs.tail], m_Buff, nRead);
			c->recvs.tail += (int)nRead;

			if (nRead >= MAX_RECV_LEN)
				break;
		}

		c->recvs.isCompleted = true;
	}

	void mySrv::writeSockfd(CONNECT_DATA * c)
	{
		if (nullptr == c || c->nSockfd == -1)
			return;

		int nLen = c->sends.tail - c->sends.head;
		if (nLen <= 0)
			return;

		ssize_t nWrite = send(c->nSockfd, &c->sends.buf[c->sends.head], nLen, 0);
		if (nWrite > 0)
		{
			int nPackLen = *(int*)(&c->sends.buf[c->sends.head]);
			int nCmd = *(int*)(&c->sends.buf[c->sends.head+4]);
			int nData = *(int*)(&c->sends.buf[c->sends.head + 8]);
			printf("writeSockfd:%d, nWrite:%d, nPackLen:%d, nCmd:%d, nData:%d\n", c->nSockfd, nWrite, nPackLen, nCmd, nData);

			c->sends.head += (int)nWrite;
			c->sends.isCompleted = true;
			return;
		}
		if (nWrite < 0)
		{
			if (errno == EINTR)
				return;
			else if (errno == EAGAIN)
				return;
			else
			{
				shutDown(c->nSockfd);
				return;
			}
		}
		else if (nWrite == 0)
		{
			shutDown(c->nSockfd);
			return;
		}
	}

	void mySrv::readPackage(CONNECT_DATA * c)
	{
		if (nullptr == c || !c->recvs.isCompleted)
			return;

		//for循环处理多个数据包
		for (int i = 0; i < 100; i++)
		{
			int nSize = c->recvs.tail - c->recvs.head;
			if (nSize < 8)
				return;

			int nDataLen = *(int*)(c->recvs.buf + c->recvs.head);
			if (c->recvs.head + nDataLen > c->recvs.tail)
				return;

			int nCmd = *(int*)(c->recvs.buf + c->recvs.head + 4);

			char buff[MAXSIZE];
			memcpy(buff, &c->recvs.buf[c->recvs.head + 8], nDataLen - 8);

			c->recvs.head += nDataLen;
			//printf("handCommand message : nDataLen:%d, nCmd:%d, data:%d\n", nDataLen, nCmd, buff[nDataLen - 8 - 1]);

			handCommand(c, nCmd);
		}
	}

	void mySrv::handCommand(CONNECT_DATA* c, int nCmd)
	{
		if (nullptr == c)
			return;

		switch (nCmd)
		{
		case 1:

			break;
		default:
			break;
		}

		//模拟
		if (nCmd % 5 == 0)
		{
			srand((unsigned int)time(NULL));
			int nRand = rand() % 10;
			createPackage(c, nCmd, &nRand, sizeof(nRand));
			printf("createPackage nCmd = %d, nRand = %d\n", nCmd, nRand);
		}
	}

	void mySrv::createPackage(CONNECT_DATA* c, int nCmd, void* pData, int nLen)
	{
		if (nullptr == c || nullptr == pData)
			return;

		if (c->sends.tail == c->sends.head)
		{
			c->sends.tail = 0;
			c->sends.head = 0;
		}

		if (c->sends.tail + nLen >= MAX_SEND_LEN)
		{
			printf("err: createPackage buff max, fd = %d", c->nSockfd);
			return;
		}

		int nDataLen = 8 + nLen;
		char* p = (char*)& nDataLen;
		for (int i = 0; i < 4; i++)
			c->sends.buf[c->sends.tail + i] = p[i];
		p = (char*)& nCmd;
		for (int i = 0; i < 4; i++)
			c->sends.buf[c->sends.tail + 4 + i] = p[i];

		memcpy(&c->sends.buf[c->sends.tail + 8], pData, nLen);
		c->sends.tail += nDataLen;
	}

	void mySrv::epollCtl(int nEpfd, int nOp, int nFd, unsigned int nEvent)
	{
		if (nEpfd != -1 && nFd != -1)
		{
			struct epoll_event ev;
			ev.events = nEvent;
			ev.data.fd = nFd;
			epoll_ctl(nEpfd, nOp, nFd, &ev);
		}
	}

	CONNECT_DATA* mySrv::findFreeData()
	{
		for (int i = 0; i < m_Connect_Data->Count(); i++)
		{
			CONNECT_DATA* c = m_Connect_Data->Value(i);
			if (c->nIdx == -1)
			{
				c->reset();
				c->nIdx = i;
				return c;
			}
		}
		return nullptr;
	}

	CONNECT_INDEX* mySrv::findConnectIndex(const int nSocketfd)
	{
		if (nSocketfd < 0 || nSocketfd >= MAX_SOCKETFD_LEN)
		{
			return nullptr;
		}

		CONNECT_INDEX* idx = m_Connect_Idx->Value(nSocketfd);
		return idx;
	}

	CONNECT_DATA* mySrv::findConnectData(const int nSocketfd)
	{
		CONNECT_INDEX* idx = findConnectIndex(nSocketfd);
		if (!idx) return nullptr;

		return findConnectDataIdx(idx->nIndex);
	}

	CONNECT_DATA* mySrv::findConnectDataIdx(const int nIndex)
	{
		if (nIndex >= 0 && nIndex < m_Connect_Data->Count())
		{
			return m_Connect_Data->Value(nIndex);
		}
		return nullptr;
	}
}