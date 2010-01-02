#include <iostream>
#include <fstream>
#include <sphere.h>

using namespace std;

extern "C"
{

int gen_idx(const SInput* input, SOutput* output, SFile* file)
{
   string textfile = file->m_strHomeDir + input->m_pcUnit;

   cout << "generating index for text file " << textfile << endl;

   string idxfile = textfile + ".idx";

   ifstream in(textfile.c_str(), ios::binary);
   ofstream out(idxfile.c_str(), ios::binary);

   // maximum line is 65536 charactors
   char* buf = new char[65536];
   int64_t offset = 0;
   out.write((char*)&offset, 8);

   while (!in.eof())
   {
      in.getline(buf, 65536);
      offset = in.tellg();

      if (offset < 0)
         break;

      out.write((char*)&offset, 8);
   }

   delete [] buf;

   in.close();
   out.close();

   output->m_iRows = 0;
   file->m_sstrFiles.insert(input->m_pcUnit + string(".idx"));


   ////////////
   char* block = (char*)file->m_pInMemoryObjects->retrieve("/memory/mystuff");
   cout << "HAHAHAHAHA " << block[0] << endl;
   delete [] block;
   file->m_pInMemoryObjects->remove("/memory/mystuff");

   file->m_sstrFiles.insert("/memory/mystuff");


   return 0;
}

}
