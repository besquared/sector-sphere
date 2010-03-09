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
   Yunhong Gu, last updated 02/19/2010
*****************************************************************************/

#include <common.h>
#include <ssltransport.h>
#include <dirent.h>
#include <signal.h>
#include <iostream>
#include <stack>
#include <sstream>
#include "master.h"

using namespace std;


Master::Master():
m_pMetadata(NULL),
m_llLastUpdateTime(0),
m_pcTopoData(NULL),
m_iTopoDataSize(0)
{
   pthread_mutex_init(&m_ReplicaLock, NULL);
   pthread_cond_init(&m_ReplicaCond, NULL);

   SSLTransport::init();
}

Master::~Master()
{
   m_SectorLog.close();
   delete m_pMetadata;
   delete [] m_pcTopoData;
   pthread_mutex_destroy(&m_ReplicaLock);
   pthread_cond_destroy(&m_ReplicaCond);

   SSLTransport::destroy();
}

int Master::init()
{
   m_SectorLog.init("sector.log");

   // read configuration from master.conf
   if (m_SysConfig.init("../conf/master.conf") < 0)
   {
      cerr << "unable to read/parse configuration file.\n";
      m_SectorLog.insert("unable to read/parse configuration file.");
      return -1;
   }

   struct stat s;
   if (stat("../conf/topology.conf", &s) < 0)
   {
      cerr << "Warning: no topology configuration found.\n";
      m_SectorLog.insert("Warning: no topology configuration found.");
   }

   m_SlaveManager.init("../conf/topology.conf");
   m_SlaveManager.serializeTopo(m_pcTopoData, m_iTopoDataSize);

   // check local directories, create them is not exist
   m_strHomeDir = m_SysConfig.m_strHomeDir;
   system((string("rm -rf ") + m_strHomeDir).c_str());
   DIR* test = opendir(m_strHomeDir.c_str());
   if (NULL == test)
   {
      if (errno != ENOENT)
      {
         cerr << "unable to configure home directory.\n";
         m_SectorLog.insert("unable to configure home directory.");
         return -1;
      }

      vector<string> dir;
      Metadata::parsePath(m_strHomeDir.c_str(), dir);

      string currpath = "/";
      for (vector<string>::iterator i = dir.begin(); i != dir.end(); ++ i)
      {
         currpath += *i;
         if ((-1 == ::mkdir(currpath.c_str(), S_IRWXU)) && (errno != EEXIST))
         {
            m_SectorLog.insert("unable to configure home directory.");
            return -1;
         }
         currpath += "/";
      }
   }
   closedir(test);

   if ((mkdir((m_strHomeDir + ".metadata").c_str(), S_IRWXU) < 0)
      || (mkdir((m_strHomeDir + ".tmp").c_str(), S_IRWXU) < 0))
   {
      cerr << "unable to create home directory.\n";
      m_SectorLog.insert("unable to create home directory.");
      return -1;
   }

   if (m_SysConfig.m_MetaType == DISK)
      m_pMetadata = new Index2;
   else
      m_pMetadata = new Index;
   m_pMetadata->init(m_strHomeDir + ".metadata");

   // load slave list and addresses
   loadSlaveAddr("../conf/slaves.list");

   // add "slave" as a special user
   User* au = new User;
   au->m_strName = "system";
   au->m_iKey = 0;
   au->m_vstrReadList.insert(au->m_vstrReadList.begin(), "/");
   //au->m_vstrWriteList.insert(au->m_vstrWriteList.begin(), "/");
   m_UserManager.insert(au);

   // running...
   m_Status = RUNNING;

   Transport::initialize();

   // start GMP
   if (m_GMP.init(m_SysConfig.m_iServerPort) < 0)
   {
      cerr << "cannot initialize GMP.\n";
      m_SectorLog.insert("cannot initialize GMP.");
      return -1;
   }

   //connect security server to get ID
   SSLTransport secconn;
   if (secconn.initClientCTX("../conf/security_node.cert") < 0)
   {
      cerr << "No security node certificate found.\n";
      m_SectorLog.insert("No security node certificate found.");
      return -1;
   }
   secconn.open(NULL, 0);
   if (secconn.connect(m_SysConfig.m_strSecServIP.c_str(), m_SysConfig.m_iSecServPort) < 0)
   {
      secconn.close();
      cerr << "Failed to find security server.\n";
      m_SectorLog.insert("Failed to find security server.");
      return -1;
   }

   int32_t cmd = 4;
   secconn.send((char*)&cmd, 4);
   secconn.recv((char*)&m_iRouterKey, 4);
   secconn.close();

   Address addr;
   addr.m_strIP = "";
   addr.m_iPort = m_SysConfig.m_iServerPort;
   m_Routing.insert(m_iRouterKey, addr);

   // start service thread
   pthread_t svcserver;
   pthread_create(&svcserver, NULL, service, this);
   pthread_detach(svcserver);

   // start management/process thread
   pthread_t msgserver;
   pthread_create(&msgserver, NULL, process, this);
   pthread_detach(msgserver);

   // start replica thread
   pthread_t repserver;
   pthread_create(&repserver, NULL, replica, this);
   pthread_detach(repserver);

   m_llStartTime = time(NULL);
   m_SectorLog.insert("Sector started.");

   cout << "Sector master is successfully running now. check sector.log for more details.\n";
   cout << "There is no further screen output from this program.\n";

   return 1;
}

int Master::join(const char* ip, const int& port)
{
   // join the server
   string cert = "../conf/master_node.cert";

   SSLTransport s;
   s.initClientCTX(cert.c_str());
   s.open(NULL, 0);
   if (s.connect(ip, port) < 0)
   {
      cerr << "unable to set up secure channel to the existing master.\n";
      return -1;
   }

   int cmd = 3;
   s.send((char*)&cmd, 4);
   int32_t key = -1;
   s.recv((char*)&key, 4);
   if (key < 0)
   {
      cerr << "security check failed. code: " << key << endl;
      return -1;
   }

   Address addr;
   addr.m_strIP = ip;
   addr.m_iPort = port;
   m_Routing.insert(key, addr);

   s.send((char*)&m_SysConfig.m_iServerPort, 4);
   s.send((char*)&m_iRouterKey, 4);

   // recv master list
   int num = 0;
   if (s.recv((char*)&num, 4) < 0)
      return -1;
   for (int i = 0; i < num; ++ i)
   {
      char ip[64];
      int port = 0;
      int id = 0;
      int size = 0;
      s.recv((char*)&id, 4);
      s.recv((char*)&size, 4);
      s.recv(ip, size);
      s.recv((char*)&port, 4);
      Address saddr;
      saddr.m_strIP = ip;
      saddr.m_iPort = port;
      m_Routing.insert(id, addr);
   }

   // recv slave list
   if (s.recv((char*)&num, 4) < 0)
      return -1;
   int size = 0;
   if (s.recv((char*)&size, 4) < 0)
      return -1;
   char* buf = new char [size];
   s.recv(buf, size);
   m_SlaveManager.deserializeSlaveList(num, buf, size);
   delete [] buf;

   // recv user list
   if (s.recv((char*)&num, 4) < 0)
      return -1;
   for (int i = 0; i < num; ++ i)
   {
      size = 0;
      s.recv((char*)&size, 4);
      char* ubuf = new char[size];
      s.recv(ubuf, size);
      User* u = new User;
      u->deserialize(ubuf, size);
      delete [] ubuf;
      m_UserManager.insert(u);
   }

   // recv metadata
   size = 0;
   s.recv((char*)&size, 4);
   s.recvfile((m_strHomeDir + ".tmp/master_meta.dat").c_str(), 0, size);
   m_pMetadata->deserialize("/", m_strHomeDir + ".tmp/master_meta.dat", NULL);
   unlink(".tmp/master_meta.dat");

   s.close();

   return 0;
}

