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
   Yunhong Gu, last updated 12/29/2008
*****************************************************************************/

#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include <openssl/evp.h>

class Crypto
{
public:
   Crypto();
   ~Crypto();

public:
   static int generateKey(unsigned char key[16], unsigned char iv[8]);

   int initEnc(unsigned char key[16], unsigned char iv[8]);
   int initDec(unsigned char key[16], unsigned char iv[8]);
   int release();

   int encrypt(unsigned char* input, int insize, unsigned char* output, int& outsize);
   int decrypt(unsigned char* input, int insize, unsigned char* output, int& outsize);

private:
   unsigned char m_pcKey[16];
   unsigned char m_pcIV[8];
   EVP_CIPHER_CTX m_CTX;
   int m_iCoderType;		// 1: encoder, -1:decoder

   static const int g_iEncBlockSize = 1024;
   static const int g_iDecBlockSize = 1032;
};

#endif
