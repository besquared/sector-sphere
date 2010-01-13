#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <util.h>
#include <fsclient.h>

using namespace std;

int upload(const char* file, const char* dst, Sector& client)
{
   timeval t1, t2;
   gettimeofday(&t1, 0);

   struct stat64 s;
   stat64(file, &s);
   cout << "uploading " << file << " of " << s.st_size << " bytes" << endl;

   SectorFile* f = client.createSectorFile();

   if (f->open(dst, SF_MODE::WRITE) < 0)
   {
      cout << "ERROR: unable to connect to server or file already exists." << endl;
      return -1;
   }

   bool finish = true;
   if (f->upload(file) < 0)
      finish = false;

   f->close();
   client.releaseSectorFile(f);

   if (finish)
   {
      gettimeofday(&t2, 0);
      float throughput = s.st_size * 8.0 / 1000000.0 / ((t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1000000.0);

      cout << "Uploading accomplished! " << "AVG speed " << throughput << " Mb/s." << endl << endl ;
   }
   else
      cout << "Uploading failed! Please retry. " << endl << endl;

   return 1;
}

int getFileList(const string& path, vector<string>& fl)
{
   fl.push_back(path);

   struct stat64 s;
   stat64(path.c_str(), &s);

   if (S_ISDIR(s.st_mode))
   {
      dirent **namelist;
      int n = scandir(path.c_str(), &namelist, 0, alphasort);

      if (n < 0)
         return -1;

      for (int i = 0; i < n; ++ i)
      {
         // skip "." and ".." and hidden directory
         if (namelist[i]->d_name[0] == '.')
         {
            free(namelist[i]);
            continue;
         }

         string subdir = path + "/" + namelist[i]->d_name;

         if (stat64(subdir.c_str(), &s) < 0)
            continue;

         if (S_ISDIR(s.st_mode))
            getFileList(subdir, fl);
         else
            fl.push_back(subdir);
      }
   }

   return fl.size();
}

int main(int argc, char** argv)
{
   if (3 != argc)
   {
      cout << "usage: upload <src file/dir> <dst dir>" << endl;
      return 0;
   }

   Sector client;

   Session s;
   s.loadInfo("../../conf/client.conf");

   if (client.init(s.m_ClientConf.m_strMasterIP, s.m_ClientConf.m_iMasterPort) < 0)
      return -1;
   if (client.login(s.m_ClientConf.m_strUserName, s.m_ClientConf.m_strPassword, s.m_ClientConf.m_strCertificate.c_str()) < 0)
      return -1;


   vector<string> fl;
   bool wc = WildCard::isWildCard(argv[1]);
   if (!wc)
   {
      struct stat64 st;
      if (stat64(argv[1], &st) < 0)
      {
         cout << "ERROR: source file does not exist.\n";
         return -1;
      }
      getFileList(argv[1], fl);
   }
   else
   {
      string path = argv[1];
      string orig = path;
      size_t p = path.rfind('/');
      if (p == string::npos)
         path = "/";
      else
      {
         path = path.substr(0, p);
         orig = orig.substr(p + 1, orig.length() - p);
      }

      dirent **namelist;
      int n = scandir(path.c_str(), &namelist, 0, alphasort);

      if (n < 0)
         return -1;

      for (int i = 0; i < n; ++ i)
      {
         // skip "." and ".." and hidden directory
         if (namelist[i]->d_name[0] == '.')
         {
            free(namelist[i]);
            continue;
         }

         if (WildCard::match(orig, namelist[i]->d_name))
            getFileList(path + "/" + namelist[i]->d_name, fl);
      }
   }


   string olddir;
   for (int i = strlen(argv[1]) - 1; i >= 0; -- i)
   {
      if (argv[1][i] != '/')
      {
         olddir = string(argv[1]).substr(0, i);
         break;
      }
   }
   size_t p = olddir.rfind('/');
   if (p == string::npos)
      olddir = "";
   else
      olddir = olddir.substr(0, p);

   string newdir = argv[2];
   SNode attr;
   int r = client.stat(newdir, attr);
   if ((r < 0) || (!attr.m_bIsDir))
   {
      cout << "destination directory on Sector does not exist.\n";
      return -1;
   }

   for (vector<string>::const_iterator i = fl.begin(); i != fl.end(); ++ i)
   {
      string dst = *i;
      if (olddir.length() > 0)
         dst.replace(0, olddir.length(), newdir);
      else
         dst = newdir + "/" + dst;

      struct stat64 s;
      if (stat64(i->c_str(), &s) < 0)
         continue;

      if (S_ISDIR(s.st_mode))
         client.mkdir(dst);
      else
         upload(i->c_str(), dst.c_str(), client);
   }

   client.logout();
   client.close();

   return 1;
}
