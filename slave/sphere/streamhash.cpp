#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <set>
#include <sphere.h>

using namespace std;

extern "C"
{

inline int getBucketID(const char* text)
{
   if (NULL == text)
      return 0;

   char idstr[64];

   for (int i = 0, n = strlen(text); (i < n) && (i < 64) ;++ i)
   {
      idstr[i] = text[i];
      if ((idstr[i] == ' ') || (idstr[i] == '\t'))
      {
         idstr[i] = '\0';
         break;
      }
   }

   return atoi(idstr);
}

int streamhash(const SInput* input, SOutput* output, SFile* file)
{
   string html = file->m_strHomeDir + input->m_pcUnit;
   cout << "processing " << html << endl;

   output->m_iRows = 0;
   output->m_pllIndex[0] = 0;

   ifstream ifs(html.c_str(), ios::in | ios::binary);
   if (ifs.bad() || ifs.fail())
   {
      cout << "failed reading input file " << html << endl;
      return 0;
   }

   ifs.seekg(output->m_llOffset);

   char* buffer = new char[65536];
   while(!ifs.eof())
   {
      ifs.getline(buffer, 65536);
      if (strlen(buffer) <= 0)
         continue;

      int bid = getBucketID(buffer);
      output->m_piBucketID[output->m_iRows] = bid;
      memcpy(output->m_pcResult + output->m_pllIndex[output->m_iRows], buffer, strlen(buffer) + 1);
      *(output->m_pcResult + output->m_pllIndex[output->m_iRows] + strlen(buffer)) = '\n';
      output->m_iRows ++;
      output->m_pllIndex[output->m_iRows] = output->m_pllIndex[output->m_iRows - 1] + strlen(buffer) + 1;

      if ((output->m_pllIndex[output->m_iRows] + 65536 >= output->m_iBufSize) || (output->m_iRows + 1 >= output->m_iIndSize))
      {
         output->m_llOffset = ifs.tellg();
         break;
      }
   }

   ifs.close();
   delete [] buffer;

   output->m_iResSize = output->m_pllIndex[output->m_iRows];

   return 0;
}

}
