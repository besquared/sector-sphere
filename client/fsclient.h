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
   Yunhong Gu [gu@lac.uic.edu], last updated 03/08/2010
*****************************************************************************/

#ifndef __SECTOR_FS_CLIENT_H__
#define __SECTOR_FS_CLIENT_H__

#include <client.h>

class FSClient
{
friend class Client;

private:
   FSClient();
   ~FSClient();
   const FSClient& operator=(const FSClient&) {return *this;}

public:
   int open(const std::string& filename, int mode = SF_MODE::READ, const std::string& hint = "");
   int64_t read(char* buf, const int64_t& size, const int64_t& prefetch = 0);
   int64_t write(const char* buf, const int64_t& size, const int64_t& buffer = 0);
   int64_t download(const char* localpath, const bool& cont = false);
   int64_t upload(const char* localpath, const bool& cont = false);
   int close();

   int64_t seekp(int64_t off, int pos = SF_POS::BEG);
   int64_t seekg(int64_t off, int pos = SF_POS::BEG);
   int64_t tellp();
   int64_t tellg();
   bool eof();

private:
   int64_t prefetch(const int64_t& offset, const int64_t& size);
   int64_t flush() {return 0;}

private:
   int32_t m_iSession;		// session ID for data channel
   std::string m_strSlaveIP;	// slave IP address
   int32_t m_iSlaveDataPort;	// slave port number

   unsigned char m_pcKey[16];
   unsigned char m_pcIV[8];

   std::string m_strFileName;	// Sector file name

   int64_t m_llSize;		// file size
   int64_t m_llCurReadPos;	// current read position
   int64_t m_llCurWritePos;	// current write position

   bool m_bRead;		// read permission
   bool m_bWrite;		// write permission
   bool m_bSecure;		// if the data transfer should be secure

   bool m_bLocal;		// if this file exist on the same node, i.e., local file
   char* m_pcLocalPath;		// path of the file if it is local

private:
   pthread_mutex_t m_FileLock;

private:
   Client* m_pClient;		// client instance
   int m_iID;			// sector file id, for client internal use
};

#endif
