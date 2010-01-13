#include <dcclient.h>
#include <util.h>
#include <iostream>

using namespace std;

int main(int argc, char** argv)
{
   if (1 != argc)
   {
      cout << "usage: wordcount" << endl;
      return 0;
   }

   Sector client;

   Session se;
   se.loadInfo("../../conf/client.conf");

   if (client.init(se.m_ClientConf.m_strMasterIP, se.m_ClientConf.m_iMasterPort) < 0)
      return -1;
   if (client.login(se.m_ClientConf.m_strUserName, se.m_ClientConf.m_strPassword, se.m_ClientConf.m_strCertificate.c_str()) < 0)
      return -1;

   vector<string> files;
   files.insert(files.end(), "/html");

   SphereStream s;
   if (s.init(files) < 0)
   {
      cout << "unable to locate input data files. quit.\n";
      return -1;
   }

   SphereStream temp;
   temp.setOutputPath("/wordcount", "word_bucket");
   temp.init(256);

   SphereProcess* myproc = client.createSphereProcess();

   if (myproc->loadOperator("./funcs/wordbucket.so") < 0)
      return -1;

   timeval t;
   gettimeofday(&t, 0);
   cout << "start time " << t.tv_sec << endl;

   if (myproc->run(s, temp, "wordbucket", 0) < 0)
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
         if (myproc->checkProgress() < 0)
         {
            cerr << "all SPEs failed\n";
            break;
         }

         if (myproc->checkProgress() == 100)
            break;
      }
      else
      {
         delete res;
      }

      gettimeofday(&t2, 0);
      if (t2.tv_sec - t1.tv_sec > 60)
      {
         cout << "PROGRESS: " << myproc->checkProgress() << "%" << endl;
         t1 = t2;
      }
   }

   gettimeofday(&t, 0);
   cout << "stage 1 accomplished " << t.tv_sec << endl;

/*
   //NOT FINISHED. PROCESS EACH BUCKET AND GENERATE INDEX

   SphereStream output;
   output.init(0);
   myproc->setProcNumPerNode(2);
   if (myproc->run(temp, output, "index", 0, NULL, 0) < 0)
   {
      cout << "failed to find any computing resources." << endl;
      return -1;
   }

   gettimeofday(&t1, 0);
   t2 = t1;
   while (true)
   {
      SphereResult* res = NULL;

      if (-1 == myproc->read(res))
      {
         if (myproc->checkProgress() < 0)
         {
            cerr << "all SPEs failed\n";
            break;
         }

         if (myproc->checkProgress() == 100)
            break;
      }
      else
      {
         delete res;
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
*/
   cout << "SPE COMPLETED " << endl;

   myproc->close();
   client.releaseSphereProcess(myproc);

   client.logout();
   client.close();

   return 0;
}
