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
   Yunhong Gu, last updated 01/12/2010
*****************************************************************************/

#ifndef __SPHERE_CLIENT_H__
#define __SPHERE_CLIENT_H__

#include "client.h"

class SphereStream
{
public:
   SphereStream();
   ~SphereStream();

public:
   int init(const std::vector<std::string>& files);

   int init(const int& num);
   void setOutputPath(const std::string& path, const std::string& name);
   void setOutputLoc(const unsigned int& bucket, const Address& addr);

public:
   std::string m_strPath;		// path for output files
   std::string m_strName;		// name prefix for output files

   std::vector<std::string> m_vFiles;	// list of files
   std::vector<int64_t> m_vSize;	// size per file
   std::vector<int64_t> m_vRecNum;	// number of record per file

   std::vector< std::set<Address, AddrComp> > m_vLocation;            // locations for each file

   int m_iFileNum;			// number of files
   int64_t m_llSize;			// total data size
   int64_t m_llRecNum;			// total number of records
   int64_t m_llStart;			// start point (record)
   int64_t m_llEnd;			// end point (record), -1 means the last record

   std::vector<std::string> m_vOrigInput;	// original input files or dirs, need SphereStream::prepareInput to further process

   int m_iStatus;			// 0: uninitialized, 1: initialized, -1: bad
};

class SphereResult
{
public:
   SphereResult();
   ~SphereResult();

public:
   int m_iResID;		// result ID

   int m_iStatus;		// if this DS is processed successfully (> 0, number of rows). If not (<0), m_pcData may contain error msg

   char* m_pcData;		// result data
   int m_iDataLen;		// result data length
   int64_t* m_pllIndex;		// result data index
   int m_iIndexLen;		// result data index length

   std::string m_strOrigFile;	// original input file
   int64_t m_llOrigStartRec;	// first record of the original input file
   int64_t m_llOrigEndRec;	// last record of the original input file
};

class SphereProcess
{
friend class Client;

private:
   SphereProcess();
   ~SphereProcess();
   const SphereProcess& operator=(const SphereProcess&) {return *this;}

public:
   int close();

   int loadOperator(const char* library);

   int run(const SphereStream& input, SphereStream& output, const std::string& op, const int& rows, const char* param = NULL, const int& size = 0, const int& type = 0);
   int run_mr(const SphereStream& input, SphereStream& output, const std::string& mr, const int& rows, const char* param = NULL, const int& size = 0);

   // rows:
   // 	n (n > 0): n rows per time
   //	0: no rows, one file per time
   //	-1: all rows in each segment

   int read(SphereResult*& res, const bool& inorder = false, const bool& wait = true);
   int checkProgress();
   int checkMapProgress();
   int checkReduceProgress();

   inline void setMinUnitSize(int size) {m_iMinUnitSize = size;}
   inline void setMaxUnitSize(int size) {m_iMaxUnitSize = size;}
   inline void setProcNumPerNode(int num) {m_iCore = num;}
   inline void setDataMoveAttr(bool move) {m_bDataMove = move;}

private:
   int m_iProcType;				// 0: sphere 1: mapreduce

   std::string m_strOperator;			// name of the UDF
   char* m_pcParam;				// parameter
   int m_iParamSize;				// size of the parameter
   SphereStream* m_pInput;			// input stream
   SphereStream* m_pOutput;			// output stream
   int m_iRows;					// number of rows per UDF processing
   int m_iOutputType;				// type of output: -1, loca; 0, none; N, buckets 
   char* m_pOutputLoc;				// output location for buckets
   int m_iSPENum;				// number of SPEs

   struct DS
   {
      int m_iID;				// DS ID
      std::string m_strDataFile;		// input data file
      int64_t m_llOffset;			// input data offset
      int64_t m_llSize;				// input data size
      int m_iSPEID;				// processing SPE
      std::set<Address, AddrComp>* m_pLoc;	// locations of DS

      int m_iStatus;                            // 0: not started yet; 1: in progress; 2: done, result ready; 3: result read; -1: failed
      int m_iRetryNum;				// number of retry due to failure
      SphereResult* m_pResult;			// processing result
   };
   std::map<int, DS*> m_mpDS;
   pthread_mutex_t m_DSLock;

   struct SPE
   {
      int32_t m_iID;				// SPE ID
      std::string m_strIP;			// SPE IP
      int m_iPort;				// SPE GMP port
      int m_iDataPort;				// SPE data port
      DS* m_pDS;				// current processing DS
      int m_iStatus;				// -1: abandond; 0: uninitialized; 1: ready; 2; running
      int m_iProgress;				// 0 - 100 (%)
      timeval m_StartTime;			// SPE start time
      timeval m_LastUpdateTime;			// SPE last update time
      int m_iSession;				// SPE session ID for data channel
   };
   std::map<int, SPE> m_mSPE;

   struct BUCKET
   {
      int32_t m_iID;				// bucket ID
      std::string m_strIP;			// slave IP address
      int m_iPort;				// slave GMP port
      int m_iDataPort;				// slave Data port
      int m_iShufflerPort;			// Shuffer GMP port
      int m_iSession;				// Shuffler session ID
      int m_iProgress;				// bucket progress, 0 or 100 
      timeval m_LastUpdateTime;			// last update time
   };
   std::map<int, BUCKET> m_mBucket;		// list of all buckets

   int m_iProgress;				// progress, 0..100
   double m_dRunningProgress;			// progress of running but incomplete jobs
   int m_iAvgRunTime;				// average running time, in seconds
   int m_iTotalDS;				// total number of data segments
   int m_iTotalSPE;				// total number of SPEs
   int m_iAvailRes;				// number of availale result to be read
   bool m_bBucketHealth;			// if all bucket nodes are alive; unable to recover from bucket failure

   pthread_mutex_t m_ResLock;
   pthread_cond_t m_ResCond;

   int m_iMinUnitSize;				// minimum data segment size
   int m_iMaxUnitSize;				// maximum data segment size, must be smaller than physical memory
   int m_iCore;					// number of processing instances on each node
   bool m_bDataMove;				// if source data is allowed to move for Sphere process

   struct OP
   {
      std::string m_strLibrary;			// UDF name
      std::string m_strLibPath;			// path of the dynamic library that contains the UDF
      int m_iSize;				// size of the library
   };
   std::vector<OP> m_vOP;
   int loadOperator(SPE& s);

private:
   int dataInfo(const std::vector<std::string>& files, std::vector<std::string>& info);
   int prepareInput(SphereStream& ss);
   int prepareSPE(const char* spenodes);
   int segmentData();
   int prepareOutput(const char* spenodes);

   static void* run(void*);
   pthread_mutex_t m_RunLock;

   int start();
   int checkSPE();
   int startSPE(SPE& s, DS* d);
   int connectSPE(SPE& s);
   int checkBucket();
   int readResult(SPE* s);

private:
   Client* m_pClient;
   int m_iID;
};

#endif
