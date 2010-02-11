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
   Yunhong Gu, last updated 01/03/2010
*****************************************************************************/


#include "slave.h"
#include <ssltransport.h>
#include <iostream>
#include <dirent.h>
#include <netdb.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <sys/times.h>
#include <utime.h>
#include "common.h"

using namespace std;

Slave::Slave():
m_iSlaveID(-1),
m_iDataPort(0),
m_iLocalPort(0),
m_strBase("./"),
m_pLocalFile(NULL)
{
}

Slave::~Slave()
{
   delete m_pLocalFile;
}

int Slave::init(const char* base)
{
   if (NULL != base)
      m_strBase = base;

   string conf = m_strBase + "/../conf/slave.conf";
   if (m_SysConfig.init(conf) < 0)
   {
      cerr << "unable to initialize from configuration file; quit.\n";
      return -1;
   }

   // obtain master IP address
   m_strMasterHost = m_SysConfig.m_strMasterHost;
   struct hostent* masterip = gethostbyname(m_strMasterHost.c_str());
   if (NULL == masterip)
   {
      cerr << "invalid master address " << m_strMasterHost << endl;
      return -1;
   }
   char buf[64];
   m_strMasterIP = inet_ntop(AF_INET, masterip->h_addr_list[0], buf, 64);
   m_iMasterPort = m_SysConfig.m_iMasterPort;

   Transport::initialize();

   // init GMP
   m_GMP.init(0);
   m_iLocalPort = m_GMP.getPort();

   // initialize local directory
   m_strHomeDir = m_SysConfig.m_strHomeDir;

   // initialize slave log
   m_SectorLog.init((m_strHomeDir + ".sector.log").c_str());

   // check local directory
   if (createSysDir() < 0)
   {
      cerr << "unable to create system directory " << m_strHomeDir << endl;
      return -1;
   }

   //copy permanent sphere libraries
   system((string("cp ") + m_strBase + "/sphere/*.so "  + m_strHomeDir + "/.sphere/perm/").c_str());

   cout << "scanning " << m_strHomeDir << endl;
   if (m_SysConfig.m_MetaType == DISK)
      m_pLocalFile = new Index2;
   else
      m_pLocalFile = new Index;
   m_pLocalFile->init(m_SysConfig.m_strHomeDir + ".metadata");
   m_pLocalFile->scan(m_strHomeDir.c_str(), "/");

   m_SectorLog.insert("Sector slave started.");

   return 1;
}

