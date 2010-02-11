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

#include <slave.h>
#include <sphere.h>
#include <dlfcn.h>
#include <iostream>
#include <algorithm>

using namespace std;

SPEResult::~SPEResult()
{
   for (vector<int64_t*>::iterator i = m_vIndex.begin(); i != m_vIndex.end(); ++ i)
      delete [] *i;
   for (vector<char*>::iterator i = m_vData.begin(); i != m_vData.end(); ++ i)
      delete [] *i;
}

void SPEResult::init(const int& n)
{
   if (n < 1)
     m_iBucketNum = 1;
   else
     m_iBucketNum = n;

   m_vIndex.resize(m_iBucketNum);
   m_vIndexLen.resize(m_iBucketNum);
   m_vIndexPhyLen.resize(m_iBucketNum);
   m_vData.resize(m_iBucketNum);
   m_vDataLen.resize(m_iBucketNum);
   m_vDataPhyLen.resize(m_iBucketNum);

   for (vector<int32_t>::iterator i = m_vIndexLen.begin(); i != m_vIndexLen.end(); ++ i)
      *i = 0;
   for (vector<int32_t>::iterator i = m_vDataLen.begin(); i != m_vDataLen.end(); ++ i)
      *i = 0;
   for (vector<int32_t>::iterator i = m_vIndexPhyLen.begin(); i != m_vIndexPhyLen.end(); ++ i)
      *i = 0;
   for (vector<int32_t>::iterator i = m_vDataPhyLen.begin(); i != m_vDataPhyLen.end(); ++ i)
      *i = 0;
   for (vector<int64_t*>::iterator i = m_vIndex.begin(); i != m_vIndex.end(); ++ i)
      *i = NULL;
   for (vector<char*>::iterator i = m_vData.begin(); i != m_vData.end(); ++ i)
      *i = NULL;

   m_llTotalDataSize = 0;
}

void SPEResult::addData(const int& bucketid, const char* data, const int64_t& len)
{
   if ((bucketid >= m_iBucketNum) || (bucketid < 0) || (len <= 0))
      return;

   // dynamically increase index buffer size
   if (m_vIndexLen[bucketid] >= m_vIndexPhyLen[bucketid])
   {
      int64_t* tmp = new int64_t[m_vIndexPhyLen[bucketid] + 256];

      if (NULL != m_vIndex[bucketid])
      {
         memcpy((char*)tmp, (char*)m_vIndex[bucketid], m_vIndexLen[bucketid] * 8);
         delete [] m_vIndex[bucketid];
      }
      else
      {
         tmp[0] = 0;
         m_vIndexLen[bucketid] = 1;
      }
      m_vIndex[bucketid] = tmp;
      m_vIndexPhyLen[bucketid] += 256;
   }

   m_vIndex[bucketid][m_vIndexLen[bucketid]] = m_vIndex[bucketid][m_vIndexLen[bucketid] - 1] + len;
   m_vIndexLen[bucketid] ++;

   // dynamically increase index buffer size
   while (m_vDataLen[bucketid] + len > m_vDataPhyLen[bucketid])
   {
      char* tmp = new char[m_vDataPhyLen[bucketid] + 65536];

      if (NULL != m_vData[bucketid])
      {
         memcpy((char*)tmp, (char*)m_vData[bucketid], m_vDataLen[bucketid]);
         delete [] m_vData[bucketid];
      }
      m_vData[bucketid] = tmp;
      m_vDataPhyLen[bucketid] += 65536;
   }

   memcpy(m_vData[bucketid] + m_vDataLen[bucketid], data, len);
   m_vDataLen[bucketid] += len;
   m_llTotalDataSize += len;
}

void SPEResult::clear()
{
   for (vector<int32_t>::iterator i = m_vIndexLen.begin(); i != m_vIndexLen.end(); ++ i)
   {
      if (*i > 0)
         *i = 1;
   }
   for (vector<int32_t>::iterator i = m_vDataLen.begin(); i != m_vDataLen.end(); ++ i)
      *i = 0;

   m_llTotalDataSize = 0;
}

SPEDestination::SPEDestination():
m_piSArray(NULL),
m_piRArray(NULL),
m_iLocNum(0),
m_pcOutputLoc(NULL),
m_piLocID(NULL)
{
}

SPEDestination::~SPEDestination()
{
   delete [] m_piSArray;
   delete [] m_piRArray;
   delete [] m_pcOutputLoc;
   delete [] m_piLocID;
}

void SPEDestination::init(const int& buckets)
{
   if (buckets > 0)
   {
      m_piSArray = new int[buckets];
      m_piRArray = new int[buckets];
      for (int i = 0; i < buckets; ++ i)
         m_piSArray[i] = m_piRArray[i] = 0;
   }
   else
   {
      m_piSArray = new int[1];
      m_piRArray = new int[1];
      m_piSArray[0] = m_piRArray[0] = 0;
   }
}

void SPEDestination::reset(const int& buckets)
{
   if (buckets > 0)
   {
      for (int i = 0; i < buckets; ++ i)
         m_piSArray[i] = m_piRArray[i] = 0;
   }
   else
      m_piSArray[0] = m_piRArray[0] = 0;
}