int Master::run()
{
   while (m_Status == RUNNING)
   {
      sleep(60);

      // check other masters
      vector<uint32_t> tbrm;

      map<uint32_t, Address> al;
      m_Routing.getListOfMasters(al);
      for (map<uint32_t, Address>::iterator i = al.begin(); i != al.end(); ++ i)
      {
         if (i->first == m_iRouterKey)
            continue;

         SectorMsg msg;
         msg.setKey(0);
         msg.setType(1005); //master node probe msg
         msg.setData(0, (char*)&i->first, 4); // ask the other master to check its router ID, in case more than one are started on the same address
         if ((m_GMP.rpc(i->second.m_strIP.c_str(), i->second.m_iPort, &msg, &msg) < 0) || (msg.getType() < 0))
         {
            m_SectorLog.insert(("Master lost " + i->second.m_strIP + ".").c_str());
            tbrm.push_back(i->first);

            // send the master drop info to all slaves
            SectorMsg msg;
            msg.setKey(0);
            msg.setType(1006);
            msg.setData(0, (char*)&i->first, 4);
            m_SlaveManager.updateSlaveList(m_vSlaveList, m_llLastUpdateTime);
            m_GMP.multi_rpc(m_vSlaveList, &msg);
         }
      }

      for (vector<uint32_t>::iterator i = tbrm.begin(); i != tbrm.end(); ++ i)
         m_Routing.remove(*i);

      // check each slave node
      SectorMsg msg;
      msg.setType(1);
      m_SlaveManager.updateSlaveList(m_vSlaveList, m_llLastUpdateTime);

      vector<CUserMessage*>* res = new vector<CUserMessage*>;
      res->resize(m_vSlaveList.size());
      for (vector<CUserMessage*>::iterator i = res->begin(); i != res->end(); ++ i)
         *i = new SectorMsg;

      m_GMP.multi_rpc(m_vSlaveList, &msg, res);

      for (int i = 0, n = m_vSlaveList.size(); i < n; ++ i)
      {
         SectorMsg* r = dynamic_cast<SectorMsg*>((*res)[i]);

         if (NULL != r)
         {
            if (r->getType() > 0)
               m_SlaveManager.updateSlaveInfo(m_vSlaveList[i], r->getData(), r->m_iDataLength);
            delete r;
         }
         else
         {
            m_SlaveManager.increaseRetryCount(m_vSlaveList[i]);
         }
      }

      // check each users, remove inactive ones
      vector<User*> iu;
      m_UserManager.checkInactiveUsers(iu);
      for (vector<User*>::iterator i = iu.begin(); i != iu.end(); ++ i)
      {
         char* text = new char[64 + (*i)->m_strName.length()];
         sprintf(text, "User %s timeout. Kicked out.", (*i)->m_strName.c_str());
         m_SectorLog.insert(text);
         delete [] text;
         delete *i;
      }


      if (m_Routing.getRouterID(m_iRouterKey) != 0)
         continue;

      // The following checks are only performed by the primary master


      // check each slave node
      // if probe fails, remove the metadata of the data on the node, and create new replicas
      map<int, Address> bad;
      map<int, Address> lost;
      m_SlaveManager.checkBadAndLost(bad, lost);

      for (map<int, Address>::iterator i = bad.begin(); i != bad.end(); ++ i)
      {
         m_SectorLog.insert(("Bad slave detected " + i->second.m_strIP + ".").c_str());
         //TODO: create replica for files on the bade nodes, gradually move data out of those nodes
      }

      for (map<int, Address>::iterator i = lost.begin(); i != lost.end(); ++ i)
      {
         m_SectorLog.insert(("Slave lost " + i->second.m_strIP + ".").c_str());

         // remove the data on that node
         Address addr;
         addr.m_strIP = i->second.m_strIP;
         addr.m_iPort = i->second.m_iPort;
         m_pMetadata->substract("/", addr);

         //remove all associated transactions and release IO locks...
         vector<int> trans;
         m_TransManager.retrieve(i->first, trans);
         for (vector<int>::iterator t = trans.begin(); t != trans.end(); ++ t)
         {
            Transaction tt;
            m_TransManager.retrieve(*t, tt);
            m_pMetadata->unlock(tt.m_strFile.c_str(), tt.m_iUserKey, tt.m_iMode);
            m_TransManager.updateSlave(*t, i->first);
         }

         m_SlaveManager.remove(i->first);

         // send lost slave info to all existing masters
         map<uint32_t, Address> al;
         m_Routing.getListOfMasters(al);
         for (map<uint32_t, Address>::iterator m = al.begin(); m != al.end(); ++ m)
         {
            if (m->first == m_iRouterKey)
               continue;

            SectorMsg msg;
            msg.setKey(0);
            msg.setType(1007);
            msg.setData(0, (char*)&(i->first), 4);
            m_GMP.rpc(m->second.m_strIP.c_str(), m->second.m_iPort, &msg, &msg);
         }

         map<string, SlaveAddr>::iterator sa = m_mSlaveAddrRec.find(i->second.m_strIP);
         if (sa != m_mSlaveAddrRec.end())
         {
            m_SectorLog.insert(("Restart slave " + sa->second.m_strAddr + " " + sa->second.m_strBase).c_str());

            // kill and restart the slave
            system((string("ssh ") + sa->second.m_strAddr + " killall -9 start_slave").c_str());
            system((string("ssh ") + sa->second.m_strAddr + " \"" + sa->second.m_strBase + "/start_slave " + sa->second.m_strBase + " &> /dev/null &\"").c_str());
         }
      }

      // update cluster statistics
      m_SlaveManager.updateClusterStat();

      // check replica, create or remove replicas if necessary
      // only the first master is responsible for replica checking
      map<string, int> special;
      populateSpecialRep("../conf/replica.conf", special);

      pthread_mutex_lock(&m_ReplicaLock);
      if (m_vstrToBeReplicated.empty())
         m_pMetadata->getUnderReplicated("/", m_vstrToBeReplicated, m_SysConfig.m_iReplicaNum, special);
      if (!m_vstrToBeReplicated.empty())
         pthread_cond_signal(&m_ReplicaCond);
      pthread_mutex_unlock(&m_ReplicaLock);
   }

   return 0;
}

int Master::stop()
{
   m_Status = STOPPED;

   return 0;
}

void* Master::service(void* s)
{
   Master* self = (Master*)s;

   //ignore SIGPIPE
   sigset_t ps;
   sigemptyset(&ps);
   sigaddset(&ps, SIGPIPE);
   pthread_sigmask(SIG_BLOCK, &ps, NULL);

   const int ServiceWorker = 4;
   for (int i = 0; i < ServiceWorker; ++ i)
   {
      pthread_t t;
      pthread_create(&t, NULL, serviceEx, self);
      pthread_detach(t);
   }

   SSLTransport serv;
   if (serv.initServerCTX("../conf/master_node.cert", "../conf/master_node.key") < 0)
   {
      self->m_SectorLog.insert("WARNING: No master_node certificate or key found.");
      return NULL;
   }
   serv.open(NULL, self->m_SysConfig.m_iServerPort);
   serv.listen();

   while (self->m_Status == RUNNING)
   {
      char ip[64];
      int port;
      SSLTransport* s = serv.accept(ip, port);
      if (NULL == s)
         continue;

      ServiceJobParam* p = new ServiceJobParam;
      p->ip = ip;
      p->port = port;
      p->ssl = s;

      self->m_ServiceJobQueue.push(p);
   }

   return NULL;
}

void* Master::serviceEx(void* param)
{
   Master* self = (Master*)param;

   SSLTransport secconn;
   if (secconn.initClientCTX("../conf/security_node.cert") < 0)
   {
      self->m_SectorLog.insert("No security node certificate found. All slave/client connection will be rejected.");
      return NULL;
   }

   while (true)
   {
      ServiceJobParam* p = (ServiceJobParam*)self->m_ServiceJobQueue.pop();
      if (NULL == p)
         break;

      SSLTransport* s = p->ssl;
      string ip = p->ip;
      //int port = p->port;
      delete p;

      int32_t cmd;
      if (s->recv((char*)&cmd, 4) < 0)
      {
         s->close();
         delete s;
         continue;
      }

      if (secconn.send((char*)&cmd, 4) < 0)
      {
         //if the permanent connection to the security server is broken, re-connect
         secconn.open(NULL, 0);
         if (secconn.connect(self->m_SysConfig.m_strSecServIP.c_str(), self->m_SysConfig.m_iSecServPort) < 0)
         {
            cmd = SectorError::E_NOSECSERV;
            s->close();
            delete s;
            continue;
         }

         secconn.send((char*)&cmd, 4);
      }

      switch (cmd)
      {
      case 1: // slave node join
         self->processSlaveJoin(*s, secconn, ip);
         break;

      case 2: // user login
         self->processUserJoin(*s, secconn, ip);
         break;

      case 3: // master join
         self->processMasterJoin(*s, secconn, ip);
        break;
      }

      s->close();
      delete s;
   }

   secconn.close();

   return NULL;
}

