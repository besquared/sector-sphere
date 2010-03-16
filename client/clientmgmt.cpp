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
   Yunhong Gu, last updated 03/16/2010
*****************************************************************************/

#include <sector.h>
#include "clientmgmt.h"

using namespace std;

ClientMgmt::ClientMgmt():
m_iID(0)
{
   pthread_mutex_init(&m_CLock, NULL);
   pthread_mutex_init(&m_FSLock, NULL);
   pthread_mutex_init(&m_DCLock, NULL);
}

ClientMgmt::~ClientMgmt()
{
   pthread_mutex_destroy(&m_CLock);
   pthread_mutex_destroy(&m_FSLock);
   pthread_mutex_destroy(&m_DCLock);
}

Client* ClientMgmt::lookupClient(const int& id)
{
   Client* c = NULL;

   pthread_mutex_lock(&m_CLock);
   map<int, Client*>::iterator i = m_mClients.find(id);
   if (i != m_mClients.end())
      c = i->second;
   pthread_mutex_unlock(&m_CLock);

   return c;
}

FSClient* ClientMgmt::lookupFS(const int& id)
{
   FSClient* f = NULL;

   pthread_mutex_lock(&m_FSLock);
   map<int, FSClient*>::iterator i = m_mSectorFiles.find(id);
   if (i != m_mSectorFiles.end())
      f = i->second;
   pthread_mutex_unlock(&m_FSLock);

   return f;
}

DCClient* ClientMgmt::lookupDC(const int& id)
{
   DCClient* d = NULL;

   pthread_mutex_lock(&m_DCLock);
   map<int, DCClient*>::iterator i = m_mSphereProcesses.find(id);
   if (i != m_mSphereProcesses.end())
      d = i->second;
   pthread_mutex_unlock(&m_DCLock);

   return d;
}

int ClientMgmt::insertClient(Client* c)
{
   pthread_mutex_lock(&m_CLock);
   int id = g_ClientMgmt.m_iID ++;
   g_ClientMgmt.m_mClients[id] = c;
   pthread_mutex_unlock(&m_CLock);

   return id;
}

int ClientMgmt::insertFS(FSClient* f)
{
   pthread_mutex_lock(&m_CLock);
   int id = g_ClientMgmt.m_iID ++;
   pthread_mutex_unlock(&m_CLock);

   pthread_mutex_lock(&m_FSLock);
   g_ClientMgmt.m_mSectorFiles[id] = f;
   pthread_mutex_unlock(&m_FSLock);

   return id;
}

int ClientMgmt::insertDC(DCClient* d)
{
   pthread_mutex_lock(&m_CLock);
   int id = g_ClientMgmt.m_iID ++;
   pthread_mutex_unlock(&m_CLock);

   pthread_mutex_lock(&m_DCLock);
   g_ClientMgmt.m_mSphereProcesses[id] = d;
   pthread_mutex_unlock(&m_DCLock);

   return id;
}

int ClientMgmt::removeClient(const int& id)
{
   pthread_mutex_lock(&m_CLock);
   g_ClientMgmt.m_mClients.erase(id);
   pthread_mutex_unlock(&m_CLock);

   return 0;
}

int ClientMgmt::removeFS(const int& id)
{
   pthread_mutex_lock(&m_FSLock);
   g_ClientMgmt.m_mSectorFiles.erase(id);
   pthread_mutex_unlock(&m_FSLock);

   return 0;
}

int ClientMgmt::removeDC(const int& id)
{
   pthread_mutex_lock(&m_DCLock);
   g_ClientMgmt.m_mSphereProcesses.erase(id);
   pthread_mutex_unlock(&m_DCLock);

   return 0;
}


int Sector::init(const string& server, const int& port)
{
   Client* c = new Client;
   c->init(server, port);

   m_iID = g_ClientMgmt.insertClient(c);

   return 0;
}

int Sector::login(const string& username, const string& password, const char* cert)
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   return c->login(username, password, cert);
}

int Sector::logout()
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   return c->logout();
}

int Sector::close()
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   g_ClientMgmt.removeClient(m_iID);

   c->close();
   delete c;

   return 0;
}

int Sector::list(const string& path, vector<SNode>& attr)
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   return c->list(path, attr);
}

int Sector::stat(const string& path, SNode& attr)
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   return c->stat(path, attr);
}

int Sector::mkdir(const string& path)
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   return c->mkdir(path);
}

int Sector::move(const string& oldpath, const string& newpath)
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   return c->move(oldpath, newpath);
}

int Sector::remove(const string& path)
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   return c->remove(path);
}

int Sector::rmr(const string& path)
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   return c->rmr(path);
}

int Sector::copy(const string& src, const string& dst)
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   return c->copy(src, dst);
}

int Sector::utime(const string& path, const int64_t& ts)
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   return c->utime(path, ts);
}

int Sector::sysinfo(SysStat& sys)
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return SectorError::E_INVALID;

   return c->sysinfo(sys);
}

SectorFile* Sector::createSectorFile()
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return NULL;

   FSClient* f = c->createFSClient();
   SectorFile* sf = new SectorFile;

   sf->m_iID = g_ClientMgmt.insertFS(f);

   return sf;
}

SphereProcess* Sector::createSphereProcess()
{
   Client* c = g_ClientMgmt.lookupClient(m_iID);

   if (NULL == c)
      return NULL;

   DCClient* d = c->createDCClient();
   SphereProcess* sp = new SphereProcess;

   sp->m_iID = g_ClientMgmt.insertDC(d);

   return sp;
}