void* Slave::SPEHandler(void* p)
{
   Slave* self = ((Param4*)p)->serv_instance;
   const string ip = ((Param4*)p)->client_ip;
   const int ctrlport = ((Param4*)p)->client_ctrl_port;
   const int dataport = ((Param4*)p)->client_data_port;
   const int speid = ((Param4*)p)->speid;
   const int transid = ((Param4*)p)->transid;
   const int key = ((Param4*)p)->key;
   const string function = ((Param4*)p)->function;
   const int rows = ((Param4*)p)->rows;
   const char* param = ((Param4*)p)->param;
   const int psize = ((Param4*)p)->psize;
   const int type = ((Param4*)p)->type;
   const string master_ip = ((Param4*)p)->master_ip;
   const int master_port = ((Param4*)p)->master_port;
   delete (Param4*)p;

   SectorMsg msg;

   cout << "rendezvous connect " << ip << " " << dataport << endl;
   if (self->m_DataChn.connect(ip, dataport) < 0)
   {
      self->logError(2, ip, ctrlport, function);
      return NULL;
   }
   cout << "connected\n";

   // read outupt parameters
   int buckets;
   if (self->m_DataChn.recv4(ip, dataport, transid, buckets) < 0)
      return NULL;

   SPEDestination dest;
   if (buckets > 0)
   {
      if (self->m_DataChn.recv4(ip, dataport, transid, dest.m_iLocNum) < 0)
         return NULL;
      int len = dest.m_iLocNum * 80;
      if (self->m_DataChn.recv(ip, dataport, transid, dest.m_pcOutputLoc, len) < 0)
         return NULL;
      len = buckets * 4;
      if (self->m_DataChn.recv(ip, dataport, transid, (char*&)dest.m_piLocID, len) < 0)
         return NULL;
   }
   else if (buckets < 0)
   {
      int32_t len;
      if (self->m_DataChn.recv(ip, dataport, transid, dest.m_pcOutputLoc, len) < 0)
         return NULL;
      dest.m_strLocalFile = dest.m_pcOutputLoc;
   }
   dest.init(buckets);


   // initialize processing function
   self->acceptLibrary(key, ip, dataport, transid);
   SPHERE_PROCESS process = NULL;
   MR_MAP map = NULL;
   MR_PARTITION partition = NULL;
   void* lh = NULL;
   self->openLibrary(key, function, lh);
   if (NULL == lh)
   {
      self->logError(3, ip, ctrlport, function);
      return NULL;
   }

   if (type == 0)
      self->getSphereFunc(lh, function, process);
   else if (type == 1)
      self->getMapFunc(lh, function, map, partition);
   else
      return NULL;


   timeval t1, t2, t3, t4;
   gettimeofday(&t1, 0);

   msg.setType(1); // success, return result
   msg.setData(0, (char*)&(speid), 4);

   SPEResult result;
   result.init(buckets);

   // processing...
   while (true)
   {
      char* dataseg = NULL;
      int size = 0;
      if (self->m_DataChn.recv(ip, dataport, transid, dataseg, size) < 0)
         break;

      // read data segment parameters
      int64_t offset = *(int64_t*)(dataseg);
      int64_t totalrows = *(int64_t*)(dataseg + 8);
      int32_t dsid = *(int32_t*)(dataseg + 16);
      string datafile = dataseg + 20;
      sprintf(dest.m_pcLocalFileID, ".%d", dsid);
      delete [] dataseg;
      cout << "new job " << datafile << " " << offset << " " << totalrows << endl;

      int64_t* index = NULL;
      if ((totalrows > 0) && (rows != 0))
         index = new int64_t[totalrows + 1];
      char* block = NULL;
      int unitrows = (rows != -1) ? rows : totalrows;
      int progress = 0;

      // read data
      if (0 != rows)
      {
         size = 0;
         if (self->SPEReadData(datafile, offset, size, index, totalrows, block) <= 0)
         {
            delete [] index;
            delete [] block;

            progress = -1;
            msg.setData(4, (char*)&progress, 4);
            msg.m_iDataLength = SectorMsg::m_iHdrSize + 8;
            int id = 0;
            self->m_GMP.sendto(ip.c_str(), ctrlport, id, &msg);

            continue;
         }
      }
      else
      {
         // store file name in "process" parameter
         block = new char[datafile.length() + 1];
         strcpy(block, datafile.c_str());
         size = datafile.length() + 1;
         totalrows = 0;
      }

      SInput input;
      input.m_pcUnit = NULL;
      input.m_pcParam = (char*)param;
      input.m_iPSize = psize;
      SOutput output;
      output.m_iBufSize = (size < 64000000) ? 64000000 : size;
      output.m_pcResult = new char[output.m_iBufSize];
      output.m_iIndSize = (totalrows < 640000) ? 640000 : totalrows + 2;
      output.m_pllIndex = new int64_t[output.m_iIndSize];
      output.m_piBucketID = new int[output.m_iIndSize];
      SFile file;
      file.m_strHomeDir = self->m_strHomeDir;
      char path[64];
      sprintf(path, "%d", key);
      file.m_strLibDir = self->m_strHomeDir + ".sphere/" + path + "/";
      file.m_strTempDir = self->m_strHomeDir + ".tmp/";
      file.m_pInMemoryObjects = &self->m_InMemoryObjects;

      result.clear();
      gettimeofday(&t3, 0);

      int deliverystatus = 0;
      int processstatus = 0;

      // process data segments
      for (int i = 0; i < totalrows; i += unitrows)
      {
         if (unitrows > totalrows - i)
            unitrows = totalrows - i;

         input.m_pcUnit = block + index[i] - index[0];
         input.m_iRows = unitrows;
         input.m_pllIndex = index + i;
         output.m_iResSize = 0;
         output.m_iRows = 0;
         output.m_strError = "";

         processstatus = self->processData(input, output, file, result, buckets, process, map, partition);
         if (processstatus < 0)
         {
            progress = -1;
            break;
         }

         timeval t;
         gettimeofday(&t, NULL);
         unsigned int seed = t.tv_sec * 1000000 + t.tv_usec;
         int ds_thresh = 32000000 * ((rand_r(&seed) % 7) + 1);
         if ((result.m_llTotalDataSize >= ds_thresh) && (buckets != 0))
            deliverystatus = self->deliverResult(buckets, speid, result, dest);

         if (deliverystatus < 0)
         {
            progress = -1;
            break;
         }

         gettimeofday(&t4, 0);
         if (t4.tv_sec - t3.tv_sec > 1)
         {
            progress = i * 100 / totalrows;
            msg.setData(4, (char*)&progress, 4);
            msg.m_iDataLength = SectorMsg::m_iHdrSize + 8;
            int id = 0;
            self->m_GMP.sendto(ip.c_str(), ctrlport, id, &msg);

            t3 = t4;
         }
      }

      // process files
      if (0 == unitrows)
      {
         struct stat64 s;
         stat64((self->m_strHomeDir + datafile).c_str(), &s);
         int64_t filesize = s.st_size;

         input.m_pcUnit = block;
         input.m_iRows = -1;
         input.m_pllIndex = NULL;
         output.m_iResSize = 0;
         output.m_iRows = 0;
         output.m_strError = "";
         output.m_llOffset = 0;

         for (int i = 0; (i == 0) || (output.m_llOffset > 0); ++ i)
         {
            processstatus = self->processData(input, output, file, result, buckets, process, map, partition);
            if (processstatus < 0)
            {
               progress = -1;
               break;
            }

            timeval t;
            gettimeofday(&t, NULL);
            unsigned int seed = t.tv_sec * 1000000 + t.tv_usec;
            int ds_thresh = 32000000 * ((rand_r(&seed) % 7) + 1);
            if ((result.m_llTotalDataSize >= ds_thresh) && (buckets != 0))
               deliverystatus = self->deliverResult(buckets, speid, result, dest);

            if (deliverystatus < 0)
            {
               progress = -1;
               break;
            }

            if (output.m_llOffset > 0)
            {
               progress = output.m_llOffset * 100LL / filesize;
               msg.setData(4, (char*)&progress, 4);
               msg.m_iDataLength = SectorMsg::m_iHdrSize + 8;
               int id = 0;
               self->m_GMP.sendto(ip.c_str(), ctrlport, id, &msg);
            }
         }
      }

      // if buckets = 0, send back to clients, otherwise deliver to local or network locations
      if ((buckets != 0) && (progress >= 0))
         deliverystatus = self->deliverResult(buckets, speid, result, dest);

      if (deliverystatus < 0)
         progress = -1;
      else
         progress = 100;

      cout << "completed " << progress << " " << ip << " " << ctrlport << endl;

      msg.setData(4, (char*)&progress, 4);

      if (100 == progress)
      {
         msg.m_iDataLength = SectorMsg::m_iHdrSize + 8;
         int id = 0;
         self->m_GMP.sendto(ip.c_str(), ctrlport, id, &msg);

         cout << "sending data back... " << buckets << endl;
         self->sendResultToClient(buckets, dest.m_piSArray, dest.m_piRArray, result, ip, dataport, transid);
         dest.reset(buckets);

         // report new files
         vector<string> filelist;
         for (set<string>::iterator i = file.m_sstrFiles.begin(); i != file.m_sstrFiles.end(); ++ i)
            filelist.push_back(*i);
         self->report(master_ip, master_port, transid, filelist, true);
         self->reportMO(master_ip, master_port, transid);
      }
      else
      {
         if (processstatus > 0)
            processstatus = -1001; // data transfer error
         msg.setData(8, (char*)&processstatus, 4);
         msg.m_iDataLength = SectorMsg::m_iHdrSize + 12;
         if (output.m_strError.length() > 0)
            msg.setData(12, output.m_strError.c_str(), output.m_strError.length() + 1);
         else if (deliverystatus < 0)
         {
            string tmp = "System Error: data transfer to buckets failed.";
            msg.setData(12, tmp.c_str(), tmp.length() + 1);
         }

         int id = 0;
         self->m_GMP.sendto(ip.c_str(), ctrlport, id, &msg);
      }

      delete [] index;
      delete [] block;
      delete [] output.m_pcResult;
      delete [] output.m_pllIndex;
      delete [] output.m_piBucketID;
      index = NULL;
      block = NULL;
   }

   gettimeofday(&t2, 0);
   int duration = t2.tv_sec - t1.tv_sec;

   self->closeLibrary(lh);
   self->m_DataChn.remove(ip, dataport);

   cout << "comp server closed " << ip << " " << ctrlport << " " << duration << endl;

   delete [] param;

   multimap<int64_t, Address> sndspd;
   for (int i = 0; i < dest.m_iLocNum; ++ i)
   {
      Address addr;
      addr.m_strIP = dest.m_pcOutputLoc + i * 80;
      addr.m_iPort = *(int32_t*)(dest.m_pcOutputLoc + i * 80 + 64);
      int dataport = *(int32_t*)(dest.m_pcOutputLoc + i * 80 + 68);
      int64_t spd = self->m_DataChn.getRealSndSpeed(addr.m_strIP, dataport);
      if (spd > 0)
         sndspd.insert(pair<int64_t, Address>(spd, addr));
   }
   vector<Address> bad;
   self->checkBadDest(sndspd, bad);

   self->reportSphere(master_ip, master_port, transid, &bad);

   return NULL;
}