int Slave::connect()
{
   // join the server
   SSLTransport::init();

   string cert = m_strBase + "/../conf/master_node.cert";

   // calculate total available disk size
   struct statfs64 slavefs;
   statfs64(m_SysConfig.m_strHomeDir.c_str(), &slavefs);
   int64_t availdisk = slavefs.f_bfree * slavefs.f_bsize;

   m_iSlaveID = -1;

   m_pLocalFile->serialize("/", m_strHomeDir + ".tmp/metadata.dat");

   set<Address, AddrComp> masters;
   Address m;
   m.m_strIP = m_strMasterIP;
   m.m_iPort = m_iMasterPort;
   masters.insert(m);
   bool first = true;

   while (!masters.empty())
   {
      string mip = masters.begin()->m_strIP;
      int mport = masters.begin()->m_iPort;
      masters.erase(masters.begin());

      SSLTransport secconn;
      secconn.initClientCTX(cert.c_str());
      secconn.open(NULL, 0);
      if (secconn.connect(mip.c_str(), mport) < 0)
      {
         cerr << "unable to set up secure channel to the master.\n";
         return -1;
      }

      if (first)
      {
         secconn.getLocalIP(m_strLocalHost);

         // init data exchange channel
         m_iDataPort = 0;
         if (m_DataChn.init(m_strLocalHost, m_iDataPort) < 0)
         {
            cerr << "unable to create data channel.\n";
            secconn.close();
            return -1;
         }
      }

      int32_t cmd = 1;
      secconn.send((char*)&cmd, 4);

      int32_t size = m_strHomeDir.length() + 1;
      secconn.send((char*)&size, 4);
      secconn.send(m_strHomeDir.c_str(), size);

      int32_t res = -1;
      secconn.recv((char*)&res, 4);
      if (res < 0)
      {
         cerr << "slave join rejected. code: " << res << endl;
         return res;
      }

      secconn.send((char*)&m_iLocalPort, 4);
      secconn.send((char*)&m_iDataPort, 4);
      secconn.send((char*)&(availdisk), 8);
      secconn.send((char*)&(m_iSlaveID), 4);

      if (first)
         m_iSlaveID = res;

      struct stat s;
      stat((m_strHomeDir + ".tmp/metadata.dat").c_str(), &s);
      size = s.st_size;
      secconn.send((char*)&size, 4);
      secconn.sendfile((m_strHomeDir + ".tmp/metadata.dat").c_str(), 0, size);

      if (!first)
      {
         secconn.close();
         continue;
      }

      // move out-of-date files to the ".attic" directory
      size = 0;
      secconn.recv((char*)&size, 4);

      if (size > 0)
      {
         secconn.recvfile((m_strHomeDir + ".tmp/metadata.left.dat").c_str(), 0, size);

         Metadata* attic = NULL;
         if (m_SysConfig.m_MetaType == DISK)
            attic = new Index2;
         else
            attic = new Index;
         attic->init(m_strHomeDir + ".tmp/metadata.left");
         attic->deserialize("/", m_strHomeDir + ".tmp/metadata.left.dat", NULL);
         unlink((m_strHomeDir + ".tmp/metadata.left.dat").c_str());

         vector<string> fl;
         attic->list_r("/", fl);
         for (vector<string>::iterator i = fl.begin(); i != fl.end(); ++ i)
            move(*i, ".attic", "");

         attic->clear();
         delete attic;

         m_SectorLog.insert("WARNING: certain files have been moved to ./attic due to conflicts.");
      }

      int id = 0;
      secconn.recv((char*)&id, 4);
      Address addr;
      addr.m_strIP = mip;
      addr.m_iPort = mport;
      m_Routing.insert(id, addr);

      int num;
      secconn.recv((char*)&num, 4);
      for (int i = 0; i < num; ++ i)
      {
         char ip[64];
         size = 0;
         secconn.recv((char*)&id, 4);
         secconn.recv((char*)&size, 4);
         secconn.recv(ip, size);
         addr.m_strIP = ip;
         secconn.recv((char*)&addr.m_iPort, 4);

         m_Routing.insert(id, addr);

         masters.insert(addr);
      }

      first = false;
      secconn.close();
   }

   SSLTransport::destroy();

   unlink((m_strHomeDir + ".tmp/metadata.dat").c_str());

   // initialize slave statistics
   m_SlaveStat.init();

   cout << "This Sector slave is successfully initialized and running now.\n";

   return 1;
}

void Slave::run()
{
   string ip;
   int port;
   int32_t id;
   SectorMsg* msg = new SectorMsg;
   msg->resize(65536);

   cout << "slave process: " << "GMP " << m_iLocalPort << " DATA " << m_DataChn.getPort() << endl;

   while (true)
   {
      if (m_GMP.recvfrom(ip, port, id, msg) < 0)
         break;

      cout << "recv cmd " << ip << " " << port << " type " << msg->getType() << endl;

      // a slave only accepts commands from the masters
      Address addr;
      addr.m_strIP = ip;
      addr.m_iPort = port;
      if (m_Routing.getRouterID(addr) < 0)
         continue;

      switch (msg->getType() / 100)
      {
      case 0:
         processSysCmd(ip, port, id, msg);
         break;

      case 1:
         processFSCmd(ip, port, id, msg);
         break;

      case 2:
         processDCCmd(ip, port, id, msg);
         break;

      case 10:
         processMCmd(ip, port, id, msg);
         break;

      default:
         break;
      }
   }

   delete msg;
}

