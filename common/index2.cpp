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
   Yunhong Gu, last updated 01/11/2009
*****************************************************************************/


#include <algorithm>
#include <common.h>
#include <dirent.h>
#include <index2.h>
#include <iostream>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <constant.h>

using namespace std;

Index2::Index2():
m_strMetaPath("/tmp")
{
   pthread_mutex_init(&m_MetaLock, NULL);
}

Index2::~Index2()
{
   pthread_mutex_destroy(&m_MetaLock);
}

void Index2::init(const string& path)
{
   m_strMetaPath = path;
}

void Index2::clear()
{
   system((string("rm -rf ") + m_strMetaPath).c_str());
}

int Index2::list(const string& path, vector<string>& filelist)
{
   string item = m_strMetaPath + "/" + path;

   struct stat s;
   if (stat((m_strMetaPath + "/" + path).c_str(), &s) < 0)
      return SectorError::E_NOEXIST;

   if (!S_ISDIR(s.st_mode))
   {
      SNode sn;
      sn.deserialize2(item);
      char buf[4096];
      sn.serialize(buf);
      filelist.insert(filelist.end(), buf);
      return 1;
   }

   dirent **namelist;
   int n = scandir(item.c_str(), &namelist, 0, alphasort);
   if (n < 0)
      return SectorError::E_NOEXIST;

   filelist.clear();

   for (int i = 0; i < n; ++ i)
   {
      // skip system directory
      if (namelist[i]->d_name[0] == '.')
      {
         free(namelist[i]);
         continue;
      }

      SNode sn;
      if (sn.deserialize2(item + "/" + namelist[i]->d_name) < 0)
      {
         free(namelist[i]);
         continue;
      }

      char buf[4096];
      sn.serialize(buf);
      filelist.insert(filelist.end(), buf);

      free(namelist[i]);
   }
   free(namelist);

   return filelist.size();
}

int Index2::list_r(const string& path, vector<string>& filelist)
{
   string item = m_strMetaPath + "/" + path;

   struct stat s;
   if (stat((m_strMetaPath + "/" + path).c_str(), &s) < 0)
      return SectorError::E_NOEXIST;

   if (!S_ISDIR(s.st_mode))
   {
      filelist.push_back(path);
      return 1;
   }

   dirent **namelist;
   int n = scandir(item.c_str(), &namelist, 0, alphasort);
   if (n < 0)
      return SectorError::E_NOEXIST;

   for (int i = 0; i < n; ++ i)
   {
      // skip system directory
      if (namelist[i]->d_name[0] == '.')
      {
         free(namelist[i]);
         continue;
      }

      struct stat s;
      if (stat((item + "/" + namelist[i]->d_name).c_str(), &s) < 0)
      {
         free(namelist[i]);
         continue;
      }

      if (S_ISDIR(s.st_mode))
         list_r((string(path) + "/" + namelist[i]->d_name).c_str(), filelist);
      else
         filelist.push_back(string(path) + "/" + namelist[i]->d_name);

      free(namelist[i]);
   }
   free(namelist);

   return filelist.size();
}

int Index2::lookup(const string& path, SNode& attr)
{
   return attr.deserialize2(m_strMetaPath + "/" + path);
}

int Index2::lookup(const string& path, set<Address, AddrComp>& addr)
{
   string item = m_strMetaPath + "/" + path;

   struct stat s;
   if (stat((m_strMetaPath + "/" + path).c_str(), &s) < 0)
      return SectorError::E_NOEXIST;

   if (!S_ISDIR(s.st_mode))
   {
      SNode sn;
      sn.deserialize2(m_strMetaPath + "/" + path);
      addr = sn.m_sLocation;
      return addr.size();
   }

   dirent **namelist;
   int n = scandir(item.c_str(), &namelist, 0, alphasort);
   if (n < 0)
      return SectorError::E_NOEXIST;

   for (int i = 0; i < n; ++ i)
   {
      // skip system directory
      if (namelist[i]->d_name[0] == '.')
      {
         free(namelist[i]);
         continue;
      }

      if (stat((item + "/" + namelist[i]->d_name).c_str(), &s) < 0)
      {
         free(namelist[i]);
         continue;
      }

      if (S_ISDIR(s.st_mode))
         lookup((string(path) + "/" + namelist[i]->d_name).c_str(), addr);
      else
      {
         SNode sn;
         sn.deserialize2(item + "/" + namelist[i]->d_name);
         for (set<Address>::iterator a = sn.m_sLocation.begin(); a != sn.m_sLocation.end(); ++ a)
            addr.insert(*a);
      }

      free(namelist[i]);
   }
   free(namelist);

   return addr.size();
}