void* Slave::SPEShuffler(void* p)
{
   Slave* self = ((Param5*)p)->serv_instance;
   string client_ip = ((Param5*)p)->client_ip;
   int client_port = ((Param5*)p)->client_ctrl_port;
   int client_data_port = ((Param5*)p)->client_data_port;
   string path = ((Param5*)p)->path;
   string localfile = ((Param5*)p)->filename;
   int bucketnum = ((Param5*)p)->bucketnum;
   CGMP* gmp = ((Param5*)p)->gmp;
   string function = ((Param5*)p)->function;

   //set up data connection, for keep-alive purpose
   if (self->m_DataChn.connect(client_ip, client_data_port) < 0)
      return NULL;

   queue<Bucket>* bq = new queue<Bucket>;
   pthread_mutex_t* bqlock = new pthread_mutex_t;
   pthread_mutex_init(bqlock, NULL);
   pthread_cond_t* bqcond = new pthread_cond_t;
   pthread_cond_init(bqcond, NULL);
   int64_t* pendingSize = new int64_t;
   *pendingSize = 0;

   ((Param5*)p)->bq = bq;
   ((Param5*)p)->bqlock = bqlock;
   ((Param5*)p)->bqcond = bqcond;
   ((Param5*)p)->pending = pendingSize;

   pthread_t ex;
   pthread_create(&ex, NULL, SPEShufflerEx, p);
   pthread_detach(ex);

   cout << "SPE Shuffler " << path << " " << localfile << " " << bucketnum << endl;

   while (true)
   {
      string speip;
      int speport;
      SectorMsg msg;
      int msgid;
      int r = gmp->recvfrom(speip, speport, msgid, &msg, false);

      // client releases the task or client has already been shutdown
      if (((r > 0) && (speip == client_ip) && (speport == client_port))
         || ((r < 0) && (!self->m_DataChn.isConnected(client_ip, client_data_port))))
      {
         Bucket b;
         b.totalnum = -1;
         b.totalsize = 0;
         pthread_mutex_lock(bqlock);
         bq->push(b);
         pthread_cond_signal(bqcond);
         pthread_mutex_unlock(bqlock);

         break;
      }

      if (r < 0)
         continue;

      if (*pendingSize > 256000000)
      {
         // too many incoming results, ask the sender to wait
         // the receiver buffer size threshold is set to 256MB. This prevents the shuffler from being overflowed
         // it also helps direct the traffic to less congested shuffler and leads to better load balance
         msg.setType(-msg.getType());
         gmp->sendto(speip, speport, msgid, &msg);
      }
      else
      {
         Bucket b;
         b.totalnum = *(int32_t*)(msg.getData() + 8);;
         b.totalsize = *(int32_t*)(msg.getData() + 12);
         b.src_ip = speip;
         b.src_dataport = *(int32_t*)msg.getData();
         b.session = *(int32_t*)(msg.getData() + 4);

         gmp->sendto(speip, speport, msgid, &msg);

         if (!self->m_DataChn.isConnected(speip, b.src_dataport))
            self->m_DataChn.connect(speip, b.src_dataport);

         pthread_mutex_lock(bqlock);
         bq->push(b);
         *pendingSize += b.totalsize;
         pthread_cond_signal(bqcond);
         pthread_mutex_unlock(bqlock);
      }
   }

   gmp->close();
   delete gmp;

   return NULL;
}

