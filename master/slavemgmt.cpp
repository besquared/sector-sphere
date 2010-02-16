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
   Yunhong Gu, last updated 02/16/2010
*****************************************************************************/

#include <slavemgmt.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <common.h>
#include <meta.h>

using namespace std;

SlaveManager::SlaveManager()
{
   pthread_mutex_init(&m_SlaveLock, NULL);
}

SlaveManager::~SlaveManager()
{
   pthread_mutex_destroy(&m_SlaveLock);
}

int SlaveManager::init(const char* topoconf)
{
   m_Cluster.m_iClusterID = 0;
   m_Cluster.m_iTotalNodes = 0;
   m_Cluster.m_llAvailDiskSpace = 0;
   m_Cluster.m_llTotalFileSize = 0;
   m_Cluster.m_llTotalInputData = 0;
   m_Cluster.m_llTotalOutputData = 0;
   m_Cluster.m_viPath.clear();

   if (m_Topology.init(topoconf) < 0)
      return -1;

   Cluster* pc = &m_Cluster;

   // insert 0/0/0/....
   for (unsigned int i = 0; i < m_Topology.m_uiLevel; ++ i)
   {
      Cluster c;
      c.m_iClusterID = 0;
      c.m_iTotalNodes = 0;
      c.m_llAvailDiskSpace = 0;
      c.m_llTotalFileSize = 0;
      c.m_llTotalInputData = 0;
      c.m_llTotalOutputData = 0;
      c.m_viPath = pc->m_viPath;
      c.m_viPath.insert(c.m_viPath.end(), 0);

      pc->m_mSubCluster[0] = c;
      pc = &(pc->m_mSubCluster[0]);
   }

   for (vector<Topology::TopoMap>::iterator i = m_Topology.m_vTopoMap.begin(); i != m_Topology.m_vTopoMap.end(); ++ i)
   {
      pc = &m_Cluster;

      for (vector<int>::iterator l = i->m_viPath.begin(); l != i->m_viPath.end(); ++ l)
      {
         if (pc->m_mSubCluster.find(*l) == pc->m_mSubCluster.end())
         {
            Cluster c;
            c.m_iClusterID = *l;
            c.m_iTotalNodes = 0;
            c.m_llAvailDiskSpace = 0;
            c.m_llTotalFileSize = 0;
            c.m_llTotalInputData = 0;
            c.m_llTotalOutputData = 0;
            c.m_viPath = pc->m_viPath;
            c.m_viPath.insert(c.m_viPath.end(), *l);
            pc->m_mSubCluster[*l] = c;
         }
         pc = &(pc->m_mSubCluster[*l]);
      }
   }

   return 1;
}

int SlaveManager::insert(SlaveNode& sn)
{
   CGuard sg(m_SlaveLock);

   sn.m_iStatus = 1;

   m_mSlaveList[sn.m_iNodeID] = sn;

   Address addr;
   addr.m_strIP = sn.m_strIP;
   addr.m_iPort = sn.m_iPort;
   m_mAddrList[addr] = sn.m_iNodeID;

   m_Topology.lookup(sn.m_strIP.c_str(), sn.m_viPath);
   map<int, Cluster>* sc = &(m_Cluster.m_mSubCluster);
   map<int, Cluster>::iterator pc;
   for (vector<int>::iterator i = sn.m_viPath.begin(); i != sn.m_viPath.end(); ++ i)
   {
      pc = sc->find(*i);
      pc->second.m_iTotalNodes ++;
      pc->second.m_llAvailDiskSpace += sn.m_llAvailDiskSpace;
      pc->second.m_llTotalFileSize += sn.m_llTotalFileSize;

      sc = &(pc->second.m_mSubCluster);
   }

   pc->second.m_sNodes.insert(sn.m_iNodeID);

   map<string, set<string> >::iterator i = m_mIPFSInfo.find(sn.m_strIP);
   if (i == m_mIPFSInfo.end())
      m_mIPFSInfo[sn.m_strIP].insert(Metadata::revisePath(sn.m_strStoragePath));
   else
      i->second.insert(sn.m_strStoragePath);

   return 1;
}

int SlaveManager::remove(int nodeid)
{
   CGuard sg(m_SlaveLock);

   map<int, SlaveNode>::iterator sn = m_mSlaveList.find(nodeid);

   if (sn == m_mSlaveList.end())
      return -1;

   Address addr;
   addr.m_strIP = sn->second.m_strIP;
   addr.m_iPort = sn->second.m_iPort;
   m_mAddrList.erase(addr);

   vector<int> path;
   m_Topology.lookup(sn->second.m_strIP.c_str(), path);
   map<int, Cluster>* sc = &(m_Cluster.m_mSubCluster);
   map<int, Cluster>::iterator pc;
   for (vector<int>::iterator i = path.begin(); i != path.end(); ++ i)
   {
      if ((pc = sc->find(*i)) == sc->end())
      {
         // something wrong
         break;
      }

      pc->second.m_iTotalNodes --;
      pc->second.m_llAvailDiskSpace -= sn->second.m_llAvailDiskSpace;
      pc->second.m_llTotalFileSize -= sn->second.m_llTotalFileSize;

      sc = &(pc->second.m_mSubCluster);
   }

   pc->second.m_sNodes.erase(nodeid);

   map<string, set<string> >::iterator i = m_mIPFSInfo.find(sn->second.m_strIP);
   if (i != m_mIPFSInfo.end())
   {
      i->second.erase(sn->second.m_strStoragePath);
      if (i->second.empty())
         m_mIPFSInfo.erase(i);
   }
   else
   {
      //something wrong
   }

   m_mSlaveList.erase(sn);

   return 1;
}

