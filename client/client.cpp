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
   Yunhong Gu, last updated 10/29/2009
*****************************************************************************/


#include <ssltransport.h>
#include <netdb.h>
#include <crypto.h>
#include "client.h"
#include <iostream>

using namespace std;

string Client::g_strServerHost = "";
string Client::g_strServerIP = "";
int Client::g_iServerPort = 0;
CGMP Client::g_GMP;
DataChn Client::g_DataChn;
Topology Client::g_Topology;
SectorError Client::g_ErrorInfo;
int32_t Client::g_iKey = 0;
int Client::g_iCount = 0;
unsigned char Client::g_pcCryptoKey[16];
unsigned char Client::g_pcCryptoIV[8];
StatCache Client::g_StatCache;
Routing Client::g_Routing;
string Client::g_strUsername = "";
string Client::g_strPassword = "";
string Client::g_strCert = "";
set<Address, AddrComp> Client::g_sMasters;
bool Client::g_bActive = false;
pthread_t Client::g_KeepAlive;
pthread_cond_t Client::g_KACond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t Client::g_KALock = PTHREAD_MUTEX_INITIALIZER;


Client::Client()
{
}

Client::~Client()
{
}

int Client::init(const string& server, const int& port)
{
   if (g_iCount ++ > 0)
      return 1;

   g_ErrorInfo.init();

   struct hostent* serverip = gethostbyname(server.c_str());
   if (NULL == serverip)
   {
      cerr << "incorrect host name.\n";
      return -1;
   }
   g_strServerHost = server;
   char buf[64];
   g_strServerIP = inet_ntop(AF_INET, serverip->h_addr_list[0], buf, 64);
   g_iServerPort = port;

   Crypto::generateKey(g_pcCryptoKey, g_pcCryptoIV);

   Transport::initialize();
   if (g_GMP.init(0) < 0)
   {
      cerr << "unable to init GMP.\n ";
      return -1;
   }

   int dataport = 0;
   if (g_DataChn.init("", dataport) < 0)
   {
      cerr << "unable to init data channel.\n";
      return -1;
   }

   g_bActive = true;
   pthread_create(&g_KeepAlive, NULL, keepAlive, NULL);

   return 1;
}

int Client::login(const string& username, const string& password, const char* cert)
{
   if (g_iKey > 0)
      return g_iKey;

   SSLTransport::init();

   string master_cert;
   if (cert != NULL)
      master_cert = cert;
   else
      master_cert = "master_node.cert";

   int result;
   SSLTransport secconn;
   if ((result = secconn.initClientCTX(master_cert.c_str())) < 0)
      return result;
   if ((result = secconn.open(NULL, 0)) < 0)
      return result;

   if ((result = secconn.connect(g_strServerHost.c_str(), g_iServerPort)) < 0)
   {
      cerr << "cannot set up secure connection to the master.\n";
      return result;
   }

   int cmd = 2;
   secconn.send((char*)&cmd, 4);

   // send username and password
   char buf[128];
   strncpy(buf, username.c_str(), 64);
   secconn.send(buf, 64);
   strncpy(buf, password.c_str(), 128);
   secconn.send(buf, 128);

   secconn.send((char*)&g_iKey, 4);
   secconn.recv((char*)&g_iKey, 4);
   if (g_iKey < 0)
      return SectorError::E_SECURITY;

   int32_t port = g_GMP.getPort();
   secconn.send((char*)&port, 4);
   port = g_DataChn.getPort();
   secconn.send((char*)&port, 4);

   // send encryption key/iv
   secconn.send((char*)g_pcCryptoKey, 16);
   secconn.send((char*)g_pcCryptoIV, 8);

   int size = 0;
   secconn.recv((char*)&size, 4);
   if (size > 0)
   {
      char* tmp = new char[size];
      secconn.recv(tmp, size);
      g_Topology.deserialize(tmp, size);
   }

   Address addr;
   int key = 0;
   secconn.recv((char*)&key, 4);
   addr.m_strIP = g_strServerIP;
   addr.m_iPort = g_iServerPort;
   g_Routing.insert(key, addr);
   g_sMasters.insert(addr);

   int num;
   secconn.recv((char*)&num, 4);
   for (int i = 0; i < num; ++ i)
   {
      char ip[64];
      int size = 0;
      secconn.recv((char*)&key, 4);
      secconn.recv((char*)&size, 4);
      secconn.recv(ip, size);
      addr.m_strIP = ip;
      secconn.recv((char*)&addr.m_iPort, 4);
      g_Routing.insert(key, addr);
   }

   int32_t tmp;
   secconn.recv((char*)&tmp, 4);

   secconn.close();
   SSLTransport::destroy();

   g_strUsername = username;
   g_strPassword = password;
   g_strCert = master_cert;

   return g_iKey;
}

