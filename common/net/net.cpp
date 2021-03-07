#include "net.h"

#include <string.h>
#include <unistd.h>

namespace common
{
	void CONNECT_DATA::init()
	{
		recvs.buf = new char[MAX_RECV_LEN];
		sends.buf = new char[MAX_SEND_LEN];

		reset();
	}

	void CONNECT_DATA::reset()
	{
		nIdx = -1;
		nSockfd = -1;

		recvs.head = 0;
		recvs.tail = 0;
		recvs.isCompleted = false;
		memset(recvs.buf, 0, MAX_RECV_LEN);

		sends.head = 0;
		sends.tail = 0;
		sends.isCompleted = true;
		memset(sends.buf, 0, MAX_SEND_LEN);
	}
}