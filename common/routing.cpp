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

#include "routing.h"

using namespace std;

Routing::Routing():
m_iKeySpace(32)
{
}

Routing::~Routing()
{
}

void Routing::init()
{
   m_vFingerTable.clear();
   m_mAddressList.clear();
   m_mKeyList.clear();
}

int Routing::insert(const uint32_t& key, const Address& node)
{
   if (m_mAddressList.find(key) != m_mAddressList.end())
      return -1;

   m_mAddressList[key] = node;
   m_mKeyList[node] = key;

   bool found = false;
   for (vector<uint32_t>::iterator i = m_vFingerTable.begin(); i != m_vFingerTable.end(); ++ i)
   {
      if (key > *i)
      {
         m_vFingerTable.insert(i, key);
         found = true;
         break;
      }
   }
   if (!found)
      m_vFingerTable.insert(m_vFingerTable.end(), key);

   return 1;
}

int Routing::remove(const uint32_t& key)
{
   map<uint32_t, Address>::iterator k = m_mAddressList.find(key);
   if (k == m_mAddressList.end())
      return -1;

   m_mKeyList.erase(k->second);
   m_mAddressList.erase(k);

   for (vector<uint32_t>::iterator i = m_vFingerTable.begin(); i != m_vFingerTable.end(); ++ i)
   {
      if (key == *i)
      {
         m_vFingerTable.erase(i);
         break;
      }
   }

   return 1;
}

int Routing::lookup(const uint32_t& key, Address& node)
{
   if (m_vFingerTable.empty())
      return -1;

   int f = key % m_vFingerTable.size();
   int r = m_vFingerTable[f];
   node = m_mAddressList[r];

   return 1;
}

int Routing::lookup(const string& path, Address& node)
{
   uint32_t key = DHash::hash(path.c_str(), m_iKeySpace);
   return lookup(key, node);
}

int Routing::getEntityID(const string& path)
{
   uint32_t key = DHash::hash(path.c_str(), m_iKeySpace);

   if (m_vFingerTable.empty())
      return -1;

   return key % m_vFingerTable.size();
}

int Routing::getRouterID(const uint32_t& key)
{
   int pos = 0;
   for (vector<uint32_t>::const_iterator i = m_vFingerTable.begin(); i != m_vFingerTable.end(); ++ i)
   {
      if (*i == key)
         return pos;
      ++ pos;
   }
   return -1;
}

int Routing::getRouterID(const Address& node)
{
   map<Address, uint32_t, AddrComp>::iterator a = m_mKeyList.find(node);
   if (a == m_mKeyList.end())
      return -1;

   return getRouterID(a->second);
}

bool Routing::match(const uint32_t& cid, const uint32_t& key)
{
   if (m_vFingerTable.empty())
      return false;

   return key == m_vFingerTable[cid % m_vFingerTable.size()];
}

bool Routing::match(const char* path, const uint32_t& key)
{
   if (m_vFingerTable.empty())
      return false;

   uint32_t pid = DHash::hash(path, m_iKeySpace);

   return key == m_vFingerTable[pid % m_vFingerTable.size()];
}

int Routing::getPrimaryMaster(Address& node)
{
   if (m_mAddressList.empty())
      return -1;

   node = m_mAddressList.begin()->second;
   return 0;
}
