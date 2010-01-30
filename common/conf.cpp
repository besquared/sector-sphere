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
   Yunhong Gu, last updated 04/22/2009
*****************************************************************************/


#include "conf.h"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>

using namespace std;

int ConfParser::init(const string& path)
{
   m_ConfFile.open(path.c_str(), ios::in);

   if (m_ConfFile.bad() || m_ConfFile.fail())
   {
      cerr << "unable to locate or open the configuration file: " << path << endl;
      return -1;
   }

   while (!m_ConfFile.eof())
   {
      char buf[1024];
      m_ConfFile.getline(buf, 1024);

      if (strlen(buf) == 0)
         continue;

      //skip comments
      if ('#' == buf[0])
         continue;

      //TODO: skip lines with all blanks and tabs

      m_vstrLines.insert(m_vstrLines.end(), buf);
   }

   m_ConfFile.close();

   m_ptrLine = m_vstrLines.begin();
   m_iLineCount = 1;

   return 0;
}

void ConfParser::close()
{
}

int ConfParser::getNextParam(Param& param)
{
   //param format
   // NAME
   // < tab >value1
   // < tab >value2
   // < tab >...
   // < tab >valuen

   if (m_ptrLine == m_vstrLines.end())
      return -1;

   param.m_strName = "";
   param.m_vstrValue.clear();

   while (m_ptrLine != m_vstrLines.end())
   {
      char buf[1024];
      strcpy(buf, m_ptrLine->c_str());

      // no blanks or tabs in front of name line
      if ((' ' == buf[0]) || ('\t' == buf[0]))
      {
         cerr << "Configuration file parsing error at line " << m_iLineCount << ": " << buf << endl;
         return -1;
      }

      char* str = buf;
      string token = "";

      if (NULL == (str = getToken(str, token)))
      {
         m_ptrLine ++;
         m_iLineCount ++;
         continue;
      }
      param.m_strName = token;

      // scan param values
      m_ptrLine ++;
      m_iLineCount ++;
      while (m_ptrLine != m_vstrLines.end())
      {
         strcpy(buf, m_ptrLine->c_str());

         if ((strlen(buf) == 0) || ('\t' != buf[0]))
            break;

         str = buf;
         if (NULL == (str = getToken(str, token)))
         {
            cerr << "Configuration file parsing error at line " << m_iLineCount << ": " << buf << endl;
            return -1;
         }

         param.m_vstrValue.insert(param.m_vstrValue.end(), token);

         m_ptrLine ++;
         m_iLineCount ++;
      }

      return param.m_vstrValue.size();
   }

   return -1;
}

char* ConfParser::getToken(char* str, string& token)
{
   char* p = str;

   // skip blank spaces
   while ((' ' == *p) || ('\t' == *p))
      ++ p;

   // nothing here...
   if ('\0' == *p)
      return NULL;

   token = "";
   while ((' ' != *p) && ('\t' != *p) && ('\0' != *p))
   {
      token.append(1, *p);
      ++ p;
   }

   return p;
}

int MasterConf::init(const string& path)
{
   m_iServerPort = 6000;

   ConfParser parser;
   Param param;

   if (0 != parser.init(path))
      return -1;

   while (parser.getNextParam(param) >= 0)
   {
      if (param.m_vstrValue.empty())
         continue;

      if ("SECTOR_PORT" == param.m_strName)
         m_iServerPort = atoi(param.m_vstrValue[0].c_str());
      else if ("SECURITY_SERVER" == param.m_strName)
      {
         char buf[128];
         strcpy(buf, param.m_vstrValue[0].c_str());

         unsigned int i = 0;
         for (; i < strlen(buf); ++ i)
         {
            if (buf[i] == ':')
               break;
         }

         buf[i] = '\0';
         m_strSecServIP = buf;
         m_iSecServPort = atoi(buf + i + 1);
      }
      else if ("MAX_ACTIVE_USER" == param.m_strName)
         m_iMaxActiveUser = atoi(param.m_vstrValue[0].c_str());
      else if ("DATA_DIRECTORY" == param.m_strName)
      {
         m_strHomeDir = param.m_vstrValue[0];
         if (m_strHomeDir.c_str()[m_strHomeDir.length() - 1] != '/')
            m_strHomeDir += "/";
      }
      else if ("REPLICA_NUM" == param.m_strName)
         m_iReplicaNum = atoi(param.m_vstrValue[0].c_str());
      else if ("META_LOC" == param.m_strName)
      {
         if ("MEMORY" == param.m_vstrValue[0])
            m_MetaType = MEMORY;
         else if ("DISK" == param.m_vstrValue[0])
            m_MetaType = DISK;
      }
      else
         cerr << "unrecongnized system parameter: " << param.m_strName << endl;
   }

   parser.close();

   return 0;
}

