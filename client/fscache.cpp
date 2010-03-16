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
   Yunhong Gu, last updated 05/23/2009
*****************************************************************************/

#include <string.h>
#include <common.h>
#include "fscache.h"

using namespace std;

void StatCache::insert(const string& path)
{
   map<string, StatRec>::iterator s = m_mOpenedFiles.find(path);

   if (s == m_mOpenedFiles.end())
   {
      StatRec r;
      r.m_iCount = 1;
      r.m_bChange = false;
      m_mOpenedFiles[path] = r;
   }
   else
   {
      s->second.m_iCount ++;
   }
}

void StatCache::update(const string& path, const int64_t& ts, const int64_t& size)
{
   map<string, StatRec>::iterator s = m_mOpenedFiles.find(path);

   if (s == m_mOpenedFiles.end())
      return;

   s->second.m_llTimeStamp = ts;
   s->second.m_llSize = size;

   s->second.m_bChange = true;
}

int StatCache::stat(const string& path, SNode& attr)
{
   map<string, StatRec>::iterator s = m_mOpenedFiles.find(path);

   if (s == m_mOpenedFiles.end())
      return -1;

   if (!s->second.m_bChange)
      return 0;

   attr.m_llTimeStamp = s->second.m_llTimeStamp;
   attr.m_llSize = s->second.m_llSize;
   return 1;
}

void StatCache::remove(const string& path)
{
   map<string, StatRec>::iterator s = m_mOpenedFiles.find(path);

   if (s == m_mOpenedFiles.end())
      return;

   if (-- s->second.m_iCount == 0)
      m_mOpenedFiles.erase(s);
}


ReadCache::ReadCache():
m_llCacheSize(0),
m_llMaxCacheSize(0),
m_llMaxCacheTime(10000000)
{
}

ReadCache::~ReadCache()
{
}

int ReadCache::insert(char* block, const std::string& path, const int64_t& offset, const int64_t& size)
{
   CacheBlock cb;
   cb.m_strFileName = path;
   cb.m_llOffset = offset;
   cb.m_llSize = size;
   cb.m_llCreateTime = CTimer::getTime();
   cb.m_llLastAccessTime = CTimer::getTime();
   cb.m_pcBlock = block;

   map<string, list<CacheBlock> >::iterator c = m_mCacheBlocks.find(path);

   if (c == m_mCacheBlocks.end())
      m_mCacheBlocks[path].push_front(cb);
   else
      c->second.push_back(cb);

   m_llCacheSize += cb.m_llSize;
   shrink();

   return 0;
}

int ReadCache::remove(const std::string& path)
{
   map<string, list<CacheBlock> >::iterator c = m_mCacheBlocks.find(path);

   if (c != m_mCacheBlocks.end())
   {
      for (list<CacheBlock>::iterator i = c->second.begin(); i != c->second.end(); ++ i)
         delete [] i->m_pcBlock;

      m_mCacheBlocks.erase(c);
   }

   return 0;
}

int ReadCache::read(const std::string& path, char* buf, const int64_t& offset, const int64_t& size)
{
   map<string, list<CacheBlock> >::iterator c = m_mCacheBlocks.find(path);

   if (c == m_mCacheBlocks.end())
      return 0;

   for (list<CacheBlock>::iterator i = c->second.begin(); i != c->second.end(); ++ i)
   {
      // this condition can be improved to provide finer granularity
      if (i->m_llOffset + size >= offset + size)
      {
         memcpy(buf, i->m_pcBlock + offset - i->m_llOffset, size);
         i->m_llLastAccessTime = CTimer::getTime();
         return size;
      }
   }

   return 0;
}

int ReadCache::shrink()
{
   int64_t currtime = CTimer::getTime();

   if (m_llCacheSize < m_llMaxCacheSize)
      return 0;

   for (map<string, list<CacheBlock> >::iterator i = m_mCacheBlocks.begin(); i != m_mCacheBlocks.end(); ++ i)
   {
      for (list<CacheBlock>::iterator j = i->second.begin(); j != i->second.end();)
      {
         if (currtime - j->m_llLastAccessTime > m_llMaxCacheTime)
         {
            list<CacheBlock>::iterator k = j;
            ++ j;
            i->second.erase(k);
         }
         else
            ++ j;
      }
   }

   return 0;
}
