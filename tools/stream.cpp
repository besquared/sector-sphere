#include <sector.h>
#include <conf.h>
#include <probot.h>
#include <cstdlib>
#include <sys/time.h>
#include <iostream>

using namespace std;

void help()
{
   cout << "stream -i input [-o output] -c command [-b buckets] [-p parameters] [-f files]" << endl;
   cout << endl;
   cout << "-i: input file or directory" << endl;
   cout << "-o: output file or directory (optional)" << endl;
   cout << "-b: number of buckets (optional)" << endl;
   cout << "-c: command or program" << endl;
   cout << "-p: parameters (optional)" << endl;
   cout << "-f: file to upload to Sector servers (optional)" << endl;
}

int main(int argc, char** argv)
{
   CmdLineParser clp;
   if (clp.parse(argc, argv) <= 0)
   {
      help();
      return 0;
   }
   
   string inpath = "";
   string outpath = "";
   string tmpdir = "";
   string cmd = "";
   string parameter = "";
   int bucket = 0;
   string upload = "";

   for (map<string, string>::const_iterator i = clp.m_mParams.begin(); i != clp.m_mParams.end(); ++ i)
   {
      if (i->first == "i")
         inpath = i->second;
      else if (i->first == "o")
         outpath = i->second;
      else if (i->first == "c")
         cmd = i->second;
      else if (i->first == "p")
         parameter = i->second;
      else if (i->first == "b")
         bucket = atoi(i->second.c_str());
      else if (i->first == "f")
         upload = i->second;
      else
      {
         help();
         return 0;
      }
   }

   if ((inpath.length() == 0) || (cmd.length() == 0))
   {
      help();
      return 0;
   }

   if (bucket > 0)
   {
      if (outpath.length() == 0)
      {
         help();
         return 0;
      }

      tmpdir = outpath + "/temp";
   }

   PRobot pr;
   pr.setCmd(cmd);
   pr.setParam(parameter);
   pr.setCmdFlag(upload.length() != 0);
   if (bucket <= 0)
      pr.setOutput(outpath);
   else
      pr.setOutput(tmpdir);
   pr.generate();
   pr.compile();

   //if pr.fail()
   //{
   //   cerr << "unable to create UDFs." << endl;
   //   return -1;
   //}

   Sector client;

   Session s;
   s.loadInfo("../conf/client.conf");

   if (client.init(s.m_ClientConf.m_strMasterIP, s.m_ClientConf.m_iMasterPort) < 0)
      return -1;
   if (client.login(s.m_ClientConf.m_strUserName, s.m_ClientConf.m_strPassword, s.m_ClientConf.m_strCertificate.c_str()) < 0)
      return -1;

   vector<string> files;
   files.insert(files.end(), inpath);

   SphereStream input;
   if (input.init(files) < 0)
   {
      cout << "unable to locate input data files. quit.\n";
      return -1;
   }

   if (client.mkdir(outpath) == SectorError::E_PERMISSION)
   {
      cout << "unable to create output path " << outpath << endl;
      return -1;
   }

   SphereStream output;
   output.init(0);

   SphereProcess* myproc = client.createSphereProcess();

   if (myproc->loadOperator((string("/tmp/") + cmd + ".so").c_str()) < 0)
      return -1;

   if (upload.length() > 0)
   {
      if (myproc->loadOperator(upload.c_str()) < 0)
         return -1;
   }

   timeval t;
   gettimeofday(&t, 0);
   cout << "start time " << t.tv_sec << endl;

   if (myproc->run(input, output, cmd, 0) < 0)
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

      if (myproc->read(res) < 0)
      {
         if (myproc->checkProgress() < 0)
         {
            cerr << "all SPEs failed\n";
            break;
         }

         if (myproc->checkProgress() == 100)
            break;
      }

      if ((outpath.length() == 0) && (res->m_iDataLen > 0))
      {
         cout << "RESULT " << res->m_strOrigFile << endl;
         cout << res->m_pcData << endl;
      }

      gettimeofday(&t2, 0);
      if (t2.tv_sec - t1.tv_sec > 60)
      {
         cout << "PROGRESS: " << myproc->checkProgress() << "%" << endl;
         t1 = t2;
      }
   }


   // If no buckets defined, stop
   // otherwise parse all temporary files in tmpdir and generate buckets

   if (bucket > 0)
   {
      SphereStream input2;
      vector<string> tmpdata;
      tmpdata.push_back(tmpdir);
      input2.init(tmpdata);

      SphereStream output2;
      output2.setOutputPath(outpath, "stream_result");
      output2.init(bucket);

      if (myproc->run(input2, output2, "streamhash", 0) < 0)
      {
         cout << "failed to find any computing resources." << endl;
         return -1;
      }

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

      client.rmr(tmpdir);
   }


   gettimeofday(&t, 0);
   cout << "mission accomplished " << t.tv_sec << endl;

   myproc->close();
   client.releaseSphereProcess(myproc);

   client.logout();
   client.close();

   return 0;
}