int Master::processSlaveJoin(SSLTransport& s, SSLTransport& secconn, const string& ip)
{
   // recv local storage path, avoid same slave joining more than once
   int32_t size = 0;
   s.recv((char*)&size, 4);
   string lspath = "";
   if (size > 0)
   {
      char* tmp = new char[size];
      s.recv(tmp, size);
      lspath = Metadata::revisePath(tmp);
      delete [] tmp;
   }

   int32_t res = -1;
   char slaveIP[64];
   strcpy(slaveIP, ip.c_str());
   secconn.send(slaveIP, 64);
   secconn.recv((char*)&res, 4);

   if ((lspath == "") || m_SlaveManager.checkDuplicateSlave(ip, lspath))
      res = SectorError::E_REPSLAVE;

   s.send((char*)&res, 4);

   if (res > 0)
   {
      SlaveNode sn;
      sn.m_iNodeID = res;
      sn.m_strIP = ip;
      s.recv((char*)&sn.m_iPort, 4);
      s.recv((char*)&sn.m_iDataPort, 4);
      sn.m_strStoragePath = lspath;
      sn.m_llLastUpdateTime = CTimer::getTime();
      sn.m_iRetryNum = 0;
      sn.m_llLastVoteTime = CTimer::getTime();

      s.recv((char*)&(sn.m_llAvailDiskSpace), 8);

      int id;
      s.recv((char*)&id, 4);
      if (id > 0)
         sn.m_iNodeID = id;

      size = 0;
      s.recv((char*)&size, 4);
      s.recvfile((m_strHomeDir + ".tmp/" + ip + ".dat").c_str(), 0, size);

      Address addr;
      addr.m_strIP = ip;
      addr.m_iPort = sn.m_iPort;

      // accept existing data on the new slave and merge it with the master metadata
      Metadata* branch = NULL;
      if (m_SysConfig.m_MetaType == DISK)
         branch = new Index2;
      else
         branch = new Index;
      branch->init(m_strHomeDir + ".tmp/" + ip);
      branch->deserialize("/", m_strHomeDir + ".tmp/" + ip + ".dat", &addr);
      m_pMetadata->merge("/", branch, m_SysConfig.m_iReplicaNum);
      unlink((m_strHomeDir + ".tmp/" + ip + ".dat").c_str());

      sn.m_llTotalFileSize = m_pMetadata->getTotalDataSize("/");

      sn.m_llCurrMemUsed = 0;
      sn.m_llCurrCPUUsed = 0;
      sn.m_llTotalInputData = 0;
      sn.m_llTotalOutputData = 0;

      m_SlaveManager.insert(sn);
      m_SlaveManager.updateClusterStat();

      if (id < 0)
      {
         //this is the first master that the slave connect to; send these information to the slave
         size = branch->getTotalFileNum("/");
         if (size <= 0)
            s.send((char*)&size, 4);
         else
         {
            branch->serialize("/", m_strHomeDir + ".tmp/" + ip + ".left");
            struct stat st;
            stat((m_strHomeDir + ".tmp/" + ip + ".left").c_str(), &st);
            size = st.st_size;
            s.send((char*)&size, 4);
            if (size > 0)
               s.sendfile((m_strHomeDir + ".tmp/" + ip + ".left").c_str(), 0, size);
            string cmd = string("rm -rf ") + m_strHomeDir + ".tmp/" + ip + ".left";
            system(cmd.c_str());
         }

         // send the list of masters to the new slave
         s.send((char*)&m_iRouterKey, 4);
         int num = m_Routing.getNumOfMasters() - 1;
         s.send((char*)&num, 4);
         map<uint32_t, Address> al;
         m_Routing.getListOfMasters(al);
         for (map<uint32_t, Address>::iterator i = al.begin(); i != al.end(); ++ i)
         {
            if (i->first == m_iRouterKey)
               continue;

            s.send((char*)&i->first, 4);
            size = i->second.m_strIP.length() + 1;
            s.send((char*)&size, 4);
            s.send(i->second.m_strIP.c_str(), size);
            s.send((char*)&i->second.m_iPort, 4);
         }
      }

      branch->clear();
      delete branch;

      char text[64];
      sprintf(text, "Slave node %s:%d joined.", ip.c_str(), sn.m_iPort);
      m_SectorLog.insert(text);
   }
   else
   {
      char text[64];
      sprintf(text, "Slave node %s join rejected.", ip.c_str());
      m_SectorLog.insert(text);
   }

   return 0;
}

int Master::processUserJoin(SSLTransport& s, SSLTransport& secconn, const std::string& ip)
{
   char user[64];
   s.recv(user, 64);
   char password[128];
   s.recv(password, 128);

   secconn.send(user, 64);
   secconn.send(password, 128);
   char clientIP[64];
   strcpy(clientIP, ip.c_str());
   secconn.send(clientIP, 64);

   int32_t key = 0;
   secconn.recv((char*)&key, 4);

   int32_t ukey;
   s.recv((char*)&ukey, 4);
   if ((key > 0) && (ukey > 0))
      key = ukey;

   s.send((char*)&key, 4);

   if (key > 0)
   {
      User* au = new User;
      au->m_strName = user;
      au->m_strIP = ip;
      au->m_iKey = key;
      au->m_llLastRefreshTime = CTimer::getTime();

      s.recv((char*)&au->m_iPort, 4);
      s.recv((char*)&au->m_iDataPort, 4);
      s.recv((char*)au->m_pcKey, 16);
      s.recv((char*)au->m_pcIV, 8);

      s.send((char*)&m_iTopoDataSize, 4);
      if (m_iTopoDataSize > 0)
         s.send(m_pcTopoData, m_iTopoDataSize);

      int32_t size = 0;
      char* buf = NULL;

      secconn.recv((char*)&size, 4);
      if (size > 0)
      {
         buf = new char[size];
         secconn.recv(buf, size);
         au->deserialize(au->m_vstrReadList, buf);
         delete [] buf;
      }

      secconn.recv((char*)&size, 4);
      if (size > 0)
      {
         buf = new char[size];
         secconn.recv(buf, size);
         au->deserialize(au->m_vstrWriteList, buf);
         delete [] buf;
      }

      int32_t exec;
      secconn.recv((char*)&exec, 4);
      au->m_bExec = exec;

      m_UserManager.insert(au);

      char text[128];
      sprintf(text, "User %s login from %s", user, ip.c_str());
      m_SectorLog.insert(text);

      if (ukey <= 0)
      {
         // send the list of masters to the new users
         s.send((char*)&m_iRouterKey, 4);
         int num = m_Routing.getNumOfMasters() - 1;
         s.send((char*)&num, 4);
         map<uint32_t, Address> al;
         m_Routing.getListOfMasters(al);
         for (map<uint32_t, Address>::iterator i = al.begin(); i != al.end(); ++ i)
         {
            if (i->first == m_iRouterKey)
               continue;

            s.send((char*)&i->first, 4);
            int size = i->second.m_strIP.length() + 1;
            s.send((char*)&size, 4);
            s.send(i->second.m_strIP.c_str(), size);
            s.send((char*)&i->second.m_iPort, 4);
         }
      }

      // for synchronization only, message content is meaningless
      s.send((char*)&key, 4);
   }
   else
   {
      char text[128];
      sprintf(text, "User %s login rejected from %s", user, ip.c_str());
      m_SectorLog.insert(text);
   }

   return 0;
}

int Master::processMasterJoin(SSLTransport& s, SSLTransport& secconn, const std::string& ip)
{
   char masterIP[64];
   strcpy(masterIP, ip.c_str());
   secconn.send(masterIP, 64);
   int32_t res = -1;
   secconn.recv((char*)&res, 4);

   if (res > 0)
      res = m_iRouterKey;
   s.send((char*)&res, 4);

   if (res > 0)
   {
      int masterPort;
      int32_t key;
      s.recv((char*)&masterPort, 4);
      s.recv((char*)&key, 4);

      // send master list
      int num = m_Routing.getNumOfMasters() - 1;
      s.send((char*)&num, 4);
      map<uint32_t, Address> al;
      m_Routing.getListOfMasters(al);
      for (map<uint32_t, Address>::iterator i = al.begin(); i != al.end(); ++ i)
      {
         if (i->first == m_iRouterKey)
            continue;

         s.send((char*)&i->first, 4);
         int size = i->second.m_strIP.length() + 1;
         s.send((char*)&size, 4);
         s.send(i->second.m_strIP.c_str(), size);
         s.send((char*)&i->second.m_iPort, 4);
      }

      // send slave list
      char* buf = NULL;
      int32_t size = 0;
      num = m_SlaveManager.serializeSlaveList(buf, size);
      s.send((char*)&num, 4);
      s.send((char*)&size, 4);
      s.send(buf, size);
      delete [] buf;

      // send user list
      num = 0;
      vector<char*> bufs;
      vector<int> sizes;
      m_UserManager.serializeUsers(num, bufs, sizes);
      s.send((char*)&num, 4);
      for (int i = 0; i < num; ++ i)
      {
         s.send((char*)&sizes[i], 4);
         s.send(bufs[i], sizes[i]);
         delete [] bufs[i];
      }

      // send metadata
      m_pMetadata->serialize("/", m_strHomeDir + ".tmp/master_meta.dat");

      struct stat st;
      stat((m_strHomeDir + ".tmp/master_meta.dat").c_str(), &st);
      size = st.st_size;
      s.send((char*)&size, 4);
      s.sendfile((m_strHomeDir + ".tmp/master_meta.dat").c_str(), 0, size);
      unlink((m_strHomeDir + ".tmp/master_meta.dat").c_str());

      // send new master info to all existing masters
      for (map<uint32_t, Address>::iterator i = al.begin(); i != al.end(); ++ i)
      {
         SectorMsg msg;
         msg.setKey(0);
         msg.setType(1001);
         msg.setData(0, (char*)&key, 4);
         msg.setData(4, masterIP, strlen(masterIP) + 1);
         msg.setData(68, (char*)&masterPort, 4);
         m_GMP.rpc(i->second.m_strIP.c_str(), i->second.m_iPort, &msg, &msg);
      }

      Address addr;
      addr.m_strIP = masterIP;
      addr.m_iPort = masterPort;
      m_Routing.insert(key, addr);

      // send new master info to all slaves
      SectorMsg msg;
      msg.setKey(0);
      msg.setType(1001);
      msg.setData(0, (char*)&key, 4);
      msg.setData(4, masterIP, strlen(masterIP) + 1);
      msg.setData(68, (char*)&masterPort, 4);
      m_SlaveManager.updateSlaveList(m_vSlaveList, m_llLastUpdateTime);
      m_GMP.multi_rpc(m_vSlaveList, &msg);
   }

   return 0;
}