bool SlaveManager::checkDuplicateSlave(const string& ip, const string& path)
{
   CGuard sg(m_SlaveLock);

   map<string, set<string> >::iterator i = m_mIPFSInfo.find(ip);
   if (i == m_mIPFSInfo.end())
      return false;

   string revised_path = Metadata::revisePath(path);
   for (set<string>::iterator j = i->second.begin(); j != i->second.end(); ++ j)
   {
      // if there is overlap between the two storage paths, it means that there is a conflict
      // the new slave should be rejected in this case

      if ((j->find(revised_path) != string::npos) || (revised_path.find(*j) != string::npos))
         return true;
   }

   return false;
}

int SlaveManager::chooseReplicaNode(set<int>& loclist, SlaveNode& sn, const int64_t& filesize)
{
   CGuard sg(m_SlaveLock);

   vector< set<int> > avail;
   avail.resize(m_Topology.m_uiLevel + 1);
   for (map<int, SlaveNode>::iterator i = m_mSlaveList.begin(); i != m_mSlaveList.end(); ++ i)
   {
      // only nodes with more than 10GB disk space are chosen
      if (i->second.m_llAvailDiskSpace < (10000000000LL + filesize))
         continue;

      int level = 0;
      for (set<int>::iterator j = loclist.begin(); j != loclist.end(); ++ j)
      {
         if (i->first == *j)
         {
            level = -1;
            break;
         }

         map<int, SlaveNode>::iterator jp = m_mSlaveList.find(*j);
         if (jp == m_mSlaveList.end())
            continue;

         int tmpl = m_Topology.match(i->second.m_viPath, jp->second.m_viPath);
         if (tmpl > level)
            level = tmpl;
      }

      if (level >= 0)
         avail[level].insert(i->first);
   }

   set<int> candidate;
   for (unsigned int i = 0; i <= m_Topology.m_uiLevel; ++ i)
   {
      if (avail[i].size() > 0)
      {
         candidate = avail[i];
         break;
      }
   }

   if (candidate.empty())
      return SectorError::E_NODISK;

   timeval t;
   gettimeofday(&t, 0);
   srand(t.tv_usec);

   int r = int(candidate.size() * (double(rand()) / RAND_MAX));

   set<int>::iterator n = candidate.begin();
   for (int i = 0; i < r; ++ i)
      n ++;

   sn = m_mSlaveList[*n];

   return 1;
}
 
int SlaveManager::chooseIONode(set<int>& loclist, const Address& client, int mode, vector<SlaveNode>& sl, int replica)
{
   CGuard sg(m_SlaveLock);

   timeval t;
   gettimeofday(&t, 0);
   srand(t.tv_usec);

   if (!loclist.empty())
   {
      // find nearest node, if equal distance, choose a random one
      set<int>::iterator n = loclist.begin();
      unsigned int dist = 1000000000;
      for (set<int>::iterator i = loclist.begin(); i != loclist.end(); ++ i)
      {
         unsigned int d = m_Topology.distance(client.m_strIP.c_str(), m_mSlaveList[*i].m_strIP.c_str());
         if (d < dist)
         {
            dist = d;
            n = i;
         }
         else if (d == dist)
         {
            if ((rand() % 2) == 0)
               n = i;
         }
      }

      sl.push_back(m_mSlaveList[*n]);

      // if this is a READ_ONLY operation, one node is enough
      if ((mode & SF_MODE::WRITE) == 0)
         return 1;

      for (set<int>::iterator i = loclist.begin(); i != loclist.end(); i ++)
      {
         if (i == n)
            continue;

         sl.push_back(m_mSlaveList[*i]);
      }
   }
   else
   {
      // no available nodes for READ_ONLY operation
      if ((mode & SF_MODE::WRITE) == 0)
         return 0;

      set<int> avail;

      for (map<int, SlaveNode>::iterator i = m_mSlaveList.begin(); i != m_mSlaveList.end(); ++ i)
      {
         // only nodes with more than 10GB disk space are chosen
         if (i->second.m_llAvailDiskSpace > 10000000000LL)
            avail.insert(i->first);
      }

      int r = 0;
      if (avail.size() > 0)
         r = int(avail.size() * (double(rand()) / RAND_MAX)) % avail.size();
      set<int>::iterator n = avail.begin();
      for (int i = 0; i < r; ++ i)
         n ++;
      sl.push_back(m_mSlaveList[*n]);

      // if this is not a high reliable write, one node is enough
      if ((mode & SF_MODE::HiRELIABLE) == 0)
         return sl.size();

      // otherwise choose more nodes for immediate replica
      for (int i = 0; i < replica - 1; ++ i)
      {
         SlaveNode sn;
         set<int> locid;
         for (vector<SlaveNode>::iterator j = sl.begin(); j != sl.end(); ++ j)
            locid.insert(j->m_iNodeID);

         pthread_mutex_unlock(&m_SlaveLock);
         if (chooseReplicaNode(locid, sn, 10000000000LL) < 0)
            continue;

         sl.push_back(sn);
      }
   }

   return sl.size();
}

