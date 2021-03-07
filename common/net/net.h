#ifndef __NET_H
#define __NET_H

#include  "malloc.h"

#define MAX_SEND_LEN  (1024)
#define MAX_RECV_LEN  (1024)

#pragma pack(push,1)
namespace net
{
	struct head
	{
		int nLen;
		int nType;
		head() : nLen(0), nType(0)
		{
		}
	};
}
#pragma pack(pop)

namespace common
{
	struct CONNECT_INDEX
	{
		int nIndex;
		inline void Reset() { nIndex = -1; }
	};
	struct BUFFS_DATA
	{
		char*   buf;	//»º³åÇø
		int     head;	//Æ«ÒÆÍ·
		int     tail;	//Æ«ÒÆÎ²
		bool    isCompleted;
	};

	template <class T>
	class hashList
	{
	public:
		hashList() {};
		hashList(int nCnt)
		{
			if (nCnt < 0)
				return;
			m_nCount = nCnt;
			m_Data = malloc(sizeof(T)*m_nCount);
		}
		virtual ~hashList()
		{
			if (m_Data)
				free(m_Data);
			m_nCount = 0;
			m_Data = nullptr;
		}

		T* Value(int nIdx)
		{
			T* data = (T*)m_Data;
			return &data[nIdx];
		}

		inline int Count() 
		{
			return m_nCount;
		}

	private:
		int m_nCount;
		void* m_Data;
	};

	struct CONNECT_DATA
	{
		void init();
		void reset();

		int nIdx;
		int nSockfd;
		BUFFS_DATA recvs;	//½ÓÊÕ»º³åÇø
		BUFFS_DATA sends;	//·¢ËÍ»º³åÇø
	};
}

#endif