void* Master::process(void* s)
{
   Master* self = (Master*)s;

   const int ProcessWorker = 4;
   for (int i = 0; i < ProcessWorker; ++ i)
   {
      pthread_t t;
      pthread_create(&t, NULL, processEx, self);
      pthread_detach(t);
   }

   while (self->m_Status == RUNNING)
   {
      string ip;
      int port;
      int32_t id;
      SectorMsg* msg = new SectorMsg;
      //msg->resize(65536);

      if (self->m_GMP.recvfrom(ip, port, id, msg) < 0)
         continue;

      int32_t key = msg->getKey();
      User* user = self->m_UserManager.lookup(key);
      if (NULL == user)
      {
         self->reject(ip, port, id, SectorError::E_EXPIRED);
         continue;
      }

      bool secure = false;

      if (key > 0)
      {
         if ((user->m_strIP == ip) && (user->m_iPort == port))
         {
            secure = true;
            user->m_llLastRefreshTime = CTimer::getTime();
         }
      }
      else if (key == 0)
      {
         Address addr;
         addr.m_strIP = ip;
         addr.m_iPort = port;
         if (self->m_SlaveManager.getSlaveID(addr) >= 0)
            secure = true;
         else if (self->m_Routing.getRouterID(addr) >= 0)
            secure = true;
      }

      if (!secure)
      {
         self->reject(ip, port, id, SectorError::E_SECURITY);
         continue;
      }

      ProcessJobParam* p = new ProcessJobParam;
      p->ip = ip;
      p->port = port;
      p->user = user;
      p->key = key;
      p->id = id;
      p->msg = msg;

      self->m_ProcessJobQueue.push(p);
   }

   return NULL;
}

void* Master::processEx(void* param)
{
   Master* self = (Master*)param;

   while (true)
   {
      ProcessJobParam* p = (ProcessJobParam*)self->m_ProcessJobQueue.pop();
      if (NULL == p)
         break;

      switch (p->msg->getType() / 100)
      {
      case 0:
         self->processSysCmd(p->ip, p->port, p->user, p->key, p->id, p->msg);
         break;

      case 1:
         self->processFSCmd(p->ip, p->port, p->user, p->key, p->id, p->msg);
         break;

      case 2:
         self->processDCCmd(p->ip, p->port, p->user, p->key, p->id, p->msg);
         break;

      case 10:
         self->processMCmd(p->ip, p->port, p->user, p->key, p->id, p->msg);
         break;

      case 11:
         self->processSyncCmd(p->ip, p->port, p->user, p->key, p->id, p->msg);
         break;

      default:
         self->reject(p->ip, p->port, p->id, SectorError::E_UNKNOWN);
      }

      delete p->msg;
      delete p;
   }

   return NULL;
}