void* Slave::SPEShufflerEx(void* p)
{
   Slave* self = ((Param5*)p)->serv_instance;
   int transid = ((Param5*)p)->transid;
   string client_ip = ((Param5*)p)->client_ip;
   int client_port = ((Param5*)p)->client_ctrl_port;
   int client_data_port = ((Param5*)p)->client_data_port;
   string path = ((Param5*)p)->path;
   string localfile = ((Param5*)p)->filename;
   int bucketnum = ((Param5*)p)->bucketnum;
   int bucketid = ((Param5*)p)->bucketid;
   const int key = ((Param5*)p)->key;
   const int type = ((Param5*)p)->type;
   string function = ((Param5*)p)->function;
   queue<Bucket>* bq = ((Param5*)p)->bq;
   pthread_mutex_t* bqlock = ((Param5*)p)->bqlock;
   pthread_cond_t* bqcond = ((Param5*)p)->bqcond;
   int64_t* pendingSize = ((Param5*)p)->pending;
   string master_ip = ((Param5*)p)->master_ip;
   int master_port = ((Param5*)p)->master_port;
   delete (Param5*)p;

   self->createDir(path);

   // remove old result data files
   for (int i = 0; i < bucketnum; ++ i)
   {
      char* tmp = new char[self->m_strHomeDir.length() + path.length() + localfile.length() + 64];
      sprintf(tmp, "%s.%d", (self->m_strHomeDir + path + "/" + localfile).c_str(), i);
      unlink(tmp);
      sprintf(tmp, "%s.%d.idx", (self->m_strHomeDir + path + "/" + localfile).c_str(), i);
      unlink(tmp);
      delete [] tmp;
   }

   // index file initial offset
   vector<int64_t> offset;
   offset.resize(bucketnum);
   for (vector<int64_t>::iterator i = offset.begin(); i != offset.end(); ++ i)
      *i = 0;
   set<int> fileid;

   while (true)
   {
      pthread_mutex_lock(bqlock);
      while (bq->empty())
         pthread_cond_wait(bqcond, bqlock);
      Bucket b = bq->front();
      bq->pop();
      *pendingSize -= b.totalsize;
      pthread_mutex_unlock(bqlock);

      if (b.totalnum == -1)
         break;

      string speip = b.src_ip;
      int dataport = b.src_dataport;
      int session = b.session;

      for (int i = 0; i < b.totalnum; ++ i)
      {
         int bucket = 0;
         if (self->m_DataChn.recv4(speip, dataport, session, bucket) < 0)
            continue;

         fileid.insert(bucket);

         char* tmp = new char[self->m_strHomeDir.length() + path.length() + localfile.length() + 64];
         sprintf(tmp, "%s.%d", (self->m_strHomeDir + path + "/" + localfile).c_str(), bucket);
         fstream datafile(tmp, ios::out | ios::binary | ios::app);
         sprintf(tmp, "%s.%d.idx", (self->m_strHomeDir + path + "/" + localfile).c_str(), bucket);
         fstream indexfile(tmp, ios::out | ios::binary | ios::app);
         delete [] tmp;
         int64_t start = offset[bucket];
         if (0 == start)
            indexfile.write((char*)&start, 8);

         int32_t len;
         char* data = NULL;
         if (self->m_DataChn.recv(speip, dataport, session, data, len) < 0)
            continue;
         datafile.write(data, len);
         delete [] data;

         tmp = NULL;
         if (self->m_DataChn.recv(speip, dataport, session, tmp, len) < 0)
            continue;
         int64_t* index = (int64_t*)tmp;
         for (int j = 0; j < len / 8; ++ j)
            index[j] += start;
         offset[bucket] = index[len / 8 - 1];
         indexfile.write(tmp, len);
         delete [] tmp;

         datafile.close();
         indexfile.close();
      }

      // update total received data
      self->m_SlaveStat.updateIO(speip, b.totalsize, 0);
   }

   pthread_mutex_destroy(bqlock);
   pthread_cond_destroy(bqcond);
   delete bqlock;
   delete bqcond;
   delete pendingSize;

   // sort and reduce
   if (type == 1)
   {
      void* lh = NULL;
      self->openLibrary(key, function, lh);
      //if (NULL == lh)
      //   break;

      MR_COMPARE comp = NULL;
      MR_REDUCE reduce = NULL;
      self->getReduceFunc(lh, function, comp, reduce);

      if (NULL != comp)
      {
         char* tmp = new char[self->m_strHomeDir.length() + path.length() + localfile.length() + 64];
         for (set<int>::iterator i = fileid.begin(); i != fileid.end(); ++ i)
         {
            sprintf(tmp, "%s.%d", (self->m_strHomeDir + path + "/" + localfile).c_str(), *i);
            self->sort(tmp, comp, reduce);
         }
         delete [] tmp;
      }

      self->closeLibrary(lh);
   }


   // report sphere output files
   char* tmp = new char[path.length() + localfile.length() + 64];
   vector<string> filelist;
   for (set<int>::iterator i = fileid.begin(); i != fileid.end(); ++ i)
   {
      sprintf(tmp, "%s.%d", (path + "/" + localfile).c_str(), *i);
      filelist.push_back(tmp);
      sprintf(tmp, "%s.%d.idx", (path + "/" + localfile).c_str(), *i);
      filelist.push_back(tmp);
   }
   delete [] tmp;

   self->report(master_ip, master_port, transid, filelist, 1);

   self->reportSphere(master_ip, master_port, transid);

   cout << "bucket completed 100 " << client_ip << " " << client_port << endl;
   SectorMsg msg;
   msg.setType(1); // success, return result
   msg.setData(0, (char*)&(bucketid), 4);
   int progress = 100;
   msg.setData(4, (char*)&progress, 4);
   msg.m_iDataLength = SectorMsg::m_iHdrSize + 8;
   int id = 0;
   self->m_GMP.sendto(client_ip.c_str(), client_port, id, &msg);

   //remove this client data channel
   self->m_DataChn.remove(client_ip, client_data_port);

   return NULL;
}

