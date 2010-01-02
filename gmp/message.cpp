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

#include <string>
#include <cstring>
#include <message.h>

using namespace std;

CUserMessage::CUserMessage():
m_iDataLength(0),
m_iBufLength(1500)
{
   m_pcBuffer = new char[m_iBufLength];
}

CUserMessage::CUserMessage(const int& len):
m_iDataLength(0),
m_iBufLength(len)
{
   if (m_iBufLength < 8)
      m_iBufLength = 8;

   m_pcBuffer = new char[m_iBufLength];
}

CUserMessage::CUserMessage(const CUserMessage& msg):
m_iDataLength(msg.m_iDataLength),
m_iBufLength(msg.m_iBufLength)
{
   m_pcBuffer = new char[m_iBufLength];
   memcpy(m_pcBuffer, msg.m_pcBuffer, m_iDataLength + 4);
}

CUserMessage::~CUserMessage()
{
   delete [] m_pcBuffer;
}

int CUserMessage::resize(const int& len)
{
   m_iBufLength = len;

   if (m_iBufLength < m_iDataLength)
      m_iBufLength = m_iDataLength;

   char* temp = new char[m_iBufLength];
   memcpy(temp, m_pcBuffer, m_iDataLength);
   delete [] m_pcBuffer;
   m_pcBuffer = temp;

   return m_iBufLength;
}


int32_t SectorMsg::getType() const
{
   return *(int32_t*)m_pcBuffer;
}

void SectorMsg::setType(const int32_t& type)
{
   *(int32_t*)m_pcBuffer = type;
}

int32_t SectorMsg::getKey() const
{
   return *(int32_t*)(m_pcBuffer + 4);
}

void SectorMsg::setKey(const int32_t& key)
{
   *(int32_t*)(m_pcBuffer + 4) = key;
}

char* SectorMsg::getData() const
{
   return m_pcBuffer + m_iHdrSize;
}

void SectorMsg::setData(const int& offset, const char* data, const int& len)
{
   while (m_iHdrSize + offset + len > m_iBufLength)
      resize(m_iBufLength << 1);

   memcpy(m_pcBuffer + m_iHdrSize + offset, data, len);

   if (m_iDataLength < m_iHdrSize + offset + len)
      m_iDataLength = m_iHdrSize + offset + len;
}
