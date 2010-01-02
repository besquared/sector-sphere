#include <dcclient.h>
#include <util.h>
#include <iostream>
#include <cmath>

using namespace std;

int main(int argc, char** argv)
{
   if (1 != argc)
   {
      cout << "usage: testdc" << endl;
      return 0;
   }

   Session se;
   se.loadInfo("../../conf/client.conf");

   if (Sector::init(se.m_ClientConf.m_strMasterIP, se.m_ClientConf.m_iMasterPort) < 0)
      return -1;
   if (Sector::login(se.m_ClientConf.m_strUserName, se.m_ClientConf.m_strPassword, se.m_ClientConf.m_strCertificate.c_str()) < 0)
      return -1;

   // remove result of last run
   Sector::rmr("/test/sorted");

   SysStat sys;
   Sector::sysinfo(sys);
   const int fn = sys.m_llTotalSlaves;
   const int32_t N = (int)log2(fn * 50.0f);
   const int rn = (int)pow(2.0f, N);

   vector<string> files;
   files.push_back("test");

   SphereStream s;
   if (s.init(files) < 0)
   {
      cout << "unable to locate input data files. quit.\n";
      return -1;
   }

   SphereStream temp;
   temp.setOutputPath("test/sorted", "stream_sort_bucket");
   temp.init(rn);

   SphereProcess myproc;

   if (myproc.loadOperator("./funcs/sorthash.so") < 0)
      return -1;
   if (myproc.loadOperator("./funcs/sort.so") < 0)
      return -1;

   timeval t;
   gettimeofday(&t, 0);
   cout << "start time " << t.tv_sec << endl;

   if (myproc.run(s, temp, "sorthash", 1, (char*)&N, 4) < 0)
   {
      cout << "failed to find any computing resources." << endl;
      return -1;
   }

   timeval t1, t2;
   gettimeofday(&t1, 0);
   t2 = t1;
   while (true)
   {
      SphereResult* res;

      if (myproc.read(res) < 0)
      {
         if (myproc.checkProgress() < 0)
         {
            cerr << "all SPEs failed\n";
            break;
         }

         if (myproc.checkProgress() == 100)
            break;
      }

      gettimeofday(&t2, 0);
      if (t2.tv_sec - t1.tv_sec > 60)
      {
         cout << "PROGRESS: " << myproc.checkProgress() << "%" << endl;
         t1 = t2;
      }
   }

   gettimeofday(&t, 0);
   cout << "stage 1 accomplished " << t.tv_sec << endl;

   SphereStream output;
   output.init(0);
   myproc.setProcNumPerNode(2);
   if (myproc.run(temp, output, "sort", 0, NULL, 0) < 0)
   {
      cout << "failed to find any computing resources." << endl;
      return -1;
   }

   gettimeofday(&t1, 0);
   t2 = t1;
   while (true)
   {
      SphereResult* res;

      if (myproc.read(res) < 0)
      {
         if (myproc.checkProgress() < 0)
         {
            cerr << "all SPEs failed\n";
            break;
         }

         if (myproc.checkProgress() == 100)
            break;
      }

      gettimeofday(&t2, 0);
      if (t2.tv_sec - t1.tv_sec > 60)
      {
         cout << "PROGRESS: " << myproc.checkProgress() << "%" << endl;
         t1 = t2;
      }
   }

   gettimeofday(&t, 0);
   cout << "stage 2 accomplished " << t.tv_sec << endl;

   cout << "SPE COMPLETED " << endl;

   myproc.close();

   Sector::logout();
   Sector::close();

   return 0;
}