int Index2::create(const string& path, bool isdir)
{
   struct stat64 s;
   if (stat64((m_strMetaPath + "/" + path).c_str(), &s) >= 0)
      return SectorError::E_EXIST;

   string tmp = path;
   size_t p = tmp.rfind('/');
   if (p != string::npos)
   {
      string cmd = string("mkdir -p ") + m_strMetaPath + "/" + tmp.substr(0, p);
      system(cmd.c_str());
   }

   if (isdir)
   {
      string cmd = string("mkdir ") + m_strMetaPath + "/" + path;
      system(cmd.c_str());
   }
   else
   {
      SNode n;
      n.m_bIsDir = false;
      n.m_llSize = 0;
      n.m_llTimeStamp = CTimer::getTime();
      n.serialize2(m_strMetaPath + "/" + path);
   }

   return 0;
}

int Index2::move(const string& oldpath, const string& newpath, const string& newname)
{
   vector<string> olddir;
   if (parsePath(oldpath, olddir) <= 0)
      return SectorError::E_NOEXIST;

   vector<string> newdir;
   if (parsePath(newpath, newdir) < 0)
      return SectorError::E_NOEXIST;

   string cmd = string("mv ") + m_strMetaPath + "/" + oldpath + " " + m_strMetaPath + "/" + newpath + "/" + newname;
   system(cmd.c_str());

   return 0;
}

int Index2::remove(const string& path, bool recursive)
{
   vector<string> dir;
   if (parsePath(path, dir) <= 0)
      return SectorError::E_INVALID;

   if (dir.empty())
      return -1;

   string cmd;
   if (recursive)
   {
      cmd = string("rm -rf ") + m_strMetaPath + "/" + path;
   }
   else
   {
      cmd = string("rm -f ") + m_strMetaPath + "/" + path;
   }

   system(cmd.c_str());

   return 0;
}

int Index2::update(const string& fileinfo, const Address& loc, const int& type)
{
   SNode sn;
   sn.deserialize(fileinfo.c_str());
   sn.m_sLocation.insert(loc);

   string path = sn.m_strName;

   vector<string> dir;
   parsePath(path.c_str(), dir);

   if (dir.empty())
      return -1;

   sn.m_strName = *dir.rbegin();

   struct stat s;
   if (stat((m_strMetaPath + "/" + path).c_str(), &s) >= 0)
   {
      if ((type == 1) || (type == 2))
      {
         SNode os;
         os.deserialize2(m_strMetaPath + "/" + path);
         os.m_llSize = sn.m_llSize;
         os.m_llTimeStamp = sn.m_llTimeStamp;
         os.m_sLocation.insert(loc);
         os.serialize2(m_strMetaPath + "/" + path);
         return 0;
      }
      else if (type == 3)
      {
         SNode os;
         os.deserialize2(m_strMetaPath + "/" + path);

         // a new replica
         if (os.m_sLocation.find(loc) != os.m_sLocation.end())
            return -1;

         if ((os.m_llSize != sn.m_llSize) || (os.m_llTimeStamp != sn.m_llTimeStamp))
            return -1;
      }
      else
      {
         return -1;
      }
   }
   else
   {
      if ((type == 3) || (type == 2))
      {
         // this is for new files only
         return -1;
      }
      sn.serialize2(m_strMetaPath + "/" + path);
   }

   return -1;
}

int Index2::utime(const string& path, const int64_t& ts)
{
   timeval t[2];
   t[0].tv_sec = ts;
   t[0].tv_usec = 0;
   t[1] = t[0];
   utimes((m_strMetaPath + "/" + path).c_str(), t);

   return 0;
}

