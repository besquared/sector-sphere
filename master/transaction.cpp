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
   Yunhong Gu, last updated 02/17/2010
*****************************************************************************/

#include <common.h>
#include "transaction.h"

using namespace std;

TransManager::TransManager():
m_iTransID(1)
{
   pthread_mutex_init(&m_TLLock, NULL);
}

TransManager::~TransManager()
{
   pthread_mutex_destroy(&m_TLLock);
}

int TransManager::create(const int type, const int key, const int cmd, const string& file, const int mode)
{
   CGuard tl(m_TLLock);

   Transaction t;
   t.m_iTransID = m_iTransID ++;
   t.m_iType = type;
   t.m_llStartTime = CTimer::getTime();
   t.m_strFile = file;
   t.m_iMode = mode;
   t.m_iUserKey = key;
   t.m_iCommand = cmd;

   m_mTransList[t.m_iTransID] = t;

   return t.m_iTransID;
}

int TransManager::addSlave(int transid, int slaveid)
{
   CGuard tl(m_TLLock);

   m_mTransList[transid].m_siSlaveID.insert(slaveid);

   return transid;
}

int TransManager::retrieve(int transid, Transaction& trans)
{
   CGuard tl(m_TLLock);

   map<int, Transaction>::iterator i = m_mTransList.find(transid);

   if (i == m_mTransList.end())
      return -1;

   trans = i->second;
   return transid;
}

int TransManager::retrieve(int slaveid, vector<int>& trans)
{
   CGuard tl(m_TLLock);

   for (map<int, Transaction>::iterator i = m_mTransList.begin(); i != m_mTransList.end(); ++ i)
   {
      if (i->second.m_siSlaveID.find(slaveid) != i->second.m_siSlaveID.end())
      {
         trans.push_back(i->first);
      }
   }

   return trans.size();
}

int TransManager::updateSlave(int transid, int slaveid)
{
   CGuard tl(m_TLLock);

   m_mTransList[transid].m_siSlaveID.erase(slaveid);
   int ret = m_mTransList[transid].m_siSlaveID.size();
   if (ret == 0)
      m_mTransList.erase(transid);

   return ret;
}

int TransManager::getUserTrans(int key, vector<int>& trans)
{
   CGuard tl(m_TLLock);

   for (map<int, Transaction>::iterator i = m_mTransList.begin(); i != m_mTransList.end(); ++ i)
   {
      if (key == i->second.m_iUserKey)
      {
         trans.push_back(i->first);
      }
   }

   return trans.size();
}

unsigned int TransManager::getTotalTrans()
{
   CGuard tl(m_TLLock);

   return m_mTransList.size();
}
