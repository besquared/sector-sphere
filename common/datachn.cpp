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
   Yunhong Gu, last updated 09/17/2009
*****************************************************************************/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <cstring>
#include <datachn.h>

using namespace std;

DataChn::DataChn()
{
   pthread_mutex_init(&m_ChnLock, NULL);
}

DataChn::~DataChn()
{
   for (map<Address, ChnInfo*, AddrComp>::iterator i = m_mChannel.begin(); i != m_mChannel.end(); ++ i)
   {
      if (NULL != i->second->m_pTrans)
      {
         i->second->m_pTrans->close();
         delete i->second->m_pTrans;
      }
      pthread_mutex_destroy(&i->second->m_SndLock);
      pthread_mutex_destroy(&i->second->m_RcvLock);
      pthread_mutex_destroy(&i->second->m_QueueLock);

      for (vector<RcvData>::iterator j = i->second->m_vDataQueue.begin(); j != i->second->m_vDataQueue.end(); ++ j)
         delete [] j->m_pcData;

      delete i->second;
   }

   m_mChannel.clear();

   pthread_mutex_destroy(&m_ChnLock);
}

int DataChn::init(const string& ip, int& port)
{
   if (m_Base.open(port, true, true) < 0)
      return -1;

   m_strIP = ip;
   m_iPort = port;

   // add itself
   ChnInfo* c = new ChnInfo;
   c->m_pTrans = NULL;
   pthread_mutex_init(&c->m_SndLock, NULL);
   pthread_mutex_init(&c->m_RcvLock, NULL);
   pthread_mutex_init(&c->m_QueueLock, NULL);
   c->m_iCount = 1;

   Address addr;
   addr.m_strIP = ip;
   addr.m_iPort = port;
   m_mChannel[addr] = c;

   return port;
}

bool DataChn::isConnected(const string& ip, int port)
{
   // no need to connect to self
   if ((ip == m_strIP) && (port == m_iPort))
      return true;

   ChnInfo* c = locate(ip, port);
   if (NULL == c)
      return false;

   return ((NULL != c->m_pTrans) && c->m_pTrans->isConnected());
}

int DataChn::connect(const string& ip, int port)
{
   // no need to connect to self
   if ((ip == m_strIP) && (port == m_iPort))
      return 1;

   Address addr;
   addr.m_strIP = ip;
   addr.m_iPort = port;

   pthread_mutex_lock(&m_ChnLock);
   map<Address, ChnInfo*, AddrComp>::iterator i = m_mChannel.find(addr);
   if (i != m_mChannel.end())
   {
      if ((NULL != i->second->m_pTrans) && i->second->m_pTrans->isConnected())
      {
         pthread_mutex_unlock(&m_ChnLock);
         return 0;
      }
      delete i->second->m_pTrans;
      i->second->m_pTrans = NULL;
   }

   ChnInfo* c = NULL;
   if (i == m_mChannel.end())
   {
      c = new ChnInfo;
      c->m_pTrans = NULL;
      pthread_mutex_init(&c->m_SndLock, NULL);
      pthread_mutex_init(&c->m_RcvLock, NULL);
      pthread_mutex_init(&c->m_QueueLock, NULL);
      m_mChannel[addr] = c;
   }
   else
   {
      c = i->second;
   }

   pthread_mutex_unlock(&m_ChnLock);

   pthread_mutex_lock(&c->m_SndLock);
   if ((NULL != c->m_pTrans) && c->m_pTrans->isConnected())
   {
      c->m_iCount ++;
      pthread_mutex_unlock(&c->m_SndLock);
      return 0;
   }

   Transport* t = new Transport;
   t->open(m_iPort, true, true);
   int r = t->connect(ip.c_str(), port);

   if (NULL == c->m_pTrans)
      c->m_pTrans = t;
   else
   {
      Transport* tmp = c->m_pTrans;
      c->m_pTrans = t;
      delete tmp;
   }

   pthread_mutex_unlock(&c->m_SndLock);

   return r;
}

