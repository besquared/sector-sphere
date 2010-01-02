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
   Yunhong Gu, last updated 12/11/2009
*****************************************************************************/

#include <string.h>
#include <meta.h>

using namespace std;

Metadata::Metadata()
{
}

Metadata::~Metadata()
{
}

int Metadata::parsePath(const string& path, vector<string>& result)
{
   result.clear();

   char* token = new char[path.length() + 1];
   int tc = 0;
   for (unsigned int i = 0, n = path.length(); i < n; ++ i)
   {
      // check invalid/special charactor such as * ; , etc.

      if (path.c_str()[i] == '/')
      {
         if (tc > 0)
         {
            token[tc] = '\0';
            result.insert(result.end(), token);
            tc = 0;
         }
      }
      else
        token[tc ++] = path.c_str()[i];
   }

   if (tc > 0)
   {
      token[tc] = '\0';
      result.insert(result.end(), token);
   }

   delete [] token;

   return result.size();
}

string Metadata::revisePath(const string& path)
{
   char* newpath = new char[path.length() + 2];
   char* np = newpath;
   *np++ = '/';
   bool slash = true;

   for (char* p = (char*)path.c_str(); *p != '\0'; ++ p)
   {
      if (*p == '/')
      {
         if (!slash)
            *np++ = '/';
         slash = true;
      }
      else
      {
         *np++ = *p;
         slash = false;
      }
   }
   *np = '\0';

   if ((strlen(newpath) > 1) && slash)
      newpath[strlen(newpath) - 1] = '\0';

   string tmp = newpath;
   delete [] newpath;

   return tmp;
}
