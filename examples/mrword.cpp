#include <sector.h>
#include <conf.h>
#include <sys/time.h>
#include <iostream>

using namespace std;

int main(int argc, char** argv)
{
   if (1 != argc)
   {
      cout << "usage: mrsort" << endl;
      return 0;
   }

   Sector client;

   Session s;
   s.loadInfo("../conf/client.conf");

   if (client.init(s.m_ClientConf.m_strMasterIP, s.m_ClientConf.m_iMasterPort) < 0)
      return -1;
   if (client.login(s.m_ClientConf.m_strUserName, s.m_ClientConf.m_strPassword, s.m_ClientConf.m_strCertificate.c_str()) < 0)
      return -1;

   vector<string> files;
   files.insert(files.end(), "/html");

   SphereStream input;
   if (input.init(files) < 0)
   {
      cout << "unable to locate input data files. quit.\n";
      return -1;
   }

   SphereStream output;
   output.setOutputPath("/mrword", "inverted_index");
   output.init(256);

   SphereProcess* myproc = client.createSphereProcess();

   if (myproc->loadOperator("./funcs/mr_word.so") < 0)
      return -1;

   timeval t;
   gettimeofday(&t, 0);
   cout << "start time " << t.tv_sec << endl;

   if (myproc->run_mr(input, output, "mr_word", 0) < 0)
   {
      cout << "failed to find any computing resources." << endl;
      return -1;
   }

   timeval t1, t2;
   gettimeofday(&t1, 0);
   t2 = t1;
   while (true)
   {
      SphereResult* res = NULL;

      if (myproc->read(res) <= 0)
      {
         if (myproc->checkMapProgress() <= 0)
         {
            cerr << "all SPEs failed\n";
            break;
         }

         if (myproc->checkMapProgress() == 100)
            break;
      }
      else
      {
         delete res;
      }

      gettimeofday(&t2, 0);
      if (t2.tv_sec - t1.tv_sec > 60)
      {
         cout << "MAP PROGRESS: " << myproc->checkProgress() << "%" << endl;
         t1 = t2;
      }
   }

   while (myproc->checkReduceProgress() < 100)
   {
      usleep(10);
   }

   gettimeofday(&t, 0);
   cout << "mapreduce sort accomplished " << t.tv_sec << endl;

   cout << "SPE COMPLETED " << endl;

   myproc->close();
   client.releaseSphereProcess(myproc);

   client.logout();
   client.close();

   return 0;
}