int Slave::SPEReadData(const string& datafile, const int64_t& offset, int& size, int64_t* index, const int64_t& totalrows, char*& block)
{
   SNode sn;
   string idxfile = datafile + ".idx";

   //read index
   if (m_pLocalFile->lookup(idxfile.c_str(), sn) >= 0)
   {
      fstream idx;
      idx.open((m_strHomeDir + idxfile).c_str(), ios::in | ios::binary);
      if (idx.bad() || idx.fail())
         return -1;
      idx.seekg(offset * 8);
      idx.read((char*)index, (totalrows + 1) * 8);
      idx.close();
   }
   else
   {
      SectorMsg msg;
      msg.setType(110); // open the index file
      msg.setKey(0);

      int32_t mode = 1;
      msg.setData(0, (char*)&mode, 4);
      int32_t port = m_DataChn.getPort();
      msg.setData(4, (char*)&port, 4);
      msg.setData(8, "\0", 1);
      msg.setData(72, idxfile.c_str(), idxfile.length() + 1);

      Address addr;
      m_Routing.lookup(idxfile, addr);

      if (m_GMP.rpc(addr.m_strIP.c_str(), addr.m_iPort, &msg, &msg) < 0)
         return -1;
      if (msg.getType() < 0)
         return -1;

      string srcip = msg.getData();
      int srcport = *(int*)(msg.getData() + 64);
      int session = *(int*)(msg.getData() + 68);

      cout << "rendezvous connect " << srcip << " " << srcport << endl;
      if (m_DataChn.connect(srcip, srcport) < 0)
         return -1;

      int32_t cmd = 1;
      m_DataChn.send(srcip, srcport, session, (char*)&cmd, 4);

      int response = -1;
      if (m_DataChn.recv4(srcip, srcport, session, response) < 0)
         return -1;

      char req[16];
      *(int64_t*)req = offset * 8;
      *(int64_t*)(req + 8) = (totalrows + 1) * 8;

      if (m_DataChn.send(srcip, srcport, session, req, 16) < 0)
         return -1;

      char* tmp = NULL;
      int size = (totalrows + 1) * 8;
      if (m_DataChn.recv(srcip, srcport, session, tmp, size) < 0)
         return -1;
      if (size > 0)
         memcpy((char*)index, tmp, size);
      delete [] tmp;

      // file close command: 5
      cmd = 5;
      m_DataChn.send(srcip, srcport, session, (char*)&cmd, 4);
      m_DataChn.recv4(srcip, srcport, session, response);

      // update total received data
      m_SlaveStat.updateIO(srcip, (totalrows + 1) * 8, 0);
   }

   size = index[totalrows] - index[0];
   block = new char[size];

   // read data file
   if (m_pLocalFile->lookup(datafile.c_str(), sn) >= 0)
   {
      fstream ifs;
      ifs.open((m_strHomeDir + datafile).c_str(), ios::in | ios::binary);
      if (ifs.bad() || ifs.fail())
         return -1;
      ifs.seekg(index[0]);
      ifs.read(block, size);
      ifs.close();
   }
   else
   {
      SectorMsg msg;
      msg.setType(110); // open the index file
      msg.setKey(0);

      int32_t mode = 1;
      msg.setData(0, (char*)&mode, 4);
      int32_t port = m_DataChn.getPort();
      msg.setData(4, (char*)&port, 4);
      msg.setData(8, "\0", 1);
      msg.setData(72, datafile.c_str(), datafile.length() + 1);

      Address addr;
      m_Routing.lookup(datafile, addr);

      if (m_GMP.rpc(addr.m_strIP.c_str(), addr.m_iPort, &msg, &msg) < 0)
         return -1;
      if (msg.getType() < 0)
         return -1;

      string srcip = msg.getData();
      int srcport = *(int*)(msg.getData() + 64);
      int session = *(int*)(msg.getData() + 68);

      cout << "rendezvous connect " << srcip << " " << srcport << endl;
      if (m_DataChn.connect(srcip, srcport) < 0)
         return -1;

      int32_t cmd = 1;
      m_DataChn.send(srcip, srcport, session, (char*)&cmd, 4);

      int response = -1;
      if (m_DataChn.recv4(srcip, srcport, session, response) < 0)
         return -1;

      char req[16];
      *(int64_t*)req = index[0];
      *(int64_t*)(req + 8) = index[totalrows] - index[0];

      if (m_DataChn.send(srcip, srcport, session, req, 16) < 0)
         return -1;

      char* tmp = NULL;
      int size = index[totalrows] - index[0];
      if (m_DataChn.recv(srcip, srcport, session, tmp, size) < 0)
         return -1;
      if (size > 0)
         memcpy(block, tmp, size);
      delete [] tmp;

      // file close command: 5
      cmd = 5;
      m_DataChn.send(srcip, srcport, session, (char*)&cmd, 4);
      m_DataChn.recv4(srcip, srcport, session, response);

      // update total received data
      m_SlaveStat.updateIO(srcip, index[totalrows] - index[0], 0);
   }

   return totalrows;
}

