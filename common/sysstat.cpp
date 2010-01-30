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

#include <iostream>
#include <sector.h>

using namespace std;

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
   for (vector<ClusterStat>::iterator i = m_vCluster.begin(); i != m_vCluster.end(); ++ i)
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
   for (vector<SlaveStat>::iterator i = m_vSlaveList.begin(); i != m_vSlaveList.end(); ++ i)
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
