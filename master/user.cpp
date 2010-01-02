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
   Yunhong Gu, last updated 06/21/2009
*****************************************************************************/

#include "user.h"
#include <string.h>

using namespace std;


int ActiveUser::deserialize(vector<string>& dirs, const string& buf)
{
   unsigned int s = 0;
   while (s < buf.length())
   {
      unsigned int t = buf.find(';', s);

      if (buf.c_str()[s] == '/')
         dirs.insert(dirs.end(), buf.substr(s, t - s));
      else
         dirs.insert(dirs.end(), "/" + buf.substr(s, t - s));
      s = t + 1;
   }

   return dirs.size();
}

bool ActiveUser::match(const string& path, int32_t rwx) const
{
   // check read flag bit 1 and write flag bit 2
   rwx &= 3;

   if ((rwx & 1) != 0)
   {
      for (vector<string>::const_iterator i = m_vstrReadList.begin(); i != m_vstrReadList.end(); ++ i)
      {
         if ((path.length() >= i->length()) && (path.substr(0, i->length()) == *i) && ((path.length() == i->length()) || (path.c_str()[i->length()] == '/') || (*i == "/")))
         {
            rwx ^= 1;
            break;
         }
      }
   }

   if ((rwx & 2) != 0)
   {
      for (vector<string>::const_iterator i = m_vstrWriteList.begin(); i != m_vstrWriteList.end(); ++ i)
      {
         if ((path.length() >= i->length()) && (path.substr(0, i->length()) == *i) && ((path.length() == i->length()) || (path.c_str()[i->length()] == '/') || (*i == "/")))
         {
            rwx ^= 2;
            break;
         }
      }
   }

   return (rwx == 0);
}

int ActiveUser::serialize(char*& buf, int& size)
{
   buf = new char[65536];
   char* p = buf;
   *(int32_t*)p = m_strName.length() + 1;
   p += 4;
   strcpy(p, m_strName.c_str());
   p += m_strName.length() + 1;
   *(int32_t*)p = m_strIP.length() + 1;
   p += 4;
   strcpy(p, m_strIP.c_str());
   p += m_strIP.length() + 1;
   *(int32_t*)p = m_iPort;
   p += 4;
   *(int32_t*)p = m_iDataPort;
   p += 4;
   *(int32_t*)p = m_iKey;
   p += 4;
   memcpy(p, m_pcKey, 16);
   p += 16;
   memcpy(p, m_pcIV, 8);
   p += 8;
   *(int32_t*)p = m_vstrReadList.size();
   p += 4;
   for (vector<string>::iterator i = m_vstrReadList.begin(); i != m_vstrReadList.end(); ++ i)
   {
      *(int32_t*)p = i->length() + 1;
      p += 4;
      strcpy(p, i->c_str());
      p += i->length() + 1;
   }
   *(int32_t*)p = m_vstrWriteList.size();
   p += 4;
   for (vector<string>::iterator i = m_vstrWriteList.begin(); i != m_vstrWriteList.end(); ++ i)
   {
      *(int32_t*)p = i->length() + 1;
      p += 4;
      strcpy(p, i->c_str());
      p += i->length() + 1;
   }
   *(int32_t*)p = m_bExec;
   p += 4;

   size = p - buf;
   return size;
}

int ActiveUser::deserialize(const char* buf, const int& size)
{
   char* p = (char*)buf;
   m_strName = p + 4;
   p += 4 + m_strName.length() + 1;
   m_strIP = p + 4;
   p += 4 + m_strIP.length() + 1;
   m_iPort = *(int32_t*)p;
   p += 4;
   m_iDataPort = *(int32_t*)p;
   p += 4;
   m_iKey = *(int32_t*)p;
   p += 4;
   memcpy(m_pcKey, p, 16);
   p += 16;
   memcpy(m_pcIV, p, 8);
   p += 8;
   int num = *(int32_t*)p;
   p += 4;
   for (int i = 0; i < num; ++ i)
   {
      p += 4;
      m_vstrReadList.push_back(p);
      p += strlen(p) + 1;
   }
   num = *(int32_t*)p;
   p += 4;
   for (int i = 0; i < num; ++ i)
   {
      p += 4;
      m_vstrWriteList.push_back(p);
      p += strlen(p) + 1;
   }
   m_bExec = *(int32_t*)p;

   return size;
}
