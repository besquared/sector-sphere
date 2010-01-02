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
   Yunhong Gu, last updated 02/06/2009
*****************************************************************************/


#ifndef __SECTOR_SLAVEMGMT_H__
#define __SECTOR_SLAVEMGMT_H__

#include <topology.h>

class SlaveManager
{
public:
   SlaveManager();
   ~SlaveManager();

public:
   int init(const char* topoconf);

   int insert(SlaveNode& sn);
   int remove(int nodeid);

public:
   int chooseReplicaNode(std::set<int>& loclist, SlaveNode& sn, const int64_t& filesize);
   int chooseIONode(std::set<int>& loclist, const Address& client, int mode, std::vector<SlaveNode>& sl, int replica);
   int chooseReplicaNode(std::set<Address, AddrComp>& loclist, SlaveNode& sn, const int64_t& filesize);
   int chooseIONode(std::set<Address, AddrComp>& loclist, const Address& client, int mode, std::vector<SlaveNode>& sl, int replica);

public:
   unsigned int getTotalSlaves();
   uint64_t getTotalDiskSpace();
   void updateClusterStat();

private:
   void updateclusterstat_(Cluster& c);
   void updateclusterio_(Cluster& c, std::map<std::string, int64_t>& data_in, std::map<std::string, int64_t>& data_out, int64_t& total);

public:
   std::map<Address, int, AddrComp> m_mAddrList;
   std::map<int, SlaveNode> m_mSlaveList;

   Topology m_Topology;
   Cluster m_Cluster;

private:
   pthread_mutex_t m_SlaveLock;
};

#endif