int Index2::serialize(const string& path, const string& dstfile)
{
   ofstream ofs(dstfile.c_str());
   if (ofs.bad() || ofs.fail())
      return -1;

   serialize(ofs, path, 1);

   ofs.close();

   return 0;
}

int Index2::deserialize(const string& path, const string& srcfile, const Address* addr)
{
   vector<string> dirs;
   dirs.resize(1024);
   string currdir = m_strMetaPath;
   int currlevel = 1;

   ifstream ifs(srcfile.c_str());
   if (ifs.bad() || ifs.fail())
      return -1;

   while (!ifs.eof())
   {
      char tmp[4096];
      tmp[4095] = 0;
      char* buf = tmp;

      ifs.getline(buf, 4096);
      int len = strlen(buf);
      if ((len <= 0) || (len >= 4095))
         continue;

      for (int i = 0; i < len; ++ i)
      {
         if (buf[i] == ' ')
         {
            buf[i] = '\0';
            break;
         }
      }

      int level = atoi(buf);

      SNode sn;
      sn.deserialize(buf + strlen(buf) + 1);
      if ((!sn.m_bIsDir) && (NULL != addr))
      {
         sn.m_sLocation.clear();
         sn.m_sLocation.insert(*addr);
      }

      if (level == currlevel)
      {
         sn.serialize2(currdir + "/" + sn.m_strName);
         dirs[level] = sn.m_strName;
      }
      else if (level == currlevel + 1)
      {
         currdir = currdir + "/" + dirs[currlevel];
         currlevel = level;

         sn.serialize2(currdir + "/" + sn.m_strName);
         dirs[level] = sn.m_strName;
      }
      else if (level < currlevel)
      {
         currdir = m_strMetaPath;

         for (int i = 1; i < level; ++ i)
            currdir = currdir + "/" + dirs[i];

         currlevel = level;

         sn.serialize2(currdir + "/" + sn.m_strName);
         dirs[level] = sn.m_strName;
      }
   }

   return 0;
}

int Index2::scan(const string& data, const string& meta)
{
   dirent **namelist;
   int n = scandir(data.c_str(), &namelist, 0, alphasort);
   if (n < 0)
      return SectorError::E_NOEXIST;

   for (int i = 0; i < n; ++ i)
   {
      // skip system directory
      if (namelist[i]->d_name[0] == '.')
      {
         free(namelist[i]);
         continue;
      }

      // check file name
      bool bad = false;
      for (char *p = namelist[i]->d_name, *q = namelist[i]->d_name + strlen(namelist[i]->d_name); p != q; ++ p)
      {
         if ((*p == 10) || (*p == 13))
         {
            bad = true;
            break;
         }
      }
      if (bad)
      {
         free(namelist[i]);
         continue;
      }

      struct stat64 s;
      if (stat64((data + "/" + namelist[i]->d_name).c_str(), &s) < 0)
      {
         free(namelist[i]);
         continue;
      }

      SNode sn;
      sn.m_strName = namelist[i]->d_name;

      sn.m_llSize = s.st_size;
      sn.m_llTimeStamp = s.st_mtime;

      if (S_ISDIR(s.st_mode))
      {
         sn.m_bIsDir = true;
         string cmd = string("mkdir -p ") +  m_strMetaPath + "/" + meta + "/" + namelist[i]->d_name;
         system(cmd.c_str());
         scan(data + "/" + namelist[i]->d_name, meta + "/" + namelist[i]->d_name);
      }
      else
      {
         sn.m_bIsDir = false;
         sn.serialize2(m_strMetaPath + "/" + meta + "/" + namelist[i]->d_name);
      }

      free(namelist[i]);
   }
   free(namelist);

   return 0;
}

int Index2::merge(const string& path, Metadata* meta, const unsigned int& replica)
{
   merge(path, ((Index2*)meta)->m_strMetaPath, replica);

   return 0;
}

