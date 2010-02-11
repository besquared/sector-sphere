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
   Yunhong Gu, last updated 01/19/2010
*****************************************************************************/


#ifndef __SECTOR_MASTER_H__
#define __SECTOR_MASTER_H__

#include <gmp.h>
#include <transport.h>
#include <log.h>
#include <conf.h>
#include <index.h>
#include <index2.h>
#include <vector>
#include <ssltransport.h>
#include <topology.h>
#include <routing.h>
#include <slavemgmt.h>
#include <transaction.h>
#include <user.h>
#include <threadpool.h>
	
struct SlaveAddr
{
   std::string m_strAddr;				// master IP address
   std::string m_strBase;				// slave executable "start_slave" path
};

class Master
{
public:
   Master();
   ~Master();

public:
   int init();
   int join(const char* ip, const int& port);
   int run();
   int stop();

private:
   ThreadJobQueue m_ServiceJobQueue;			// job queue for service thread pool

   struct ServiceJobParam
   {
      std::string ip;
      int port;
      SSLTransport* ssl;
   };

   static void* service(void* s);
   static void* serviceEx(void* p);

   int processSlaveJoin(SSLTransport& s, SSLTransport& secconn, const std::string& ip);
   int processUserJoin(SSLTransport& s, SSLTransport& secconn, const std::string& ip);
   int processMasterJoin(SSLTransport& s, SSLTransport& secconn, const std::string& ip);

   static void* process(void* s);

   int processSysCmd(const std::string& ip, const int port,  const ActiveUser* user, const int32_t key, int id, SectorMsg* msg);
   int processFSCmd(const std::string& ip, const int port,  const ActiveUser* user, const int32_t key, int id, SectorMsg* msg);
   int processDCCmd(const std::string& ip, const int port,  const ActiveUser* user, const int32_t key, int id, SectorMsg* msg);
   int processMCmd(const std::string& ip, const int port,  const ActiveUser* user, const int32_t key, int id, SectorMsg* msg);

   int sync(const char* fileinfo, const int& size, const int& type);
   int processSyncCmd(const std::string& ip, const int port,  const ActiveUser* user, const int32_t key, int id, SectorMsg* msg);

private:
   inline void reject(const std::string& ip, const int port, int id, int32_t code);

private:
   static void* replica(void* s);

   pthread_mutex_t m_ReplicaLock;
   pthread_cond_t m_ReplicaCond;

   // string format: <src file>,<dst file>
   std::vector<std::string> m_vstrToBeReplicated;	// list of files to be replicated/copied
   std::set<std::string> m_sstrOnReplicate;		// list of files currently being replicated

   int createReplica(const std::string& src, const std::string& dst);

private:
   CGMP m_GMP;						// GMP messenger

   MasterConf m_SysConfig;				// master configuration
   std::string m_strHomeDir;				// home data directory, for system metadata

   SectorLog m_SectorLog;				// sector log

   int m_iMaxActiveUser;				// maximum number of active users allowed
   std::map<int, ActiveUser> m_mActiveUser;		// list of active users

   Metadata* m_pMetadata;				// metadata
   SlaveManager m_SlaveManager;				// slave management
   TransManager m_TransManager;				// transaction management

   enum Status {INIT, RUNNING, STOPPED} m_Status;	// system status

   char* m_pcTopoData;					// serialized topology data
   int m_iTopoDataSize;					// size of the topology data

   Routing m_Routing;					// master routing module
   uint32_t m_iRouterKey;				// identification for this master

private:
   std::map<std::string, SlaveAddr> m_mSlaveAddrRec;	// slave and its executale path
   void loadSlaveAddr(const std::string& file);

private:
   int64_t m_llStartTime;
   int serializeSysStat(char*& buf, int& size);
};

#endif