int Client::login(const string& serv_ip, const int& serv_port)
{
   Address addr;
   addr.m_strIP = serv_ip;
   addr.m_iPort = serv_port;
   if (g_sMasters.find(addr) != g_sMasters.end())
      return 0;

   if (g_iKey < 0)
      return -1;

   SSLTransport::init();

   int result;
   SSLTransport secconn;
   if ((result = secconn.initClientCTX(g_strCert.c_str())) < 0)
      return result;
   if ((result = secconn.open(NULL, 0)) < 0)
      return result;

   if ((result = secconn.connect(serv_ip.c_str(), serv_port)) < 0)
   {
      cerr << "cannot set up secure connection to the master.\n";
      return result;
   }

   int cmd = 2;
   secconn.send((char*)&cmd, 4);

   // send username and password
   char buf[128];
   strncpy(buf, g_strUsername.c_str(), 64);
   secconn.send(buf, 64);
   strncpy(buf, g_strPassword.c_str(), 128);
   secconn.send(buf, 128);

   secconn.send((char*)&g_iKey, 4);
   int32_t key = -1;
   secconn.recv((char*)&key, 4);
   if (key < 0)
      return SectorError::E_SECURITY;

   int32_t port = g_GMP.getPort();
   secconn.send((char*)&port, 4);
   port = g_DataChn.getPort();
   secconn.send((char*)&port, 4);

   // send encryption key/iv
   secconn.send((char*)g_pcCryptoKey, 16);
   secconn.send((char*)g_pcCryptoIV, 8);

   int32_t tmp;
   secconn.recv((char*)&tmp, 4);

   secconn.close();
   SSLTransport::destroy();

   g_sMasters.insert(addr);

   return 0;
}

int Client::logout()
{
   for (set<Address, AddrComp>::iterator i = g_sMasters.begin(); i != g_sMasters.end(); ++ i)
   {
      SectorMsg msg;
      msg.setKey(g_iKey);
      msg.setType(2);
      msg.m_iDataLength = SectorMsg::m_iHdrSize;
      g_GMP.rpc(i->m_strIP.c_str(), i->m_iPort, &msg, &msg);
   }

   g_sMasters.clear();

   g_iKey = 0;
   return 0;
}

int Client::close()
{
   if (-- g_iCount == 0)
   {
      if (g_iKey > 0)
         logout();

      pthread_mutex_lock(&g_KALock);
      g_bActive = false;
      pthread_cond_signal(&g_KACond);
      pthread_mutex_unlock(&g_KALock);
      pthread_join(g_KeepAlive, NULL);

      g_strServerHost = "";
      g_strServerIP = "";
      g_iServerPort = 0;
      g_GMP.close();
      Transport::release();
   }

   return 1;
}

