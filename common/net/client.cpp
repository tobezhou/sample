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
#include <sys/select.h>
#include <sys/types.h>

#include "client.h"

namespace myclient
{
	myClient::myClient()
	{
		m_Clientfd = -1;

		memset(m_BuffWrite, 0, sizeof(m_BuffWrite));
		memset(m_BuffRead, 0, sizeof(m_BuffRead));

		m_ConnectData.init();
	}

	myClient::~myClient()
	{
		if (m_Clientfd != -1)
		{
			close(m_Clientfd);
			m_Clientfd = -1;
		}
	}

	void myClient::init()
	{
		initSocket();
		initThread();
	}

	void myClient::initSocket()
	{
		//创建套接字
		m_Clientfd = socket(AF_INET, SOCK_STREAM, 0);

		//设置非阻塞模式
		if (!setNonblock())
		{
			perror("setNonblock error:\n");
			exit(1);
		}
	}

	void myClient::initThread()
	{
		//创建 socket 处理线程
		std::thread t(&myClient::doThreadRun, this);
		t.detach();

		//创建发送数据线程，模拟
		std::thread ts(&myClient::createData, this);
		ts.detach();

		//逻辑处理线程
		std::thread tr(&myClient::doRountine, this);
		tr.detach();
	}

	bool myClient::setNonblock()
	{
		int flags = fcntl(m_Clientfd, F_GETFL);
		if (flags < 0)
			return false;

		flags |= O_NONBLOCK;
		if (fcntl(m_Clientfd, F_SETFL, flags) < 0)
			return false;

		return true;
	}

