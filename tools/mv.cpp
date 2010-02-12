#include <iostream>
#include <sector.h>
#include <conf.h>

using namespace std;

int main(int argc, char** argv)
{
   if (argc != 3)
   {
      cout << "USAGE: mv <src_file/dir> <dst_file/dir>\n";
      return -1;
   }

   Sector client;

   Session s;
   s.loadInfo("../conf/client.conf");

   if (client.init(s.m_ClientConf.m_strMasterIP, s.m_ClientConf.m_iMasterPort) < 0)
      return -1;
   if (client.login(s.m_ClientConf.m_strUserName, s.m_ClientConf.m_strPassword, s.m_ClientConf.m_strCertificate.c_str()) < 0)
      return -1;

   string path = argv[1];
   bool wc = WildCard::isWildCard(path);

   if (!wc)
   {
      int r = client.move(argv[1], argv[2]);
      if (r < 0)
         cout << "ERROR: " << r << " " << SectorError::getErrorMsg(r) << endl;
   }
   else
   {
      SNode attr;
      if (client.stat(argv[2], attr) < 0)
      {
         cout << "destination directory does not exist.\n";
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
         int r = client.list(path, filelist);
         if (r < 0)
            cout << "ERROR: " << r << " " << SectorError::getErrorMsg(r) << endl;

         vector<string> filtered;
         for (vector<SNode>::iterator i = filelist.begin(); i != filelist.end(); ++ i)
         {
            if (WildCard::match(orig, i->m_strName))
               filtered.push_back(path + "/" + i->m_strName);
         }

         for (vector<string>::iterator i = filtered.begin(); i != filtered.end(); ++ i)
            client.move(*i, argv[2]);
      }
   }

   client.logout();
   client.close();

   return 1;
}
