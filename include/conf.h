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
   Yunhong Gu, last updated 01/30/2010
*****************************************************************************/


#ifndef __SECTOR_CONF_H__
#define __SECTOR_CONF_H__

#include <map>
#include <string>
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <stdint.h>

struct Param
{
   std::string m_strName;
   std::vector<std::string> m_vstrValue;
};

class ConfParser
{
public:
   int init(const std::string& path);
   void close();
   int getNextParam(Param& param);

private:
   char* getToken(char* str, std::string& token);

private:
   std::ifstream m_ConfFile;
   std::vector<std::string> m_vstrLines;
   std::vector<std::string>::iterator m_ptrLine;
   int m_iLineCount;
};

enum MetaForm {MEMORY = 1, DISK};

class MasterConf
{
public:
   int init(const std::string& path);

public:
   int m_iServerPort;		// server port
   std::string m_strSecServIP;	// security server IP
   int m_iSecServPort;		// security server port
   int m_iMaxActiveUser;	// maximum active user
   std::string m_strHomeDir;	// data directory
   int m_iReplicaNum;		// number of replicas of each file
   MetaForm m_MetaType;		// form of metadata
};

class SlaveConf
{
public:
   int init(const std::string& path);

public:
   std::string m_strMasterHost;
   int m_iMasterPort;
   std::string m_strHomeDir;
   int64_t m_llMaxDataSize;
   int m_iMaxServiceNum;
   std::string m_strLocalIP;
   std::string m_strPublicIP;
   int m_iClusterID;
   MetaForm m_MetaType;		// form of metadata
};

class ClientConf
{
public:
   int init(const std::string& path);

public:
   std::string m_strUserName;
   std::string m_strPassword;

   std::string m_strMasterIP;
   int m_iMasterPort;

   std::string m_strCertificate;
};

class WildCard
{
public:
   static bool isWildCard(const std::string& path);
   static bool match(const std::string& card, const std::string& path);
};

class Session
{
public:
   int loadInfo(const std::string& conf);

public:
   ClientConf m_ClientConf;
};

class CmdLineParser
{
public:
   int parse(int argc, char** argv);

public:
   std::map<std::string, std::string> m_mParams;
};

#endif
