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
   Yunhong Gu, last updated 01/12/2010
*****************************************************************************/


#ifndef __SECTOR_CLIENT_H__
#define __SECTOR_CLIENT_H__

#include <gmp.h>
#include <index.h>
#include <sysstat.h>
#include <topology.h>
#include <constant.h>
#include <pthread.h>
#include <datachn.h>
#include <routing.h>
#include "fscache.h"

class SectorFile;
class SphereProcess;

class Client
{
friend class SectorFile;
friend class SphereProcess;

public:
   Client();
   ~Client();

public:
   int init(const std::string& server, const int& port);
   int login(const std::string& username, const std::string& password, const char* cert = NULL);
   int login(const std::string& serv_ip, const int& serv_port);
   int logout();
   int close();

   int list(const std::string& path, std::vector<SNode>& attr);
   int stat(const std::string& path, SNode& attr);
   int mkdir(const std::string& path);
   int move(const std::string& oldpath, const std::string& newpath);
   int remove(const std::string& path);
   int rmr(const std::string& path);
   int copy(const std::string& src, const std::string& dst);
   int utime(const std::string& path, const int64_t& ts);

   int sysinfo(SysStat& sys);

public:
   SectorFile* createSectorFile();
   SphereProcess* createSphereProcess();
   int releaseSectorFile(SectorFile* sf);
   int releaseSphereProcess(SphereProcess* sp);

protected:
   int updateMasters();

protected:
   std::string m_strServerHost;		// original master server domain name
   std::string m_strServerIP;		// original master server IP address
   int m_iServerPort;			// original master server port
   CGMP m_GMP;				// GMP
   DataChn m_DataChn;			// data channel
   int32_t m_iKey;			// user key

   // this is the global key/iv for this client. do not share this for all connections; a new connection should duplicate this
   unsigned char m_pcCryptoKey[16];
   unsigned char m_pcCryptoIV[8];

   Topology m_Topology;			// slave system topology

   SectorError m_ErrorInfo;		// error description

   StatCache m_StatCache;		// cache for stat() call, used for updating file that is not reflected in the master system

private:
   int m_iCount;			// number of concurrent logins

protected: // master routing
   Routing m_Routing;			// master routing module

protected:
   std::string m_strUsername;		// user account name
   std::string m_strPassword;		// user password
   std::string m_strCert;		// master certificate

   std::set<Address, AddrComp> m_sMasters;	// masters

protected: // the following are used for keeping alive with the masters
   bool m_bActive;
   pthread_t m_KeepAlive;
   pthread_cond_t m_KACond;
   pthread_mutex_t m_KALock;
   static void* keepAlive(void*);

protected:
   pthread_mutex_t m_IDLock;
   int m_iID;
   std::map<int, SectorFile*> m_mFSList;
   std::map<int, SphereProcess*> m_mDCList;
};

typedef Client Sector;

#endif
