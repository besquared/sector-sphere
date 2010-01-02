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
   Yunhong Gu, last updated 07/30/2008
*****************************************************************************/

#include <sphere.h>
#include <cstring>

int SOutput::resizeResBuf(const int64_t& newsize)
{
   char* tmp = NULL;

   try
   {
      tmp = new char[newsize];
   }
   catch (...)
   {
      return -1;
   }

   memcpy(tmp, m_pcResult, m_iResSize);
   delete [] m_pcResult;
   m_pcResult = tmp;

   m_iBufSize = newsize;

   return newsize;
}

int SOutput::resizeIdxBuf(const int64_t& newsize)
{
   int64_t* tmp1 = NULL;
   int* tmp2 = NULL;

   try
   {
      tmp1 = new int64_t[newsize];
      tmp2 = new int[newsize];
   }
   catch (...)
   {
      return -1;
   }

   memcpy(tmp1, m_pllIndex, m_iRows * 8);
   delete [] m_pllIndex;
   m_pllIndex = tmp1;

   memcpy(tmp2, m_piBucketID, m_iRows * sizeof(int));
   delete [] m_piBucketID;
   m_piBucketID = tmp2;

   m_iIndSize = newsize;

   return newsize;
}

