#include <iostream>
#include <fstream>
#include <cstring>
#include <set>
#include <sphere.h>

using namespace std;

extern "C"
{

inline int wordid(const char* text)
{
   if (NULL == text)
      return 0;

   return text[0];
}

int wordbucket(const SInput* input, SOutput* output, SFile* file)
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

   int delim[256];
   for (int i = 0; i < 256; ++ i)
      delim[i] = 0;
   for (int i = 48; i <= 57; ++ i)
      delim[i] = 1;
   for (int i = 65; i <= 90; ++ i)
      delim[i] = 1;
   for (int i = 97; i <= 122; ++ i)
      delim[i] = 1;

   set<string> wordset;

   char* buffer = new char[65536];
   while(!ifs.eof())
   {
      ifs.getline(buffer, 65536);
      if (strlen(buffer) <= 0)
         continue;

      char* token = NULL;
      char* end = buffer + strlen(buffer);
      bool tag = false;
      for (char* p = buffer; p != end; ++ p)
      {
         if ('<' == *p)
            tag = true;
         if (tag)
         {
            if ('>' == *p)
               tag = false;
            continue;
         }

         if ((1 == delim[*p]) && (NULL == token))
            token = p;
         else if ((0 == delim[*p]) && (NULL != token))
         {
            *p = '\0';
            wordset.insert(token);
            token = NULL;
         }
      }
   }
   delete [] buffer;

   for(set<string>::iterator i = wordset.begin(); i != wordset.end(); ++ i)
   {
      //TODO: check size of output->m_pcResult, output->m_pllIndex, and output->m_piBucketID
      // resize these buffers if possible

      char item[64];
      sprintf(item, "%s %s\n", i->c_str(), input->m_pcUnit);
      output->m_piBucketID[output->m_iRows] = wordid(i->c_str());
      strcpy(output->m_pcResult + output->m_pllIndex[output->m_iRows], item);
      output->m_iRows ++;
      output->m_pllIndex[output->m_iRows] = output->m_pllIndex[output->m_iRows - 1] + strlen(item) + 1;
   }

   output->m_iResSize = output->m_pllIndex[output->m_iRows];
   wordset.clear();
   return 0;
}

}
