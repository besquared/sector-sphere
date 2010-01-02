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

#include <util.h>
#include <cstring>
#include <cstdlib>
#include <iostream>

using namespace std;

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