int DataChn::remove(const string& ip, int port)
{
   Address addr;
   addr.m_strIP = ip;
   addr.m_iPort = port;

   pthread_mutex_lock(&m_ChnLock);
   map<Address, ChnInfo*, AddrComp>::iterator i = m_mChannel.find(addr);
   if (i != m_mChannel.end())
   {
      -- i->second->m_iCount;
      if (0 == i->second->m_iCount)
      {
         if ((NULL != i->second->m_pTrans) && i->second->m_pTrans->isConnected())
            i->second->m_pTrans->close();
         delete i->second->m_pTrans;
         pthread_mutex_destroy(&i->second->m_SndLock);
         pthread_mutex_destroy(&i->second->m_RcvLock);
         pthread_mutex_destroy(&i->second->m_QueueLock);
         m_mChannel.erase(i);
      }
   }
   pthread_mutex_unlock(&m_ChnLock);

   return 0;
}

int DataChn::setCryptoKey(const string& ip, int port, unsigned char key[16], unsigned char iv[8])
{
   ChnInfo* c = locate(ip, port);
   if (NULL == c)
      return -1;

   c->m_pTrans->initCoder(key, iv);

   return 0;
}

DataChn::ChnInfo* DataChn::locate(const string& ip, int port)
{
   Address addr;
   addr.m_strIP = ip;
   addr.m_iPort = port;

   pthread_mutex_lock(&m_ChnLock);
   map<Address, ChnInfo*, AddrComp>::iterator i = m_mChannel.find(addr);
   if ((i == m_mChannel.end()) || ((NULL != i->second->m_pTrans) && !i->second->m_pTrans->isConnected()))
   {
       pthread_mutex_unlock(&m_ChnLock);
       return NULL;
   }
   pthread_mutex_unlock(&m_ChnLock);

   return i->second;
}

int DataChn::send(const string& ip, int port, int session, const char* data, int size, bool secure)
{
   ChnInfo* c = locate(ip, port);
   if (NULL == c)
      return -1;

   if ((ip == m_strIP) && (port == m_iPort))
   {
      // send data to self
      RcvData q;
      q.m_iSession = session;
      q.m_iSize = size;
      q.m_pcData = new char[size];
      memcpy(q.m_pcData, data, size);

      pthread_mutex_lock(&c->m_QueueLock);
      c->m_vDataQueue.push_back(q);
      pthread_mutex_unlock(&c->m_QueueLock);

      return size;
   }

   pthread_mutex_lock(&c->m_SndLock);
   c->m_pTrans->send((char*)&session, 4);
   c->m_pTrans->send((char*)&size, 4);
   if (!secure)
      c->m_pTrans->send(data, size);
   else
      c->m_pTrans->sendEx(data, size, true);
   pthread_mutex_unlock(&c->m_SndLock);

   return size;
}

