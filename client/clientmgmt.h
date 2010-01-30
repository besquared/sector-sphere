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
   Yunhong Gu, last updated 01/12/2010
*****************************************************************************/


#ifndef __SECTOR_CLIENT_MGMT_H__
#define __SECTOR_CLIENT_MGMT_H__

#include <map>
#include <pthread.h>

#include "client.h"
#include "fsclient.h"
#include "dcclient.h"

class ClientMgmt
{
public:
   ClientMgmt();
   ~ClientMgmt();

   Client* lookupClient(const int& id);
   FSClient* lookupFS(const int& id);
   DCClient* lookupDC(const int& id);

   int insertClient(Client* c);
   int insertFS(FSClient* f);
   int insertDC(DCClient* d);

   int removeClient(const int& id);
   int removeFS(const int& id);
   int removeDC(const int& id);

private:
   std::map<int, Client*> m_mClients;
   std::map<int, FSClient*> m_mSectorFiles;
   std::map<int, DCClient*> m_mSphereProcesses;

   int m_iID;

   pthread_mutex_t m_CLock;
   pthread_mutex_t m_FSLock;
   pthread_mutex_t m_DCLock;
};

ClientMgmt g_ClientMgmt;

#endif
