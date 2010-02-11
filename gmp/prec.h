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
   Yunhong Gu, last updated 02/10/2010
*****************************************************************************/


#ifndef __GMP_PREC_H__
#define __GMP_PREC_H__

#ifndef WIN32
   #include <pthread.h>
#else
   #include <udt.h>
   #include <common.h>
#endif

#include <set>
#include <map>
#include <list>
#include <string>

struct CPeerRecord
{
   std::string m_strIP;
   int m_iPort;
   int m_iSession;
   int32_t m_iID;
   int64_t m_llTimeStamp;
   int m_iRTT;
   int32_t m_iFlowWindow;
};

struct CFPeerRec
{
   bool operator()(const CPeerRecord* p1, const CPeerRecord* p2) const
   {
      if (p1->m_strIP == p2->m_strIP)
      {
         if (p1->m_iPort == p2->m_iPort)
            return (p1->m_iSession > p2->m_iSession);

         return (p1->m_iPort > p2->m_iPort);
      }

      return (p1->m_strIP > p2->m_strIP);
   }
};

struct CFPeerRecByIP
{
   bool operator()(const CPeerRecord* p1, const CPeerRecord* p2) const
   {
      return (p1->m_strIP > p2->m_strIP);
   }
};

struct CFPeerRecByTS
{
   bool operator()(const CPeerRecord* p1, const CPeerRecord* p2) const
   {      
      return (p1->m_llTimeStamp > p2->m_llTimeStamp);
   }
};

class CPeerManagement
{
public:
   CPeerManagement();
   ~CPeerManagement();

public:
   void insert(const std::string& ip, const int& port, const int& session, const int32_t& id = -1, const int& rtt = -1, const int& fw = 0);
   int getRTT(const std::string& ip);
   void clearRTT(const std::string& ip);
   int flowControl(const std::string& ip, const int& port, const int& session);

   int32_t hash(const std::string& ip, const int& port, const int& session, const int32_t& id);
   bool hit(const std::string& ip, const int& port, const int& session, const int32_t& id);

private:
   int addRecentPR(const CPeerRecord& pr);

private:
   std::set<CPeerRecord*, CFPeerRec> m_sPeerRec;
   std::set<CPeerRecord*, CFPeerRecByTS> m_sPeerRecByTS;
   std::map<std::string, int> m_mRTT;

   static const unsigned int m_uiHashSpace = 20;
   std::map<int, std::list<CPeerRecord> > m_mRecentRec;

   pthread_mutex_t m_PeerRecLock;

private:
   static const unsigned int m_uiRecLimit = 65536;
};

#endif
