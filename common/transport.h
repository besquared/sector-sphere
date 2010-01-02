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
   Yunhong Gu, last updated 12/01/2008
*****************************************************************************/


#ifndef __CB_TRANSPORT_H__
#define __CB_TRANSPORT_H__

#include <udt.h>
#include <crypto.h>

class Transport
{
public:
   Transport();
   ~Transport();

public:
   static void initialize();
   static void release();

   int open(int& port, bool rendezvous = true, bool reuseaddr = false);

   int listen();
   int accept(Transport& t, sockaddr* addr = NULL, int* addrlen = NULL);

   int connect(const char* ip, int port);
   int send(const char* buf, int size);
   int recv(char* buf, int size);
   int64_t sendfile(std::fstream& ifs, int64_t offset, int64_t size);
   int64_t recvfile(std::fstream& ofs, int64_t offset, int64_t size);
   int close();
   bool isConnected();
   int64_t getRealSndSpeed();
   int getsockname(sockaddr* addr);

public: // secure data/file transfer
   int initCoder(unsigned char key[16], unsigned char iv[8]);
   int releaseCoder();

   int secure_send(const char* buf, int size);
   int secure_recv(char* buf, int size);
   int64_t secure_sendfile(std::fstream& ifs, int64_t offset, int64_t size);
   int64_t secure_recvfile(std::fstream& ofs, int64_t offset, int64_t size);

public:
   int sendEx(const char* buf, int size, bool secure);
   int recvEx(char* buf, int size, bool secure);
   int64_t sendfileEx(std::fstream& ifs, int64_t offset, int64_t size, bool secure);
   int64_t recvfileEx(std::fstream& ofs, int64_t offset, int64_t size, bool secure);

private:
   UDTSOCKET m_Socket;

   Crypto m_Encoder;
   Crypto m_Decoder;
};

#endif