int Master::processSysCmd(const string& ip, const int port, const User* user, const int32_t key, int id, SectorMsg* msg)
{
   // internal system commands

   switch (msg->getType())
   {
   case 1: // slave reports transaction status
   {
      int transid = *(int32_t*)msg->getData();
      int slaveid = *(int32_t*)(msg->getData() + 4);

      Transaction t;
      if (m_TransManager.retrieve(transid, t) < 0)
      {
         m_GMP.sendto(ip, port, id, msg);
         break;
      }
            
      int change = *(int32_t*)(msg->getData() + 8);
      Address addr;
      addr.m_strIP = ip;
      addr.m_iPort = port;

      int num = *(int32_t*)(msg->getData() + 12);
      int pos = 16;
      for (int i = 0; i < num; ++ i)
      {
         int size = *(int32_t*)(msg->getData() + pos);         
         string path = msg->getData() + pos + 4;
         pos += size + 4;

         m_pMetadata->update(path.c_str(), addr, change);

         if (change == 3)
         {
            SNode attr;
            attr.deserialize(path.c_str());
            m_sstrOnReplicate.erase(attr.m_strName);
         }
      }
      // send file changes to all other masters
      if (m_Routing.getNumOfMasters() > 1)
      {
         SectorMsg newmsg;
         newmsg.setData(0, (char*)&change, 4);
         newmsg.setData(4, ip.c_str(), 64);
         newmsg.setData(68, (char*)&port, 4);
         newmsg.setData(72, msg->getData() + 12, msg->m_iDataLength - 12);
         sync(newmsg.getData(), newmsg.m_iDataLength, 1100);
      }

      // unlock the file, if this is a file operation
      // update transaction status, if this is a file operation; if it is sphere, a final sphere report will be sent, see #4.
      if (t.m_iType == 0)
      {
         m_pMetadata->unlock(t.m_strFile.c_str(), t.m_iUserKey, t.m_iMode);
         m_TransManager.updateSlave(transid, slaveid);
      }

      msg->m_iDataLength = SectorMsg::m_iHdrSize;
      //TODO: feedback failed files, so that slave will delete them
      //if (r < 0)
      //   msg->setType(-msg->getType());
      m_GMP.sendto(ip, port, id, msg);
      pthread_mutex_lock(&m_ReplicaLock);
      if (!m_vstrToBeReplicated.empty())
         pthread_cond_signal(&m_ReplicaCond);
      pthread_mutex_unlock(&m_ReplicaLock);

      break;
   }

   case 2: // client logout
   {
      char text[128];
      sprintf(text, "User %s logout from %s.", user->m_strName.c_str(), ip.c_str());
      m_SectorLog.insert(text);

      m_UserManager.remove(key);

      m_GMP.sendto(ip, port, id, msg);

      break;
   }

   case 3: // sysinfo
   {
      if (!m_Routing.match(key, m_iRouterKey))
      {
         reject(ip, port, id, SectorError::E_MASTER);
         break;
      }

      char* buf = NULL;
      int size = 0;
      serializeSysStat(buf, size);
      msg->setData(0, buf, size);
      delete [] buf;
      m_GMP.sendto(ip, port, id, msg);

      if (user->m_strName == "root")
      {
         //TODO: send current users, current transactions
      }

      //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "sysinfo", "", "SUCCESS", "");

      break;
   }

   case 4: // sphere status & performance report
   {
      int transid = *(int32_t*)msg->getData();
      int slaveid = *(int32_t*)(msg->getData() + 4);

      Transaction t;
      if ((m_TransManager.retrieve(transid, t) < 0) || (t.m_iType != 1))
      {
         m_GMP.sendto(ip, port, id, msg);
         break;
      }

      // the slave votes slow slaves
      int num = *(int*)(msg->getData() + 8);
      Address voter;
      voter.m_strIP = ip;
      voter.m_iPort = port;
      m_SlaveManager.voteBadSlaves(voter, num, msg->getData() + 12);

      m_TransManager.updateSlave(transid, slaveid);

      msg->m_iDataLength = SectorMsg::m_iHdrSize;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   case 5: //update master lists
   {
      int num = m_Routing.getNumOfMasters() - 1;
      msg->setData(0, (char*)&num, 4);
      int p = 4;
      map<uint32_t, Address> al;
      m_Routing.getListOfMasters(al);
      for (map<uint32_t, Address>::iterator i = al.begin(); i != al.end(); ++ i)
      {
         if (i->first == m_iRouterKey)
            continue;

         msg->setData(p, (char*)&i->first, 4);
         int size = i->second.m_strIP.length() + 1;
         msg->setData(p + 4, i->second.m_strIP.c_str(), size);
         msg->setData(p + size + 4, (char*)&i->second.m_iPort, 4);
         p += size + 8;
      }

      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   case 6: // client keep-alive messages
   {
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   case 7: // unregister in-memory objects
   {
      int num = *(int32_t*)(msg->getData() + 8);
      int pos = 12;
      for (int i = 0; i < num; ++ i)
      {
         int size = *(int32_t*)(msg->getData() + pos);
         string path = msg->getData() + pos + 4;
         pos += size + 4;

         m_pMetadata->remove(path.c_str());

         // erase this from all other masters
         sync(path.c_str(), path.length() + 1, 1105);
      }

      msg->m_iDataLength = SectorMsg::m_iHdrSize;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   default:
      reject(ip, port, id, SectorError::E_UNKNOWN);
      return -1;
   }

   return 0;
}

int Master::processFSCmd(const string& ip, const int port,  const User* user, const int32_t key, int id, SectorMsg* msg)
{
   // 100+ storage system

   switch (msg->getType())
   {
   case 101: // ls
   {
      if (!m_Routing.match(msg->getData(), m_iRouterKey))
      {
         reject(ip, port, id, SectorError::E_MASTER);
         break;
      }

      int rwx = SF_MODE::READ;
      string dir = msg->getData();
      if (!user->match(dir, rwx))
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "ls", dir.c_str(), "REJECT", "");
         break;
      }

      vector<string> filelist;
      m_pMetadata->list(dir.c_str(), filelist);

      msg->m_iDataLength = SectorMsg::m_iHdrSize;
      int size = 0;
      for (vector<string>::iterator i = filelist.begin(); i != filelist.end(); ++ i)
      {
         msg->setData(size, i->c_str(), i->length());
         size += i->length();
         msg->setData(size, ";", 1);
         size += 1;
      }
      msg->setData(size, "\0", 1);

      m_GMP.sendto(ip, port, id, msg);

      //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "ls", dir.c_str(), "SUCCESS", "");

      break;
   }

   case 102: // stat
   {
      if (!m_Routing.match(msg->getData(), m_iRouterKey))
      {
         reject(ip, port, id, SectorError::E_MASTER);
         break;
      }

      int rwx = SF_MODE::READ;
      if (!user->match(msg->getData(), rwx))
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         break;
      }

      SNode attr;
      int r = m_pMetadata->lookup(msg->getData(), attr);
      if (r < 0)
      {
         reject(ip, port, id, SectorError::E_NOEXIST);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "stat", msg->getData(), "REJECT", "");
      }
      else
      {
         char buf[128];
         attr.serialize(buf);
         msg->setData(0, buf, strlen(buf) + 1);

         int c = 0;

         if (!attr.m_bIsDir)
         {
            for (set<Address, AddrComp>::iterator i = attr.m_sLocation.begin(); i != attr.m_sLocation.end(); ++ i)
            {
               msg->setData(128 + c * 68, i->m_strIP.c_str(), i->m_strIP.length() + 1);
               msg->setData(128 + c * 68 + 64, (char*)&(i->m_iPort), 4);
               ++ c;
            }
         }
         else
         {
            set<Address, AddrComp> addr;
            m_pMetadata->lookup(attr.m_strName.c_str(), addr);

            for (set<Address, AddrComp>::iterator i = addr.begin(); i != addr.end(); ++ i)
            {
               msg->setData(128 + c * 68, i->m_strIP.c_str(), i->m_strIP.length() + 1);
               msg->setData(128 + c * 68 + 64, (char*)&(i->m_iPort), 4);
               ++ c;
            }
         }

         m_GMP.sendto(ip, port, id, msg);

         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "stat", msg->getData(), "SUCCESS", "");
      }

      break;
   }

   case 103: // mkdir
   {
      if (!m_Routing.match(msg->getData(), m_iRouterKey))
      {
         reject(ip, port, id, SectorError::E_MASTER);
         break;
      }

      int rwx = SF_MODE::WRITE;
      if (!user->match(msg->getData(), rwx))
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "mkdir", msg->getData(), "REJECT E_PERMISSION", "");
         break;
      }

      SNode attr;
      if (m_pMetadata->lookup(msg->getData(), attr) >= 0)
      {
         // directory already exist
         reject(ip, port, id, SectorError::E_EXIST);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "mkdir", msg->getData(), "REJECT E_EXIST", "");
         break;
      }

      Address client;
      client.m_strIP = ip;
      client.m_iPort = port;
      set<int> empty;
      vector<SlaveNode> addr;
      if (m_SlaveManager.chooseIONode(empty, client, SF_MODE::WRITE, addr, 1) <= 0)
      {
         reject(ip, port, id, SectorError::E_RESOURCE);
         break;
      }

      int msgid = 0;
      m_GMP.sendto(addr.begin()->m_strIP.c_str(), addr.begin()->m_iPort, msgid, msg);

      m_pMetadata->create(msg->getData(), true);

      // send file changes to all other masters
      sync(msg->getData(), msg->m_iDataLength, 1103);

      m_GMP.sendto(ip, port, id, msg);

      //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "mkdir", msg->getData(), "SUCCESS", addr.begin()->m_strIP.c_str());

      break;
   }

   case 104: // move a dir/file
   {
      string src = msg->getData() + 4;
      string dst = msg->getData() + 4 + src.length() + 1 + 4;
      string uplevel = dst.substr(0, dst.rfind('/') + 1);
      string sublevel = dst + src.substr(src.rfind('/'), src.length());

      if (!m_Routing.match(src.c_str(), m_iRouterKey))
      {
         reject(ip, port, id, SectorError::E_MASTER);
         break;
      }

      SNode tmp;
      if ((uplevel.length() > 0) && (m_pMetadata->lookup(uplevel.c_str(), tmp) < 0))
      {
         reject(ip, port, id, SectorError::E_NOEXIST);
         break;
      }
      if (m_pMetadata->lookup(sublevel.c_str(), tmp) >= 0)
      {
         reject(ip, port, id, SectorError::E_EXIST);
         break;
      }

      int rwx = SF_MODE::READ;
      if (!user->match(src.c_str(), rwx))
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         break;
      }
      rwx = SF_MODE::WRITE;
      if (!user->match(dst.c_str(), rwx))
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         break;
      }

      SNode as, at;
      int rs = m_pMetadata->lookup(src.c_str(), as);
      int rt = m_pMetadata->lookup(dst.c_str(), at);
      set<Address, AddrComp> addrlist;
      m_pMetadata->lookup(src.c_str(), addrlist);

      if (rs < 0)
      {
         reject(ip, port, id, SectorError::E_NOEXIST);
         break;
      }
      if ((rt >= 0) && (!at.m_bIsDir))
      {
         reject(ip, port, id, SectorError::E_EXIST);
         break;
      }

      string newname = dst.substr(dst.rfind('/') + 1, dst.length());
      if (rt < 0)
         m_pMetadata->move(src.c_str(), uplevel.c_str(), newname.c_str());
      else
         m_pMetadata->move(src.c_str(), dst.c_str());

      msg->setData(0, src.c_str(), src.length() + 1);
      msg->setData(src.length() + 1, uplevel.c_str(), uplevel.length() + 1);
      msg->setData(src.length() + 1 + uplevel.length() + 1, newname.c_str(), newname.length() + 1);
      for (set<Address, AddrComp>::iterator i = addrlist.begin(); i != addrlist.end(); ++ i)
      {
         int msgid = 0;
         m_GMP.sendto(i->m_strIP.c_str(), i->m_iPort, msgid, msg);
      }

      // send file changes to all other masters
      if (m_Routing.getNumOfMasters() > 1)
      {
         SectorMsg newmsg;
         newmsg.setData(0, (char*)&rt, 4);
         newmsg.setData(4, src.c_str(), src.length() + 1);
         int pos = 4 + src.length() + 1;
         if (rt < 0)
         {
            newmsg.setData(pos, uplevel.c_str(), uplevel.length() + 1);
            pos += uplevel.length() + 1;
            newmsg.setData(pos, newname.c_str(), newname.length() + 1);
         }
         else
            newmsg.setData(pos, dst.c_str(), dst.length() + 1);
         sync(newmsg.getData(), newmsg.m_iDataLength, 1104);
      }

      m_GMP.sendto(ip, port, id, msg);

      break;
   }

   case 105: // delete dir/file
   {
      if (!m_Routing.match(msg->getData(), m_iRouterKey))
      {
         reject(ip, port, id, SectorError::E_MASTER);
         break;
      }

      int rwx = SF_MODE::WRITE;
      if (!user->match(msg->getData(), rwx))
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "delete", msg->getData(), "REJECT", "");
         break;
      }

      SNode attr;
      int n = m_pMetadata->lookup(msg->getData(), attr);

      if (n < 0)
      {
         reject(ip, port, id, SectorError::E_NOEXIST);
         break;
      }
      else if (attr.m_bIsDir)
      {
         vector<string> fl;
         if (m_pMetadata->list(msg->getData(), fl) > 0)
         {
            // directory not empty
            reject(ip, port, id, SectorError::E_NOEMPTY);
            break;
         }
      }

      string filename = msg->getData();

      if (!attr.m_bIsDir)
      {
         for (set<Address, AddrComp>::iterator i = attr.m_sLocation.begin(); i != attr.m_sLocation.end(); ++ i)
         {
            int msgid = 0;
            m_GMP.sendto(i->m_strIP.c_str(), i->m_iPort, msgid, msg);
         }
      }
      else
      {
         m_SlaveManager.updateSlaveList(m_vSlaveList, m_llLastUpdateTime);
         for (vector<Address>::iterator i = m_vSlaveList.begin(); i != m_vSlaveList.end(); ++ i)
         {
            int msgid = 0;
            m_GMP.sendto(i->m_strIP.c_str(), i->m_iPort, msgid, msg);
         }
      }

      m_pMetadata->remove(filename.c_str(), true);

      // send file changes to all other masters
      sync(filename.c_str(), filename.length() + 1, 1105);

      msg->m_iDataLength = SectorMsg::m_iHdrSize;
      m_GMP.sendto(ip, port, id, msg);

      //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "delete", filename.c_str(), "SUCCESS", "");

      break;
   }

   case 106: // make a copy of a file/dir
   {
      string src = msg->getData() + 4;
      string dst = msg->getData() + 4 + src.length() + 1 + 4;
      string uplevel = dst.substr(0, dst.find('/'));
      string sublevel = dst + src.substr(src.rfind('/'), src.length());

      if (!m_Routing.match(src.c_str(), m_iRouterKey))
      {
         reject(ip, port, id, SectorError::E_MASTER);
         break;
      }

      SNode tmp;
      if ((uplevel.length() > 0) && (m_pMetadata->lookup(uplevel.c_str(), tmp) < 0))
      {
         reject(ip, port, id, SectorError::E_NOEXIST);
         break;
      }
      if (m_pMetadata->lookup(sublevel.c_str(), tmp) >= 0)
      {
         reject(ip, port, id, SectorError::E_EXIST);
         break;
      }

      int rwx = SF_MODE::READ;
      if (!user->match(src.c_str(), rwx))
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         break;
      }
      rwx = SF_MODE::WRITE;
      if (!user->match(dst.c_str(), rwx))
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         break;
      }

      SNode as, at;
      int rs = m_pMetadata->lookup(src.c_str(), as);
      int rt = m_pMetadata->lookup(dst.c_str(), at);
      vector<string> filelist;
      m_pMetadata->list_r(src.c_str(), filelist);
      if (rs < 0)
      {
         reject(ip, port, id, SectorError::E_NOEXIST);
         break;
      }
      if ((rt >= 0) && (!at.m_bIsDir))
      {
         reject(ip, port, id, SectorError::E_EXIST);
         break;
      }

      // replace the directory prefix with dst
      string rep;
      if (rt < 0)
         rep = src;
      else
         rep = src.substr(0, src.rfind('/'));

      pthread_mutex_lock(&m_ReplicaLock);
      for (vector<string>::iterator i = filelist.begin(); i != filelist.end(); ++ i)
      {
         string target = *i;
         target.replace(0, rep.length(), dst);
         m_vstrToBeReplicated.insert(m_vstrToBeReplicated.begin(), src + "\t" + target);
      }
      if (!m_vstrToBeReplicated.empty())
         pthread_cond_signal(&m_ReplicaCond);
      pthread_mutex_unlock(&m_ReplicaLock);

      m_GMP.sendto(ip, port, id, msg);

      break;
   }

   case 107: // utime
   {
      if (!m_Routing.match(msg->getData(), m_iRouterKey))
      {
         reject(ip, port, id, SectorError::E_MASTER);
         break;
      }

      int rwx = SF_MODE::WRITE;
      if (!user->match(msg->getData(), rwx))
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "delete", msg->getData(), "REJECT", "");
         break;
      }

      SNode attr;
      if (m_pMetadata->lookup(msg->getData(), attr) < 0)
      {
         reject(ip, port, id, SectorError::E_NOEXIST);
         break;
      }

      for (set<Address, AddrComp>::iterator i = attr.m_sLocation.begin(); i != attr.m_sLocation.end(); ++ i)
      {
         int msgid = 0;
         m_GMP.sendto(i->m_strIP.c_str(), i->m_iPort, msgid, msg);
      }

      string path = msg->getData();
      int64_t newts = *(int64_t*)(msg->getData() + strlen(msg->getData()) + 1);

      m_pMetadata->utime(path, newts);

      // send file changes to all other masters
      if (m_Routing.getNumOfMasters() > 1)
      {
         SectorMsg newmsg;
         newmsg.setData(0, (char*)&newts, 8);
         newmsg.setData(8, path.c_str(), path.length() + 1);
         sync(newmsg.getData(), newmsg.m_iDataLength, 1107);
      }

      msg->m_iDataLength = SectorMsg::m_iHdrSize;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   case 110: // open file
   {
      int32_t mode = *(int32_t*)(msg->getData());
      int32_t dataport = *(int32_t*)(msg->getData() + 4);
      string hintip = msg->getData() + 8;
      string path = msg->getData() + 72;

      if (!m_Routing.match(path.c_str(), m_iRouterKey))
      {
         reject(ip, port, id, SectorError::E_MASTER);
         break;
      }

      // check user's permission on that file
      int rwx = mode;
      if (!user->match(path.c_str(), rwx))
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "open", msg->getData(), "REJECT", "");
         break;
      }

      SNode attr;
      int r = m_pMetadata->lookup(path.c_str(), attr);

      Address hint;
      if (hintip.length() == 0)
         hint.m_strIP = ip;
      else
         hint.m_strIP = hintip;
      hint.m_iPort = port;
      vector<SlaveNode> addr;

      if (r < 0)
      {
         // file does not exist
         if (!(mode & SF_MODE::WRITE))
         {
            reject(ip, port, id, SectorError::E_NOEXIST);
            //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "open", path.c_str(), "REJECT", "");
            break;
         }

         // otherwise, create a new file for write
         // choose a slave node for the new file
         set<int> empty;
         if (m_SlaveManager.chooseIONode(empty, hint, mode, addr, m_SysConfig.m_iReplicaNum) <= 0)
         {
            reject(ip, port, id, SectorError::E_NODISK);
            //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "open", msg->getData(), "REJECT", "");
            break;
         }

         // create the new file in the metadata, no loc info yet
         m_pMetadata->create(path.c_str(), false);
         m_pMetadata->lock(path.c_str(), key, rwx);
      }
      else
      {
         r = m_pMetadata->lock(path.c_str(), key, rwx);
         if (r < 0)
         {
            reject(ip, port, id, SectorError::E_BUSY);
            //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "open", path.c_str(), "REJECT", "");
            break;
         }

         m_SlaveManager.chooseIONode(attr.m_sLocation, hint, mode, addr, m_SysConfig.m_iReplicaNum);
      }

      int transid = m_TransManager.create(0, key, msg->getType(), path, mode);

      //set up all slave nodes, chain of write
      string srcip;
      int srcport;
      string dstip = "";
      int dstport = 0;

      msg->setData(136, (char*)&key, 4);
      msg->setData(140, (char*)&mode, 4);
      msg->setData(144, (char*)&transid, 4);
      msg->setData(148, (char*)user->m_pcKey, 16);
      msg->setData(164, (char*)user->m_pcIV, 8);
      msg->setData(172, path.c_str(), path.length() + 1);

      for (vector<SlaveNode>::iterator i = addr.begin(); i != addr.end();)
      {
         vector<SlaveNode>::iterator curraddr = i ++;

         if (i == addr.end())
         {
            srcip = ip;
            srcport = dataport;
         }
         else
         {
            srcip = i->m_strIP;
            srcport = i->m_iDataPort;
            // do not use secure transfer between slave nodes
            int m = mode & (0xFFFFFFFF - SF_MODE::SECURE);
            msg->setData(140, (char*)&m, 4);
         }

         msg->setData(0, srcip.c_str(), srcip.length() + 1);
         msg->setData(64, (char*)&(srcport), 4);
         msg->setData(68, dstip.c_str(), dstip.length() + 1);
         msg->setData(132, (char*)&(dstport), 4);
         SectorMsg response;
         if ((m_GMP.rpc(curraddr->m_strIP.c_str(), curraddr->m_iPort, msg, &response) < 0) || (response.getType() < 0))
         {
            reject(ip, port, id, SectorError::E_RESOURCE);
            //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "open", path.c_str(), "FAIL", "");

            //TODO: FIX THIS, ROLLBACK TRANS
         }

         dstip = curraddr->m_strIP;
         dstport = curraddr->m_iDataPort;

         m_TransManager.addSlave(transid, curraddr->m_iNodeID);
      }

      // send the connection information back to the client
      msg->setData(0, dstip.c_str(), dstip.length() + 1);
      msg->setData(64, (char*)&dstport, 4);
      msg->setData(68, (char*)&transid, 4);
      msg->setData(72, (char*)&attr.m_llSize, 8);
      msg->m_iDataLength = SectorMsg::m_iHdrSize + 80;

      m_GMP.sendto(ip, port, id, msg);

      //if (key != 0)
      //   m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "open", path.c_str(), "SUCCESS", addr.rbegin()->m_strIP.c_str());

      break;
   }

   default:
      reject(ip, port, id, SectorError::E_UNKNOWN);
      return -1;
   }

   return 0;
}

