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
   Yunhong Gu, last updated 01/27/2010
*****************************************************************************/

#include "dcclient.h"
#include <errno.h>
#include <iostream>

using namespace std;

DCClient* Client::createDCClient()
{
   DCClient* sp = new DCClient;
   sp->m_pClient = this;
   pthread_mutex_lock(&m_IDLock);
   sp->m_iID = m_iID ++;
   m_mDCList[sp->m_iID] = sp;
   pthread_mutex_unlock(&m_IDLock);

   return sp;
}

int Client::releaseDCClient(DCClient* sp)
{
   pthread_mutex_lock(&m_IDLock);
   m_mDCList.erase(sp->m_iID);
   pthread_mutex_unlock(&m_IDLock);
   delete sp;

   return 0;
}

SphereStream::SphereStream():
m_piLocID(NULL),
m_iFileNum(0),
m_llSize(0),
m_llRecNum(0),
m_llStart(0),
m_llEnd(-1),
m_iStatus(0)
{
   m_vFiles.clear();
   m_vRecNum.clear();
   m_vSize.clear();
   m_vLocation.clear();
}

SphereStream::~SphereStream()
{
   delete [] m_piLocID;
}

int SphereStream::init(const vector<string>& files)
{
   m_vOrigInput = files;
   return 0;
}

int SphereStream::init(const int& num)
{
   m_iFileNum = num;
   m_llSize = 0;
   m_llRecNum = 0;
   m_llStart = 0;
   m_llEnd = -1;
   m_iStatus = 1;

   if (num <= 0)
      return 0;

   m_vLocation.resize(num);
   m_vFiles.resize(num);
   m_vSize.resize(num);
   m_vRecNum.resize(num);

   m_piLocID = new int32_t[num];

   for (vector<string>::iterator i = m_vFiles.begin(); i != m_vFiles.end(); ++ i)
      *i = "";
   for (vector<int64_t>::iterator i = m_vSize.begin(); i != m_vSize.end(); ++ i)
      *i = 0;
   for (vector<int64_t>::iterator i = m_vRecNum.begin(); i != m_vRecNum.end(); ++ i)
      *i = 0;

   return num;
}

void SphereStream::setOutputPath(const string& path, const string& name)
{
   m_strPath = path;
   m_strName = name;
}

void SphereStream::setOutputLoc(const unsigned int& bucket, const Address& addr)
{
   if (bucket >= m_vLocation.size())
      return;

   m_vLocation[bucket].insert(addr);
}


//
SphereResult::SphereResult():
m_iResID(-1),
m_iStatus(0),
m_pcData(NULL),
m_iDataLen(0),
m_pllIndex(NULL),
m_iIndexLen(0)
{
}

SphereResult::~SphereResult()
{
   delete [] m_pcData;
   delete [] m_pllIndex;
}

//
DCClient::DCClient():
m_iMinUnitSize(1000000),
m_iMaxUnitSize(256000000),
m_iCore(1),
m_bDataMove(true)
{
   m_strOperator = "";
   m_pcParam = NULL;
   m_iParamSize = 0;
   m_pOutput = NULL;
   m_iOutputType = 0;
   m_pOutputLoc = NULL;

   m_mpDS.clear();
   m_mBucket.clear();
   m_mSPE.clear();

   m_iProgress = 0;
   m_dRunningProgress = 0;
   m_iAvgRunTime = -1;
   m_iTotalDS = 0;
   m_iTotalSPE = 0;
   m_iAvailRes = 0;
   m_bBucketHealth = true;

   pthread_mutex_init(&m_DSLock, NULL);
   pthread_mutex_init(&m_ResLock, NULL);
   pthread_cond_init(&m_ResCond, NULL);
   pthread_mutex_init(&m_RunLock, NULL);
}

DCClient::~DCClient()
{
   delete [] m_pcParam;
   delete [] m_pOutputLoc;

   pthread_mutex_destroy(&m_DSLock);
   pthread_mutex_destroy(&m_ResLock);
   pthread_cond_destroy(&m_ResCond);
   pthread_mutex_destroy(&m_RunLock);
}

int DCClient::loadOperator(const char* library)
{
   struct stat st;
   if (::stat(library, &st) < 0)
   {
      cerr << "loadOperator: no library found.\n";
      return SectorError::E_LOCALFILE;
   }

   ifstream lib;
   lib.open(library, ios::in | ios::binary);
   if (lib.bad() || lib.fail())
   {
      cerr << "loadOperator: bad file.\n";
      return SectorError::E_LOCALFILE;
   }
   lib.close();

   // TODO : check ".so"

   vector<string> dir;
   Index::parsePath(library, dir);

   OP op;
   op.m_strLibrary = dir[dir.size() - 1];
   op.m_strLibPath = library;
   op.m_iSize = st.st_size;

   m_vOP.insert(m_vOP.end(), op);

   return 0;
}

