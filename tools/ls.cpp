#include <iostream>
#include <iomanip>
#include <sector.h>
#include <conf.h>

using namespace std;

int main(int argc, char** argv)
{
   if (argc != 2)
   {
      cout << "USAGE: ls <dir>\n";
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
   string orig = path;
   bool wc = WildCard::isWildCard(path);
   if (wc)
   {
      size_t p = path.rfind('/');
      if (p == string::npos)
         path = "/";
      else
      {
         path = path.substr(0, p);
         orig = orig.substr(p + 1, orig.length() - p);
      }
   }

   vector<SNode> filelist;
   int r = client.list(path, filelist);
   if (r < 0)
   {
      cout << "ERROR: " << r << " " << SectorError::getErrorMsg(r) << endl;
      return -1;
   }

   for (vector<SNode>::iterator i = filelist.begin(); i != filelist.end(); ++ i)
   {
      if (wc && !WildCard::match(orig, i->m_strName))
         continue;

      cout << setiosflags(ios::left) << setw(24) << i->m_strName << "\t";
      if (i->m_bIsDir)
         cout << "<dir>" << endl;
      else
      {
         time_t t = i->m_llTimeStamp;
         cout << i->m_llSize << " bytes " << "\t" << ctime(&t);
      }
   }

   client.logout();
   client.close();

   return 1;
}
