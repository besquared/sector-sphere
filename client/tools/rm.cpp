#include <fsclient.h>
#include <util.h>
#include <iostream>

using namespace std;

bool isRecursive(const string& path)
{
   cout << "Directory " << path << " is not empty. Force to remove? Y/N: ";
   char input;
   cin >> input;

   return (input == 'Y') || (input == 'y');
}

int main(int argc, char** argv)
{
   if (argc != 2)
   {
      cout << "USAGE: rm <dir>\n";
      return -1;
   }

   Session s;
   s.loadInfo("../../conf/client.conf");

   if (Sector::init(s.m_ClientConf.m_strMasterIP, s.m_ClientConf.m_iMasterPort) < 0)
      return -1;
   if (Sector::login(s.m_ClientConf.m_strUserName, s.m_ClientConf.m_strPassword, s.m_ClientConf.m_strCertificate.c_str()) < 0)
      return -1;

   string path = argv[1];
   bool wc = WildCard::isWildCard(path);

   if (!wc)
   {
      int r = Sector::remove(path);

      if (r == SectorError::E_NOEMPTY)
      {
         if (isRecursive(path))
            Sector::rmr(path);
      }
      else if (r < 0)
         cout << "ERROR: " << r << " " << SectorError::getErrorMsg(r) << endl;
   }
   else
   {
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

      bool recursive = false;

      vector<string> filtered;
      for (vector<SNode>::iterator i = filelist.begin(); i != filelist.end(); ++ i)
      {
         if (WildCard::match(orig, i->m_strName))
         {
            if (recursive)
               Sector::rmr(path + "/" + i->m_strName);
            else
            {
               r = Sector::remove(path + "/" + i->m_strName);

               if (r == SectorError::E_NOEMPTY)
               {
                  recursive = isRecursive(path + "/" + i->m_strName);
                  if (recursive)
                     Sector::rmr(path + "/" + i->m_strName);
               }
               else if (r < 0)
                  cout << "ERROR: " << r << " " << SectorError::getErrorMsg(r) << endl;
            }
         }
      }
   }

   Sector::logout();
   Sector::close();

   return 1;
}
