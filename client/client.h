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
   Yunhong Gu, last updated 01/22/2009
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

class Client
{
public:
   Client();
   virtual ~Client();

public:
   static int init(const std::string& server, const int& port);
   static int login(const std::string& username, const std::string& password, const char* cert = NULL);
   static int login(const std::string& serv_ip, const int& serv_port);
   static int logout();
   static int close();

   static int list(const std::string& path, std::vector<SNode>& attr);
   static int stat(const std::string& path, SNode& attr);
   static int mkdir(const std::string& path);
   static int move(const std::string& oldpath, const std::string& newpath);
   static int remove(const std::string& path);
   static int rmr(const std::string& path);
   static int copy(const std::string& src, const std::string& dst);
   static int utime(const std::string& path, const int64_t& ts);

   static int sysinfo(SysStat& sys);

public:
   static int dataInfo(const std::vector<std::string>& files, std::vector<std::string>& info);

protected:
   static int updateMasters();

protected:
   static std::string g_strServerHost;
   static std::string g_strServerIP;
   static int g_iServerPort;
   static CGMP g_GMP;
   static DataChn g_DataChn;
   static int32_t g_iKey;

   // this is the global key/iv for this client. do not use this for any connection; a new connection should duplicate this
   static unsigned char g_pcCryptoKey[16];
   static unsigned char g_pcCryptoIV[8];

   static Topology g_Topology;

   static SectorError g_ErrorInfo;

   static StatCache g_StatCache;

private:
   static int g_iCount;

protected: // master routing
   static Routing g_Routing;

protected:
   static std::string g_strUsername;
   static std::string g_strPassword;
   static std::string g_strCert;

   static std::set<Address, AddrComp> g_sMasters;

   static bool g_bActive;
   static pthread_t g_KeepAlive;
   static pthread_cond_t g_KACond;
   static pthread_mutex_t g_KALock;
   static void* keepAlive(void*);
};

typedef Client Sector;

#endif