	void myClient::connectSrv()
	{
		//连接socket
		struct sockaddr_in  servaddr;
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(PORT);
		inet_pton(AF_INET, IPADDRESS, &servaddr.sin_addr);
		int nRet = connect(m_Clientfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
		if (nRet < 0)
		{
			if (errno == EINTR || errno == EAGAIN)
			{
				//阻塞或者重新连接
				return;
			}
			if (errno == EINPROGRESS)
			{
				//connect 已经返回，但是连接还未完成，继续处理连接
				struct timeval tv;
				tv.tv_sec = 5;
				tv.tv_usec = 0;

				fd_set wSet;
				FD_ZERO(&wSet);
				FD_SET(m_Clientfd, &wSet);

				int error = select(m_Clientfd + 1, NULL, &wSet, NULL, &tv);
				switch (error)
				{
				case 0:
				case -1:
					this->disconnectSrv();
					break;
				default:
					if (FD_ISSET(m_Clientfd, &wSet))
						connectSrv();
					break;
				}
				return;
			}
			if (errno == EISCONN)
			{
				//已经连接好了
				return;
			}
		}

		//设置已经连接
		m_ConnectData.nSockfd = m_Clientfd;
	}

	void myClient::disconnectSrv()
	{
		if (m_ConnectData.nSockfd == -1) return;

		close(m_Clientfd);

		m_ConnectData.reset();

		m_Clientfd = -1;
		initSocket();
	}

	void myClient::doThreadRun()
	{
		while (true)
		{
			//未连接，先连接
			if (m_ConnectData.nSockfd < 0)
			{
				connectSrv();

				sleep(2);
				continue;
			}


			//已经连接，处理数据
			fd_set  frecv;
			FD_ZERO(&frecv);
			FD_SET(m_Clientfd, &frecv);

			int nErr = select(m_Clientfd + 1, &frecv, NULL, NULL, NULL);
			switch (nErr)
			{
			case -1:
			{
				switch (errno)
				{
				case EINTR:
					break;
				default:
					close(m_Clientfd);
					break;
				}
			}
			break;
			case 0:
				break;
			default:
				//有数据到来
				if (FD_ISSET(m_Clientfd, &frecv))
				{
					//解析 保存数据
					memset(m_BuffRead, 0, sizeof(m_BuffRead));
					ssize_t nRecvLen = recv(m_Clientfd, m_BuffRead, MAXSIZE, 0);
					if (nRecvLen > 0)
					{
						if (m_ConnectData.recvs.tail == m_ConnectData.recvs.head)
						{
							m_ConnectData.recvs.tail = 0;
							m_ConnectData.recvs.head = 0;
						}
						if (m_ConnectData.recvs.tail + nRecvLen > MAX_RECV_LEN)
						{
							printf("err: recv buff max, fd = %d", m_Clientfd);
							break;
						}

						memcpy(&m_ConnectData.recvs.buf[m_ConnectData.recvs.tail], m_BuffRead, nRecvLen);
						m_ConnectData.recvs.tail += (int)nRecvLen;

						//printf("read message fd:%d, len:%d, data:%s\n", m_Clientfd, nRecvLen, m_BuffRead);
					}
				}
				break;
			}
		}
	}

	void myClient::createData()
	{
		int nCmd = 0;
		while (true)
		{
			if (m_Clientfd != -1)
			{
				nCmd++;
				srand((unsigned int)time(NULL));
				int nRand = rand() % 5 + 1;
				char data[MAXSIZE] = { 0 };
				memset(data, nRand, nRand);

				createPackage(nCmd, data, nRand);
			}

			sleep(2);
		}
	}

	void myClient::doRountine()
	{
		while (true)
		{
			if (m_Clientfd < 0)
				continue;

			readPackage();

			//发送数据处理
			doSendData();

			usleep(500);
		}
	}

	void myClient::doSendData()
	{
		if (m_Clientfd < 0)
			return;

		int nLen = m_ConnectData.sends.tail - m_ConnectData.sends.head;
		if (nLen <= 0)
			return;

		ssize_t nWrite = send(m_Clientfd, &m_ConnectData.sends.buf[m_ConnectData.sends.head], nLen, 0);
		if (nWrite > 0)
		{
			m_ConnectData.sends.head += (int)nWrite;
			return;
		}

		switch (errno)
		{
		case EINTR:
		case EAGAIN:
			break;
		default:
			close(m_Clientfd);
			return;
		}
	}

	void myClient::readPackage()
	{
		//for循环处理多个数据包
		for (int i = 0; i < 100; i++)
		{
			int nSize = m_ConnectData.recvs.tail - m_ConnectData.recvs.head;
			if (nSize < 8)
				return;

			int nDataLen = *(int*)(m_ConnectData.recvs.buf + m_ConnectData.recvs.head);
			if (m_ConnectData.recvs.head + nDataLen > m_ConnectData.recvs.tail)
				return;

			int nCmd = *(int*)(m_ConnectData.recvs.buf + m_ConnectData.recvs.head + 4);

			char buff[MAXSIZE];
			memcpy(&buff[0], &m_ConnectData.recvs.buf[m_ConnectData.recvs.head + 8], nDataLen - 8);

			m_ConnectData.recvs.head += nDataLen;
			printf("readPackage message : nDataLen:%d, nCmd:%d, data:%d\n", nDataLen, nCmd, *(int*)(&buff[0]));

			handCommand(nCmd);
		}
	}

	void myClient::handCommand(int nCmd)
	{
		switch (nCmd)
		{
		case 1:
			break;
		default:
			break;
		}
	}

	void myClient::createPackage(int nCmd, void * pData, int nLen)
	{
		if (nullptr == pData)
			return;

		if (m_ConnectData.sends.tail == m_ConnectData.sends.head)
		{
			m_ConnectData.sends.tail = 0;
			m_ConnectData.sends.head = 0;
		}

		if (m_ConnectData.sends.tail + nLen >= MAX_SEND_LEN)
		{
			printf("err: createPackage buff max, fd = %d, nLen:%d", m_Clientfd, nLen);
			return;
		}

		int nDataLen = 8 + nLen;
		char* p = (char*)& nDataLen;
		for (int i = 0; i < 4; i++)
			m_ConnectData.sends.buf[m_ConnectData.sends.tail + i] = p[i];
		p = (char*)& nCmd;
		for (int i = 0; i < 4; i++)
			m_ConnectData.sends.buf[m_ConnectData.sends.tail + 4 + i] = p[i];

		memcpy(&m_ConnectData.sends.buf[m_ConnectData.sends.tail + 8], pData, nLen);
		m_ConnectData.sends.tail += nDataLen;
	}
}