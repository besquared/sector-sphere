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
   Yunhong Gu, last updated 11/12/2008
*****************************************************************************/

#include "sysstat.h"
#include <time.h>
#include <cstring>
#include <iostream>

using namespace std;

const int SysStat::g_iSize = 40 + 4 + 4 + 4;

int SysStat::serialize(char* buf, int& size, map<uint32_t, Address>& ml, map<int, SlaveNode>& sl, Cluster& c)
{
   if (size < int(g_iSize + 8 + c.m_mSubCluster.size() * 48 + sl.size() * 72))
      return -1;

   *(int64_t*)buf = m_llStartTime;
   *(int64_t*)(buf + 8) = m_llAvailDiskSpace;
   *(int64_t*)(buf + 16) = m_llTotalFileSize;
   *(int64_t*)(buf + 24) = m_llTotalFileNum;
   *(int64_t*)(buf + 32) = m_llTotalSlaves;

   char* p = buf + 40;
   *(int32_t*)p = c.m_mSubCluster.size();
   p += 4;
   for (map<int, Cluster>::iterator i = c.m_mSubCluster.begin(); i != c.m_mSubCluster.end(); ++ i)
   {
      *(int64_t*)p = i->second.m_iClusterID;
      *(int64_t*)(p + 8) = i->second.m_iTotalNodes;
      *(int64_t*)(p + 16) = i->second.m_llAvailDiskSpace;
      *(int64_t*)(p + 24) = i->second.m_llTotalFileSize;
      *(int64_t*)(p + 32) = i->second.m_llTotalInputData;
      *(int64_t*)(p + 40) = i->second.m_llTotalOutputData;

      p += 48;
   }

   *(int32_t*)p = ml.size();
   p += 4;
   for (map<uint32_t, Address>::iterator i = ml.begin(); i != ml.end(); ++ i)
   {
      strcpy(p, i->second.m_strIP.c_str());
      p += 16;
      *(int32_t*)p = i->second.m_iPort;
      p += 4;
   }

   *(int32_t*)p = sl.size();
   p += 4;
   for (map<int, SlaveNode>::iterator i = sl.begin(); i != sl.end(); ++ i)
   {
      strcpy(p, i->second.m_strIP.c_str());
      *(int64_t*)(p + 16) = i->second.m_llAvailDiskSpace;
      *(int64_t*)(p + 24) = i->second.m_llTotalFileSize;
      *(int64_t*)(p + 32) = i->second.m_llCurrMemUsed;
      *(int64_t*)(p + 40) = i->second.m_llCurrCPUUsed;
      *(int64_t*)(p + 48) = i->second.m_llTotalInputData;
      *(int64_t*)(p + 56) = i->second.m_llTotalOutputData;
      *(int64_t*)(p + 64) = i->second.m_llTimeStamp;

      p += 72;
   }

   size = g_iSize + c.m_mSubCluster.size() * 48 + ml.size() * 20 + sl.size() * 72;

   return 0;
}

