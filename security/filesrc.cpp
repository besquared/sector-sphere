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
   Yunhong Gu, last updated 12/13/2009
*****************************************************************************/

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <filesrc.h>
#include <conf.h>

using namespace std;

int FileSrc::loadACL(vector<IPRange>& acl, const void* src)
{
   ifstream af((char*)src);

   if (af.fail() || af.bad())
      return -1;

   acl.clear();

   char line[128];
   while (!af.eof())
   {
      af.getline(line, 128);
      if (strlen(line) == 0)
         continue;

      IPRange ipr;
      if (parseIPRange(ipr, line) >= 0)
        acl.push_back(ipr);
   }

   af.close();

   return acl.size();
}

int FileSrc::loadUsers(map<string, User>& users, const void* src)
{
   string path = (char*)src;

   dirent **namelist;
   int n = scandir(path.c_str(), &namelist, 0, alphasort);

   if (n < 0)
      return -1;

   users.clear();

   for (int i = 0; i < n; ++ i)
   {
      // skip "." and ".."
      if ((strcmp(namelist[i]->d_name, ".") == 0) || (strcmp(namelist[i]->d_name, "..") == 0))
      {
         free(namelist[i]);
         continue;
      }

      struct stat s;
      stat((path + "/" + namelist[i]->d_name).c_str(), &s);

      if (S_ISDIR(s.st_mode))
      {
         free(namelist[i]);
         continue;
      }

      User u;
      if (parseUser(u, namelist[i]->d_name, (path + "/" + namelist[i]->d_name).c_str()) > 0)
         users[u.m_strName] = u;

      free(namelist[i]);
   }
   free(namelist);

   return users.size();
}

int FileSrc::parseIPRange(IPRange& ipr, const char* ip)
{
   char buf[128];
   unsigned int i = 0;
   for (unsigned int n = strlen(ip); i < n; ++ i)
   {
      if ('/' == ip[i])
         break;

      buf[i] = ip[i];
   }
   buf[i] = '\0';

   in_addr addr;
   if (inet_pton(AF_INET, buf, &addr) <= 0)
      return -1;

   ipr.m_uiIP = addr.s_addr;
   ipr.m_uiMask = 0xFFFFFFFF;

   if (i == strlen(ip))
      return 0;

   if ('/' != ip[i])
      return -1;
   ++ i;

   bool format = false;
   int j = 0;
   for (unsigned int n = strlen(ip); i < n; ++ i, ++ j)
   {
      if ('.' == ip[i])
         format = true;

      buf[j] = ip[i];
   }
   buf[j] = '\0';

   if (format)
   {
      //255.255.255.0
      if (inet_pton(AF_INET, buf, &addr) < 0)
         return -1;
      ipr.m_uiMask = addr.s_addr;
   }
   else
   {
      char* p;
      unsigned int bit = strtol(buf, &p, 10);

      if ((p == buf) || (bit > 32) || (bit < 0))
         return -1;

      if (bit < 32)
         ipr.m_uiMask = (bit == 0) ? 0 : htonl(~((1 << (32 - bit)) - 1));
   }

   return 0;
}

int FileSrc::parseUser(User& user, const char* name, const char* ufile)
{
   user.m_iID = 0;
   user.m_strName = name;
   user.m_strPassword = "";
   user.m_vstrReadList.clear();
   user.m_vstrWriteList.clear();
   user.m_bExec = false;
   user.m_llQuota = -1;

   ConfParser parser;
   Param param;

   if (0 != parser.init(ufile))
      return -1;

   while (parser.getNextParam(param) >= 0)
   {
      if (param.m_vstrValue.empty())
         continue;

      if ("PASSWORD" == param.m_strName)
         user.m_strPassword = param.m_vstrValue[0];
      else if ("READ_PERMISSION" == param.m_strName)
         user.m_vstrReadList = param.m_vstrValue;
      else if ("WRITE_PERMISSION" == param.m_strName)
         user.m_vstrWriteList = param.m_vstrValue;
      else if ("EXEC_PERMISSION" == param.m_strName)
      {
         if (param.m_vstrValue[0] == "TRUE")
            user.m_bExec = true;
      }
      else if ("ACL" == param.m_strName)
      {
         for (vector<string>::iterator i = param.m_vstrValue.begin(); i != param.m_vstrValue.end(); ++ i)
         {
            IPRange ipr;
            parseIPRange(ipr, i->c_str());
            user.m_vACL.push_back(ipr);
         }
      }
      else if ("QUOTA" == param.m_strName)
         user.m_llQuota = atoll(param.m_vstrValue[0].c_str());
      else
         cerr << "unrecongnized user configuration parameter: " << param.m_strName << endl;
   }

   parser.close();

   return 1;
}