int Slave::sendResultToFile(const SPEResult& result, const string& localfile, const int64_t& offset)
{
   fstream datafile, idxfile;
   datafile.open((m_strHomeDir + localfile).c_str(), ios::out | ios::binary | ios::app);
   idxfile.open((m_strHomeDir + localfile + ".idx").c_str(), ios::out | ios::binary | ios::app);

   datafile.write(result.m_vData[0], result.m_vDataLen[0]);

   if (offset == 0)
      idxfile.write((char*)&offset, 8);
   else
   {
      for (int i = 1; i <= result.m_vIndexLen[0]; ++ i)
         result.m_vIndex[0][i] += offset;
   }
   idxfile.write((char*)(result.m_vIndex[0] + 1), (result.m_vIndexLen[0] - 1) * 8);

   datafile.close();
   idxfile.close();

   return 0;
}

int Slave::sendResultToClient(const int& buckets, const int* sarray, const int* rarray, const SPEResult& result, const string& clientip, int clientport, int session)
{
   if (buckets == -1)
   {
      // send back result file/record size
      m_DataChn.send(clientip, clientport, session, (char*)sarray, 4);
      m_DataChn.send(clientip, clientport, session, (char*)rarray, 4);
   }
   else if (buckets == 0)
   {
      // send back the result data
      m_DataChn.send(clientip, clientport, session, result.m_vData[0], result.m_vDataLen[0]);
      m_DataChn.send(clientip, clientport, session, (char*)result.m_vIndex[0], result.m_vIndexLen[0] * 8);
   }
   else
   {
      // send back size and rec_num information
      m_DataChn.send(clientip, clientport, session, (char*)sarray, buckets * 4);
      m_DataChn.send(clientip, clientport, session, (char*)rarray, buckets * 4);
   }

   return 0;
}

int Slave::sendResultToBuckets(const int& speid, const int& buckets, const SPEResult& result, const SPEDestination& dest)
{
   map<int, set<int> > ResByLoc;
   map<int, int> SizeByLoc;

   for (int i = 0; i < dest.m_iLocNum; ++ i)
   {
      set<int> tmp;
      ResByLoc[i] = tmp;
      SizeByLoc[i] = 0;
   }

   for (int r = 0; r < buckets; ++ r)
   {
      int i = dest.m_piLocID[r];
      if (0 != result.m_vDataLen[r])
      {
         ResByLoc[i].insert(r);
         SizeByLoc[i] += result.m_vDataLen[r] + (result.m_vIndexLen[r] - 1) * 8;
      }
   }

   unsigned int tn = 0;
   map<int, set<int> >::iterator p = ResByLoc.begin();

   while(!ResByLoc.empty())
   {
      int i = p->first;
      if (++ p == ResByLoc.end())
         p = ResByLoc.begin();

      char* dstip = dest.m_pcOutputLoc + i * 80;
      int32_t dstport = *(int32_t*)(dest.m_pcOutputLoc + i * 80 + 68);
      int32_t shufflerport = *(int32_t*)(dest.m_pcOutputLoc + i * 80 + 72);
      int32_t session = *(int32_t*)(dest.m_pcOutputLoc + i * 80 + 76);

      SectorMsg msg;
      int32_t srcport = m_DataChn.getPort();
      msg.setData(0, (char*)&srcport, 4);
      msg.setData(4, (char*)&session, 4);
      int totalnum = ResByLoc[i].size();
      msg.setData(8, (char*)&totalnum, 4);
      int totalsize = SizeByLoc[i];
      msg.setData(12, (char*)&totalsize, 4);
      msg.m_iDataLength = SectorMsg::m_iHdrSize + 16;

      msg.setType(1);
      if (m_GMP.rpc(dstip, shufflerport, &msg, &msg) < 0)
         return -1;

      if (msg.getType() < 0)
      {
         // if all shufflers are busy, wait here a little while
         if (tn ++ > ResByLoc.size())
         {
            tn = 0;
            usleep(100000);
         }
         continue;
      }

      if (!m_DataChn.isConnected(dstip, dstport))
      {
         if (m_DataChn.connect(dstip, dstport) < 0)
            return -1;
      }

      for (set<int>::iterator r = ResByLoc[i].begin(); r != ResByLoc[i].end(); ++ r)
      {
         int32_t id = *r;
         m_DataChn.send(dstip, dstport, session, (char*)&id, 4);
         m_DataChn.send(dstip, dstport, session, result.m_vData[id], result.m_vDataLen[id]);
         m_DataChn.send(dstip, dstport, session, (char*)(result.m_vIndex[id] + 1), (result.m_vIndexLen[id] - 1) * 8);
      }

      // update total sent data
      m_SlaveStat.updateIO(dstip, SizeByLoc[i], 1);

      ResByLoc.erase(i);
      SizeByLoc.erase(i);
   }

   return 1;
}