int DataChn::recv(const string& ip, int port, int session, char*& data, int& size, bool secure)
{
   ChnInfo* c = locate(ip, port);
   if (NULL == c)
      return -1;

   while ((NULL == c->m_pTrans) || c->m_pTrans->isConnected())
   {
      pthread_mutex_lock(&c->m_QueueLock);
      for (vector<RcvData>::iterator q = c->m_vDataQueue.begin(); q != c->m_vDataQueue.end(); ++ q)
      {
         if (session == q->m_iSession)
         {
            size = q->m_iSize;
            data = q->m_pcData;
            c->m_vDataQueue.erase(q);

            pthread_mutex_unlock(&c->m_QueueLock);
            return size;
         }
      }
      pthread_mutex_unlock(&c->m_QueueLock);

      if (pthread_mutex_trylock(&c->m_RcvLock) != 0)
      {
         // if another thread is receiving data, wait a little while and check the queue again
         usleep(10);
         continue;
      }

      bool found = false;
      pthread_mutex_lock(&c->m_QueueLock);
      for (vector<RcvData>::iterator q = c->m_vDataQueue.begin(); q != c->m_vDataQueue.end(); ++ q)
      {
         if (session == q->m_iSession)
         {
            size = q->m_iSize;
            data = q->m_pcData;
            c->m_vDataQueue.erase(q);

            found = true;
            break;
         }
      }
      pthread_mutex_unlock(&c->m_QueueLock);

      if (found)
      {
         pthread_mutex_unlock(&c->m_RcvLock);
         return size;
      }

      if (NULL == c->m_pTrans)
      {
         // if this is local recv, just wait for the sender (aka itself) to pass the data
         pthread_mutex_unlock(&c->m_RcvLock);
         usleep(10);
         continue;
      }

      RcvData rd;
      if (c->m_pTrans->recv((char*)&rd.m_iSession, 4) < 0)
      {
         pthread_mutex_unlock(&c->m_RcvLock);
         return -1;
      }
      if (c->m_pTrans->recv((char*)&rd.m_iSize, 4) < 0)
      {
         pthread_mutex_unlock(&c->m_RcvLock);
         return -1;
      }
      if (!secure)
      {
         rd.m_pcData = new char[rd.m_iSize];
         if (c->m_pTrans->recv(rd.m_pcData, rd.m_iSize) < 0)
         {
            delete [] rd.m_pcData;
            pthread_mutex_unlock(&c->m_RcvLock);
            return -1;
         }
      }
      else
      {
         rd.m_pcData = new char[rd.m_iSize + 64];
         if (c->m_pTrans->recvEx(rd.m_pcData, rd.m_iSize, true) < 0)
         {
            delete [] rd.m_pcData;
            pthread_mutex_unlock(&c->m_RcvLock);
            return -1;
         }
      }

      if (session == rd.m_iSession)
      {
         size = rd.m_iSize;
         data = rd.m_pcData;
         pthread_mutex_unlock(&c->m_RcvLock);
         return size;
      }

      pthread_mutex_lock(&c->m_QueueLock);
      c->m_vDataQueue.push_back(rd);
      pthread_mutex_unlock(&c->m_QueueLock);

      pthread_mutex_unlock(&c->m_RcvLock);
   }

   size = 0;
   data = NULL;
   return -1;
}

int64_t DataChn::sendfile(const string& ip, int port, int session, fstream& ifs, int64_t offset, int64_t size, bool secure)
{
   ChnInfo* c = locate(ip, port);
   if (NULL == c)
      return -1;

   if ((ip == m_strIP) && (port == m_iPort))
   {
      // send data to self
      RcvData q;
      q.m_iSession = session;
      q.m_iSize = size;
      q.m_pcData = new char[size];
      ifs.seekg(offset);
      ifs.read(q.m_pcData, size);

      pthread_mutex_lock(&c->m_QueueLock);
      c->m_vDataQueue.push_back(q);
      pthread_mutex_unlock(&c->m_QueueLock);

      return size;
   }

   pthread_mutex_lock(&c->m_SndLock);
   c->m_pTrans->send((char*)&session, 4);
   c->m_pTrans->send((char*)&size, 4);
   c->m_pTrans->sendfile(ifs, offset, size);
   pthread_mutex_unlock(&c->m_SndLock);

   return size;
}

