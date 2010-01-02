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
   Yunhong Gu, last updated 06/23/2009
*****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <iostream>

using namespace std;

void gen_cert(const string& name)
{
   string keyname = name + "_node.key";
   string certname = name + "_node.cert";

   system(("openssl genrsa 1024 > " + keyname).c_str());
   system(("chmod 400 " + keyname).c_str());
   system(("openssl req -new -x509 -nodes -sha1 -days 365 -batch -key " + keyname + " > " + certname).c_str());
}

int main(int argc, char** argv)
{
   if (argc != 2)
   {
      cout << "usage: ssl_cert_gen security OR ssl_cert_gen_master" << endl;
      return -1;
   }

   if (0 == strcmp(argv[1], "security"))
   {
      struct stat s;
      if (stat("../conf/security_node.key", &s) != -1)
      {
         cerr << "Key already exist\n";
         return -1;
      }

      gen_cert("security");
      system("mv security_node.* ../conf");
      system("rm -f security_node.*");
   }
   else if (0 == strcmp(argv[1], "master"))
   {
      struct stat s;
      if (stat("../conf/master_node.key", &s) != -1)
      {
         cerr << "Key already exist\n";
         return -1;
      }

      gen_cert("master");
      system("mv master_node.* ../conf");
      system("rm -f master_node.*");
   }
   else
      return -1;

   return 0;
}
