#include <iostream>
#include <sector.h>
#include <conf.h>

using namespace std;

int main(int argc, char** argv)
{
   if (argc != 2)
   {
      cout << "USAGE: stat file\n";
      return -1;
   }

   Sector client;

   Session s;
   s.loadInfo("../conf/client.conf");

   if (client.init(s.m_ClientConf.m_strMasterIP, s.m_ClientConf.m_iMasterPort) < 0)
      return -1;
   if (client.login(s.m_ClientConf.m_strUserName, s.m_ClientConf.m_strPassword, s.m_ClientConf.m_strCertificate.c_str()) < 0)
      return -1;

   SNode attr;
   int r = client.stat(argv[1], attr);

   if (r < 0)
   {
      cout << "ERROR: " << r << " " << SectorError::getErrorMsg(r) << endl;
   }
   else
   {
      cout << "FILE NAME: " << attr.m_strName << endl;
      if (attr.m_bIsDir)
         cout << "DIR: TRUE\n";
      else
         cout << "DIR: FALSE\n";
      cout << "SIZE: " << attr.m_llSize << " bytes" << endl;
      time_t ft = attr.m_llTimeStamp;
      cout << "LAST MODIFIED: " << ctime(&ft);
      cout << "LOCATION: ";
      for (set<Address, AddrComp>::iterator i = attr.m_sLocation.begin(); i != attr.m_sLocation.end(); ++ i)
      {
         cout << i->m_strIP << ":" << i->m_iPort << " ";
      }
      cout << endl;
   }

   client.logout();
   client.close();

   return 1;
}
