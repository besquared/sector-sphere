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
   Yunhong Gu, last updated 01/29/2010
*****************************************************************************/


#ifndef __SECTOR_H__
#define __SECTOR_H__

#include <vector>
#include <string>
#include <map>
#include <set>

struct Address
{
   std::string m_strIP;
   unsigned short int m_iPort;
};

struct AddrComp
{
   bool operator()(const Address& a1, const Address& a2) const
   {
      if (a1.m_strIP == a2.m_strIP)
         return a1.m_iPort < a2.m_iPort;
      return a1.m_strIP < a2.m_strIP;
   }
};

class SNode
{
public:
   SNode();
   ~SNode();

public:
   std::string m_strName;
   bool m_bIsDir;
   std::set<Address, AddrComp> m_sLocation;
   std::map<std::string, SNode> m_mDirectory;
   int64_t m_llTimeStamp;
   int64_t m_llSize;
   std::string m_strChecksum;

public:
   int serialize(char* buf);
   int deserialize(const char* buf);
   int serialize2(const std::string& file);
   int deserialize2(const std::string& file);
};

class SysStat
{
public:
   int64_t m_llStartTime;

   int64_t m_llAvailDiskSpace;
   int64_t m_llTotalFileSize;
   int64_t m_llTotalFileNum;

   int64_t m_llTotalSlaves;

   std::vector<Address> m_vMasterList;
   struct SlaveStat
   {
      std::string m_strIP;
      int64_t m_llTimeStamp;
      int64_t m_llAvailDiskSpace;
      int64_t m_llTotalFileSize;
      int64_t m_llCurrMemUsed;
      int64_t m_llCurrCPUUsed;
      int64_t m_llTotalInputData;
      int64_t m_llTotalOutputData;
   };
   std::vector<SlaveStat> m_vSlaveList;
   struct ClusterStat
   {
      int m_iClusterID;
      int m_iTotalNodes;
      int64_t m_llAvailDiskSpace;
      int64_t m_llTotalFileSize;
      int64_t m_llTotalInputData;
      int64_t m_llTotalOutputData;
   };
   std::vector<ClusterStat> m_vCluster;

public:
   void print();
};

class SectorFile;
class SphereProcess;

class Sector
{
public:
   int init(const std::string& server, const int& port);
   int login(const std::string& username, const std::string& password, const char* cert = NULL);
   int logout();
   int close();

   int list(const std::string& path, std::vector<SNode>& attr);
   int stat(const std::string& path, SNode& attr);
   int mkdir(const std::string& path);
   int move(const std::string& oldpath, const std::string& newpath);
   int remove(const std::string& path);
   int rmr(const std::string& path);
   int copy(const std::string& src, const std::string& dst);
   int utime(const std::string& path, const int64_t& ts);

   int sysinfo(SysStat& sys);

public:
   SectorFile* createSectorFile();
   SphereProcess* createSphereProcess();
   int releaseSectorFile(SectorFile* sf);
   int releaseSphereProcess(SphereProcess* sp);

public:
   int m_iID;
};

// file open mode
struct SF_MODE
{
   static const int READ = 1;                   // read only
   static const int WRITE = 2;                  // write only
   static const int RW = 3;                     // read and write
   static const int TRUNC = 4;                  // trunc the file upon opening
   static const int APPEND = 8;                 // move the write offset to the end of the file upon opening
   static const int SECURE = 16;                // encrypted file transfer
   static const int HiRELIABLE = 32;            // replicate data writting at real time (otherwise periodically)
};

//file IO position base
struct SF_POS
{
   static const int BEG = 1;
   static const int CUR = 2;
   static const int END = 3;
};

class SectorFile
{
friend class Sector;

private:
   SectorFile() {}
   ~SectorFile() {}
   const SectorFile& operator=(const SectorFile&) {return *this;}

public:
   int open(const std::string& filename, int mode = SF_MODE::READ, const std::string& hint = "");
   int64_t read(char* buf, const int64_t& size);
   int64_t write(const char* buf, const int64_t& size);
   int download(const char* localpath, const bool& cont = false);
   int upload(const char* localpath, const bool& cont = false);
   int close();

