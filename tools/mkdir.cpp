#include <iostream>
#include <sector.h>
#include <conf.h>

using namespace std;

int main(int argc, char** argv)
{
   if (argc != 2)
   {
      cout << "USAGE: mkdir <dir>\n";
      return -1;
   }

   Sector client;

   Session s;
   s.loadInfo("../conf/client.conf");

   if (client.init(s.m_ClientConf.m_strMasterIP, s.m_ClientConf.m_iMasterPort) < 0)
      return -1;
   if (client.login(s.m_ClientConf.m_strUserName, s.m_ClientConf.m_strPassword, s.m_ClientConf.m_strCertificate.c_str()) < 0)
      return -1;

   int r = client.mkdir(argv[1]);
   if (r < 0)
      cout << "ERROR: " << r << " " << SectorError::getErrorMsg(r) << endl;

   client.logout();
   client.close();

   return 1;
}