int Sector::releaseSectorFile(SectorFile* sf)
{
   if (NULL == sf)
      return 0;

   Client* c = g_ClientMgmt.lookupClient(m_iID);
   FSClient* f = g_ClientMgmt.lookupFS(sf->m_iID);

   if ((NULL == c) || (NULL == f))
      return SectorError::E_INVALID;

   g_ClientMgmt.removeFS(sf->m_iID);
   c->releaseFSClient(f);
   delete sf;
   return 0;
}

int Sector::releaseSphereProcess(SphereProcess* sp)
{
   if (NULL == sp)
      return 0;

   Client* c = g_ClientMgmt.lookupClient(m_iID);
   DCClient* d = g_ClientMgmt.lookupDC(sp->m_iID);

   if ((NULL == c) || (NULL == d))
      return SectorError::E_INVALID;

   g_ClientMgmt.removeDC(sp->m_iID);
   c->releaseDCClient(d);
   delete sp;
   return 0;
}

int SectorFile::open(const string& filename, int mode, const string& hint)
{
   FSClient* f = g_ClientMgmt.lookupFS(m_iID);

   if (NULL == f)
      return SectorError::E_INVALID;

   return f->open(filename, mode, hint);
}

int64_t SectorFile::read(char* buf, const int64_t& size, const int64_t& prefetch)
{
   FSClient* f = g_ClientMgmt.lookupFS(m_iID);

   if (NULL == f)
      return SectorError::E_INVALID;

   return f->read(buf, size, prefetch);
}

int64_t SectorFile::write(const char* buf, const int64_t& size, const int64_t& buffer)
{
   FSClient* f = g_ClientMgmt.lookupFS(m_iID);

   if (NULL == f)
      return SectorError::E_INVALID;

   return f->write(buf, size, buffer);
}

int SectorFile::download(const char* localpath, const bool& cont)
{
   FSClient* f = g_ClientMgmt.lookupFS(m_iID);

   if (NULL == f)
      return SectorError::E_INVALID;

   return f->download(localpath, cont);
}

int SectorFile::upload(const char* localpath, const bool& cont)
{
   FSClient* f = g_ClientMgmt.lookupFS(m_iID);

   if (NULL == f)
      return SectorError::E_INVALID;

   return f->upload(localpath, cont);
}

int SectorFile::close()
{
   FSClient* f = g_ClientMgmt.lookupFS(m_iID);

   if (NULL == f)
      return SectorError::E_INVALID;

   return f->close();
}

int SectorFile::seekp(int64_t off, int pos)
{
   FSClient* f = g_ClientMgmt.lookupFS(m_iID);

   if (NULL == f)
      return SectorError::E_INVALID;

   return f->seekp(off, pos);
}

int SectorFile::seekg(int64_t off, int pos)
{
   FSClient* f = g_ClientMgmt.lookupFS(m_iID);

   if (NULL == f)
      return SectorError::E_INVALID;

   return f->seekg(off, pos);
}

int64_t SectorFile::tellp()
{
   FSClient* f = g_ClientMgmt.lookupFS(m_iID);

   if (NULL == f)
      return SectorError::E_INVALID;

   return f->tellp();
}

int64_t SectorFile::tellg()
{
   FSClient* f = g_ClientMgmt.lookupFS(m_iID);

   if (NULL == f)
      return SectorError::E_INVALID;

   return f->tellg();
}

bool SectorFile::eof()
{
   FSClient* f = g_ClientMgmt.lookupFS(m_iID);

   if (NULL == f)
      return true;

   return f->eof();
}

int SphereProcess::close()
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL == d)
      return SectorError::E_INVALID;

   return d->close();
}

int SphereProcess::loadOperator(const char* library)
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL == d)
      return SectorError::E_INVALID;

   return d->loadOperator(library);
}

int SphereProcess::run(const SphereStream& input, SphereStream& output, const string& op, const int& rows, const char* param, const int& size, const int& type)
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL == d)
      return SectorError::E_INVALID;

   return d->run(input, output, op, rows, param, size, type);
}

int SphereProcess::run_mr(const SphereStream& input, SphereStream& output, const string& mr, const int& rows, const char* param, const int& size)
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL == d)
      return SectorError::E_INVALID;

   return d->run_mr(input, output, mr, rows, param, size);
}

int SphereProcess::read(SphereResult*& res, const bool& inorder, const bool& wait)
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL == d)
      return SectorError::E_INVALID;

   return d->read(res, inorder, wait);
}

int SphereProcess::checkProgress()
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL == d)
      return SectorError::E_INVALID;

   return d->checkProgress();
}

int SphereProcess::checkMapProgress()
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL == d)
      return SectorError::E_INVALID;

   return d->checkMapProgress();
}

int SphereProcess::checkReduceProgress()
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL == d)
      return SectorError::E_INVALID;

   return d->checkReduceProgress();
}

void SphereProcess::setMinUnitSize(int size)
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL != d)
      d->setMinUnitSize(size);
}

void SphereProcess::setMaxUnitSize(int size)
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL != d)
      d->setMaxUnitSize(size);
}

void SphereProcess::setProcNumPerNode(int num)
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL != d)
      d->setProcNumPerNode(num);
}

void SphereProcess::setDataMoveAttr(bool move)
{
   DCClient* d = g_ClientMgmt.lookupDC(m_iID);

   if (NULL != d)
      d->setDataMoveAttr(move);
}
