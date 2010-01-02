/*****************************************************************************
Copyright (c) 2005 - 2010, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu, last updated 01/01/2010
*****************************************************************************/


#ifndef __GMP_H__
#define __GMP_H__

#ifndef WIN32
   #include <pthread.h>
   #include <sys/time.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <sys/stat.h>
   #include <netinet/in.h>
   #include <arpa/inet.h>

   #define GMP_API
#else
   #include <windows.h>

   #ifdef GMP_EXPORTS
      #define GMP_API __declspec(dllexport)
   #else
      #define GMP_API __declspec(dllimport)
   #endif
#endif

#include <transport.h>
#include <message.h>
#include <prec.h>

#include <string>
#include <cstring>
#include <map>
#include <list>
#include <queue>
#include <vector>

class CGMPMessage
{
public:
   CGMPMessage();
   CGMPMessage(const CGMPMessage& msg);
   ~CGMPMessage();

   int32_t& m_iType;		// 0 Data; 1 ACK
   int32_t& m_iSession;
   int32_t& m_iID;		// message ID
   int32_t& m_iInfo;		//

   char* m_pcData;
   int m_iLength;

   int32_t m_piHeader[4];

public:
   void pack(const char* data, const int& len, const int32_t& info = 0);
   void pack(const int32_t& type, const int32_t& info = 0);

public:
   static int32_t g_iSession;

private:
   static int32_t initSession();

   static int32_t g_iID;
   static pthread_mutex_t g_IDLock;
   static const int32_t g_iMaxID = 0xFFFFFFF;
   static const int m_iHdrSize = 16;
};

struct CMsgRecord
{
   char m_pcIP[64];
   int m_iPort;
   CGMPMessage* m_pMsg;
   int64_t m_llTimeStamp;
};

struct CFMsgRec
{
   bool operator()(const CMsgRecord* m1, const CMsgRecord* m2) const
   {
      if (strcmp(m1->m_pcIP, m2->m_pcIP) == 0)
      {
         if (m1->m_iPort == m2->m_iPort)
            return m1->m_pMsg->m_iID > m2->m_pMsg->m_iID;

         return (m1->m_iPort > m2->m_iPort);
      }
      
      return (strcmp(m1->m_pcIP, m2->m_pcIP) > 0);
   }
};


class GMP_API CGMP
{
public:
   CGMP();
   ~CGMP();

public:
   int init(const int& port = 0);
   int close();
   int getPort();

private:
   int UDPsend(const char* ip, const int& port, int32_t& id, const char* data, const int& len, const bool& reliable = true);
   int UDPsend(const char* ip, const int& port, CGMPMessage* msg);
   int UDTsend(const char* ip, const int& port, int32_t& id, const char* data, const int& len);
   int UDTsend(const char* ip, const int& port, CGMPMessage* msg);

public:
   int sendto(const char* ip, const int& port, int32_t& id, const CUserMessage* msg);
   int recvfrom(char* ip, int& port, int32_t& id, CUserMessage* msg, const bool& block = true);
   int recv(const int32_t& id, CUserMessage* msg);
   int rpc(const char* ip, const int& port, CUserMessage* req, CUserMessage* res);
   int multi_rpc(const std::vector<char*>& ips, const std::vector<int>& ports, CUserMessage* req);

   int rtt(const char* ip, const int& port, const bool& clear = false);

private:
   pthread_t m_SndThread;
   pthread_t m_RcvThread;
   pthread_t m_UDTRcvThread;
#ifndef WIN32
   static void* sndHandler(void*);
   static void* rcvHandler(void*);
   static void* udtRcvHandler(void*);
#else
   static DWORD WINAPI sndHandler(LPVOID);
   static DWORD WINAPI rcvHandler(LPVOID);
   static DWORD WINAPI udtRcvHandler(LPVOID);
#endif

   pthread_mutex_t m_SndQueueLock;
   pthread_cond_t m_SndQueueCond;
   pthread_mutex_t m_RcvQueueLock;
   pthread_cond_t m_RcvQueueCond;
   pthread_mutex_t m_ResQueueLock;
   pthread_cond_t m_ResQueueCond;
   pthread_mutex_t m_RTTLock;
   pthread_cond_t m_RTTCond;

private:
   int m_iPort;

   #ifndef WIN32
      int m_UDPSocket;
   #else
      SOCKET m_UDPSocket;
   #endif

   Transport m_UDTSocket;
   int m_iUDTReusePort;

   std::list<CMsgRecord*> m_lSndQueue;
   std::queue<CMsgRecord*> m_qRcvQueue;
   std::map<int32_t, CMsgRecord*> m_mResQueue;
   CPeerManagement m_PeerHistory;

   volatile bool m_bInit;
   volatile bool m_bClosed;

private:
   static const int m_iMaxUDPMsgSize = 1456;
};

#endif
