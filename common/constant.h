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
   Yunhong Gu, last updated 07/15/2009
*****************************************************************************/

#ifndef __SECTOR_CONSTANT_H__
#define __SECTOR_CONSTANT_H__

#include <string>
#include <map>

// ERROR codes
class SectorError
{
public:
   static const int E_UNKNOWN = -1;		// unknown error
   static const int E_PERMISSION = -1001;	// no permission for IO
   static const int E_EXIST = -1002;		// file/dir already exist
   static const int E_NOEXIST = -1003;		// file/dir not found
   static const int E_BUSY = -1004;		// file busy
   static const int E_LOCALFILE = -1005;	// local file failure
   static const int E_NOEMPTY = -1006;          // directory is not empty (for rmdir)
   static const int E_NOTDIR = -1007;		// directory does not exist or not a directory
   static const int E_SECURITY = -2000;		// security check failed
   static const int E_NOCERT = -2001;		// no certificate found
   static const int E_ACCOUNT = -2002;		// account does not exist
   static const int E_PASSWORD = -2003;		// incorrect password
   static const int E_ACL = -2004;		// visit from unallowd IP address
   static const int E_INITCTX = -2005;		// failed to initialize CTX
   static const int E_NOSECSERV = -2006;	// security server is down or cannot connect to it
   static const int E_EXPIRED = - 2007;		// connection time out due to no activity
   static const int E_CONNECTION = - 3000;	// cannot connect to master
   static const int E_RESOURCE = -4000;		// no available resources
   static const int E_NODISK = -4001;		// no enough disk
   static const int E_TIMEDOUT = -5000;		// timeout
   static const int E_INVALID = -6000;		// invalid parameter
   static const int E_SUPPORT = -6001;		// operation not supported
   static const int E_BUCKET = -7001;		// bucket failure

public: // internal error
   static const int E_MASTER = -101;		// incorrect master node to handle the request

public:
   static int init();
   static std::string getErrorMsg(int ecode);

private:
   static std::map<int, std::string> s_mErrorMsg;
};

// file open mode
struct SF_MODE
{
   static const int READ = 1;			// read only
   static const int WRITE = 2;			// write only
   static const int RW = 3;			// read and write
   static const int TRUNC = 4;			// trunc the file upon opening
   static const int APPEND = 8;			// move the write offset to the end of the file upon opening
   static const int SECURE = 16;		// encrypted file transfer
   static const int HiRELIABLE = 32;		// replicate data writting at real time (otherwise periodically)
};

//file IO position base
struct SF_POS
{
   static const int BEG = 1;
   static const int CUR = 2;
   static const int END = 3;
};

#endif