int DCClient::loadOperator(SPE& s)
{
   int num = m_vOP.size();
   m_pClient->m_DataChn.send(s.m_strIP, s.m_iDataPort, s.m_iSession, (char*)&num, 4);

   for (vector<OP>::iterator i = m_vOP.begin(); i != m_vOP.end(); ++ i)
   {
      m_pClient->m_DataChn.send(s.m_strIP, s.m_iDataPort, s.m_iSession, i->m_strLibrary.c_str(), i->m_strLibrary.length() + 1);

      ifstream lib;
      lib.open(i->m_strLibPath.c_str(), ios::in | ios::binary);
      char* buf = new char[i->m_iSize];
      lib.read(buf, i->m_iSize);
      lib.close();

      m_pClient->m_DataChn.send(s.m_strIP, s.m_iDataPort, s.m_iSession, buf, i->m_iSize);
   }

   return 0;
}

int DCClient::run(const SphereStream& input, SphereStream& output, const string& op, const int& rows, const char* param, const int& size, const int& type)
{
   pthread_mutex_lock(&m_RunLock);
   pthread_mutex_unlock(&m_RunLock);

   m_iProcType = type;
   m_strOperator = op;
   m_pcParam = new char[size];
   memcpy(m_pcParam, param, size);
   m_iParamSize = size;
   m_pInput = (SphereStream*)&input;
   m_pOutput = &output;
   m_iRows = rows;
   m_iOutputType = m_pOutput->m_iFileNum;

   // when processing files, data will not be moved
   if (rows == 0)
      m_bDataMove = false;

   m_mpDS.clear();
   m_mBucket.clear();
   m_mSPE.clear();

   prepareInput();
   if (m_pInput->m_llSize <= 0)
      return 0;

   cout << "JOB " << m_pInput->m_llSize << " " << m_pInput->m_llRecNum << endl;

   SectorMsg msg;
   msg.setType(202); // locate available SPE
   msg.setKey(m_pClient->m_iKey);
   msg.m_iDataLength = SectorMsg::m_iHdrSize;

   Address serv;
   m_pClient->m_Routing.getPrimaryMaster(serv);
   if ((m_pClient->m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0) || (msg.getType() < 0))
   {
      cerr << "unable to locate any SPE.\n";
      return -1;
   }

   m_iSPENum = (msg.m_iDataLength - 4) / 72;
   if (0 == m_iSPENum)
   {
      cerr << "no available SPE found.\n";
      return SectorError::E_RESOURCE;
   }

   prepareSPE(msg.getData());

   if (segmentData() <= 0)
   {
      cerr << "data segmentation error.\n";
      return -1;
   }

   if (m_iOutputType == -1)
      m_pOutput->init(m_mpDS.size());

   if (prepareOutput(msg.getData()) < 0)
   {
      cerr << "fail to locate any shufflers.\n";
      return -1;
   }

   m_iProgress = 0;
   m_iAvgRunTime = -1;
   m_iTotalDS = m_mpDS.size();
   m_iTotalSPE = m_mSPE.size();
   m_iAvailRes = 0;
   m_bBucketHealth = true;

   cout << m_mSPE.size() << " spes found! " << m_mpDS.size() << " data seg total." << endl;

   // starting...
   pthread_t reduce;
   pthread_create(&reduce, NULL, run, this);
   pthread_detach(reduce);

   return 0;
}

int DCClient::run_mr(const SphereStream& input, SphereStream& output, const string& mr, const int& rows, const char* param, const int& size)
{
   return run(input, output, mr, rows, param, size, 1);
}

int DCClient::close()
{
   pthread_mutex_lock(&m_RunLock);
   pthread_mutex_unlock(&m_RunLock);

   // restore initial value for next run
   m_strOperator = "";
   m_pcParam = NULL;
   m_iParamSize = 0;
   m_pOutput = NULL;
   m_iOutputType = 0;
   m_pOutputLoc = NULL;

   m_mpDS.clear();
   m_mBucket.clear();
   m_mSPE.clear();

   m_iProgress = 0;
   m_iAvgRunTime = -1;
   m_iTotalDS = 0;
   m_iTotalSPE = 0;
   m_iAvailRes = 0;

   return 0;
}