int SysStat::deserialize(char* buf, const int& size)
{
   if (size < g_iSize)
      return -1;

   m_llStartTime = *(int64_t*)buf;
   m_llAvailDiskSpace = *(int64_t*)(buf + 8);
   m_llTotalFileSize = *(int64_t*)(buf + 16);
   m_llTotalFileNum = *(int64_t*)(buf + 24);
   m_llTotalSlaves = *(int64_t*)(buf + 32);

   char* p = buf + 40;
   int c = *(int32_t*)p;
   m_vCluster.resize(c);
   p += 4;
   for (vector<Cluster>::iterator i = m_vCluster.begin(); i != m_vCluster.end(); ++ i)
   {
      i->m_iClusterID = *(int64_t*)p;
      i->m_iTotalNodes = *(int64_t*)(p + 8);
      i->m_llAvailDiskSpace = *(int64_t*)(p + 16);
      i->m_llTotalFileSize = *(int64_t*)(p + 24);
      i->m_llTotalInputData = *(int64_t*)(p + 32);
      i->m_llTotalOutputData = *(int64_t*)(p + 40);

      p += 48;
   }

   int m = *(int32_t*)p;
   p += 4;
   m_vMasterList.resize(m);
   for (vector<Address>::iterator i = m_vMasterList.begin(); i != m_vMasterList.end(); ++ i)
   {
      i->m_strIP = p;
      p += 16;
      i->m_iPort = *(int32_t*)p;
      p += 4;      
   }

   int n = *(int32_t*)p;
   p += 4;
   m_vSlaveList.resize(n);
   for (vector<SlaveNode>::iterator i = m_vSlaveList.begin(); i != m_vSlaveList.end(); ++ i)
   {
      i->m_strIP = p;
      i->m_llAvailDiskSpace = *(int64_t*)(p + 16);
      i->m_llTotalFileSize = *(int64_t*)(p + 24);
      i->m_llCurrMemUsed = *(int64_t*)(p + 32);
      i->m_llCurrCPUUsed = *(int64_t*)(p + 40);
      i->m_llTotalInputData = *(int64_t*)(p + 48);
      i->m_llTotalOutputData = *(int64_t*)(p + 56);
      i->m_llTimeStamp = *(int64_t*)(p + 64);

      p += 72;
   }

   return 0;
}

void SysStat::print()
{
   const int MB = 1024 * 1024;

   cout << "Sector System Information:" << endl;
   time_t st = m_llStartTime;
   cout << "Running since " << ctime(&st);
   cout << "Available Disk Size " << m_llAvailDiskSpace / MB << " MB" << endl;
   cout << "Total File Size " << m_llTotalFileSize / MB << " MB" << endl;
   cout << "Total Number of Files " << m_llTotalFileNum << endl;
   cout << "Total Number of Slave Nodes " << m_llTotalSlaves << endl;

   cout << "------------------------------------------------------------\n";
   cout << "MASTER ID \t IP \t PORT\n";
   int m = 1;
   for (vector<Address>::iterator i = m_vMasterList.begin(); i != m_vMasterList.end(); ++ i)
   {
      cout << m++ << ": \t" << i->m_strIP << "\t" << i->m_iPort << endl;
   }

   cout << "------------------------------------------------------------\n";

   cout << "Total number of clusters " << m_vCluster.size() << endl;
   cout << "Cluster_ID  Total_Nodes  AvailDisk(MB)  FileSize(MB)  NetIn(MB)  NetOut(MB)\n";
   for (vector<Cluster>::iterator i = m_vCluster.begin(); i != m_vCluster.end(); ++ i)
   {
      cout << i->m_iClusterID << ":  " 
           << i->m_iTotalNodes << "  " 
           << i->m_llAvailDiskSpace / MB << "  " 
           << i->m_llTotalFileSize / MB << "  " 
           << i->m_llTotalInputData / MB << "  " 
           << i->m_llTotalOutputData / MB << endl;
   }

   cout << "------------------------------------------------------------\n";
   cout << "SLAVE_ID  IP  TS(us)  AvailDisk(MB)  TotalFile(MB)  Mem(MB)  CPU(us)  NetIn(MB)  NetOut(MB)\n";

   int s = 1;
   for (vector<SlaveNode>::iterator i = m_vSlaveList.begin(); i != m_vSlaveList.end(); ++ i)
   {
      cout << s++ << ":  "
           << i->m_strIP << "  " 
           << i->m_llTimeStamp << "  " 
           << i->m_llAvailDiskSpace / MB << "  " 
           << i->m_llTotalFileSize / MB << "  " 
           << i->m_llCurrMemUsed / MB << "  " 
           << i->m_llCurrCPUUsed << "  " 
           << i->m_llTotalInputData / MB << "  " 
           << i->m_llTotalOutputData / MB << endl;
   }
}