int Master::processDCCmd(const string& ip, const int port,  const User* user, const int32_t key, int id, SectorMsg* msg)
{
   // 200+ SPE

   switch (msg->getType())
   {
   case 201: // prepare SPE input information
   {
      if (!user->m_bExec)
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "request SPE", "", "REJECTED DUE TO PERMISSION", "");
         break;
      }

      vector<string> result;
      char* req = msg->getData();
      int32_t size = *(int32_t*)req;
      int offset = 0;
      bool notfound = false;
      while (size != -1)
      {
         int r = m_pMetadata->collectDataInfo(req + offset + 4, result);
         if (r < 0)
         {
            notfound = true;
            break;
         }

         offset += 4 + size;
         size = *(int32_t*)(req + offset);
      }

      if (notfound)
      {
         reject(ip, port, id, SectorError::E_NOEXIST);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "request SPE", "", "REJECTED: FILE NO EXIST", "");
         break;
      }

      offset = 0;
      for (vector<string>::iterator i = result.begin(); i != result.end(); ++ i)
      {
         msg->setData(offset, i->c_str(), i->length() + 1);
         offset += i->length() + 1;
      }

      msg->m_iDataLength = SectorMsg::m_iHdrSize + offset;
      m_GMP.sendto(ip, port, id, msg);

      //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "request SPE", "", "SUCCESS", "");

      break;
   }

   case 202: // locate SPEs
   {
      if (!user->m_bExec)
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "locate SPE", "", "REJECTED DUE TO PERMISSION", "");
         break;
      }

      //TODO: locate and pack SPE info in m_SlaveManager
      vector<SlaveNode> sl;
      Address client;
      client.m_strIP = ip;
      client.m_iPort = port;
      m_SlaveManager.chooseSPENodes(client, sl);

      int c = 0;
      for (vector<SlaveNode>::iterator i = sl.begin(); i != sl.end(); ++ i)
      {
         msg->setData(c * 72, i->m_strIP.c_str(), i->m_strIP.length() + 1);
         msg->setData(c * 72 + 64, (char*)&(i->m_iPort), 4);
         msg->setData(c * 72 + 68, (char*)&(i->m_iDataPort), 4);
         c ++;
      }

      m_GMP.sendto(ip, port, id, msg);

      //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "locate SPE", "", "SUCCESS", "");

      break;
   }

   case 203: // start spe
   {
      if (!user->m_bExec)
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "start SPE", "", "REJECTED DUE TO PERMISSION", "");
         break;
      }

      Address addr;
      addr.m_strIP = msg->getData();
      addr.m_iPort = *(int32_t*)(msg->getData() + 64);

      int transid = m_TransManager.create(1, key, msg->getType(), "", 0);
      int slaveid = m_SlaveManager.getSlaveID(addr);
      m_TransManager.addSlave(transid, slaveid);

      msg->setData(0, ip.c_str(), ip.length() + 1);
      msg->setData(64, (char*)&port, 4);
      msg->setData(68, (char*)&user->m_iDataPort, 4);
      msg->setData(msg->m_iDataLength - SectorMsg::m_iHdrSize, (char*)&transid, 4);

      if ((m_GMP.rpc(addr.m_strIP.c_str(), addr.m_iPort, msg, msg) < 0) || (msg->getType() < 0))
      {
         reject(ip, port, id, SectorError::E_RESOURCE);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "start SPE", "", "SLAVE FAILURE", "");
         break;
      }

      msg->setData(0, (char*)&transid, 4);
      msg->m_iDataLength = SectorMsg::m_iHdrSize + 4;
      m_GMP.sendto(ip, port, id, msg);

      //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "start SPE", "", "SUCCESS", addr.m_strIP.c_str());

      break;
   }

   case 204: // start shuffler
   {
      if (!user->m_bExec)
      {
         reject(ip, port, id, SectorError::E_PERMISSION);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "start Shuffler", "", "REJECTED DUE TO PERMISSION", "");
         break;
      }

      Address addr;
      addr.m_strIP = msg->getData();
      addr.m_iPort = *(int32_t*)(msg->getData() + 64);

      int transid = m_TransManager.create(1, key, msg->getType(), "", 0);
      m_TransManager.addSlave(transid, m_SlaveManager.getSlaveID(addr));

      msg->setData(0, ip.c_str(), ip.length() + 1);
      msg->setData(64, (char*)&port, 4);
      msg->setData(msg->m_iDataLength - SectorMsg::m_iHdrSize, (char*)&transid, 4);
      msg->setData(msg->m_iDataLength - SectorMsg::m_iHdrSize, (char*)&(user->m_iDataPort), 4);

      if ((m_GMP.rpc(addr.m_strIP.c_str(), addr.m_iPort, msg, msg) < 0) || (msg->getType() < 0))
      {
         reject(ip, port, id, SectorError::E_RESOURCE);
         //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "start Shuffler", "", "SLAVE FAILURE", "");
         break;
      }

      m_GMP.sendto(ip, port, id, msg);

      //m_SectorLog.logUserActivity(user->m_strName.c_str(), ip, "start Shuffler", "", "SUCCESS", addr.m_strIP.c_str());

      break;
   }

   default:
      reject(ip, port, id, SectorError::E_UNKNOWN);
      return -1;
   }

   return 0;
}