int Client::list(const string& path, vector<SNode>& attr)
{
   string revised_path = Metadata::revisePath(path);

   SectorMsg msg;
   msg.resize(65536);
   msg.setType(101);
   msg.setKey(g_iKey);
   msg.setData(0, revised_path.c_str(), revised_path.length() + 1);

   Address serv;
   g_Routing.lookup(revised_path, serv);
   login(serv.m_strIP, serv.m_iPort);

   if (g_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   string filelist = msg.getData();

   unsigned int s = 0;
   while (s < filelist.length())
   {
      int t = filelist.find(';', s);
      SNode sn;
      sn.deserialize(filelist.substr(s, t - s).c_str());
      attr.insert(attr.end(), sn);
      s = t + 1;
   }

   return attr.size();
}

int Client::stat(const string& path, SNode& attr)
{
   string revised_path = Metadata::revisePath(path);

   SectorMsg msg;
   msg.resize(65536);
   msg.setType(102);
   msg.setKey(g_iKey);
   msg.setData(0, revised_path.c_str(), revised_path.length() + 1);

   Address serv;
   g_Routing.lookup(revised_path, serv);
   login(serv.m_strIP, serv.m_iPort);

   if (g_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   attr.deserialize(msg.getData());

   int n = (msg.m_iDataLength - SectorMsg::m_iHdrSize - 128) / 68;
   char* al = msg.getData() + 128;

   for (int i = 0; i < n; ++ i)
   {
      Address addr;
      addr.m_strIP = al + 68 * i;
      addr.m_iPort = *(int32_t*)(al + 68 * i + 64);
      attr.m_sLocation.insert(addr);
   }

   // check local cache: updated files may not be sent to the master yet
   g_StatCache.stat(path, attr);

   return 1;
}

int Client::mkdir(const string& path)
{
   string revised_path = Metadata::revisePath(path);

   SectorMsg msg;
   msg.setType(103);
   msg.setKey(g_iKey);
   msg.setData(0, revised_path.c_str(), revised_path.length() + 1);

   Address serv;
   g_Routing.lookup(revised_path, serv);
   login(serv.m_strIP, serv.m_iPort);

   if (g_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   return 1;
}

int Client::move(const string& oldpath, const string& newpath)
{
   string src = Metadata::revisePath(oldpath);
   string dst = Metadata::revisePath(newpath);

   SectorMsg msg;
   msg.setType(104);
   msg.setKey(g_iKey);

   int32_t size = src.length() + 1;
   msg.setData(0, (char*)&size, 4);
   msg.setData(4, src.c_str(), src.length() + 1);
   size = dst.length() + 1;
   msg.setData(4 + src.length() + 1, (char*)&size, 4);
   msg.setData(4 + src.length() + 1 + 4, dst.c_str(), dst.length() + 1);

   Address serv;
   g_Routing.lookup(src, serv);
   login(serv.m_strIP, serv.m_iPort);

   if (g_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   return 1;
}

int Client::remove(const string& path)
{
   string revised_path = Metadata::revisePath(path);

   SectorMsg msg;
   msg.setType(105);
   msg.setKey(g_iKey);
   msg.setData(0, revised_path.c_str(), revised_path.length() + 1);

   Address serv;
   g_Routing.lookup(revised_path, serv);
   login(serv.m_strIP, serv.m_iPort);

   if (g_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   return 1;
}

int Client::rmr(const string& path)
{
   SNode attr;
   int r = stat(path.c_str(), attr);
   if (r < 0)
      return r;

   if (attr.m_bIsDir)
   {
      vector<SNode> subdir;
      list(path, subdir);

      for (vector<SNode>::iterator i = subdir.begin(); i != subdir.end(); ++ i)
      {
         if (i->m_bIsDir)
            rmr(path + "/" + i->m_strName);
         else
            remove(path + "/" + i->m_strName);
      }
   }

   return remove(path);
}

int Client::copy(const string& src, const string& dst)
{
   string rsrc = Metadata::revisePath(src);
   string rdst = Metadata::revisePath(dst);

   SectorMsg msg;
   msg.setType(106);
   msg.setKey(g_iKey);

   int32_t size = rsrc.length() + 1;
   msg.setData(0, (char*)&size, 4);
   msg.setData(4, rsrc.c_str(), rsrc.length() + 1);
   size = rdst.length() + 1;
   msg.setData(4 + rsrc.length() + 1, (char*)&size, 4);
   msg.setData(4 + rsrc.length() + 1 + 4, rdst.c_str(), rdst.length() + 1);

   Address serv;
   g_Routing.lookup(rsrc, serv);
   login(serv.m_strIP, serv.m_iPort);

   if (g_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   return 1;
}

int Client::utime(const string& path, const int64_t& ts)
{
   string revised_path = Metadata::revisePath(path);

   SectorMsg msg;
   msg.setType(107);
   msg.setKey(g_iKey);
   msg.setData(0, revised_path.c_str(), revised_path.length() + 1);
   msg.setData(revised_path.length() + 1, (char*)&ts, 8);

   Address serv;
   g_Routing.lookup(revised_path, serv);
   login(serv.m_strIP, serv.m_iPort);

   if (g_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   return 1;
}

int Client::sysinfo(SysStat& sys)
{
   SectorMsg msg;
   msg.setKey(g_iKey);
   msg.setType(3);
   msg.m_iDataLength = SectorMsg::m_iHdrSize;

   Address serv;
   g_Routing.lookup(g_iKey, serv);
   login(serv.m_strIP, serv.m_iPort);

   if (g_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   sys.deserialize(msg.getData(), msg.m_iDataLength);

   for (vector<Address>::iterator i = sys.m_vMasterList.begin(); i != sys.m_vMasterList.end(); ++ i)
   {
      if (i->m_strIP.length() == 0)
      {
         i->m_strIP = serv.m_strIP;
         break;
      }
   }

   return 1;
}

int Client::updateMasters()
{
   SectorMsg msg;
   msg.setKey(g_iKey);

   for (map<uint32_t, Address>::iterator i = g_Routing.m_mAddressList.begin(); i != g_Routing.m_mAddressList.end(); ++ i)
   {
      msg.setType(5);

      if (g_GMP.rpc(i->second.m_strIP.c_str(), i->second.m_iPort, &msg, &msg) > 0)
      {
         Address addr;
         addr.m_strIP = i->second.m_strIP;
         addr.m_iPort = i->second.m_iPort;
         uint32_t key = i->first;
         
         g_Routing.init();
         g_Routing.insert(key, addr);

         int n = *(int32_t*)msg.getData();
         int p = 4;
         for (int m = 0; m < n; ++ m)
         {
            key = *(int32_t*)(msg.getData() + p);
            p += 4;
            addr.m_strIP = msg.getData() + p;
            p += addr.m_strIP.length() + 1;
            addr.m_iPort = *(int32_t*)(msg.getData() + p);
            p += 4;

            g_Routing.insert(key, addr);
         }

         return n + 1;
      }
   }

   return -1;
}

void* Client::keepAlive(void*)
{
   while (g_bActive)
   {
      timeval t;
      gettimeofday(&t, NULL);
      timespec ts;
      ts.tv_sec  = t.tv_sec + 60 * 10;
      ts.tv_nsec = t.tv_usec * 1000;

      pthread_mutex_lock(&g_KALock);
      pthread_cond_timedwait(&g_KACond, &g_KALock, &ts);
      pthread_mutex_unlock(&g_KALock);

      if (!g_bActive)
      {
         break;
      }

      for (set<Address, AddrComp>::iterator i = g_sMasters.begin(); i != g_sMasters.end(); ++ i)
      {
         // send keep-alive msg to each logged in master
         SectorMsg msg;
         msg.setKey(g_iKey);
         msg.setType(6);
         msg.m_iDataLength = SectorMsg::m_iHdrSize;
         g_GMP.rpc(g_strServerIP.c_str(), g_iServerPort, &msg, &msg);
      }
   }

   return NULL;
}
