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
   Yunhong Gu, last updated 01/08/2010
*****************************************************************************/

#include <snode.h>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

using namespace std;

SNode::SNode():
m_strName(""),
m_bIsDir(false),
m_llTimeStamp(0),
m_llSize(0),
m_strChecksum("")
{
   m_sLocation.clear();
   m_mDirectory.clear();
}

SNode::~SNode()
{
}

int SNode::serialize(char* buf)
{
   int namelen = m_strName.length();
   sprintf(buf, "%d,%s,%d,%lld,%lld", namelen, m_strName.c_str(), m_bIsDir, m_llTimeStamp, m_llSize);
   char* p = buf + strlen(buf);
   for (set<Address>::iterator i = m_sLocation.begin(); i != m_sLocation.end(); ++ i)
   {
      sprintf(p, ",%s,%d", i->m_strIP.c_str(), i->m_iPort);
      p = p + strlen(p);
   }
   return 0;
}

int SNode::deserialize(const char* buf)
{
   char buffer[4096];
   char* tmp = buffer;

   bool stop = true;

   // file name
   strcpy(tmp, buf);
   for (unsigned int i = 0; i < strlen(tmp); ++ i)
   {
      if (tmp[i] == ',')
      {
         stop = false;
         tmp[i] = '\0';
         break;
      }
   }
   unsigned int namelen = atoi(tmp);

   if (stop)
      return -1;

   tmp = tmp + strlen(tmp) + 1;
   if (strlen(tmp) < namelen)
      return -1;
   tmp[namelen] = '\0';
   m_strName = tmp;

   stop = true;

   // restore dir 
   tmp = tmp + strlen(tmp) + 1;
   for (unsigned int i = 0; i < strlen(tmp); ++ i)
   {
      if (tmp[i] == ',')
      {
         stop = false;
         tmp[i] = '\0';
         break;
      }
   }
   m_bIsDir = atoi(tmp);

   if (stop)
      return -1;
   stop = true;

   // restore timestamp
   tmp = tmp + strlen(tmp) + 1;
   for (unsigned int i = 0; i < strlen(tmp); ++ i)
   {
      if (tmp[i] == ',')
      {
         stop = false;
         tmp[i] = '\0';
         break;
      }
   }
   m_llTimeStamp = atoll(tmp);

   if (stop)
      return -1;
   stop = true;

   // restore size
   tmp = tmp + strlen(tmp) + 1;
   for (unsigned int i = 0; i < strlen(tmp); ++ i)
   {
      if (tmp[i] == ',')
      {
         stop = false;
         tmp[i] = '\0';
         break;
      }
   }
   m_llSize = atoll(tmp);

   // restore locations
   while (!stop)
   {
      tmp = tmp + strlen(tmp) + 1;

      stop = true;

      Address addr;
      for (unsigned int i = 0; i < strlen(tmp); ++ i)
      {
         if (tmp[i] == ',')
         {
            stop = false;
            tmp[i] = '\0';
            break;
         }
      }
      addr.m_strIP = tmp;

      if (stop)
         return -1;
      stop = true;

      tmp = tmp + strlen(tmp) + 1;
      for (unsigned int i = 0; i < strlen(tmp); ++ i)
      {
         if (tmp[i] == ',')
         {
            stop = false;
            tmp[i] = '\0';
            break;
         }
      }
      addr.m_iPort = atoi(tmp);

      m_sLocation.insert(addr);
   }

   return 0;
}

int SNode::serialize2(const string& file)
{
   string tmp = file;
   size_t p = tmp.rfind('/');
   struct stat s;
   if (stat(tmp.c_str(), &s) < 0)
   {
      string cmd = string("mkdir -p ") + tmp.substr(0, p);
      system(cmd.c_str());
   }

   if (m_bIsDir)
   {
      string cmd = string("mkdir ") + file;
      system(cmd.c_str());
      return 0;
   }

   fstream ofs(file.c_str(), ios::out | ios::trunc);
   ofs << m_llSize << endl;
   ofs << m_llTimeStamp << endl;

   for (set<Address>::iterator i = m_sLocation.begin(); i != m_sLocation.end(); ++ i)
   {
      ofs << i->m_strIP << endl;
      ofs << i->m_iPort << endl;
   }
   ofs.close();

   return 0;
}

int SNode::deserialize2(const string& file)
{
   struct stat s;
   if (stat(file.c_str(), &s))
      return -1;

   size_t p = file.rfind('/');
   if (p == string::npos)
      m_strName = file;
   else
      m_strName = file.substr(p + 1);

   if (S_ISDIR(s.st_mode))
   {
      m_bIsDir = true;
      m_llSize = s.st_size;
      m_llTimeStamp = s.st_mtime;
   }
   else
   {
      m_bIsDir = false; 

      fstream ifs(file.c_str(), ios::in);
      ifs >> m_llSize;
      ifs >> m_llTimeStamp;
      while (!ifs.eof())
      {
         Address addr;
         ifs >> addr.m_strIP;
         if (addr.m_strIP.length() == 0)
            break;
         ifs >> addr.m_iPort;
         m_sLocation.insert(addr);
      }
      ifs.close();
   }

   return 0;
}
