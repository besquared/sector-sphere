/*****************************************************************************
Copyright (c) 2005 - 2009, The Board of Trustees of the University of Illinois.
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
   Yunhong Gu, last updated 04/23/2009
*****************************************************************************/


#ifndef __CB_DATACHN_H__
#define __CB_DATACHN_H__

#include <transport.h>
#include <topology.h>
#include <map>
#include <string>

class DataChn
{
public:
   DataChn();
   ~DataChn();

   int init(const std::string& ip, int& port);
   int getPort() {return m_iPort;}

public:
   bool isConnected(const std::string& ip, int port);

   int connect(const std::string& ip, int port);
   int remove(const std::string& ip, int port);

   int setCryptoKey(const std::string& ip, int port, unsigned char key[16], unsigned char iv[8]);

   int send(const std::string& ip, int port, int session, const char* data, int size, bool secure = false);
   int recv(const std::string& ip, int port, int session, char*& data, int& size, bool secure = false);
   int64_t sendfile(const std::string& ip, int port, int session, std::fstream& ifs, int64_t offset, int64_t size, bool secure = false);
   int64_t recvfile(const std::string& ip, int port, int session, std::fstream& ofs, int64_t offset, int64_t& size, bool secure = false);

   int recv4(const std::string& ip, int port, int session, int32_t& val);
   int recv8(const std::string& ip, int port, int session, int64_t& val);

   int64_t getRealSndSpeed(const std::string& ip, int port);

   int getSelfAddr(const std::string& peerip, int peerport, std::string& localip, int& localport);

private:
   struct RcvData
   {
      int m_iSession;
      int m_iSize;
      char* m_pcData;
   };

   struct ChnInfo
   {
      Transport* m_pTrans;
      std::vector<RcvData> m_vDataQueue;
      pthread_mutex_t m_SndLock;
      pthread_mutex_t m_RcvLock;
      pthread_mutex_t m_QueueLock;
      int m_iCount;
   };

   std::map<Address, ChnInfo*, AddrComp> m_mChannel;

   Transport m_Base;
   std::string m_strIP;
   int m_iPort;

   pthread_mutex_t m_ChnLock;

private:
   ChnInfo* locate(const std::string& ip, int port);
};


#endif