int Slave::acceptLibrary(const int& key, const string& ip, int port, int session)
{
   int32_t num = -1;
   m_DataChn.recv4(ip, port, session, num);

   for(int i = 0; i < num; ++ i)
   {
      char* lib = NULL;
      int size = 0;
      m_DataChn.recv(ip, port, session, lib, size);
      char* buf = NULL;
      m_DataChn.recv(ip, port, session, buf, size);

      char* path = new char[m_strHomeDir.length() + 64];
      sprintf(path, "%s/.sphere/%d", m_strHomeDir.c_str(), key);

      struct stat s;
      if (stat((string(path) + "/" + lib).c_str(), &s) < 0)
      {
         ::mkdir(path, S_IRWXU);

         fstream ofs((string(path) + "/" + lib).c_str(), ios::out | ios::trunc | ios::binary);
         ofs.write(buf, size);
         ofs.close();

         system((string("chmod +x ") + reviseSysCmdPath(path) + "/" + reviseSysCmdPath(lib)).c_str());
      }

      delete [] lib;
      delete [] buf;
      delete [] path;
   }

   return 0;
}

int Slave::openLibrary(const int& key, const string& lib, void*& lh)
{
   char path[64];
   sprintf(path, "%d", key);
   lh = dlopen((m_strHomeDir + ".sphere/" + path + "/" + lib + ".so").c_str(), RTLD_LAZY);
   if (NULL == lh)
   {
      // if no user uploaded lib, check permanent lib
      lh = dlopen((m_strHomeDir + ".sphere/perm/" + lib + ".so").c_str(), RTLD_LAZY);

      if (NULL == lh)
      {
         cerr << dlerror() << endl;
         return -1;
      }
   }

   return 0;
}

int Slave::getSphereFunc(void* lh, const string& function, SPHERE_PROCESS& process)
{
   process = (SPHERE_PROCESS)dlsym(lh, function.c_str());
   if (NULL == process)
   {
      cerr << dlerror() << endl;
      return -1;
   }

   return 0;
}

int Slave::getMapFunc(void* lh, const string& function, MR_MAP& map, MR_PARTITION& partition)
{
   map = (MR_MAP)dlsym(lh, (function + "_map").c_str());
   if (NULL == map)
      cerr << dlerror() << endl;

   partition = (MR_PARTITION)dlsym(lh, (function + "_partition").c_str());
   if (NULL == partition)
   {
      cerr << dlerror() << endl;
      return -1;
   }

   return 0;
}

int Slave::getReduceFunc(void* lh, const string& function, MR_COMPARE& compare, MR_REDUCE& reduce)
{
   reduce = (MR_REDUCE)dlsym(lh, (function + "_reduce").c_str());
   if (NULL == reduce)
      cerr << dlerror() << endl;

   compare = (MR_COMPARE)dlsym(lh, (function + "_compare").c_str());
   if (NULL == compare)
   {
      cerr << dlerror() << endl;
      return -1;
   }

   return 0;

}

int Slave::closeLibrary(void* lh)
{
   return dlclose(lh);
}

int Slave::sort(const string& bucket, MR_COMPARE comp, MR_REDUCE red)
{
   fstream ifs(bucket.c_str(), ios::in | ios::binary);
   if (ifs.fail())
      return -1;

   ifs.seekg(0, ios::end);
   int size = ifs.tellg();
   ifs.seekg(0, ios::beg);
   char* rec = new char[size];
   ifs.read(rec, size);
   ifs.close();

   ifs.open((bucket + ".idx").c_str());
   if (ifs.fail())
   {
      delete [] rec;
      return -1;
   }

   ifs.seekg(0, ios::end);
   size = ifs.tellg();
   ifs.seekg(0, ios::beg);
   int64_t* idx = new int64_t[size / 8];
   ifs.read((char*)idx, size);
   ifs.close();

   size = size / 8 - 1;

   vector<MRRecord> vr;
   vr.resize(size);
   int64_t offset = 0;
   for (vector<MRRecord>::iterator i = vr.begin(); i != vr.end(); ++ i)
   {
      i->m_pcData = rec + idx[offset];
      i->m_iSize = idx[offset + 1] - idx[offset];
      i->m_pCompRoutine = comp;
      offset ++;
   }

   std::sort(vr.begin(), vr.end(), ltrec());

   if (red != NULL)
      reduce(vr, bucket, red, NULL, 0);

   fstream sorted((bucket + ".sorted").c_str(), ios::out | ios::binary | ios::trunc);
   fstream sortedidx((bucket + ".sorted.idx").c_str(), ios::out | ios::binary | ios::trunc);
   offset = 0;
   sortedidx.write((char*)&offset, 8);
   for (vector<MRRecord>::iterator i = vr.begin(); i != vr.end(); ++ i)
   {
      sorted.write(i->m_pcData, i->m_iSize);
      offset += i->m_iSize;
      sortedidx.write((char*)&offset, 8);
   }
   sorted.close();
   sortedidx.close();

   delete [] rec;
   delete [] idx;

   return 0;
}

