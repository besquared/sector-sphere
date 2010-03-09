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
   Yunhong Gu, last updated 03/08/2010
*****************************************************************************/


#ifndef __SECTOR_METADATA_H__
#define __SECTOR_METADATA_H__

#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <topology.h>
#include <sector.h>

class Metadata
{
public:
   Metadata();
   virtual ~Metadata();

   virtual void init(const std::string& path) = 0;
   virtual void clear() = 0;

public:	// list and lookup operations
   virtual int list(const std::string& path, std::vector<std::string>& filelist) = 0;
   virtual int list_r(const std::string& path, std::vector<std::string>& filelist) = 0;

      // Functionality:
      //    look up a specific file or directory in the metadata.
      // Parameters:
      //    [1] path: file or dir name
      //    [2] attr: SNode structure to store the information.
      // Returned value:
      //    If exist, 0 for a file, number of files or sub-dirs for a directory, or -1 on error.

   virtual int lookup(const std::string& path, SNode& attr) = 0;
   virtual int lookup(const std::string& path, std::set<Address, AddrComp>& addr) = 0;

public:	// update operations
   virtual int create(const std::string& path, bool isdir = false) = 0;
   virtual int move(const std::string& oldpath, const std::string& newpath, const std::string& newname = "") = 0;
   virtual int remove(const std::string& path, bool recursive = false) = 0;

      // Functionality:
      //    update the information of a file. e.g., new size, time, or replica.
      // Parameters:
      //    [1] fileinfo: serialized file info
      //    [2] addr: location of the replica to be updated
      //    [3] type: update type, size/time update or new replica
      // Returned value:
      //    number of replicas of the file, or -1 on error.

   virtual int update(const std::string& fileinfo, const Address& addr, const int& type) = 0;
   virtual int utime(const std::string& path, const int64_t& ts) = 0;

public:	// lock/unlock
   virtual int lock(const std::string& path, int user, int mode);
   virtual int unlock(const std::string& path, int user, int mode);

public:	// serialization
   virtual int serialize(const std::string& path, const std::string& dstfile) = 0;
   virtual int deserialize(const std::string& path, const std::string& srcfile, const Address* addr) = 0;
   virtual int scan(const std::string& data_dir, const std::string& meta_dir) = 0;

public:	// medadata and file system operations

      // Functionality:
      //    merge a slave's index with the system file index.
      // Parameters:
      //    1) [in] path: merge into this director, usually "/"
      //    2) [in, out] branch: new metadata to be included; excluded/conflict metadata will be left, while others will be removed
      //    0) [in] replica: number of replicas
      // Returned value:
      //    0 on success, or -1 on error.

   virtual int merge(const std::string& path, Metadata* branch, const unsigned int& replica) = 0;

   virtual int substract(const std::string& path, const Address& addr) = 0;
   virtual int64_t getTotalDataSize(const std::string& path) = 0;
   virtual int64_t getTotalFileNum(const std::string& path) = 0;
   virtual int collectDataInfo(const std::string& path, std::vector<std::string>& result) = 0;
   virtual int getUnderReplicated(const std::string& path, std::vector<std::string>& replica, const unsigned int& thresh, const std::map<std::string, int>& special) = 0;

public:
   static int parsePath(const std::string& path, std::vector<std::string>& result);
   static std::string revisePath(const std::string& path);

protected:
   pthread_mutex_t m_MetaLock;

   struct LockSet
   {
      std::set<int> m_sReadLock;
      std::set<int> m_sWriteLock;
   };
   std::map<std::string, LockSet> m_mLock;
};

#endif
