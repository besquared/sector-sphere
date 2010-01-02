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
   Yunhong Gu, last updated 01/21/2009
*****************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "crypto.h"

Crypto::Crypto():
m_iCoderType(0)
{
}

Crypto::~Crypto()
{
}

int Crypto::generateKey(unsigned char key[16], unsigned char iv[8])
{
   timeval t;
   gettimeofday(&t, 0);

   srand(t.tv_sec * 1000000 + t.tv_usec);

   for (int i = 0; i < 16; ++ i)
      key[i] = int(255.0 * (double(rand()) / RAND_MAX));

   for (int i = 0; i < 8; ++ i)
      iv[i] = int(255.0 * (double(rand()) / RAND_MAX));

   //for (int i = 0; i < 16; i++)
   //   printf("%d \t", key[i]);
   //for (int i = 0; i < 8; i++)
   //   printf ("%d \t", iv[i]);

   return 0;
}

int Crypto::initEnc(unsigned char key[16], unsigned char iv[8])
{
   memcpy(m_pcKey, key, 16);
   memcpy(m_pcIV, iv, 8);

   EVP_CIPHER_CTX_init(&m_CTX);
   EVP_EncryptInit(&m_CTX, EVP_bf_cbc(), m_pcKey, m_pcIV);

   m_iCoderType = 1;

   return 0;
}

int Crypto::initDec(unsigned char key[16], unsigned char iv[8])
{
   memcpy(m_pcKey, key, 16);
   memcpy(m_pcIV, iv, 8);

   EVP_CIPHER_CTX_init(&m_CTX);
   EVP_DecryptInit(&m_CTX, EVP_bf_cbc(), m_pcKey, m_pcIV);

   m_iCoderType = -1;

   return 0;
}

int Crypto::release()
{
   EVP_CIPHER_CTX_cleanup(&m_CTX);
   m_iCoderType = 0;
   return 0;
}

int Crypto::encrypt(unsigned char* input, int insize, unsigned char* output, int& outsize)
{
   if (1 != m_iCoderType)
      return -1;

   unsigned char* ip = input;
   unsigned char* op = output;

   for (int ts = insize; ts > 0; )
   {
      int unitsize = (ts < g_iEncBlockSize) ? ts : g_iEncBlockSize;

      int len;
      if (EVP_EncryptUpdate(&m_CTX, op, &len, ip, unitsize) != 1)
      {
         printf ("error in encrypt update\n");
         return 0;
      }

      ip += unitsize;
      op += len;

      if (EVP_EncryptFinal(&m_CTX, op, &len) != 1)
      {
          printf ("error in encrypt final\n");
          return 0;
      }

      op += len;
      ts -= unitsize;
   }

   outsize = op  - output;
   return 1;
}

int Crypto::decrypt(unsigned char* input, int insize, unsigned char* output, int& outsize)
{
   if (-1 != m_iCoderType)
      return -1;

   unsigned char* ip = input;
   unsigned char* op = output;

   for (int ts = insize; ts > 0; )
   {
      int unitsize = (ts < g_iDecBlockSize) ? ts : g_iDecBlockSize;

      int len;
      if (EVP_DecryptUpdate(&m_CTX, op, &len, ip, unitsize) != 1)
      {
         printf("error in decrypt update\n");
         return 0;
      }

      ip += unitsize;
      op += len;

      if (EVP_DecryptFinal(&m_CTX, op, &len) != 1)
      {
         printf("error in decrypt final\n");
         return 0;
      }

      op += len;
      ts -= unitsize;
   }

   outsize = op - output;
   return 1;
}

