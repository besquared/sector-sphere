/*****************************************************************************
Copyright (c) 2005 - 2009, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu, last updated 07/16/2009
*****************************************************************************/


#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <probot.h>

using namespace std;

PRobot::PRobot():
m_strSrc(),
m_strCmd(),
m_strParam(),
m_bLocal(false),
m_strOutput()
{
}

void PRobot::setCmd(const string& name)
{
   m_strSrc = name;
   m_strCmd = name;
}

void PRobot::setParam(const string& param)
{
   m_strParam = "";
   for (char* p = (char*)param.c_str(); *p != '\0'; ++ p)
   {
      if (*p == '"')
         m_strParam.push_back('\\');
      m_strParam.push_back(*p);
   }
}

void PRobot::setCmdFlag(const bool& local)
{
   m_bLocal = local;
}

void PRobot::setOutput(const string& output)
{
   m_strOutput = output;
}

int PRobot::generate()
{
   fstream cpp;
   cpp.open((m_strSrc + ".cpp").c_str(), ios::in | ios::out | ios::trunc);

   cpp << "#include <iostream>" << endl;
   cpp << "#include <fstream>" << endl;
   cpp << "#include <cstring>" << endl;
   cpp << "#include <cstdlib>" << endl;
   cpp << "#include <sphere.h>" << endl;
   cpp << endl;
   cpp << "using namespace std;" << endl;
   cpp << endl;
   cpp << "extern \"C\"" << endl;
   cpp << "{" << endl;
   cpp << endl;

   cpp << "int " << m_strCmd << "(const SInput* input, SOutput* output, SFile* file)" << endl;
   cpp << "{" << endl;
   cpp << "   string ifile = file->m_strHomeDir + input->m_pcUnit;" << endl;
   cpp << "   string ofile = ifile + \".result\";" << endl;
   cpp << endl;

   // Python: .py
   // Perl: .pl

   // system((string("") + FUNC + " " + ifile + " " + PARAM + " > " + ofile).c_str());
   cpp << "   system((string(\"\") + ";
   if (m_bLocal)
      cpp << "file->m_strLibDir + \"/\" + ";
   cpp << "\"";
   cpp << m_strCmd;
   cpp << "\" + ";
   cpp << "\" ";
   cpp << m_strParam;
   cpp << "\" + ";
   cpp << " \" \" + ifile + \" \" + ";
   cpp << " \" > \" + ofile).c_str());" << endl;

   cpp << endl;
   if (m_strOutput.length() == 0)
   {
      cpp << "   ifstream dat(ofile.c_str());" << endl;
      cpp << "   dat.seekg(0, ios::end);" << endl;
      cpp << "   int size = dat.tellg();" << endl;
      cpp << "   dat.seekg(0);" << endl;
      cpp << endl;
      cpp << "   output->m_iRows = 1;" << endl;
      cpp << "   output->m_pllIndex[0] = 0;" << endl;
      cpp << "   output->m_pllIndex[1] = size + 1;" << endl;
      cpp << "   dat.read(output->m_pcResult, size);" << endl;
      cpp << "   output->m_pcResult[size] = '\\0';" << endl;
      cpp << "   dat.close();" << endl;
      cpp << "   unlink(ofile.c_str());" << endl;
   }
   else
   {
      cpp << "   output->m_iRows = 0;" << endl;
      cpp << endl;
      cpp << "   string sfile;" << endl;
      cpp << "   for (int i = 1, n = strlen(input->m_pcUnit); i < n; ++ i)" << endl;
      cpp << "   {" << endl;
      cpp << "      if (input->m_pcUnit[i] == '/')" << endl;
      cpp << "         sfile.push_back('.');" << endl;
      cpp << "      else" << endl;
      cpp << "         sfile.push_back(input->m_pcUnit[i]);" << endl;
      cpp << "   }" << endl;
      cpp << "   sfile = string(" << "\"" << m_strOutput << "\")" << " + \"/\" + sfile;" << endl;
      cpp << endl;
      cpp << "   system((\"mkdir -p \" + file->m_strHomeDir + " << "\"" << m_strOutput << "\"" << ").c_str());" << endl;
      cpp << "   system((\"mv \" + ofile + \" \" + file->m_strHomeDir + sfile).c_str());" << endl;
      cpp << endl;
      cpp << "   file->m_sstrFiles.insert(sfile);" << endl;
   }
   cpp << endl;

   cpp << "   return 0;" << endl;
   cpp << "}" << endl;
   cpp << endl;
   cpp << "}" << endl;

   cpp.close();

   return 0;
}

int PRobot::compile()
{
   string CCFLAGS = "-I../ -I../../udt -I../../gmp -I../../common -I../../security";
   system(("g++ " + CCFLAGS + " -shared -fPIC -O3 -o " + m_strSrc + ".so -lstdc++ " + m_strSrc + ".cpp").c_str());

   system(("mv " + m_strSrc + ".* /tmp").c_str());

   return 0;
}