void* DCClient::run(void* param)
{
   DCClient* self = (DCClient*)param;

   pthread_mutex_lock(&self->m_RunLock);

   while (self->m_iProgress < self->m_iTotalDS)
   {
      if (0 == self->checkSPE())
         break;

      string ip;
      int port;
      int tmp;
      SectorMsg msg;
      if (self->m_pClient->m_GMP.recvfrom(ip, port, tmp, &msg, false) < 0)
         continue;

      int32_t speid = *(int32_t*)(msg.getData());

      map<int, SPE>::iterator s = self->m_mSPE.find(speid);
      if (s == self->m_mSPE.end())
         continue;

      if (s->second.m_iStatus <= 1)
         continue;

      int progress = *(int32_t*)(msg.getData() + 4);
      gettimeofday(&s->second.m_LastUpdateTime, 0);
      if (progress < 0)
      {
         cerr << "SPE PROCESSING ERROR " << ip << " " << port << endl;

         //error, quit this segment on the SPE
         s->second.m_pDS->m_iStatus = -1;
         s->second.m_pDS->m_iSPEID = -1;
         s->second.m_iStatus = 1;

         s->second.m_pDS->m_pResult->m_iStatus = *(int32_t*)(msg.getData() + 8);
         int errsize = msg.m_iDataLength - SectorMsg::m_iHdrSize - 12;
         if (errsize > 0)
         {
            s->second.m_pDS->m_pResult->m_pcData = new char[errsize];
            strcpy(s->second.m_pDS->m_pResult->m_pcData, msg.getData() + 12);
         }

         ++ self->m_iProgress;
         pthread_mutex_lock(&self->m_ResLock);
         ++ self->m_iAvailRes;
         pthread_cond_signal(&self->m_ResCond);
         pthread_mutex_unlock(&self->m_ResLock);

         continue;
      }
      if (progress > s->second.m_iProgress)
         s->second.m_iProgress = progress;
      if (progress < 100)
         continue;

      self->readResult(&(s->second));

      // one SPE completes!
      timeval t;
      gettimeofday(&t, 0);
      if (self->m_iAvgRunTime <= 0)
         self->m_iAvgRunTime = t.tv_sec - s->second.m_StartTime.tv_sec;
      else
         self->m_iAvgRunTime = (self->m_iAvgRunTime * 7 + (t.tv_sec - s->second.m_StartTime.tv_sec)) / 8;
   }

   self->m_dRunningProgress = 0;

   // disconnect all SPEs and close all Shufflers
   for (map<int, SPE>::iterator i = self->m_mSPE.begin(); i != self->m_mSPE.end(); ++ i)
      self->m_pClient->m_DataChn.remove(i->second.m_strIP, i->second.m_iDataPort);

   for(map<int, BUCKET>::iterator i = self->m_mBucket.begin(); i != self->m_mBucket.end(); ++ i)
   {
      SectorMsg msg;
      int32_t cmd = -1;
      msg.setData(0, (char*)&cmd, 4);
      int id = 0;
      self->m_pClient->m_GMP.sendto(i->second.m_strIP.c_str(), i->second.m_iShufflerPort, id, &msg);
   }

   //TODO: need to detect lost slaves
   while (self->checkBucket() > 0)
   {
      string ip;
      int port;
      int tmp;
      SectorMsg msg;
      if (self->m_pClient->m_GMP.recvfrom(ip, port, tmp, &msg, false) < 0)
         continue;

      int32_t bucketid = *(int32_t*)(msg.getData());
      map<int, BUCKET>::iterator b = self->m_mBucket.find(bucketid);
      if (b == self->m_mBucket.end())
         continue;
      b->second.m_iProgress = 100;
      self->m_pClient->m_DataChn.remove(b->second.m_strIP, b->second.m_iDataPort);

      pthread_cond_signal(&self->m_ResCond);
   }

   // set totalSPE = 0, so that read() will return error immediately
   if (self->m_iProgress < 100)
      self->m_iTotalSPE = 0;

   pthread_mutex_unlock(&self->m_RunLock);

   return NULL;
}

int DCClient::checkSPE()
{
   timeval t;
   gettimeofday(&t, 0);

   bool spe_busy = false;
   bool ds_found = false;

   m_dRunningProgress = 0.0;

   for (map<int, SPE>::iterator s = m_mSPE.begin(); s != m_mSPE.end(); ++ s)
   {
      // this SPE is abandond
      if (-1 == s->second.m_iStatus)
         continue;

      // if the SPE is not running
      if (2 != s->second.m_iStatus)
      {
         Address sn;
         sn.m_strIP = s->second.m_strIP;
         sn.m_iPort = s->second.m_iPort;

         // find a new DS and start it
         pthread_mutex_lock(&m_DSLock);

         // start from random node
         map<int, DS*>::iterator dss = m_mpDS.end();
         int rs = 0;
         if (!m_mpDS.empty())
            rs = int(m_mpDS.size() * (double(rand()) / RAND_MAX)) % m_mpDS.size();
         map<int, DS*>::iterator d = m_mpDS.begin();
         for (int i = 0; i < rs; ++ i)
            ++ d;

         unsigned int MinDist = 1000000000;
         unsigned int MinLoc = 1000000000;

         for (int i = 0, n = m_mpDS.size(); i < n; ++ i)
         {
            if (++ d == m_mpDS.end())
               d = m_mpDS.begin();

            if (0 != d->second->m_iStatus)
               continue;

            unsigned int dist = m_pClient->m_Topology.distance(sn, *(d->second->m_pLoc));

            if (dist < MinDist)
            {
               // find nearest data segment
               dss = d;
               MinDist = dist;
               MinLoc = d->second->m_pLoc->size();
            }
            else if (dist == MinDist)
            {
               if (d->second->m_pLoc->size() < MinLoc)
               {
                  // for same distance, process DS with less copies first
                  dss = d;
                  MinLoc = d->second->m_pLoc->size();
               }
               else if (d->second->m_pLoc->size() == MinLoc)
               {
                  // process data file with the slower progress first
               }
            }
         }

         if ((MinDist != 0) && !m_bDataMove)
         {
            // if a file is processed via pass by filename, it must be processed on its original location
            // also, this depends on if the source data is allowed to move

            dss = m_mpDS.end();
         }

         if (dss != m_mpDS.end())
         {
            startSPE(s->second, dss->second);
            ds_found = true;
         }

         pthread_mutex_unlock(&m_DSLock);
      }
      else 
      {
         if (m_pClient->m_DataChn.isConnected(s->second.m_strIP, s->second.m_iDataPort))
         {
            spe_busy = true;

            m_dRunningProgress += s->second.m_iProgress / 100.0;
         }
         else
         {
            cerr << "SPE lost " << s->second.m_strIP << endl;

            if (!m_mBucket.empty())
            {
               cerr << "cannot recover the hashing bucket due to the lost SPE. Process failed." << endl;
               m_bBucketHealth = false;
               return 0;
            }

            // dismiss this SPE and release its job
            s->second.m_iStatus = -1;
            m_pClient->m_DataChn.remove(s->second.m_strIP, s->second.m_iDataPort);
            m_iTotalSPE --;

            pthread_mutex_lock(&m_DSLock);

            if (++ s->second.m_pDS->m_iRetryNum > 3)
            {
               //if the DS still fails after several retries, it means there is a bug in processing the specific data.
               s->second.m_pDS->m_iStatus = -1;

               ++ m_iProgress;
               pthread_mutex_lock(&m_ResLock);
               ++ m_iAvailRes;
               pthread_cond_signal(&m_ResCond);
               pthread_mutex_unlock(&m_ResLock);
            }
            else
               s->second.m_pDS->m_iStatus = 0;

            s->second.m_pDS->m_iSPEID = -1;

            pthread_mutex_unlock(&m_DSLock);
         }
      }
   }

   // All SPEs are spare but none of them can be assigned a DS. Error occurs!
   if (!spe_busy && !ds_found && (m_iProgress < m_iTotalDS))
   {
      cerr << "Cannot allocate SPE for certain data segments. Process failed." << endl;
      return 0;
   }

   return m_iTotalSPE;
}

