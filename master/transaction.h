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


#ifndef __SECTOR_TRANS_H__
#define __SECTOR_TRANS_H__

#include <set>
#include <vector>
#include <map>
#include <string>
#include <pthread.h>

struct Transaction
{
   int m_iTransID;		// unique id
   int m_iType;			// 0: file, 1: sphere
   int64_t m_llStartTime;	// start time
   std::string m_strFile;	// if type = 0, this is the file being accessed
   int m_iMode;			// if type = 0, this is the file access mode
   std::set<int> m_siSlaveID;	// set of slave id involved in this transaction
   int m_iUserKey;		// user key
   int m_iCommand;		// user's command, 110, 201, etc.
};

class TransManager
{
public:
   TransManager();
   ~TransManager();

public:
   int create(const int type, const int key, const int cmd, const std::string& file, const int mode);
   int addSlave(int transid, int slaveid);
   int retrieve(int transid, Transaction& trans);
   int retrieve(int slaveid, std::vector<int>& trans);
   int updateSlave(int transid, int slaveid);
   int getUserTrans(int key, std::vector<int>& trans);

public:
   unsigned int getTotalTrans();

public:
   std::map<int, Transaction> m_mTransList;	// list of active transactions
   int m_iTransID;				// seed of transaction id

   pthread_mutex_t m_TLLock;
};

#endif