int Index2::substract(const string& path, const Address& addr)
{
   struct stat s;
   if (stat((m_strMetaPath + "/" + path).c_str(), &s) < 0)
      return -1;

   if (!S_ISDIR(s.st_mode))
   {
      SNode sn;
      sn.deserialize2(m_strMetaPath + "/" + path);
      sn.m_sLocation.erase(addr);
      //if (sn.m_sLocation.empty())
      //   unlink(m_strMetaPath + path);
      sn.serialize2(m_strMetaPath + "/" + path);

      return 0;
   }

   dirent **namelist;
   int n = scandir((m_strMetaPath + "/" + path).c_str(), &namelist, 0, alphasort);
   if (n < 0)
      return SectorError::E_NOEXIST;

   for (int i = 0; i < n; ++ i)
   {
      // skip system directory
      if (namelist[i]->d_name[0] == '.')
      {
         free(namelist[i]);
         continue;
      }

      substract(path + "/" + namelist[i]->d_name, addr);

      free(namelist[i]);
   }
   free(namelist);

   return 0;
}

int64_t Index2::getTotalDataSize(const string& path)
{
   struct stat s;
   if (stat((m_strMetaPath + "/" + path).c_str(), &s) < 0)
      return -1;

   if (!S_ISDIR(s.st_mode))
   {
      SNode sn;
      sn.deserialize2(m_strMetaPath + "/" + path);
      return sn.m_llSize;
   }

   dirent **namelist;
   int n = scandir((m_strMetaPath + "/" + path).c_str(), &namelist, 0, alphasort);
   if (n < 0)
      return SectorError::E_NOEXIST;

   int64_t size = 0;

   for (int i = 0; i < n; ++ i)
   {
      // skip system directory
      if (namelist[i]->d_name[0] == '.')
      {
         free(namelist[i]);
         continue;
      }

      size += getTotalDataSize(path + "/" + namelist[i]->d_name);

      free(namelist[i]);
   }
   free(namelist);

   return size;
}

int64_t Index2::getTotalFileNum(const string& path)
{
   struct stat s;
   if (stat((m_strMetaPath + "/" + path).c_str(), &s) < 0)
      return -1;

   if (!S_ISDIR(s.st_mode))
      return 1;

   dirent **namelist;
   int n = scandir((m_strMetaPath + "/" + path).c_str(), &namelist, 0, alphasort);
   if (n < 0)
      return SectorError::E_NOEXIST;

   int64_t count = 0;

   for (int i = 0; i < n; ++ i)
   {
      // skip system directory
      if (namelist[i]->d_name[0] == '.')
      {
         free(namelist[i]);
         continue;
      }

      count += getTotalFileNum(path + "/" + namelist[i]->d_name);

      free(namelist[i]);
   }
   free(namelist);

   return count;
}

int Index2::collectDataInfo(const string& path, vector<string>& result)
{
   struct stat s;
   if (stat((m_strMetaPath + "/" + path).c_str(), &s) < 0)
      return -1;

   if (!S_ISDIR(s.st_mode))
   {
      // ignore index file
      int t = path.length();
      if ((t > 4) && (path.substr(t - 4, t) == ".idx"))
         return result.size();

      string idx = m_strMetaPath + "/" + path + ".idx";
      int rows = -1;
      SNode is;
      if (is.deserialize2(idx) >= 0)
         rows = is.m_llSize / 8 - 1;

      SNode sn;
      sn.deserialize2(m_strMetaPath + "/" + path);

      char buf[1024];
      sprintf(buf, "%s %lld %d", path.c_str(), sn.m_llSize, rows);

      for (set<Address, AddrComp>::iterator i = sn.m_sLocation.begin(); i != sn.m_sLocation.end(); ++ i)
         sprintf(buf + strlen(buf), " %s %d", i->m_strIP.c_str(), i->m_iPort);

      result.insert(result.end(), buf);
      return result.size();
   }

   dirent **namelist;
   int n = scandir((m_strMetaPath + "/" + path).c_str(), &namelist, 0, alphasort);
   if (n < 0)
      return SectorError::E_NOEXIST;

   for (int i = 0; i < n; ++ i)
   {
      // skip system directory
      if (namelist[i]->d_name[0] == '.')
      {
         free(namelist[i]);
         continue;
      }

      collectDataInfo((path + "/" + namelist[i]->d_name).c_str(), result);

      free(namelist[i]);
   }
   free(namelist);

   return result.size();
}