int Slave::processSysCmd(const string& ip, const int port, int id, SectorMsg* msg)
{
   switch (msg->getType())
   {
   case 1: // probe
   {
      // calculate total available disk size
      struct statfs64 slavefs;
      statfs64(m_SysConfig.m_strHomeDir.c_str(), &slavefs);
      m_SlaveStat.m_llAvailSize = slavefs.f_bfree * slavefs.f_bsize;
      m_SlaveStat.m_llDataSize = m_pLocalFile->getTotalDataSize("/");

      m_SlaveStat.refresh();

      msg->setData(0, (char*)&(m_SlaveStat.m_llTimeStamp), 8);
      msg->setData(8, (char*)&m_SlaveStat.m_llAvailSize, 8);
      msg->setData(16, (char*)&m_SlaveStat.m_llDataSize, 8);
      msg->setData(24, (char*)&(m_SlaveStat.m_llCurrMemUsed), 8);
      msg->setData(32, (char*)&(m_SlaveStat.m_llCurrCPUUsed), 8);
      msg->setData(40, (char*)&(m_SlaveStat.m_llTotalInputData), 8);
      msg->setData(48, (char*)&(m_SlaveStat.m_llTotalOutputData), 8);
      int size = (m_SlaveStat.m_mSysIndInput.size() + m_SlaveStat.m_mSysIndOutput.size() + m_SlaveStat.m_mCliIndInput.size() + m_SlaveStat.m_mCliIndOutput.size()) * 24 + 16;
      char* buf = new char[size];
      m_SlaveStat.serializeIOStat(buf, size);
      msg->setData(56, buf, size);
      delete [] buf;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   case 3: // stop
   {
      // stop the slave node
      break;
   }

   default:
      return -1;
   }

   return 0;
}

int Slave::processFSCmd(const string& ip, const int port, int id, SectorMsg* msg)
{
   switch (msg->getType())
   {
   case 103: // mkdir
   {
      createDir(msg->getData());
      char* tmp = new char[64 + strlen(msg->getData())];
      sprintf(tmp, "created new directory %s.", msg->getData());
      m_SectorLog.insert(tmp);
      delete [] tmp;
      break;
   }

   case 104: // move dir/file
   {
      string src = msg->getData();
      string dst = msg->getData() + src.length() + 1;
      string newname = msg->getData() + src.length() + 1 + dst.length() + 1;

      m_pLocalFile->move(src.c_str(), dst.c_str(), newname.c_str());
      move(src, dst, newname);
      break;
   }

   case 105: // remove dir/file
   {
      char* path = msg->getData();
      m_pLocalFile->remove(path, true);
      string sysrm = string("rm -rf ") + reviseSysCmdPath(m_strHomeDir) + reviseSysCmdPath(path);
      system(sysrm.c_str());

      char* tmp = new char[64 + strlen(path)];
      sprintf(tmp, "removed directory %s.", path);
      m_SectorLog.insert(tmp);
      delete [] tmp;

      break;
   }

   case 107: // utime
   {
      char* path = msg->getData();

      utimbuf ut;
      ut.actime = *(int64_t*)(msg->getData() + strlen(path) + 1);
      ut.modtime = *(int64_t*)(msg->getData() + strlen(path) + 1);;
      utime((m_strHomeDir + path).c_str(), &ut);

      break;
   }

   case 110: // open file
   {
      cout << "===> start file server " << ip << " " << port << endl;

      Param2* p = new Param2;
      p->serv_instance = this;
      p->src_ip = msg->getData();
      p->src_port = *(int*)(msg->getData() + 64);
      p->dst_ip = msg->getData() + 68;
      p->dst_port = *(int*)(msg->getData() + 132);
      p->key = *(int*)(msg->getData() + 136);
      p->mode = *(int*)(msg->getData() + 140);
      p->transid = *(int*)(msg->getData() + 144);
      memcpy(p->crypto_key, msg->getData() + 148, 16);
      memcpy(p->crypto_iv, msg->getData() + 164, 8);
      p->filename = msg->getData() + 172;

      p->master_ip = ip;
      p->master_port = port;

      char* tmp = new char[64 + p->filename.length()];
      sprintf(tmp, "opened file %s from %s:%d.", p->filename.c_str(), p->src_ip.c_str(), p->src_port);
      m_SectorLog.insert(tmp);
      delete [] tmp;

      pthread_t file_handler;
      pthread_create(&file_handler, NULL, fileHandler, p);
      pthread_detach(file_handler);

      msg->m_iDataLength = SectorMsg::m_iHdrSize;
      m_GMP.sendto(ip, port, id, msg);

      break;
   }

   case 111: // create a replica to local
   {
      Param3* p = new Param3;
      p->serv_instance = this;
      p->transid = *(int32_t*)msg->getData();
      p->timestamp = *(int64_t*)(msg->getData() + 4);
      p->src = msg->getData() + 12;
      p->dst = msg->getData() + 12 + p->src.length() + 1;

      p->master_ip = ip;
      p->master_port = port;

      char* tmp = new char[64 + p->src.length() + p->dst.length()];
      sprintf(tmp, "created replica %s %s.", p->src.c_str(), p->dst.c_str());
      m_SectorLog.insert(tmp);
      delete [] tmp;

      pthread_t replica_handler;
      pthread_create(&replica_handler, NULL, copy, p);
      pthread_detach(replica_handler);

      m_GMP.sendto(ip, port, id, msg);

      break;
   }

   default:
      return -1;
   }

   return 0;
}

int Slave::processDCCmd(const string& ip, const int port, int id, SectorMsg* msg)
{
   switch (msg->getType())
   {
   case 203: // processing engine
   {
      Param4* p = new Param4;
      p->serv_instance = this;
      p->client_ip = msg->getData();
      p->client_ctrl_port = *(int32_t*)(msg->getData() + 64);
      p->client_data_port = *(int32_t*)(msg->getData() + 68);
      p->speid = *(int32_t*)(msg->getData() + 72);
      p->key = *(int32_t*)(msg->getData() + 76);
      p->function = msg->getData() + 80;
      int offset = 80 + p->function.length() + 1;
      p->rows = *(int32_t*)(msg->getData() + offset);
      p->psize = *(int32_t*)(msg->getData() + offset + 4);
      if (p->psize > 0)
      {
         p->param = new char[p->psize];
         memcpy(p->param, msg->getData() + offset + 8, p->psize);
      }
      else
         p->param = NULL;
      p->type = *(int32_t*)(msg->getData() + msg->m_iDataLength - SectorMsg::m_iHdrSize - 8);
      p->transid = *(int32_t*)(msg->getData() + msg->m_iDataLength - SectorMsg::m_iHdrSize - 4);

      p->master_ip = ip;
      p->master_port = port;

      cout << "starting SPE ... " << p->speid << " " << p->client_data_port << " " << p->function << " " << p->transid << endl;
      char* tmp = new char[64 + p->function.length()];
      sprintf(tmp, "starting SPE ... %d %d %s %d.", p->speid, p->client_data_port, p->function.c_str(), p->transid);
      m_SectorLog.insert(tmp);
      delete [] tmp;

      pthread_t spe_handler;
      pthread_create(&spe_handler, NULL, SPEHandler, p);
      pthread_detach(spe_handler);

      msg->m_iDataLength = SectorMsg::m_iHdrSize;
      m_GMP.sendto(ip, port, id, msg);

      break;
   }

   case 204: // accept SPE buckets
   {
      CGMP* gmp = new CGMP;
      gmp->init();

      Param5* p = new Param5;
      p->serv_instance = this;
      p->client_ip = msg->getData();
      p->client_ctrl_port = *(int32_t*)(msg->getData() + 64);
      p->bucketnum = *(int32_t*)(msg->getData() + 68);
      p->bucketid = *(int32_t*)(msg->getData() + 72);
      p->path = msg->getData() + 80;
      int offset = 80 + p->path.length() + 1 + 4;

      p->filename = msg->getData() + offset;
      p->gmp = gmp;

      offset += p->filename.length() + 1;

      p->key = *(int32_t*)(msg->getData() + offset);
      p->type = *(int32_t*)(msg->getData() + offset + 4);
      if (p->type == 1)
      {
         p->function = msg->getData() + offset + 4 + 4 + 4;
      }
      p->transid = *(int32_t*)(msg->getData() + msg->m_iDataLength - SectorMsg::m_iHdrSize - 8);
      p->client_data_port = *(int32_t*)(msg->getData() + msg->m_iDataLength - SectorMsg::m_iHdrSize - 4);

      p->master_ip = ip;
      p->master_port = port;

      char* tmp = new char[64 + p->filename.length()];
      sprintf(tmp, "starting SPE Bucket... %s %d %d %d.", p->filename.c_str(), p->key, p->type, p->transid);
      m_SectorLog.insert(tmp);
      delete [] tmp;

      pthread_t spe_shuffler;
      pthread_create(&spe_shuffler, NULL, SPEShuffler, p);
      pthread_detach(spe_shuffler);

      *(int32_t*)msg->getData() = gmp->getPort();
      msg->m_iDataLength = SectorMsg::m_iHdrSize + 4;
      m_GMP.sendto(ip, port, id, msg);

      break;
   }

   default:
      return -1;
   }

   return 0;
}

int Slave::processMCmd(const string& ip, const int port, int id, SectorMsg* msg)
{
   switch (msg->getType())
   {
   case 1001: // new master
   {
      int32_t key = *(int32_t*)msg->getData();
      Address addr;
      addr.m_strIP = msg->getData() + 4;
      addr.m_iPort = *(int32_t*)(msg->getData() + 68);
      m_Routing.insert(key, addr);
      msg->m_iDataLength = SectorMsg::m_iHdrSize + 4;
      m_GMP.sendto(ip, port, id, msg);

      break;
   }

   case 1006: // master lost
   {
      m_Routing.remove(*(int32_t*)msg->getData());
      msg->m_iDataLength = SectorMsg::m_iHdrSize + 4;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   default:
      return -1; 
   }

   return 0;
}

int Slave::report(const string& master_ip, const int& master_port, const int32_t& transid, const string& filename, const int& change)
{
   vector<string> filelist;
   filelist.push_back(filename);

   return report(master_ip, master_port, transid, filelist, change);
}

int Slave::report(const string& master_ip, const int& master_port, const int32_t& transid, const vector<string>& filelist, const int& change)
{
   vector<string> serlist;
   for (vector<string>::const_iterator i = filelist.begin(); i != filelist.end(); ++ i)
   {
      struct stat64 s;
      if (-1 == stat64((m_strHomeDir + *i).c_str(), &s))
         continue;

      SNode sn;
      sn.m_strName = *i;
      sn.m_bIsDir = 0;
      sn.m_llTimeStamp = s.st_mtime;
      sn.m_llSize = s.st_size;

      char buf[1024];
      sn.serialize(buf);

      //update local
      Address addr;
      addr.m_strIP = "127.0.0.1";
      addr.m_iPort = 0;
      m_pLocalFile->update(buf, addr, change);

      serlist.push_back(buf);
   }

   if (serlist.empty())
      return 0;

   SectorMsg msg;
   msg.setType(1);
   msg.setKey(0);
   msg.setData(0, (char*)&transid, 4);
   msg.setData(4, (char*)&m_iSlaveID, 4);
   msg.setData(8, (char*)&change, 4);
   int32_t num = serlist.size();
   msg.setData(12, (char*)&num, 4);
   int pos = 16;
   for (vector<string>::iterator i = serlist.begin(); i != serlist.end(); ++ i)
   {
      int32_t bufsize = i->length() + 1;
      msg.setData(pos, (char*)&bufsize, 4);
      msg.setData(pos + 4, i->c_str(), bufsize);
      pos += bufsize + 4;
   }

   cout << "report " << master_ip << " " << master_port << " " << num << endl;

   if (m_GMP.rpc(master_ip.c_str(), master_port, &msg, &msg) < 0)
      return -1;

   if (msg.getType() < 0)
      return *(int32_t*)msg.getData();

   return 1;
}

int Slave::reportMO(const std::string& master_ip, const int& master_port, const int32_t& transid)
{
   vector<MemObj> tba;
   vector<string> tbd;
   if (m_InMemoryObjects.update(tba, tbd) <= 0)
      return 0;

   if (!tba.empty())
   {
      vector<string> serlist;
      for (vector<MemObj>::const_iterator i = tba.begin(); i != tba.end(); ++ i)
      {
         SNode sn;
         sn.m_strName = i->m_strName;
         sn.m_bIsDir = 0;
         sn.m_llTimeStamp = i->m_llCreationTime;
         sn.m_llSize = 8;

         char buf[1024];
         sn.serialize(buf);
      }

      SectorMsg msg;
      msg.setType(1);
      msg.setKey(0);
      msg.setData(0, (char*)&transid, 4);
      msg.setData(4, (char*)&m_iSlaveID, 4);
      int32_t num = serlist.size();
      msg.setData(8, (char*)&num, 4);
      int pos = 12;
      for (vector<string>::iterator i = serlist.begin(); i != serlist.end(); ++ i)
      {
         int32_t bufsize = i->length() + 1;
         msg.setData(pos, (char*)&bufsize, 4);
         msg.setData(pos + 4, i->c_str(), bufsize);
         pos += bufsize + 4;
      }

      if (m_GMP.rpc(master_ip.c_str(), master_port, &msg, &msg) < 0)
         return -1;
   }

   if (!tbd.empty())
   {
      SectorMsg msg;
      msg.setType(7);
      msg.setKey(0);
      msg.setData(0, (char*)&transid, 4);
      msg.setData(4, (char*)&m_iSlaveID, 4);
      int32_t num = tbd.size();
      msg.setData(8, (char*)&num, 4);
      int pos = 12;
      for (vector<string>::iterator i = tbd.begin(); i != tbd.end(); ++ i)
      {
         int32_t bufsize = i->length() + 1;
         msg.setData(pos, (char*)&bufsize, 4);
         msg.setData(pos + 4, i->c_str(), bufsize);
         pos += bufsize + 4;
      }

      if (m_GMP.rpc(master_ip.c_str(), master_port, &msg, &msg) < 0)
         return -1;
   }

   return 0;
}

int Slave::reportSphere(const string& master_ip, const int& master_port, const int& transid, const vector<Address>* bad)
{
   SectorMsg msg;
   msg.setType(4);
   msg.setKey(0);
   msg.setData(0, (char*)&transid, 4);
   msg.setData(4, (char*)&m_iSlaveID, 4);

   int num = (NULL == bad) ? 0 : bad->size();
   msg.setData(8, (char*)&num, 4);
   for (int i = 0; i < num; ++ i)
   {
      msg.setData(12 + 68 * i, (*bad)[i].m_strIP.c_str(), (*bad)[i].m_strIP.length() + 1);
      msg.setData(12 + 68 * i + 64, (char*)&((*bad)[i].m_iPort), 4);
   }

   cout << "reportSphere " << master_ip << " " << master_port << " " << transid << endl;

   if (m_GMP.rpc(master_ip.c_str(), master_port, &msg, &msg) < 0)
      return -1;

   if (msg.getType() < 0)
      return *(int32_t*)msg.getData();

   return 1;
}

int Slave::createDir(const string& path)
{
   vector<string> dir;
   Metadata::parsePath(path.c_str(), dir);

   string currpath = m_strHomeDir;
   for (vector<string>::iterator i = dir.begin(); i != dir.end(); ++ i)
   {
      currpath += *i;
      if ((-1 == ::mkdir(currpath.c_str(), S_IRWXU)) && (errno != EEXIST))
         return -1;
      currpath += "/";
   }

   return 1;
}

int Slave::createSysDir()
{
   // check local directory
   DIR* test = opendir(m_strHomeDir.c_str());
   if (NULL == test)
   {
      if (errno != ENOENT)
         return -1;

      vector<string> dir;
      Metadata::parsePath(m_strHomeDir.c_str(), dir);

      string currpath = "/";
      for (vector<string>::iterator i = dir.begin(); i != dir.end(); ++ i)
      {
         currpath += *i;
         if ((-1 == ::mkdir(currpath.c_str(), S_IRWXU)) && (errno != EEXIST))
            return -1;
         currpath += "/";
      }
   }
   closedir(test);

   test = opendir((m_strHomeDir + ".metadata").c_str());
   if (NULL == test)
   {
      if ((errno != ENOENT) || (mkdir((m_strHomeDir + ".metadata").c_str(), S_IRWXU) < 0))
         return -1;
   }
   closedir(test);
   system(("rm -rf " + reviseSysCmdPath(m_strHomeDir) + ".metadata/*").c_str());

   test = opendir((m_strHomeDir + ".sphere").c_str());
   if (NULL == test)
   {
      if ((errno != ENOENT) || (mkdir((m_strHomeDir + ".sphere").c_str(), S_IRWXU) < 0))
         return -1;
   }
   closedir(test);
   system(("rm -rf " + reviseSysCmdPath(m_strHomeDir) + ".sphere/*").c_str());

   test = opendir((m_strHomeDir + ".sphere/perm").c_str());
   if (NULL == test)
   {
      if ((errno != ENOENT) || (mkdir((m_strHomeDir + ".sphere/perm").c_str(), S_IRWXU) < 0))
         return -1;
   }
   closedir(test);
   //system(("rm -rf " + reviseSysCmdPath(m_strHomeDir) + ".sphere/perm/*").c_str());

   test = opendir((m_strHomeDir + ".tmp").c_str());
   if (NULL == test)
   {
      if ((errno != ENOENT) || (mkdir((m_strHomeDir + ".tmp").c_str(), S_IRWXU) < 0))
         return -1;
   }
   closedir(test);
   system(("rm -rf " + reviseSysCmdPath(m_strHomeDir) + ".tmp/*").c_str());

   return 0;
}

string Slave::reviseSysCmdPath(const string& path)
{
   string rpath;
   for (const char *p = path.c_str(), *q = path.c_str() + path.length(); p != q; ++ p)
   {
      if ((*p == 32) || (*p == 34) || (*p == 38) || (*p == 39))
         rpath.append(1, char(92));
      rpath.append(1, *p);
   }

   return rpath;
}

int Slave::move(const string& src, const string& dst, const string& newname)
{
   createDir(dst);
   system((string("mv ") + reviseSysCmdPath(m_strHomeDir + src) + " " + reviseSysCmdPath(m_strHomeDir + dst + newname)).c_str());
   return 1;
}


void SlaveStat::init()
{
   m_llStartTime = m_llTimeStamp = CTimer::getTime();
   m_llCurrMemUsed = 0;
   m_llCurrCPUUsed = 0;
   m_llTotalInputData = 0;
   m_llTotalOutputData = 0;
   m_mSysIndInput.clear();
   m_mSysIndOutput.clear();
   m_mCliIndInput.clear();
   m_mCliIndOutput.clear();

   pthread_mutex_init(&m_StatLock, 0);
}

void SlaveStat::refresh()
{
   // THIS CODE IS FOR LINUX ONLY. NOT PORTABLE

   m_llTimeStamp = CTimer::getTime();

   int pid = getpid();

   char memfile[64];
   sprintf(memfile, "/proc/%d/status", pid);

   ifstream ifs;
   ifs.open(memfile, ios::in);
   char buf[1024];
   for (int i = 0; i < 12; ++ i)
      ifs.getline(buf, 1024);
   string tmp;
   ifs >> tmp;
   ifs >> m_llCurrMemUsed;
   m_llCurrMemUsed *= 1024;
   ifs.close();

   clock_t hz = sysconf(_SC_CLK_TCK);
   tms cputime;
   times(&cputime);
   m_llCurrCPUUsed = (cputime.tms_utime + cputime.tms_stime) * 1000000LL / hz;
}

void SlaveStat::updateIO(const string& ip, const int64_t& size, const int& type)
{
   pthread_mutex_lock(&m_StatLock);

   map<string, int64_t>::iterator a;

   if (type == 0)
   {
      map<string, int64_t>::iterator a = m_mSysIndInput.find(ip);
      if (a == m_mSysIndInput.end())
      {
         m_mSysIndInput[ip] = 0;
         a = m_mSysIndInput.find(ip);
      }

      a->second += size;
      m_llTotalInputData += size;
   }
   else if (type == 1)
   {
      map<string, int64_t>::iterator a = m_mSysIndOutput.find(ip);
      if (a == m_mSysIndOutput.end())
      {
         m_mSysIndOutput[ip] = 0;
         a = m_mSysIndOutput.find(ip);
      }

      a->second += size;
      m_llTotalOutputData += size;
   }
   else if (type == 2)
   {
      map<string, int64_t>::iterator a = m_mCliIndInput.find(ip);
      if (a == m_mCliIndInput.end())
      {
         m_mCliIndInput[ip] = 0;
         a = m_mCliIndInput.find(ip);
      }

      a->second += size;
      m_llTotalInputData += size;
   }
   else if (type == 3)
   {
      map<string, int64_t>::iterator a = m_mCliIndOutput.find(ip);
      if (a == m_mCliIndOutput.end())
      {
         m_mCliIndOutput[ip] = 0;
         a = m_mCliIndOutput.find(ip);
      }

      a->second += size;
      m_llTotalOutputData += size;
   }

   pthread_mutex_unlock(&m_StatLock);
}

int SlaveStat::serializeIOStat(char* buf, unsigned int size)
{
   if (size < (m_mSysIndInput.size() + m_mSysIndOutput.size() + m_mCliIndInput.size() + m_mCliIndOutput.size()) * 24 + 16)
      return -1;

   pthread_mutex_lock(&m_StatLock);

   char* p = buf;
   *(int32_t*)p = m_mSysIndInput.size();
   p += 4;
   for (map<string, int64_t>::iterator i = m_mSysIndInput.begin(); i != m_mSysIndInput.end(); ++ i)
   {
      strcpy(p, i->first.c_str());
      *(int64_t*)(p + 16) = i->second;
      p += 24;
   }

   *(int32_t*)p = m_mSysIndOutput.size();
   p += 4;
   for (map<string, int64_t>::iterator i = m_mSysIndOutput.begin(); i != m_mSysIndOutput.end(); ++ i)
   {
      strcpy(p, i->first.c_str());
      *(int64_t*)(p + 16) = i->second;
      p += 24;
   }

   *(int32_t*)p = m_mCliIndInput.size();
   p += 4;
   for (map<string, int64_t>::iterator i = m_mCliIndInput.begin(); i != m_mCliIndInput.end(); ++ i)
   {
      strcpy(p, i->first.c_str());
      *(int64_t*)(p + 16) = i->second;
      p += 24;
   }

   *(int32_t*)p = m_mCliIndOutput.size();
   p += 4;
   for (map<string, int64_t>::iterator i = m_mCliIndOutput.begin(); i != m_mCliIndOutput.end(); ++ i)
   {
      strcpy(p, i->first.c_str());
      *(int64_t*)(p + 16) = i->second;
      p += 24;
   }

   pthread_mutex_unlock(&m_StatLock);

   return (m_mSysIndInput.size() + m_mSysIndOutput.size() + m_mCliIndInput.size() + m_mCliIndOutput.size()) * 24 + 16;
}

void Slave::logError(int type, const string& ip, const int& port, const string& name)
{
   char* tmp = new char[64 + name.length()];

   switch (type)
   {
   case 1:
      sprintf(tmp, "failed to connect to file client %s:%d %s.", ip.c_str(), port, name.c_str());
      break;

   case 2:
      sprintf(tmp, "failed to connect spe client %s:%d %s.", ip.c_str(), port, name.c_str());
      break;

   case 3:
      sprintf(tmp, "failed to load spe library %s:%d %s.", ip.c_str(), port, name.c_str());
      break;

   default:
      sprintf(tmp, "unknown error.");
      break;
   }

   m_SectorLog.insert(tmp);
   delete [] tmp;
}
