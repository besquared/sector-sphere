#include <iostream>
#include <fstream>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <cstring>
#include <cstdlib>
#include <sphere.h>

using namespace std;

extern "C"
{

// 10 byte key, 90 byte value
struct Key
{
   uint32_t k1;
   uint32_t k2;
   uint16_t k3;
};

void keyinit();
void keygen(char* key);

int randwriter(const SInput* input, SOutput* output, SFile* file)
{
   // input->m_pcUnit: file ID
   // input->m_pcParam: filename prefix: e.g., /test/sortinput
   // target file is $SECTOR_HOME/test/sortinput.i.dat

   // mkdir
   string rname = input->m_pcParam;
   int slash = rname.find('/', 1);
   while (slash != string::npos)
   {
      ::mkdir((file->m_strHomeDir + rname.substr(0, slash)).c_str(), S_IRWXU);
      slash = rname.find('/', slash + 1);
   }

   char filename[256];
   sprintf(filename, "%s.%d.dat", input->m_pcParam, *(int32_t*)input->m_pcUnit);

   ofstream ofs;
   ofs.open((file->m_strHomeDir + filename).c_str(), ios::out | ios::binary | ios::trunc);

   if (ofs.bad() || ofs.fail())
      return -1;

   char record[100];
   
   keyinit();

   //10GB = 100 * 100000000
   for (long long int i = 0; i < 100000000; ++ i)
   {
      keygen(record);
      ofs.write(record, 100);
   }

   ofs.close();

   ofstream idx((file->m_strHomeDir + filename + ".idx").c_str(), ios::out | ios::binary | ios::trunc);

   for (long long int i = 0; i < 100000001; ++ i)
   {
      long long int d = i * 100;
      idx.write((char*)&d, 8);
   }

   idx.close();

   output->m_iRows = 0;

   file->m_sstrFiles.insert(filename);
   file->m_sstrFiles.insert(string(filename) + ".idx");

   return 0;
}

void keyinit()
{
   timeval t;
   gettimeofday(&t, 0);
   srand(t.tv_usec);
}

void keygen(char* key)
{
   int r = rand();
   *(int*)key = r;
   r = rand();
   *(int*)(key + 4) = r;
   r = rand();
   *(int*)(key + 8) = r;
}

}