int Index2::getUnderReplicated(const string& path, vector<string>& replica, const unsigned int& thresh)
{
   dirent **namelist;
   int n = scandir((m_strMetaPath + "/" + path).c_str(), &namelist, 0, alphasort);
   if (n < 0)
      return -1;

   for (int i = 0; i < n; ++ i)
   {
      // skip system directory
      if (namelist[i]->d_name[0] == '.')
      {
         free(namelist[i]);
         continue;
      }

      struct stat s;
      if (stat((m_strMetaPath + "/" + path + "/" + namelist[i]->d_name).c_str(), &s) < 0)
         return -1;

      if (!S_ISDIR(s.st_mode))
      {
         SNode sn;
         sn.deserialize2(m_strMetaPath + "/" + path + "/" + namelist[i]->d_name);
         if (sn.m_sLocation.size() < thresh)
            replica.push_back(path + "/" + namelist[i]->d_name);
      }
      else
         getUnderReplicated(path + "/" + namelist[i]->d_name, replica, thresh);

      free(namelist[i]);
   }
   free(namelist);

   return replica.size();
}

/////////////////////////////////////////////////////////////////////////////////

int Index2::serialize(ofstream& ofs, const string& currdir, int level)
{
   string item = m_strMetaPath + "/" + currdir;

   dirent **namelist;
   int n = scandir(item.c_str(), &namelist, 0, alphasort);
   if (n < 0)
      return SectorError::E_NOEXIST;

   for (int i = 0; i < n; ++ i)
   {
      // skip system directory
      if (namelist[i]->d_name[0] == '.')
      {
         free(namelist[i]);
         continue;
      }

      SNode sn;
      if (sn.deserialize2(item + "/" + namelist[i]->d_name) < 0)
         return -1;

      char* buf = new char[currdir.length() + 64];
      sn.serialize(buf);
      ofs << level << " " << buf << endl;
      delete [] buf;

      if (sn.m_bIsDir)
         serialize(ofs, currdir + "/" + namelist[i]->d_name, level + 1);

      free(namelist[i]);
   }
   free(namelist);

   return n;
}

int Index2::merge(const string& prefix, const string& path, const unsigned int& replica)
{
   dirent **namelist;
   int n = scandir(path.c_str(), &namelist, 0, alphasort);
   if (n < 0)
      return SectorError::E_NOEXIST;

   for (int i = 0; i < n; ++ i)
   {
      // skip system directory
      if (namelist[i]->d_name[0] == '.')
      {
         free(namelist[i]);
         continue;
      }

      struct stat s;
      if (stat((path + "/" + namelist[i]->d_name).c_str(), &s) < 0)
         continue;

      if (S_ISDIR(s.st_mode))
      {
         string dir = m_strMetaPath + "/" + prefix + "/" + namelist[i]->d_name;

         if (stat(dir.c_str(), &s) < 0)
         {
            //not exist
            string cmd = string("mv ") + path + "/" + namelist[i]->d_name + " " + m_strMetaPath + "/" + prefix;
            system(cmd.c_str());
         }
         else
         {
            merge(prefix + "/" + namelist[i]->d_name, path + "/" + namelist[i]->d_name, replica);
         }
      }
      else
      {
         string file = m_strMetaPath + "/" + prefix + "/" + namelist[i]->d_name;

         if (stat(file.c_str(), &s) < 0)
         {
            string cmd = string("mv ") + path + "/" + namelist[i]->d_name + " " + m_strMetaPath + "/" + prefix;
            system(cmd.c_str());
         }
         else
         {
            SNode os, ns;
            os.deserialize2(file);
            ns.deserialize2(path + "/" + namelist[i]->d_name);

            if ((os.m_llSize == ns.m_llSize) && (os.m_llTimeStamp == ns.m_llTimeStamp) && (os.m_sLocation.size() < replica))
            {
               // files with same name, size, timestamp
               // and the number of replicas is below the threshold
               for (set<Address>::iterator a = ns.m_sLocation.begin(); a != ns.m_sLocation.end(); ++ a)
                  os.m_sLocation.insert(*a);

               os.serialize2(file);
            }

            unlink((path + "/" + namelist[i]->d_name).c_str());
         }
      }

      free(namelist[i]);
   }
   free(namelist);

   return 0;
}
