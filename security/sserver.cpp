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
   Yunhong Gu, last updated 12/13/2009
*****************************************************************************/

#include <security.h>
#include <filesrc.h>
#include <iostream>

using namespace std;

int main(int argc, char** argv)
{
   SServer ss;

   int port = 5000;
   if (argc == 2)
      port = atoi(argv[1]);

   if (ss.init(port, "../conf/security_node.cert", "../conf/security_node.key") < 0)
   {
      cerr << "failed to initialize security server at port " << port << endl;
      cerr << "Secuirty server failed to start. Please fix the problem.\n";
      return -1;
   }

   SSource* src = new FileSrc;

   if (ss.loadMasterACL(src, "../conf/master_acl.conf") < 0)
   {
      cerr << "WARNING: failed to read master ACL configuration file master_acl.conf in ../conf. No masters would be able to join.\n";
      cerr << "Secuirty server failed to start. Please fix the problem.\n";
      return -1;
   }

   if (ss.loadSlaveACL(src, "../conf/slave_acl.conf") < 0)
   {
      cerr << "WARNING: failed to read slave ACL configuration file slave_acl.conf in ../conf. No slaves would be able to join.\n";
      cerr << "Secuirty server failed to start. Please fix the problem.\n";
      return -1;
   }

   if (ss.loadShadowFile(src, "../conf/users") < 0)
   {
      cerr << "WARNING: no users account initialized.\n";
      cerr << "Secuirty server failed to start. Please fix the problem.\n";
      return -1;
   }

   delete src;

   cout << "Sector Security server running at port " << port << endl << endl;
   cout << "The server is started successfully; there is no further output from this program. Please do not shutdown the security server; otherwise no client may be able to login. If the server is down for any reason, you can restart it without restarting the masters and the slaves.\n";

   ss.run();

   return 1;
}