int SlaveManager::chooseReplicaNode(set<Address, AddrComp>& loclist, SlaveNode& sn, const int64_t& filesize)
{
   set<int> locid;
   for (set<Address>::iterator i = loclist.begin(); i != loclist.end(); ++ i)
   {
      locid.insert(m_mAddrList[*i]);
   }

   return chooseReplicaNode(locid, sn, filesize);
}

int SlaveManager::chooseIONode(set<Address, AddrComp>& loclist, const Address& client, int mode, vector<SlaveNode>& sl, int replica)
{
   set<int> locid;
   for (set<Address>::iterator i = loclist.begin(); i != loclist.end(); ++ i)
   {
      locid.insert(m_mAddrList[*i]);
   }

   return chooseIONode(locid, client, mode, sl, replica);
}

unsigned int SlaveManager::getTotalSlaves()
{
   CGuard sg(m_SlaveLock);

   return m_mSlaveList.size();
}

uint64_t SlaveManager::getTotalDiskSpace()
{
   CGuard sg(m_SlaveLock);

   uint64_t size = 0;
   for (map<int, SlaveNode>::iterator i = m_mSlaveList.begin(); i != m_mSlaveList.end(); ++ i)
   {
      size += i->second.m_llAvailDiskSpace;
   }

   return size;
}

void SlaveManager::updateClusterStat()
{
   CGuard sg(m_SlaveLock);

   updateclusterstat_(m_Cluster);
}

void SlaveManager::updateclusterstat_(Cluster& c)
{
   if (c.m_mSubCluster.empty())
   {
      c.m_llAvailDiskSpace = 0;
      c.m_llTotalFileSize = 0;
      c.m_llTotalInputData = 0;
      c.m_llTotalOutputData = 0;
      c.m_mSysIndInput.clear();
      c.m_mSysIndOutput.clear();

      for (set<int>::iterator i = c.m_sNodes.begin(); i != c.m_sNodes.end(); ++ i)
      {
         c.m_llAvailDiskSpace += m_mSlaveList[*i].m_llAvailDiskSpace;
         c.m_llTotalFileSize += m_mSlaveList[*i].m_llTotalFileSize;
         updateclusterio_(c, m_mSlaveList[*i].m_mSysIndInput, c.m_mSysIndInput, c.m_llTotalInputData);
         updateclusterio_(c, m_mSlaveList[*i].m_mSysIndOutput, c.m_mSysIndOutput, c.m_llTotalOutputData);
      }
   }
   else
   {
      c.m_llAvailDiskSpace = 0;
      c.m_llTotalFileSize = 0;
      c.m_llTotalInputData = 0;
      c.m_llTotalOutputData = 0;
      c.m_mSysIndInput.clear();
      c.m_mSysIndOutput.clear();
	    
      for (map<int, Cluster>::iterator i = c.m_mSubCluster.begin(); i != c.m_mSubCluster.end(); ++ i)
      {
         updateclusterstat_(i->second);

         c.m_llAvailDiskSpace += i->second.m_llAvailDiskSpace;
         c.m_llTotalFileSize += i->second.m_llTotalFileSize;
         updateclusterio_(c, i->second.m_mSysIndInput, c.m_mSysIndInput, c.m_llTotalInputData);
         updateclusterio_(c, i->second.m_mSysIndOutput, c.m_mSysIndOutput, c.m_llTotalOutputData);
      }
   }
}

void SlaveManager::updateclusterio_(Cluster& c, map<string, int64_t>& data_in, map<string, int64_t>& data_out, int64_t& total)
{
   for (map<string, int64_t>::iterator p = data_in.begin(); p != data_in.end(); ++ p)
   {
      vector<int> path;
      m_Topology.lookup(p->first.c_str(), path);
      if (m_Topology.match(c.m_viPath, path) == c.m_viPath.size())
         continue;

      map<string, int64_t>::iterator n = data_out.find(p->first);
      if (n == data_out.end())
         data_out[p->first] = p->second;
      else
         n->second += p->second;

      total += p->second;
   }
}