int DCClient::checkBucket()
{
   int count = 0;
   for (map<int, BUCKET>::iterator b = m_mBucket.begin(); b != m_mBucket.end(); ++ b)
   {
      if (b->second.m_iProgress == 100)
         count ++;
   }

   return m_mBucket.size() - count;
}

int DCClient::startSPE(SPE& s, DS* d)
{
   int res = 0;

   if (0 == s.m_iStatus)
   {
      // start an SPE at real time
      if (connectSPE(s) < 0)
         return SectorError::E_CONNECTION;
   }

   s.m_pDS = d;

   int32_t size = 20 + s.m_pDS->m_strDataFile.length() + 1;
   char* dataseg = new char[size];

   *(int64_t*)(dataseg) = s.m_pDS->m_llOffset;
   *(int64_t*)(dataseg + 8) = s.m_pDS->m_llSize;
   *(int32_t*)(dataseg + 16) = s.m_pDS->m_iID;
   strcpy(dataseg + 20, s.m_pDS->m_strDataFile.c_str());

   if (m_pClient->m_DataChn.send(s.m_strIP, s.m_iDataPort, s.m_iSession, dataseg, size) > 0)
   {
      d->m_iSPEID = s.m_iID;
      d->m_iStatus = 1;
      s.m_iStatus = 2;
      s.m_iProgress = 0;
      gettimeofday(&s.m_StartTime, 0);
      gettimeofday(&s.m_LastUpdateTime, 0);
      res = 1;
   }

   delete [] dataseg;

   return res;
}

int DCClient::checkProgress()
{
   if ((0 == m_iTotalSPE) && (m_iProgress < m_iTotalDS))
      return SectorError::E_RESOURCE;

   if (!m_bBucketHealth)
      return SectorError::E_BUCKET;

   int progress;

   if (m_iTotalDS <= 0)
      progress = 100;
   else
      progress = (m_iProgress + m_dRunningProgress) * 100 / m_iTotalDS;

   if ((progress == 100) && (checkBucket() > 0))
      return 99;

   return progress;
}

int DCClient::checkMapProgress()
{
   return checkProgress();
}

int DCClient::checkReduceProgress()
{
   if (m_mBucket.empty())
      return 100;

   int count = 0;
   for (map<int, BUCKET>::iterator b = m_mBucket.begin(); b != m_mBucket.end(); ++ b)
   {
      if (b->second.m_iProgress == 100)
         count ++;
   }

   return count * 100 / m_mBucket.size();   
}

