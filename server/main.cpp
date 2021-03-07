#include <unistd.h>
#include <cstring>
#include <cstdio>

#include "server.h"

using namespace myserver;

int main()
{
	mySrv ms;
	ms.startUp();

	char sCmdBuf[512];
	while (true)
	{
		sCmdBuf[0] = 0;
		scanf("%s", sCmdBuf);

		if (strcmp(sCmdBuf, "exit") == 0)
			break;

		sleep(2);
	}

	return 0;
}