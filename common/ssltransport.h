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
   Yunhong Gu, last updated 06/13/2008
*****************************************************************************/

#ifndef __SSL_TRANSPORT_H__
#define __SSL_TRANSPORT_H__

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include <string>

class SSLTransport
{
public:
   SSLTransport();
   ~SSLTransport();

public:
   static void init();
   static void destroy();

public:
   int initServerCTX(const char* cert, const char* key);
   int initClientCTX(const char* cert);

   int open(const char* ip, const int& port);
   int listen();
   SSLTransport* accept(char* ip, int& port);
   int connect(const char* ip, const int& port);
   int close();

   int send(const char* data, const int& size);
   int recv(char* data, const int& size);

   int64_t sendfile(const char* file, const int64_t& offset, const int64_t& size);
   int64_t recvfile(const char* file, const int64_t& offset, const int64_t& size);

   int getLocalIP(std::string& ip);

private:
   SSL_CTX* m_pCTX;
   SSL* m_pSSL;
   int m_iSocket;

   bool m_bConnected;

private:
   static int g_iInstance;
};

#endif