int SlaveConf::init(const string& path)
{
   m_strMasterHost = "";
   m_iMasterPort = 0;
   m_llMaxDataSize = -1;
   m_iMaxServiceNum = 2;
   m_strLocalIP = "";
   m_strPublicIP = "";
   
   ConfParser parser;
   Param param;

   if (0 != parser.init(path))
      return -1;

   while (parser.getNextParam(param) >= 0)
   {
      if (param.m_vstrValue.empty())
         continue;

      if ("MASTER_ADDRESS" == param.m_strName)
      {
         char buf[128];
         strcpy(buf, param.m_vstrValue[0].c_str());

         unsigned int i = 0;
         for (; i < strlen(buf); ++ i)
         {
            if (buf[i] == ':')
               break;
         }

         buf[i] = '\0';
         m_strMasterHost = buf;
         m_iMasterPort = atoi(buf + i + 1);
      }
      else if ("DATA_DIRECTORY" == param.m_strName)
      {
         m_strHomeDir = param.m_vstrValue[0];
         if (m_strHomeDir.c_str()[m_strHomeDir.length() - 1] != '/')
            m_strHomeDir += "/";
      }
      else if ("MAX_DATA_SIZE" == param.m_strName)
         m_llMaxDataSize = atoll(param.m_vstrValue[0].c_str()) * 1024 * 1024;
      else if ("MAX_SERVICE_INSTANCE" == param.m_strName)
         m_iMaxServiceNum = atoi(param.m_vstrValue[0].c_str());
      else if ("LOCAL_ADDRESS" == param.m_strName)
         m_strLocalIP = param.m_vstrValue[0];
      else if ("PUBLIC_ADDRESS" == param.m_strName)
         m_strPublicIP = param.m_vstrValue[0];
      else if ("META_LOC" == param.m_strName)
      {
         if ("MEMORY" == param.m_vstrValue[0])
            m_MetaType = MEMORY;
         else if ("DISK" == param.m_vstrValue[0])
            m_MetaType = DISK;
      }
      else
         cerr << "unrecongnized system parameter: " << param.m_strName << endl;
   }

   parser.close();

   return 0;
}

int ClientConf::init(const string& path)
{
   m_strUserName = "";
   m_strPassword = "";
   m_strMasterIP = "";
   m_iMasterPort = 0;
   m_strCertificate = "";

   ConfParser parser;
   Param param;

   if (0 != parser.init(path))
      return -1;

   while (parser.getNextParam(param) >= 0)
   {
      if (param.m_vstrValue.empty())
         continue;

      if ("MASTER_ADDRESS" == param.m_strName)
      {
         char buf[128];
         strcpy(buf, param.m_vstrValue[0].c_str());

         unsigned int i = 0;
         for (; i < strlen(buf); ++ i)
         {
            if (buf[i] == ':')
               break;
         }

         buf[i] = '\0';
         m_strMasterIP = buf;
         m_iMasterPort = atoi(buf + i + 1);
      }
      else if ("USERNAME" == param.m_strName)
      {
         m_strUserName = param.m_vstrValue[0];
      }
      else if ("PASSWORD" == param.m_strName)
      {
         m_strPassword = param.m_vstrValue[0];
      }
      else if ("CERTIFICATE" == param.m_strName)
      {
         m_strCertificate = param.m_vstrValue[0];
      }
      else
         cerr << "unrecongnized client.conf parameter: " << param.m_strName << endl;
   }

   parser.close();

   return 0;
}

bool WildCard::isWildCard(const string& path)
{
   if (path.find('*') != string::npos)
      return true;

   if (path.find('?') != string::npos)
      return true;

   return false;
}

bool WildCard::match(const string& card, const string& path)
{
   const char* p = card.c_str();
   const char* q = path.c_str();

   unsigned int i = 0;
   unsigned int j = 0;
   while ((i < card.length()) && (j < path.length()))
   {
      switch (p[i])
      {
      case '*':
         if (i == card.length() - 1)
            return true;

         while (p[i] == '*')
            ++ i;

         for (; j < path.length(); ++ j)
         {
            if (((q[j] == p[i]) || (p[i] == '?') ) && match(p + i, q + j))
               return true;
         }

         return false;

      case '?':
         break;

      default:
         if (p[i] != q[j])
            return false;
      }

      ++ i;
      ++ j;
   }

   if ((i != card.length()) || (j != path.length()))
      return false;

   return true;
}


int Session::loadInfo(const string& conf)
{
   m_ClientConf.init(conf);

   if (m_ClientConf.m_strMasterIP == "")
   {
      cout << "please input the master address (e.g., 123.123.123.123:1234): ";
      string addr;
      cin >> addr;

      char buf[128];
      strcpy(buf, addr.c_str());

      unsigned int i = 0;
      for (; i < strlen(buf); ++ i)
      {
         if (buf[i] == ':')
            break;
      }

      buf[i] = '\0';
      m_ClientConf.m_strMasterIP = buf;
      m_ClientConf.m_iMasterPort = atoi(buf + i + 1);
   }

   if (m_ClientConf.m_strUserName == "")
   {
      cout << "please input the user name: ";
      cin >> m_ClientConf.m_strUserName;
   }

   if (m_ClientConf.m_strPassword == "")
   {
      cout << "please input the password: ";
      cin >> m_ClientConf.m_strPassword;
   }

   if (m_ClientConf.m_strCertificate == "")
   {
      cout << "please specify the location of the master certificate: ";
      cin >> m_ClientConf.m_strCertificate;
   }

   //TODO: if m_strCert is relative dir, use getcwd to change it into absolute dir

   return 1;
}

int CmdLineParser::parse(int argc, char** argv)
{
   m_mParams.clear();

   bool dash = false;
   string key;
   for (int i = 1; i < argc; ++ i)
   {
      if (argv[i][0] == '-')
      {
         dash = true;
         key = argv[i] + 1;
      }
      else
      {
         if (!dash)
            return -1;

         m_mParams[key] = argv[i];
      }
   }

   return m_mParams.size();
}