int DCClient::read(SphereResult*& res, const bool& inorder, const bool& wait)
{
   while (0 == m_iAvailRes)
   {
      if (!wait || (0 == m_iTotalSPE))
         return -1;

      if (m_iProgress == m_iTotalDS)
         return 0;

      struct timeval now;
      struct timespec timeout;

      gettimeofday(&now, 0);
      timeout.tv_sec = now.tv_sec + 10;
      timeout.tv_nsec = now.tv_usec * 1000;

      pthread_mutex_lock(&m_ResLock);
      int retcode = pthread_cond_timedwait(&m_ResCond, &m_ResLock, &timeout);
      pthread_mutex_unlock(&m_ResLock);

      if (retcode == ETIMEDOUT)
         return SectorError::E_TIMEDOUT;
   }

   pthread_mutex_lock(&m_DSLock);

   map<int, DS*>::iterator d = m_mpDS.end();
   for (map<int, DS*>::iterator i = m_mpDS.begin(); i != m_mpDS.end(); ++ i)
   {
      // find completed DS, -1: error, 2: successful
      // TODO: deal with order...
      if ((i->second->m_iStatus == -1) || (i->second->m_iStatus == 2))
      {
         d = i;
         break;
      }
   }

   bool found = (d != m_mpDS.end());

   if (found)
   {
      res = d->second->m_pResult;
      d->second->m_pResult = NULL;
      res->m_strOrigFile = d->second->m_strDataFile;

      delete d->second;
      m_mpDS.erase(d);
   }

   pthread_mutex_unlock(&m_DSLock);

   if (found)
   {
     pthread_mutex_lock(&m_ResLock);
      -- m_iAvailRes;
     pthread_mutex_unlock(&m_ResLock);

     return 1;
   }

   return -1;
}

