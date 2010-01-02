#ifdef WIN32
   #include <windows.h>
#else
   #include <unistd.h>
   #include <sys/ioctl.h>
#endif

#include <fstream>
#include <fsclient.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <util.h>

using namespace std;

int download(const char* file, const char* dest)
{
   #ifndef WIN32
      timeval t1, t2;
   #else
      DWORD t1, t2;
   #endif

   #ifndef WIN32
      gettimeofday(&t1, 0);
   #else
      t1 = GetTickCount();
   #endif

   SNode attr;
   if (Sector::stat(file, attr) < 0)
   {
      cout << "ERROR: cannot locate file " << file << endl;
      return -1;
   }

   if (attr.m_bIsDir)
   {
      ::mkdir((string(dest) + "/" + file).c_str(), S_IRWXU);
      return 1;
   }

   long long int size = attr.m_llSize;
   cout << "downloading " << file << " of " << size << " bytes" << endl;

   SectorFile f;

   if (f.open(file) < 0)
   {
      cout << "unable to locate file" << endl;
      return -1;
   }

   int sn = strlen(file) - 1;
   for (; sn >= 0; sn --)
   {
      if (file[sn] == '/')
         break;
   }
   string localpath;
   if (dest[strlen(dest) - 1] != '/')
      localpath = string(dest) + string("/") + string(file + sn + 1);
   else
      localpath = string(dest) + string(file + sn + 1);

   bool finish = true;
   if (f.download(localpath.c_str(), true) < 0)
      finish = false;

   f.close();

   if (finish)
   {
      #ifndef WIN32
         gettimeofday(&t2, 0);
         float throughput = size * 8.0 / 1000000.0 / ((t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1000000.0);
      #else
         float throughput = size * 8.0 / 1000000.0 / ((GetTickCount() - t1) / 1000.0);
      #endif

      cout << "Downloading accomplished! " << "AVG speed " << throughput << " Mb/s." << endl << endl ;

      return 1;
   }

   return -1;
}

int getFileList(const string& path, vector<string>& fl)
{
   SNode attr;
   if (Sector::stat(path.c_str(), attr) < 0)
      return -1;

   fl.push_back(path);

   if (attr.m_bIsDir)
   {
      vector<SNode> subdir;
      Sector::list(path, subdir);

      for (vector<SNode>::iterator i = subdir.begin(); i != subdir.end(); ++ i)
      {
         if (i->m_bIsDir)
            getFileList(path + "/" + i->m_strName, fl);
         else
            fl.push_back(path + "/" + i->m_strName);
      }
   }

   return fl.size();
}

int main(int argc, char** argv)
{
   if (argc != 3)
   {
      cout << "USAGE: download <src file/dir> <local dir>\n";
      return -1;
   }

   Session s;
   s.loadInfo("../../conf/client.conf");

   if (Sector::init(s.m_ClientConf.m_strMasterIP, s.m_ClientConf.m_iMasterPort) < 0)
   {
      cout << "unable to connect to the server at " << argv[1] << endl;
      return -1;
   }
   if (Sector::login(s.m_ClientConf.m_strUserName, s.m_ClientConf.m_strPassword, s.m_ClientConf.m_strCertificate.c_str()) < 0)
   {
      cout << "login failed\n";
      return -1;
   }


   vector<string> fl;
   bool wc = WildCard::isWildCard(argv[1]);
   if (!wc)
   {
      SNode attr;
      if (Sector::stat(argv[1], attr) < 0)
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

      vector<SNode> filelist;
      int r = Sector::list(path, filelist);
      if (r < 0)
         cout << "ERROR: " << r << " " << SectorError::getErrorMsg(r) << endl;

      for (vector<SNode>::iterator i = filelist.begin(); i != filelist.end(); ++ i)
      {
         if (WildCard::match(orig, i->m_strName))
            getFileList(path + "/" + i->m_strName, fl);
      }
   }


   struct stat64 st;
   int r = stat64(argv[2], &st);
   if ((r < 0) || !S_ISDIR(st.st_mode))
   {
      cout << "ERROR: destination directory does not exist.\n";
      return -1;
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

   for (vector<string>::iterator i = fl.begin(); i != fl.end(); ++ i)
   {
      string dst = *i;
      if (olddir.length() > 0)
         dst.replace(0, olddir.length(), newdir);
      else
         dst = newdir + "/" + dst;

      string localdir = dst.substr(0, dst.rfind('/'));

      // if localdir does not exist, create it
      if (stat64(localdir.c_str(), &st) < 0)
      {
         for (unsigned int p = 0; p < localdir.length(); ++ p)
         {
            if (localdir.c_str()[p] == '/')
            {
               string substr = localdir.substr(0, p);

               if ((-1 == ::mkdir(substr.c_str(), S_IRWXU)) && (errno != EEXIST))
               {
                  cout << "ERROR: unable to create local directory " << substr << endl;
                  return -1;
               }
            }
         }

         if ((-1 == ::mkdir(localdir.c_str(), S_IRWXU)) && (errno != EEXIST))
         {
            cout << "ERROR: unable to create local directory " << localdir << endl;
            return -1;
         }
      }

      download(i->c_str(), localdir.c_str());
   }

   Sector::logout();
   Sector::close();

   return 1;
}