int Master::processMCmd(const string& ip, const int port,  const User* user, const int32_t key, int id, SectorMsg* msg)
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

      m_GMP.sendto(ip, port, id, msg);

      break;
   }

   case 1005: // master probe
   {
      if (*(uint32_t*)msg->getData() != m_iRouterKey)
         msg->setType(-msg->getType());
      msg->m_iDataLength = SectorMsg::m_iHdrSize + 4;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   case 1007: // slave lost
   {
      int32_t sid = *(int32_t*)msg->getData();

      Address addr;
      if (m_SlaveManager.getSlaveAddr(sid, addr) >= 0)
         m_pMetadata->substract("/", addr);

      m_SlaveManager.remove(sid);

      msg->m_iDataLength = SectorMsg::m_iHdrSize + 4;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   default:
      reject(ip, port, id, SectorError::E_UNKNOWN);
      return -1;
   }

   return 0;
}

int Master::sync(const char* fileinfo, const int& size, const int& type)
{
   SectorMsg msg;
   msg.setKey(0);
   msg.setType(type);
   msg.setData(0, fileinfo, size);

   // send file changes to all other masters
   map<uint32_t, Address> al;
   m_Routing.getListOfMasters(al);
   for (map<uint32_t, Address>::iterator i = al.begin(); i != al.end(); ++ i)
   {
      if (i->first == m_iRouterKey)
         continue;

      m_GMP.rpc(i->second.m_strIP.c_str(), i->second.m_iPort, &msg, &msg);
   }

   return 0;
}