   int seekp(int64_t off, int pos = SF_POS::BEG);
   int seekg(int64_t off, int pos = SF_POS::BEG);
   int64_t tellp();
   int64_t tellg();
   bool eof();

public:
   int m_iID;
};

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
   std::string m_strPath;               // path for output files
   std::string m_strName;               // name prefix for output files

   std::vector<std::string> m_vFiles;   // list of files
   std::vector<int64_t> m_vSize;        // size per file
   std::vector<int64_t> m_vRecNum;      // number of record per file

   std::vector< std::set<Address, AddrComp> > m_vLocation;      // locations for each file
   int32_t* m_piLocID;                  // for output bucket

   int m_iFileNum;                      // number of files
   int64_t m_llSize;                    // total data size
   int64_t m_llRecNum;                  // total number of records
   int64_t m_llStart;                   // start point (record)
   int64_t m_llEnd;                     // end point (record), -1 means the last record

   std::vector<std::string> m_vOrigInput;                       // original input files or dirs, need SphereStream::prepareInput to further process

   int m_iStatus;                       // 0: uninitialized, 1: initialized, -1: bad
};

class SphereResult
{
public:
   SphereResult();
   ~SphereResult();

public:
   int m_iResID;                // result ID

   int m_iStatus;               // if this DS is processed successfully (> 0, number of rows). If not (<0), m_pcData may contain error msg

   char* m_pcData;              // result data
   int m_iDataLen;              // result data length
   int64_t* m_pllIndex;         // result data index
   int m_iIndexLen;             // result data index length

   std::string m_strOrigFile;   // original input file
   int64_t m_llOrigStartRec;    // first record of the original input file
   int64_t m_llOrigEndRec;      // last record of the original input file
};

class SphereProcess
{
friend class Sector;

private:
   SphereProcess() {}
   ~SphereProcess() {}
   const SphereProcess& operator=(const SphereProcess&) {return *this;}

public:
   int close();

   int loadOperator(const char* library);

   int run(const SphereStream& input, SphereStream& output, const std::string& op, const int& rows, const char* param = NULL, const int& size = 0, const int& type = 0);
   int run_mr(const SphereStream& input, SphereStream& output, const std::string& mr, const int& rows, const char* param = NULL, const int& size = 0);

   int read(SphereResult*& res, const bool& inorder = false, const bool& wait = true);
   int checkProgress();
   int checkMapProgress();
   int checkReduceProgress();

   void setMinUnitSize(int size);
   void setMaxUnitSize(int size);
   void setProcNumPerNode(int num);
   void setDataMoveAttr(bool move);

public:
   int m_iID;
};

// ERROR codes
class SectorError
{
public:
   static const int E_UNKNOWN = -1;             // unknown error
   static const int E_PERMISSION = -1001;       // no permission for IO
   static const int E_EXIST = -1002;            // file/dir already exist
   static const int E_NOEXIST = -1003;          // file/dir not found
   static const int E_BUSY = -1004;             // file busy
   static const int E_LOCALFILE = -1005;        // local file failure
   static const int E_NOEMPTY = -1006;          // directory is not empty (for rmdir)
   static const int E_NOTDIR = -1007;           // directory does not exist or not a directory
   static const int E_SECURITY = -2000;         // security check failed
   static const int E_NOCERT = -2001;           // no certificate found
   static const int E_ACCOUNT = -2002;          // account does not exist
   static const int E_PASSWORD = -2003;         // incorrect password
   static const int E_ACL = -2004;              // visit from unallowd IP address
   static const int E_INITCTX = -2005;          // failed to initialize CTX
   static const int E_NOSECSERV = -2006;        // security server is down or cannot connect to it
   static const int E_EXPIRED = - 2007;         // connection time out due to no activity
   static const int E_CONNECTION = - 3000;      // cannot connect to master
   static const int E_RESOURCE = -4000;         // no available resources
   static const int E_NODISK = -4001;           // no enough disk
   static const int E_TIMEDOUT = -5000;         // timeout
   static const int E_INVALID = -6000;          // invalid parameter
   static const int E_SUPPORT = -6001;          // operation not supported
   static const int E_BUCKET = -7001;           // bucket failure

public: // internal error
   static const int E_MASTER = -101;            // incorrect master node to handle the request
   static const int E_REPSLAVE = -102;          // slave is already in the system; conflict

public:
   static int init();
   static std::string getErrorMsg(int ecode);

private:
   static std::map<int, std::string> s_mErrorMsg;
};

#endif
