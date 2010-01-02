#include <iostream>
#include <fstream>
#include <cstring>
#include <set>
#include <vector>
#include <sphere.h>

using namespace std;

extern "C"
{

int mr_word_map(const SInput* input, SOutput* output, SFile* file)
{
   string html = file->m_strHomeDir + input->m_pcUnit;
   cout << "processing " << html << endl;

   output->m_iRows = 0;
   output->m_pllIndex[0] = 0;

   ifstream ifs(html.c_str());
   if (ifs.bad() || ifs.fail())
      return 0;

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
   buffer[65535] = '\0';
   while(!ifs.eof())
   {
      ifs.getline(buffer, 65535);
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
      if (i->length() > 256)
         continue;

      //TODO: check size of output->m_pcResult, output->m_pllIndex
      // resize these buffers if possible

      char item[1024];
      sprintf(item, "%s %s\n", i->c_str(), input->m_pcUnit);
      strcpy(output->m_pcResult + output->m_pllIndex[output->m_iRows], item);
      output->m_iRows ++;
      output->m_pllIndex[output->m_iRows] = output->m_pllIndex[output->m_iRows - 1] + strlen(item) + 1;
   }

   output->m_iResSize = output->m_pllIndex[output->m_iRows];
   output->m_llOffset = -1;
   wordset.clear();
   return 0;

}

int mr_word_partition(const char* record, int size, void* param, int psize)
{
   if (NULL == record)
      return 0;

   return record[0];
}

int mr_word_compare(const char* r1, int s1, const char* r2, int s2)
{
   char* p1 = (char*)r1;
   char* p2 = (char*)r2;
   while (*p1 != ' ')
      ++ p1;
   *p1 = '\0';
   while (*p2 != ' ')
      ++ p2;
   *p2 = '\0';

   int res = strcmp(r1, r2);
   *p1 = ' ';
   *p2 = ' ';

   return res;
}

int mr_word_reduce(const SInput* input, SOutput* output, SFile* file)
{
   vector<string> docs;
   for (int i = 0; i < input->m_iRows; ++ i)
   {
      char* p = input->m_pcUnit + input->m_pllIndex[i];
      while (*p != ' ')
         ++ p;
      docs.insert(docs.end(), p);
   }

   char* word = (char*)input->m_pcUnit;
   char* p = input->m_pcUnit;
   while (*p != ' ')
      ++ p;
   *p = '\0';
   sprintf(output->m_pcResult, "%s", word);
   *p = ' ';

   for (vector<string>::iterator i = docs.begin(); i != docs.end(); ++ i)
      sprintf(output->m_pcResult + strlen(output->m_pcResult), " %s", i->c_str());

   output->m_iRows = 1;
   output->m_pllIndex[0] = 0;
   output->m_pllIndex[1] = strlen(output->m_pcResult);

   return 1;
}

}