int Master::processSyncCmd(const string& ip, const int port,  const User* user, const int32_t key, int id, SectorMsg* msg)
{
   switch (msg->getType())
   {
   case 1100: // file change
   {
      int change = *(int32_t*)msg->getData();
      Address addr;
      addr.m_strIP = msg->getData() + 4;
      addr.m_iPort = *(int32_t*)(msg->getData() + 68);

      int num = *(int32_t*)(msg->getData() + 72);
      int pos = 76;
      for (int i = 0; i < num; ++ i)
      {
         int size = *(int32_t*)(msg->getData() + pos);
         string path = msg->getData() + pos + 4;
         pos += size + 4;

         m_pMetadata->update(path.c_str(), addr, change);
      }

      msg->m_iDataLength = SectorMsg::m_iHdrSize + 4;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   case 1103: // mkdir
   {
      m_pMetadata->create(msg->getData(), true);

      msg->m_iDataLength = SectorMsg::m_iHdrSize + 4;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   case 1104: // mv
   {
      int rt = *(int32_t*)msg->getData();
      string src = msg->getData() + 4;

      if (rt < 0)
      {
         string uplevel = msg->getData() + 4 + src.length() + 1;
         string newname = msg->getData() + 4 + src.length() + 1 + uplevel.length() + 1;
         m_pMetadata->move(src.c_str(), uplevel.c_str(), newname.c_str());
      }
      else
      {
         string dst = msg->getData() + 4 + src.length() + 1;
         m_pMetadata->move(src.c_str(), dst.c_str());
      }

      msg->m_iDataLength = SectorMsg::m_iHdrSize + 4;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   case 1105: // delete
   {
      m_pMetadata->remove(msg->getData(), true);

      msg->m_iDataLength = SectorMsg::m_iHdrSize + 4;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   case 1107: // utime
   {
      m_pMetadata->utime(msg->getData() + 8, *(int64_t*)msg->getData());
      msg->m_iDataLength = SectorMsg::m_iHdrSize + 4;
      m_GMP.sendto(ip, port, id, msg);
      break;
   }

   default:
      reject(ip, port, id, SectorError::E_UNKNOWN);
      return -1;
   }

   return 0;
}

void Master::reject(const string& ip, const int port, int id, int32_t code)
{
   SectorMsg msg;
   msg.setType(-1);
   msg.setData(0, (char*)&code, 4);
   msg.m_iDataLength = SectorMsg::m_iHdrSize + 4;
   m_GMP.sendto(ip, port, id, &msg);
}

void* Master::replica(void* s)
{
   Master* self = (Master*)s;

   while (self->m_Status == RUNNING)
   {
      pthread_mutex_lock(&self->m_ReplicaLock);

      vector<string>::iterator r = self->m_vstrToBeReplicated.begin();

      for (; r != self->m_vstrToBeReplicated.end(); ++ r)
      {
         if (self->m_TransManager.getTotalTrans() + self->m_sstrOnReplicate.size() >= self->m_SlaveManager.getNumberOfSlaves())
            break;

         int pos = r->find('\t');
         string src = r->substr(0, pos);
         string dst = r->substr(pos + 1, r->length());

         if (src != dst)
         {
            self->createReplica(src, dst);
         }
         else
         {
            // avoid replicate a file that is currently being replicated
            if (self->m_sstrOnReplicate.find(src) != self->m_sstrOnReplicate.end())
               continue;

            SNode sn;
            self->m_pMetadata->lookup(src, sn);
            if ((int)sn.m_sLocation.size() > self->m_SysConfig.m_iReplicaNum)
               continue;

            self->createReplica(src, dst);
         }
      }

      // remove those already been replicated
      self->m_vstrToBeReplicated.erase(self->m_vstrToBeReplicated.begin(), r);

      pthread_cond_wait(&self->m_ReplicaCond, &self->m_ReplicaLock);

      pthread_mutex_unlock(&self->m_ReplicaLock);
   }

   return NULL;
}

int Master::createReplica(const string& src, const string& dst)
{
   SNode attr;
   int r = m_pMetadata->lookup(src.c_str(), attr);
   if (r < 0)
      return r;

   SlaveNode sn;
   if (src == dst)
   {
      if (m_SlaveManager.chooseReplicaNode(attr.m_sLocation, sn, attr.m_llSize) < 0)
         return -1;
   }
   else
   {
      set<Address, AddrComp> empty;
      if (m_SlaveManager.chooseReplicaNode(empty, sn, attr.m_llSize) < 0)
         return -1;
   }

   int transid = m_TransManager.create(0, 0, 111, dst, 0);

   SectorMsg msg;
   msg.setType(111);
   msg.setData(0, (char*)&transid, 4);
   msg.setData(4, (char*)&attr.m_llTimeStamp, 8);
   msg.setData(12, src.c_str(), src.length() + 1);
   msg.setData(12 + src.length() + 1, dst.c_str(), dst.length() + 1);

   if ((m_GMP.rpc(sn.m_strIP.c_str(), sn.m_iPort, &msg, &msg) < 0) || (msg.getData() < 0))
      return -1;

   m_TransManager.addSlave(transid, sn.m_iNodeID);

   if (src == dst)
      m_sstrOnReplicate.insert(src);

   // replicate index file to the same location
   string idx = src + ".idx";
   r = m_pMetadata->lookup(idx.c_str(), attr);

   if (r < 0)
      return 0;

   transid = m_TransManager.create(0, 0, 111, dst + ".idx", 0);

   msg.setType(111);
   msg.setData(0, (char*)&transid, 4);
   msg.setData(4, (char*)&attr.m_llTimeStamp, 8);
   msg.setData(12, idx.c_str(), idx.length() + 1);
   msg.setData(12 + idx.length() + 1, (dst + ".idx").c_str(), (dst + ".idx").length() + 1);

   if ((m_GMP.rpc(sn.m_strIP.c_str(), sn.m_iPort, &msg, &msg) < 0) || (msg.getData() < 0))
      return 0;

   m_TransManager.addSlave(transid, sn.m_iNodeID);

   if (src == dst)
      m_sstrOnReplicate.insert(idx);

   return 0;
}

void Master::loadSlaveAddr(const string& file)
{   
   ifstream ifs(file.c_str(), ios::in);

   if (ifs.bad() || ifs.fail())
      return;

   while (!ifs.eof())
   {
      char line[256];
      line[0] = '\0';
      ifs.getline(line, 256);
      if (*line == '\0')
         continue;

      int i = 0;
      int n = strlen(line);
      for (; i < n; ++ i)
      {
         if ((line[i] != ' ') && (line[i] != '\t'))
            break;
      }

      if ((i == n) && (line[i] == '#'))
         continue;

      char newline[256];
      bool blank = false;
      char* p = newline;
      for (; i <= n; ++ i)
      {
         if ((line[i] == ' ') || (line[i] == '\t'))
         {
            if (!blank)
               *p++ = ' ';
            blank = true;
         }
         else
         {
            *p++ = line[i];
            blank = false;
         }
      }

      SlaveAddr sa;
      sa.m_strAddr = newline;
      sa.m_strAddr = sa.m_strAddr.substr(0, sa.m_strAddr.find(' '));
      sa.m_strBase = newline;
      sa.m_strBase = sa.m_strBase.substr(sa.m_strBase.find(' ') + 1, sa.m_strBase.length());
      string ip = sa.m_strAddr.substr(sa.m_strAddr.find('@') + 1, sa.m_strAddr.length());

      m_mSlaveAddrRec[ip] = sa;
   }
}

int Master::serializeSysStat(char*& buf, int& size)
{
   size = 52 + m_SlaveManager.getNumberOfClusters() * 48 + m_Routing.getNumOfMasters() * 20 + m_SlaveManager.getNumberOfSlaves() * 72;
   buf = new char[size];

   *(int64_t*)buf = m_llStartTime;
   *(int64_t*)(buf + 8) = m_SlaveManager.getTotalDiskSpace();
   *(int64_t*)(buf + 16) = m_pMetadata->getTotalDataSize("/");
   *(int64_t*)(buf + 24) = m_pMetadata->getTotalFileNum("/");
   *(int64_t*)(buf + 32) = m_SlaveManager.getNumberOfSlaves();

   char* p = buf + 40;
   *(int32_t*)p = m_SlaveManager.getNumberOfClusters();
   p += 4;
   int s = 0;
   m_SlaveManager.serializeClusterInfo(p, s);
   p += s;

   *(int32_t*)p = m_Routing.getNumOfMasters();
   p += 4;
   map<uint32_t, Address> al;
   m_Routing.getListOfMasters(al);
   for (map<uint32_t, Address>::iterator i = al.begin(); i != al.end(); ++ i)
   {
      strcpy(p, i->second.m_strIP.c_str());
      p += 16;
      *(int32_t*)p = i->second.m_iPort;
      p += 4;
   }

   *(int32_t*)p = m_SlaveManager.getNumberOfSlaves();
   p += 4;
   s = 0;
   m_SlaveManager.serializeSlaveInfo(p, s);

   return size;
}

int Master::populateSpecialRep(const std::string& conf, std::map<std::string, int>& special)
{
   special.clear();

   ifstream cf(conf.c_str());
   if (cf.bad() || cf.fail())
      return -1;

   while (!cf.eof())
   {
      char buf[1024];
      cf.getline(buf, 1024);
      if ((0 == strlen(buf)) || ('#' == buf[0]))
         continue;

      stringstream ss (ios::in | ios::out);
      ss << buf;
      string path;
      int thresh;
      ss >> path >> thresh;

      if (thresh <= 0)
         continue;

      string revised_path = Metadata::revisePath(path);
      SNode sn;
      if (m_pMetadata->lookup(revised_path, sn) < 0)
         continue;

      if (sn.m_bIsDir)
         revised_path.append("/");

      special[revised_path] = thresh;
   }

   return special.size();
}
