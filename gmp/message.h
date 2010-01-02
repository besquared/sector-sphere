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
   Yunhong Gu, last updated 01/25/2007
*****************************************************************************/


#ifndef __GMP_MESSAGE_H__
#define __GMP_MESSAGE_H__

#ifndef WIN32
   #include <sys/types.h>
   #define GMP_API
#else
   #include <windows.h>
   #include <udt.h>
   #ifdef GMP_EXPORTS
      #define GMP_API __declspec(dllexport)
   #else
      #define GMP_API __declspec(dllimport)
   #endif
#endif


class GMP_API CUserMessage
{
friend class CGMP;

public:
   CUserMessage();
   CUserMessage(const int& len);
   CUserMessage(const CUserMessage& msg);
   virtual ~CUserMessage();

public:
   int resize(const int& len);

public:
   char* m_pcBuffer;
   int m_iDataLength;
   int m_iBufLength;
};

class GMP_API SectorMsg: public CUserMessage
{
public:
   SectorMsg() {m_iDataLength = m_iHdrSize;}

   int32_t getType() const;
   void setType(const int32_t& type);
   int32_t getKey() const;
   void setKey(const int32_t& key);
   char* getData() const;
   void setData(const int& offset, const char* data, const int& len);

public:
   static const int m_iHdrSize = 8;
};

#endif