int64_t DataChn::recvfile(const string& ip, int port, int session, fstream& ofs, int64_t offset, int64_t& size, bool secure)
{
   ChnInfo* c = locate(ip, port);
   if (NULL == c)
      return -1;

   while ((NULL == c->m_pTrans) || c->m_pTrans->isConnected())
   {
      pthread_mutex_lock(&c->m_QueueLock);
      for (vector<RcvData>::iterator q = c->m_vDataQueue.begin(); q != c->m_vDataQueue.end(); ++ q)
      {
         if (session == q->m_iSession)
         {
            size = q->m_iSize;
            ofs.seekp(offset);
            ofs.write(q->m_pcData, size);
            delete [] q->m_pcData;
            c->m_vDataQueue.erase(q);

            pthread_mutex_unlock(&c->m_QueueLock);
            return size;
         }
      }
      pthread_mutex_unlock(&c->m_QueueLock);

      if (pthread_mutex_trylock(&c->m_RcvLock) != 0)
      {
         // if another thread is receiving data, wait a little while and check the queue again
         usleep(10);
         continue;
      }

      bool found = false;
      pthread_mutex_lock(&c->m_QueueLock);
      for (vector<RcvData>::iterator q = c->m_vDataQueue.begin(); q != c->m_vDataQueue.end(); ++ q)
      {
         if (session == q->m_iSession)
         {
            size = q->m_iSize;
            ofs.seekp(offset);
            ofs.write(q->m_pcData, size);
            delete [] q->m_pcData;
            c->m_vDataQueue.erase(q);

            found = true;
            break;
         }
      }
      pthread_mutex_unlock(&c->m_QueueLock);

      if (found)
      {
         pthread_mutex_unlock(&c->m_RcvLock);
         return size;
      }

      if (NULL == c->m_pTrans)
      {
         // if this is local recv, just wait for the sender (aka itself) to pass the data
         pthread_mutex_unlock(&c->m_RcvLock);
         usleep(10);
         continue;
      }

      RcvData rd;
      if (c->m_pTrans->recv((char*)&rd.m_iSession, 4) < 0)
      {
         pthread_mutex_unlock(&c->m_RcvLock);
         return -1;
      }
      if (c->m_pTrans->recv((char*)&rd.m_iSize, 4) < 0)
      {
         pthread_mutex_unlock(&c->m_RcvLock);
         return -1;
      }
      if (!secure)
      {
         if (session == rd.m_iSession)
         {
            if (c->m_pTrans->recvfile(ofs, offset, rd.m_iSize) < 0)
            {
               pthread_mutex_unlock(&c->m_RcvLock);
               return -1;
            }
         }
         else
         {
            rd.m_pcData = new char[rd.m_iSize];
            if (c->m_pTrans->recv(rd.m_pcData, rd.m_iSize) < 0)
            {
               delete [] rd.m_pcData;
               pthread_mutex_unlock(&c->m_RcvLock);
               return -1;
            }
         }
      }
      else
      {
         if (session == rd.m_iSession)
         {
            if (c->m_pTrans->recvfileEx(ofs, offset, rd.m_iSize, true) < 0)
            {
               pthread_mutex_unlock(&c->m_RcvLock);
               return -1;
            }
         }
         else
         {
            rd.m_pcData = new char[rd.m_iSize + 64];
            if (c->m_pTrans->recvEx(rd.m_pcData, rd.m_iSize, true) < 0)
            {
               delete [] rd.m_pcData;
               pthread_mutex_unlock(&c->m_RcvLock);
               return -1;
            }
         }
      }
      pthread_mutex_unlock(&c->m_RcvLock);


      if (session == rd.m_iSession)
      {
         size = rd.m_iSize;
         return size;
      }


      pthread_mutex_lock(&c->m_QueueLock);
      c->m_vDataQueue.push_back(rd);
      pthread_mutex_unlock(&c->m_QueueLock);
   }

   size = 0;
   return -1;
}

int DataChn::recv4(const string& ip, int port, int session, int32_t& val)
{
   char* buf = NULL;
   int size = 4;
   if (recv(ip, port, session, buf, size) < 0)
      return -1;

   if (size != 4)
   {
      delete [] buf;
      return -1;
   }

   val = *(int32_t*)buf;
   delete [] buf;
   return 4;
}

int DataChn::recv8(const string& ip, int port, int session, int64_t& val)
{
   char* buf = NULL;
   int size = 8;
   if (recv(ip, port, session, buf, size) < 0)
      return -1;

   if (size != 8)
   {
      delete [] buf;
      return -1;
   }

   val = *(int64_t*)buf;
   delete [] buf;
   return 8;
}

int64_t DataChn::getRealSndSpeed(const string& ip, int port)
{
   ChnInfo* c = locate(ip, port);
   if ((NULL == c) || (NULL == c->m_pTrans))
      return -1;

   return c->m_pTrans->getRealSndSpeed();
}

int DataChn::getSelfAddr(const string& peerip, int peerport, string& localip, int& localport)
{
   ChnInfo* c = locate(peerip, peerport);
   if (NULL == c)
      return -1;

   if ((peerip == m_strIP) && (peerport == m_iPort))
   {
      localip = m_strIP;
      localport = m_iPort;
      return 0;
   }

   sockaddr_in addr;
   if (c->m_pTrans->getsockname((sockaddr*)&addr) < 0)
      return -1;

   localip = inet_ntoa(addr.sin_addr);
   localport = ntohs(addr.sin_port);
   return 0;
}