int Slave::reduce(vector<MRRecord>& vr, const string& bucket, MR_REDUCE red, void* param, int psize)
{
   SInput input;
   input.m_pcUnit = NULL;
   input.m_pcParam = (char*)param;
   input.m_iPSize = psize;


   int rdsize = 256000000;
   int risize = 1000000;

   SOutput output;
   output.m_pcResult = new char[rdsize];
   output.m_iBufSize = rdsize;
   output.m_pllIndex = new int64_t[risize];
   output.m_iIndSize = risize;
   output.m_piBucketID = new int[risize];
   output.m_llOffset = 0;

   SFile file;
   file.m_strHomeDir = m_strHomeDir;
//   file.m_strLibDir = m_strHomeDir + ".sphere/" + path + "/";
   file.m_strTempDir = m_strHomeDir + ".tmp/";
   file.m_pInMemoryObjects = &m_InMemoryObjects;

   char* idata = new char[256000000];
   int64_t* iidx = new int64_t[1000000];

   fstream reduced((bucket + ".reduced").c_str(), ios::out | ios::binary | ios::trunc);
   fstream reducedidx((bucket + ".reduced.idx").c_str(), ios::out | ios::binary | ios::trunc);
   int64_t roff = 0;

   for (vector<MRRecord>::iterator i = vr.begin(); i != vr.end();)
   {
      iidx[0] = 0;
      vector<MRRecord>::iterator curr = i;
      memcpy(idata, i->m_pcData, i->m_iSize);
      iidx[1] = i->m_iSize;
      int offset = 1;

      i ++;
      while ((i != vr.end()) && (i->m_pCompRoutine(curr->m_pcData, curr->m_iSize, i->m_pcData, i->m_iSize) == 0))
      {
         memcpy(idata + iidx[offset], i->m_pcData, i->m_iSize);
         iidx[offset + 1] = iidx[offset] + i->m_iSize;
         offset ++;
         i ++;
      }

      input.m_pcUnit = idata;
      input.m_pllIndex = iidx;
      input.m_iRows = offset;
      red(&input, &output, &file);

      for (int r = 0; r < output.m_iRows; ++ r)
      {
         reduced.write(output.m_pcResult + output.m_pllIndex[r], output.m_pllIndex[r + 1] - output.m_pllIndex[r]);
         roff += output.m_pllIndex[r + 1] - output.m_pllIndex[r];
         reducedidx.write((char*)&roff, 8);
      }
   }

   reduced.close();
   reducedidx.close();

   delete [] output.m_pcResult;
   delete [] output.m_pllIndex;
   delete [] output.m_piBucketID;
   delete [] idata;
   delete [] iidx;

   return 0;
}

int Slave::processData(SInput& input, SOutput& output, SFile& file, SPEResult& result, int buckets, SPHERE_PROCESS process, MR_MAP map, MR_PARTITION partition)
{
   // pass relative offset, from 0, to the processing function
   int64_t uoff = (input.m_pllIndex != NULL) ? input.m_pllIndex[0] : 0;
   for (int p = 0; p <= input.m_iRows; ++ p)
      input.m_pllIndex[p] = input.m_pllIndex[p] - uoff;

   if (NULL != process)
   {
      process(&input, &output, &file);
      if (buckets > 0)
      {
         for (int r = 0; r < output.m_iRows; ++ r)
            result.addData(output.m_piBucketID[r], output.m_pcResult + output.m_pllIndex[r], output.m_pllIndex[r + 1] - output.m_pllIndex[r]);
      }
      else
      {
         // if no bucket is used, do NOT check the BucketID field, so devlopers do not need to assign these values
         for (int r = 0; r < output.m_iRows; ++ r)
            result.addData(0, output.m_pcResult + output.m_pllIndex[r], output.m_pllIndex[r + 1] - output.m_pllIndex[r]);
      }
   }
   else
   {
      if (NULL == map)
      {
         // partition input directly if there is no map
         for (int r = 0; r < input.m_iRows; ++ r)
         {
            char* data = input.m_pcUnit + input.m_pllIndex[r];
            int size = input.m_pllIndex[r + 1] - input.m_pllIndex[r];
            result.addData(partition(data, size, input.m_pcParam, input.m_iPSize), data, size);
         }
      }
      else
      {
         map(&input, &output, &file);
         for (int r = 0; r < output.m_iRows; ++ r)
         {
            char* data = output.m_pcResult + output.m_pllIndex[r];
            int size = output.m_pllIndex[r + 1] - output.m_pllIndex[r];
            result.addData(partition(data, size, input.m_pcParam, input.m_iPSize), data, size);
	 }
      }
   }

   // restore the original offset
   for (int p = 0; p <= input.m_iRows; ++ p)
      input.m_pllIndex[p] = input.m_pllIndex[p] + uoff;

   return 0;
}

int Slave::deliverResult(const int& buckets, const int& speid, SPEResult& result, SPEDestination& dest)
{
   int ret = 0;

   if (buckets == -1)
      ret = sendResultToFile(result, dest.m_strLocalFile + dest.m_pcLocalFileID, dest.m_piSArray[0]);
   else if (buckets > 0)
      ret = sendResultToBuckets(speid, buckets, result, dest);

   for (int b = 0; b < buckets; ++ b)
   {
      dest.m_piSArray[b] += result.m_vDataLen[b];
      if (result.m_vDataLen[b] > 0)
         dest.m_piRArray[b] += result.m_vIndexLen[b] - 1;
   }

   if (buckets != 0)
      result.clear();

   return ret;
}

int Slave::checkBadDest(multimap<int64_t, Address>& sndspd, vector<Address>& bad)
{
   bad.clear();

   if (sndspd.empty())
      return 0;

   int m = sndspd.size() / 2;
   multimap<int64_t, Address>::iterator p = sndspd.begin();
   for (int i = 0; i < m; ++ i)
      ++ p;

   int64_t median = p->first;

   int locpos = 0;
   for (multimap<int64_t, Address>::iterator i = sndspd.begin(); i != sndspd.end(); ++ i)
   {
      if (i->first > (median / 2))
         return bad.size();

      bad.push_back(i->second);
      locpos ++;
   }

   return bad.size();
}
