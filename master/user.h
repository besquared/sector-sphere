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
   Yunhong Gu, last updated 02/25/2010
*****************************************************************************/


#ifndef __SECTOR_USER_H__
#define __SECTOR_USER_H__

#include <string>
#include <vector>
#include <map>
#include <stdint.h>

class User
{
public:
   int deserialize(std::vector<std::string>& dirs, const std::string& buf);
   bool match(const std::string& path, int rwx) const;

public:
   int serialize(char*& buf, int& size);
   int deserialize(const char* buf, const int& size);

public:
   std::string m_strName;			// user name

   std::string m_strIP;				// client IP address
   int m_iPort;					// client port (GMP)
   int m_iDataPort;				// data channel port

   int32_t m_iKey;				// client key

   unsigned char m_pcKey[16];			// client crypto key
   unsigned char m_pcIV[8];			// client crypto iv

   int64_t m_llLastRefreshTime;			// timestamp of last activity
   std::vector<std::string> m_vstrReadList;	// readable directories
   std::vector<std::string> m_vstrWriteList;	// writable directories
   bool m_bExec;				// permission to run Sphere application
};

class UserManager
{
public:
   UserManager();
   ~UserManager();

public:
   int insert(User* u);
   int checkInactiveUsers(std::vector<User*>& iu);
   int serializeUsers(int& num, std::vector<char*>& buf, std::vector<int>& size);
   User* lookup(int key);
   int remove(int key);

private:
   std::map<int, User*> m_mActiveUsers;
   pthread_mutex_t m_Lock;
};

#endif
