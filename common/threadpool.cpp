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
   Yunhong Gu, last updated 01/04/2010
*****************************************************************************/

#include "threadpool.h"

using namespace std;

ThreadJobQueue::ThreadJobQueue()
{
   pthread_mutex_init(&m_QueueLock, NULL);
   pthread_cond_init(&m_QueueCond, NULL);
}

ThreadJobQueue::~ThreadJobQueue()
{
   pthread_mutex_destroy(&m_QueueLock);
   pthread_cond_destroy(&m_QueueCond);
}

int ThreadJobQueue::push(void* param)
{
   pthread_mutex_lock(&m_QueueLock);

   m_qJobs.push(param);

   pthread_cond_signal(&m_QueueCond);
   pthread_mutex_unlock(&m_QueueLock);

   return 0;
}

void* ThreadJobQueue::pop()
{
   pthread_mutex_lock(&m_QueueLock);

   while (m_qJobs.empty())
      pthread_cond_wait(&m_QueueCond, &m_QueueLock);

   void* param = m_qJobs.front();
   m_qJobs.pop();

   pthread_mutex_unlock(&m_QueueLock);

   return param;
}

int ThreadJobQueue::release(int num)
{
   for (int i = 0; i < num; ++ i)
      push(NULL);

   return 0;
}