int DCClient::dataInfo(const vector<string>& files, vector<string>& info)
{
   SectorMsg msg;
   msg.setType(201);
   msg.setKey(m_pClient->m_iKey);

   int offset = 0;
   int32_t size = -1;
   for (vector<string>::const_iterator i = files.begin(); i != files.end(); ++ i)
   {
      string path = Metadata::revisePath(*i);
      size = path.length() + 1;
      msg.setData(offset, (char*)&size, 4);
      msg.setData(offset + 4, path.c_str(), size);
      offset += 4 + size;
   }

   size = -1;
   msg.setData(offset, (char*)&size, 4);

   Address serv;
   m_pClient->m_Routing.getPrimaryMaster(serv);
   if (m_pClient->m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   char* buf = msg.getData();
   size = msg.m_iDataLength - SectorMsg::m_iHdrSize;

   while (size > 0)
   {
      info.insert(info.end(), buf);
      size -= strlen(buf) + 1;
      buf += strlen(buf) + 1;
   }

   return info.size();
}


int DCClient::prepareInput()
{
   if ((m_pInput->m_iStatus != 0) || m_pInput->m_vOrigInput.empty())
      return -1;

   vector<string> datainfo;
   int res = dataInfo(m_pInput->m_vOrigInput, datainfo);
   if (res < 0)
      return res;

   m_pInput->m_iFileNum = datainfo.size();
   if (0 == m_pInput->m_iFileNum)
      return 0;

   m_pInput->m_iStatus = -1;

   m_pInput->m_vFiles.resize(m_pInput->m_iFileNum);
   m_pInput->m_vSize.resize(m_pInput->m_iFileNum);
   m_pInput->m_vRecNum.resize(m_pInput->m_iFileNum);
   m_pInput->m_vLocation.resize(m_pInput->m_iFileNum);
   vector<string>::iterator f = m_pInput->m_vFiles.begin();
   vector<int64_t>::iterator s = m_pInput->m_vSize.begin();
   vector<int64_t>::iterator r = m_pInput->m_vRecNum.begin();
   vector< set<Address, AddrComp> >::iterator a = m_pInput->m_vLocation.begin();

   bool indexfound = true;

   for (vector<string>::iterator i = datainfo.begin(); i != datainfo.end(); ++ i)
   {
      char* buf = new char[i->length() + 2];
      strcpy(buf, i->c_str());
      buf[strlen(buf) + 1] = '\0';

      //file_name 800 -1 192.168.136.30 37209 192.168.136.32 39805

      int n = strlen(buf) + 1;
      char* p = buf;
      for (int j = 0; j < n; ++ j, ++ p)
      {
         if (*p == ' ')
            *p = '\0';
      }
      p = buf;

      *f = p;
      p = p + strlen(p) + 1;
      *s = atoll(p);
      m_pInput->m_llSize += *s;
      p = p + strlen(p) + 1;
      *r = atoi(p);
      p = p + strlen(p) + 1;

      if (*r == -1)
      {
         // no record index found
         m_pInput->m_llRecNum = -1;
         indexfound = false;
      }
      else if (indexfound)
      {
         m_pInput->m_llRecNum += *r;
      }

      // retrieve all the locations
      while (true)
      {
         if (strlen(p) == 0)
            break;

         Address addr;
         addr.m_strIP = p;
         p = p + strlen(p) + 1;
         addr.m_iPort = atoi(p);
         p = p + strlen(p) + 1;

         a->insert(addr);
      }

      delete [] buf;

      f ++;
      s ++;
      r ++;
      a ++;
   }

   m_pInput->m_llEnd = m_pInput->m_llRecNum;

   m_pInput->m_iStatus = 1;
   return m_pInput->m_iFileNum;
}

int DCClient::prepareSPE(const char* spenodes)
{
   for (int c = 0; c < m_iCore; ++ c)
   {
      for (int i = 0; i < m_iSPENum; ++ i)
      {
         SPE spe;
         spe.m_iID = c * m_iSPENum + i;
         spe.m_pDS = NULL;
         spe.m_iStatus = 0;
         spe.m_iProgress = 0;

         spe.m_strIP = spenodes + i * 72;
         spe.m_iPort = *(int32_t*)(spenodes + i * 72 + 64);
         spe.m_iDataPort = *(int32_t*)(spenodes + i * 72 + 68);

         m_mSPE[spe.m_iID] = spe;
      }
   }

   return m_mSPE.size();
}

int DCClient::connectSPE(SPE& s)
{
   if (s.m_iStatus != 0)
      return -1;

   SectorMsg msg;
   msg.setType(203); // start processing engine
   msg.setKey(m_pClient->m_iKey);
   msg.setData(0, s.m_strIP.c_str(), s.m_strIP.length() + 1);
   msg.setData(64, (char*)&(s.m_iPort), 4);
   // leave a 4-byte blank spot for data port
   msg.setData(72, (char*)&(s.m_iID), 4);
   msg.setData(76, (char*)&m_pClient->m_iKey, 4);
   msg.setData(80, m_strOperator.c_str(), m_strOperator.length() + 1);
   int offset = 80 + m_strOperator.length() + 1;
   msg.setData(offset, (char*)&m_iRows, 4);
   msg.setData(offset + 4, (char*)&m_iParamSize, 4);
   msg.setData(offset + 8, m_pcParam, m_iParamSize);
   offset += 4 + 8 + m_iParamSize;
   msg.setData(offset, (char*)&m_iProcType, 4);

   Address serv;
   m_pClient->m_Routing.getPrimaryMaster(serv);
   if ((m_pClient->m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0) || (msg.getType() < 0))
   {
      cerr << "failed to connect SPE " << s.m_strIP << " " << s.m_iPort << endl;
      return -1;
   }

   s.m_iSession = *(int*)msg.getData();

   m_pClient->m_DataChn.connect(s.m_strIP, s.m_iDataPort);

   cout << "connect SPE " << s.m_strIP.c_str() << " " << *(int*)(msg.getData()) << endl;

   // send output information
   m_pClient->m_DataChn.send(s.m_strIP, s.m_iDataPort, s.m_iSession, (char*)&m_iOutputType, 4);
   if (m_iOutputType > 0)
   {
      int bnum = m_mBucket.size();
      m_pClient->m_DataChn.send(s.m_strIP, s.m_iDataPort, s.m_iSession, (char*)&bnum, 4);
      m_pClient->m_DataChn.send(s.m_strIP, s.m_iDataPort, s.m_iSession, m_pOutputLoc, bnum * 80);
      m_pClient->m_DataChn.send(s.m_strIP, s.m_iDataPort, s.m_iSession, (char*)m_pOutput->m_piLocID, m_iOutputType * 4);
   }
   else if (m_iOutputType < 0)
      m_pClient->m_DataChn.send(s.m_strIP, s.m_iDataPort, s.m_iSession, m_pOutputLoc, strlen(m_pOutputLoc) + 1);

   loadOperator(s);

   s.m_iStatus = 1;

   return 0;
}

int DCClient::segmentData()
{
   if (0 == m_iRows)
   {
      int seq = 0;
      for (int i = 0; i < m_pInput->m_iFileNum; ++ i)
      {
         if ((0 == m_pInput->m_vFiles[i].length()) || (0 == m_pInput->m_vSize[i]))
            continue;

         if (m_pInput->m_vLocation[i].empty())
            return -1;

         DS* ds = new DS;
         ds->m_iID = seq ++;
         ds->m_strDataFile = m_pInput->m_vFiles[i];
         ds->m_llOffset = 0;
         ds->m_llSize = m_pInput->m_vRecNum[i];
         ds->m_iSPEID = -1;
         ds->m_iStatus = 0;
         ds->m_pLoc = &m_pInput->m_vLocation[i];

         ds->m_pResult = new SphereResult;
         ds->m_pResult->m_iResID = ds->m_iID;
         ds->m_pResult->m_strOrigFile = ds->m_strDataFile;
         ds->m_pResult->m_llOrigStartRec = 0;
         ds->m_pResult->m_llOrigEndRec = -1;

         m_mpDS[ds->m_iID] = ds;
      }
   }
   else if (m_pInput->m_llRecNum != -1)
   {
      int64_t avg = m_pInput->m_llSize / m_iSPENum;
      int64_t unitsize;
      if (avg > m_iMaxUnitSize)
      {
         int n = m_pInput->m_llSize / m_iMaxUnitSize;
         if (m_pInput->m_llSize % m_iMaxUnitSize != 0)
            n ++;
         unitsize = m_pInput->m_llRecNum / n;
      }
      else if (avg < m_iMinUnitSize)
      {
         int n = m_pInput->m_llSize / m_iMinUnitSize;
         if (m_pInput->m_llSize % m_iMinUnitSize != 0)
            n ++;
         unitsize = m_pInput->m_llRecNum / n;
      }
      else
         unitsize = m_pInput->m_llRecNum / m_iSPENum;

      // at least 1 record per segement
      if (unitsize < 1)
         unitsize = 1;

      int seq = 0;
      for (int i = 0; i < m_pInput->m_iFileNum; ++ i)
      {
         int64_t off = 0;
         while (off < m_pInput->m_vRecNum[i])
         {
            if ((0 == m_pInput->m_vFiles[i].length()) || (0 == m_pInput->m_vSize[i]))
               continue;

            if (m_pInput->m_vLocation[i].empty())
               return -1;

            DS* ds = new DS;
            ds->m_iID = seq ++;
            ds->m_strDataFile = m_pInput->m_vFiles[i];
            ds->m_llOffset = off;
            ds->m_llSize = (m_pInput->m_vRecNum[i] - off > unitsize) ? unitsize : (m_pInput->m_vRecNum[i] - off);
            ds->m_iSPEID = -1;
            ds->m_iStatus = 0;
            ds->m_pLoc = &m_pInput->m_vLocation[i];

            ds->m_pResult = new SphereResult;
            ds->m_pResult->m_iResID = ds->m_iID;
            ds->m_pResult->m_strOrigFile = ds->m_strDataFile;
            ds->m_pResult->m_llOrigStartRec = ds->m_llOffset;
            ds->m_pResult->m_llOrigEndRec = ds->m_llSize;

            m_mpDS[ds->m_iID] = ds;

            off += ds->m_llSize;
         }
      }
   }
   else
   {
      cerr << "You have specified the number of records to be processed each time, but there is no record index found.\n";
      return -1;
   }

   return m_mpDS.size();
}

int DCClient::prepareOutput(const char* spenodes)
{
   m_pOutputLoc = NULL;
   m_pOutput->m_llSize = 0;
   m_pOutput->m_llRecNum = 0;

   // prepare output stream locations
   if (m_iOutputType > 0)
   {
      SectorMsg msg;
      msg.setType(204);
      msg.setKey(m_pClient->m_iKey);

      for (int i = 0; i < m_iSPENum; ++ i)
      {
         msg.setData(0, spenodes + i * 72, strlen(spenodes + i * 72) + 1);
         msg.setData(64, spenodes + i * 72 + 64, 4);
         msg.setData(68, (char*)&(m_pOutput->m_iFileNum), 4);
         msg.setData(72, (char*)&i, 4);
         int size = m_pOutput->m_strPath.length() + 1;
         int offset = 76;
         msg.setData(offset, (char*)&size, 4);
         msg.setData(offset + 4, m_pOutput->m_strPath.c_str(), m_pOutput->m_strPath.length() + 1);
         offset += 4 + size;
         size = m_pOutput->m_strName.length() + 1;
         msg.setData(offset, (char*)&size, 4);
         msg.setData(offset + 4, m_pOutput->m_strName.c_str(), m_pOutput->m_strName.length() + 1);
         offset += 4 + size;
         msg.setData(offset, (char*)&m_pClient->m_iKey, 4);
         offset += 4;
         msg.setData(offset, (char*)&m_iProcType, 4);
         if (m_iProcType == 1)
         {
            offset += 4;
            size = m_strOperator.length() + 1;
            msg.setData(offset, (char*)&size, 4);
            msg.setData(offset + 4, m_strOperator.c_str(), m_strOperator.length() + 1);
         }

         cout << "request shuffler " << spenodes + i * 72 << " " << *(int*)(spenodes + i * 72 + 64) << endl;
         Address serv;
         m_pClient->m_Routing.getPrimaryMaster(serv);
         if ((m_pClient->m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0) || (msg.getType() < 0))
            continue;

         BUCKET b;
         b.m_iID = i;
         b.m_strIP = spenodes + i * 72;
         b.m_iPort = *(int32_t*)(spenodes + i * 72 + 64);
         b.m_iDataPort = *(int32_t*)(spenodes + i * 72 + 68);
         b.m_iShufflerPort = *(int32_t*)msg.getData();
         b.m_iSession = *(int32_t*)(msg.getData() + 4);
         b.m_iProgress = 0;
         gettimeofday(&b.m_LastUpdateTime, 0);
         m_mBucket[b.m_iID] = b;

         // set up data connection, not for data transfter, but for keep-alive
         m_pClient->m_DataChn.connect(b.m_strIP, b.m_iDataPort);
      }

      if (m_mBucket.empty())
         return -1;

      m_pOutputLoc = new char[m_mBucket.size() * 80];
      int l = 0;
      for (map<int, BUCKET>::iterator b = m_mBucket.begin(); b != m_mBucket.end(); ++ b)
      {
         strcpy(m_pOutputLoc + l * 80, b->second.m_strIP.c_str());
         *(int32_t*)(m_pOutputLoc + l * 80 + 64) = b->second.m_iPort;
         *(int32_t*)(m_pOutputLoc + l * 80 + 68) = b->second.m_iDataPort;
         *(int32_t*)(m_pOutputLoc + l * 80 + 72) = b->second.m_iShufflerPort;
         *(int32_t*)(m_pOutputLoc + l * 80 + 76) = b->second.m_iSession;
         ++ l;
      }

      // result locations
      map<int, BUCKET>::iterator b = m_mBucket.begin();
      for (int i = 0; i < m_pOutput->m_iFileNum; ++ i)
      {
         char* tmp = new char[m_pOutput->m_strPath.length() + m_pOutput->m_strName.length() + 64];
         sprintf(tmp, "%s/%s.%d", m_pOutput->m_strPath.c_str(), m_pOutput->m_strName.c_str(), i);
         m_pOutput->m_vFiles[i] = tmp;
         delete [] tmp;

         if (m_pOutput->m_vLocation[i].empty())
         {
            // if user didn't specify output location, simply pick the next bucket location and rotate
            // this should be the normal case
            Address loc;
            loc.m_strIP = b->second.m_strIP;
            loc.m_iPort = b->second.m_iPort;
            m_pOutput->m_vLocation[i].insert(loc);
            m_pOutput->m_piLocID[i] = b->first;
         }
         else
         {
            // otherwise find if the user-sepcified location is available
            map<int, BUCKET>::iterator p = m_mBucket.begin();
            for (; p != m_mBucket.end(); ++ p)
            {
               if ((p->second.m_strIP == m_pOutput->m_vLocation[i].begin()->m_strIP) && (p->second.m_iPort == m_pOutput->m_vLocation[i].begin()->m_iPort))
                 break;
            }

            if (p == m_mBucket.end())
            {
               Address loc;
               loc.m_strIP = b->second.m_strIP;
               loc.m_iPort = b->second.m_iPort;
               m_pOutput->m_vLocation[i].insert(loc);
               m_pOutput->m_piLocID[i] = b->first;
            }
            else
            {
               Address loc;
               loc.m_strIP = p->second.m_strIP;
               loc.m_iPort = p->second.m_iPort;
               m_pOutput->m_vLocation[i].insert(loc);
               m_pOutput->m_piLocID[i] = p->first;
            }
         }

         if (++ b == m_mBucket.end())
            b = m_mBucket.begin();
      }
   }
   else if (m_iOutputType < 0)
   {
      char* localname = new char[m_pOutput->m_strPath.length() + m_pOutput->m_strName.length() + 64];
      sprintf(localname, "%s/%s", m_pOutput->m_strPath.c_str(), m_pOutput->m_strName.c_str());
      m_pOutputLoc = new char[strlen(localname) + 1];
      memcpy(m_pOutputLoc, localname, strlen(localname) + 1);
   }

   return m_pOutput->m_iFileNum;
}

int DCClient::readResult(SPE* s)
{
   if (m_iOutputType == 0)
   {
      m_pClient->m_DataChn.recv(s->m_strIP, s->m_iDataPort, s->m_iSession, s->m_pDS->m_pResult->m_pcData, s->m_pDS->m_pResult->m_iDataLen);
      char* tmp = NULL;
      m_pClient->m_DataChn.recv(s->m_strIP, s->m_iDataPort, s->m_iSession, tmp, s->m_pDS->m_pResult->m_iIndexLen);
      s->m_pDS->m_pResult->m_pllIndex = (int64_t*)tmp;
      s->m_pDS->m_pResult->m_iIndexLen /= 8;

      s->m_pDS->m_pResult->m_iStatus = s->m_pDS->m_pResult->m_iIndexLen;

      m_pOutput->m_llSize += s->m_pDS->m_pResult->m_iDataLen;
      m_pOutput->m_llRecNum += s->m_pDS->m_pResult->m_iIndexLen;
   }
   else if (m_iOutputType == -1)
   {
      int size = 0;
      m_pClient->m_DataChn.recv4(s->m_strIP, s->m_iDataPort, s->m_iSession, size);
      m_pOutput->m_vSize[s->m_pDS->m_iID] = size;
      m_pOutput->m_llSize += size;

      m_pClient->m_DataChn.recv4(s->m_strIP, s->m_iDataPort, s->m_iSession, size);
      m_pOutput->m_vRecNum[s->m_pDS->m_iID] = size - 1;
      m_pOutput->m_llRecNum += size -1;

      if (m_pOutput->m_iFileNum < 0)
         m_pOutput->m_iFileNum = 1;
      else
         m_pOutput->m_iFileNum ++;
   }
   else
   {
      char* sarray = NULL;
      char* rarray = NULL;
      int size;
      m_pClient->m_DataChn.recv(s->m_strIP, s->m_iDataPort, s->m_iSession, sarray, size);
      m_pClient->m_DataChn.recv(s->m_strIP, s->m_iDataPort, s->m_iSession, rarray, size);

      for (int i = 0; i < m_pOutput->m_iFileNum; ++ i)
      {
         m_pOutput->m_vSize[i] += *(int32_t*)(sarray + 4 * i);
         m_pOutput->m_vRecNum[i] += *(int32_t*)(rarray + 4 * i);
         m_pOutput->m_llSize += *(int32_t*)(sarray + 4 * i);;
         m_pOutput->m_llRecNum += *(int32_t*)(rarray + 4 * i);
      }

      delete [] sarray;
      delete [] rarray;
   }

   s->m_pDS->m_iStatus = 2;
   s->m_iStatus = 1;
   ++ m_iProgress;
   pthread_mutex_lock(&m_ResLock);
   ++ m_iAvailRes;
   pthread_cond_signal(&m_ResCond);
   pthread_mutex_unlock(&m_ResLock);

   return 0;
}
