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
   Yunhong Gu, last updated 09/17/2009
*****************************************************************************/

#ifndef WIN32
   #include <sys/time.h>
   #include <time.h>
#else
   #include <windows.h>
#endif

#include <common.h>
#include <dhash.h>
#include <prec.h>

using namespace std;

CPeerManagement::CPeerManagement()
{
   #ifndef WIN32
      pthread_mutex_init(&m_PeerRecLock, NULL);
   #else
      m_PeerRecLock = CreateMutex(NULL, false, NULL);
   #endif

   int n = 1 << m_uiHashSpace;
   m_pHashRec = new CPeerRecord [n];
}

CPeerManagement::~CPeerManagement()
{
   #ifndef WIN32
      pthread_mutex_destroy(&m_PeerRecLock);
   #else
      CloseHandle(m_PeerRecLock);
   #endif

   delete [] m_pHashRec;
}

void CPeerManagement::insert(const string& ip, const int& port, const int& session, const int32_t& id, const int& rtt, const int& fw)
{
   CGuard recguard(m_PeerRecLock);

   if (rtt > 0)
   {
      map<string, int>::iterator t = m_mRTT.find(ip);
      if (t != m_mRTT.end())
         t->second = (t->second * 7 + rtt) >> 3;
      else
         m_mRTT[ip] = rtt;
   }

   CPeerRecord* pr = new CPeerRecord;
   pr->m_strIP = ip;
   pr->m_iPort = port;
   pr->m_iSession = session;
   pr->m_iID = id;

   int key = hash(ip, port, session, id);
   m_pHashRec[key] = *pr;

   set<CPeerRecord*, CFPeerRec>::iterator i = m_sPeerRec.find(pr);
   map<string, int>::iterator t = m_mRTT.find(ip);

   if (i != m_sPeerRec.end())
   {
      if (id > (*i)->m_iID)
         (*i)->m_iID = id;
      (*i)->m_iFlowWindow = fw;

      m_sPeerRecByTS.erase(*i);
      (*i)->m_llTimeStamp = CTimer::getTime();
      m_sPeerRecByTS.insert(*i);

      delete pr;
   }
   else
   {
      if (id > 0)
         pr->m_iID = id;
      else
         pr->m_iID = -1;

      pr->m_llTimeStamp = CTimer::getTime();
      pr->m_iFlowWindow = fw;

      m_sPeerRec.insert(pr);
      m_sPeerRecByTS.insert(pr);

      if (m_sPeerRecByTS.size() > m_uiRecLimit)
      {
         // delete first one
         set<CPeerRecord*, CFPeerRecByTS>::iterator j = m_sPeerRecByTS.begin();

         CPeerRecord* t = *j;
         m_sPeerRec.erase(t);
         m_sPeerRecByTS.erase(j);

         bool delip = true;
         for (set<CPeerRecord*, CFPeerRec>::iterator k = m_sPeerRec.begin(); k != m_sPeerRec.end(); ++ k)
         {
            if ((*k)->m_strIP == t->m_strIP)
            {
               delip = false;
               break;
            }
         }

         if (delip)
            m_mRTT.erase(t->m_strIP);

         delete t;
      }
   }
}

int CPeerManagement::getRTT(const string& ip)
{
   CGuard recguard(m_PeerRecLock);

   map<string, int>::iterator t = m_mRTT.find(ip);
   if (t != m_mRTT.end())
      return t->second;

   return -1;
}

int CPeerManagement::getLastID(const string& ip, const int& port, const int& session)
{
   CPeerRecord pr;
   pr.m_strIP = ip;
   pr.m_iPort = port;
   pr.m_iSession = session;

   CGuard recguard(m_PeerRecLock);

   set<CPeerRecord*, CFPeerRec>::iterator i = m_sPeerRec.find(&pr);
   if (i != m_sPeerRec.end())
      return (*i)->m_iID;

   return -1;
}

void CPeerManagement::clearRTT(const string& ip)
{
   CGuard recguard(m_PeerRecLock);
   m_mRTT.erase(ip);
}

int CPeerManagement::flowControl(const string& ip, const int& port, const int& session)
{
   CPeerRecord pr;
   pr.m_strIP = ip;
   pr.m_iPort = port;
   pr.m_iSession = session;

   CGuard recguard(m_PeerRecLock);

   set<CPeerRecord*, CFPeerRec>::iterator i = m_sPeerRec.find(&pr);
   if (i == m_sPeerRec.end())
      return 0;

   int thresh = (*i)->m_iFlowWindow - (CTimer::getTime() - (*i)->m_llTimeStamp) / 1000;

   if (thresh > 100)
   {
      usleep(100000);
      return 100000;
   }

   if (thresh > 10)
   {
      usleep(10000);
      return 10000;
   }

   return 0;
}

int32_t CPeerManagement::hash(const string& ip, const int& port, const int& session, const int32_t& id)
{
   char tmp[1024];
   sprintf(tmp, "%s%d%d%d", ip.c_str(), port, session, id);

   return DHash::hash(tmp, m_uiHashSpace);
}

bool CPeerManagement::hit(const string& ip, const int& port, const int& session, const int32_t& id)
{
   int key = hash(ip, port, session, id);
   CPeerRecord* pr = m_pHashRec + key;

   return (ip == pr->m_strIP) && (port == pr->m_iPort) && (session == pr->m_iSession) && (id == pr->m_iID);
}
