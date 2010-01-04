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
   Yunhong Gu, last updated 12/12/2009
*****************************************************************************/


#ifndef __MEMORY_OBJECT_H__
#define __MEMORY_OBJECT_H__

#include <string>
#include <map>
#include <vector>
#include <pthread.h>

struct MemObj
{
   std::string m_strName;
   void* m_pLoc;
   std::string m_strUser;
   int64_t m_llCreationTime;
   int64_t m_llLastRefTime;
};

class MOMgmt
{
public:
   MOMgmt();
   ~MOMgmt();

public:
   int add(const std::string& name, void* loc, const std::string& user);
   void* retrieve(const std::string& name);
   int remove(const std::string& name);

public:
   int update(std::vector<MemObj>& tba, std::vector<std::string>& tbd);

private:
   std::map<std::string, MemObj> m_mObjects;
   pthread_mutex_t m_MOLock;

private:
   std::vector<MemObj> m_vTBA;
   std::vector<std::string> m_vTBD;
};

#endif
