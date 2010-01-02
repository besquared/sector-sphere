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
   Yunhong Gu, last updated 09/16/2009
*****************************************************************************/


#include <slave.h>
#include <iostream>
#include <utime.h>

using namespace std;

void* Slave::fileHandler(void* p)
{
   Slave* self = ((Param2*)p)->serv_instance;
   string filename = self->m_strHomeDir + ((Param2*)p)->filename;
   string sname = ((Param2*)p)->filename;
   int key = ((Param2*)p)->key;
   int mode = ((Param2*)p)->mode;
   int transid = ((Param2*)p)->transid;
   string src_ip = ((Param2*)p)->src_ip;
   int src_port = ((Param2*)p)->src_port;
   string dst_ip = ((Param2*)p)->dst_ip;
   int dst_port = ((Param2*)p)->dst_port;
   unsigned char crypto_key[16];
   unsigned char crypto_iv[8];
   memcpy(crypto_key, ((Param2*)p)->crypto_key, 16);
   memcpy(crypto_iv, ((Param2*)p)->crypto_iv, 8);
   string master_ip = ((Param2*)p)->master_ip;
   int master_port = ((Param2*)p)->master_port;
   delete (Param2*)p;

   bool bRead = mode & 1;
   bool bWrite = mode & 2;
   bool bSecure = mode & 16;

   bool run = true;

   cout << "rendezvous connect source " << src_ip << " " << src_port << " " << filename << endl;

   if (self->m_DataChn.connect(src_ip, src_port) < 0)
   {
      self->logError(1, src_ip, src_port, sname);
      return NULL;
   }

   if (bSecure)
      self->m_DataChn.setCryptoKey(src_ip, src_port, crypto_key, crypto_iv);

   if (dst_port > 0)
      self->m_DataChn.connect(dst_ip, dst_port);

   //create a new directory or file in case it does not exist
   int change = 0;
   if (mode > 1)
   {
      self->createDir(sname.substr(0, sname.rfind('/')));

      struct stat64 t;
      if (stat64(filename.c_str(), &t) == -1)
      {
         ofstream newfile(filename.c_str(), ios::out | ios::binary | ios::trunc);
         newfile.close();
         change = 1;
      }
   }

   cout << "connected\n";

   timeval t1, t2;
   gettimeofday(&t1, 0);
   int64_t rb = 0;
   int64_t wb = 0;

   int32_t cmd = 0;

   while (run)
   {
      if (self->m_DataChn.recv4(src_ip, src_port, transid, cmd) < 0)
         break;

      fstream fhandle;

      if (cmd <= 4)
      {
         int32_t response = 0;

         if (((2 == cmd) || (4 == cmd)) && !bWrite)
            response = -1;
         else if (((1 == cmd) || (3 == cmd)) && !bRead)
            response = -1;

         fhandle.open(filename.c_str(), ios::in | ios::out | ios::binary);
         if (fhandle.fail() || fhandle.bad())
            response = -1;

         if (self->m_DataChn.send(src_ip, src_port, transid, (char*)&response, 4) < 0)
            break;

         if (response == -1)
            break;
      }

      switch (cmd)
      {
      case 1: // read
         {
            char* param = NULL;
            int tmp = 8 * 2;
            if (self->m_DataChn.recv(src_ip, src_port, transid, param, tmp) < 0)
            {
               run = false;
               break;
            }
            int64_t offset = *(int64_t*)param;
            int64_t size = *(int64_t*)(param + 8);
            delete [] param;

            if (self->m_DataChn.sendfile(src_ip, src_port, transid, fhandle, offset, size, bSecure) < 0)
               run = false;
            else
               rb += size;

            // update total sent data size
            self->m_SlaveStat.updateIO(src_ip, param[1], (key == 0) ? 1 : 3);

            break;
         }

      case 2: // write
         {
            char* param = NULL;
            int tmp = 8 * 2;
            if (self->m_DataChn.recv(src_ip, src_port, transid, param, tmp) < 0)
            {
               run = false;
               break;
            }
            int64_t offset = *(int64_t*)param;
            int64_t size = *(int64_t*)(param + 8);
            delete [] param;

            if (self->m_DataChn.recvfile(src_ip, src_port, transid, fhandle, offset, size, bSecure) < 0)
               run = false;
            else
               wb += size;

            // update total received data size
            self->m_SlaveStat.updateIO(src_ip, size, (key == 0) ? 0 : 2);

            if (change != 1)
               change = 2;

            if (dst_port > 0)
            {
               self->m_DataChn.send(dst_ip, dst_port, transid, (char*)&cmd, 4);
               int response;
               if ((self->m_DataChn.recv4(dst_ip, dst_port, transid, response) < 0) || (-1 == response))
                  break;

               // replicate data to another node
               char req[16];
               *(int64_t*)req = offset;
               *(int64_t*)(req + 8) = size;

               if (self->m_DataChn.send(dst_ip, dst_port, transid, req, 16) < 0)
                  break;

               self->m_DataChn.sendfile(dst_ip, dst_port, transid, fhandle, offset, size);
            }

            break;
         }

      case 3: // download
         {
            int64_t offset;
            if (self->m_DataChn.recv8(src_ip, src_port, transid, offset) < 0)
            {
               run = false;
               break;
            }

            fhandle.seekg(0, ios::end);
            int64_t size = (int64_t)(fhandle.tellg());
            fhandle.seekg(0, ios::beg);

            size -= offset;

            int64_t unit = 64000000; //send 64MB each time
            int64_t tosend = size;
            int64_t sent = 0;
            while (tosend > 0)
            {
               int64_t block = (tosend < unit) ? tosend : unit;
               if (self->m_DataChn.sendfile(src_ip, src_port, transid, fhandle, offset + sent, block, bSecure) < 0)
               {
                  run = false;
                  break;
               }

               sent += block;
               tosend -= block;
            }

            rb += sent;

            // update total sent data size
            self->m_SlaveStat.updateIO(src_ip, size, (key == 0) ? 1 : 3);

            break;
         }

      case 4: // upload
         {
            int64_t offset = 0;
            int64_t size;
            if (self->m_DataChn.recv8(src_ip, src_port, transid, size) < 0)
            {
               run = false;
               break;
            }

            int64_t unit = 64000000; //send 64MB each time
            int64_t torecv = size;
            int64_t recd = 0;

            while (torecv > 0)
            {
               int64_t block = (torecv < unit) ? torecv : unit;

               if (self->m_DataChn.recvfile(src_ip, src_port, transid, fhandle, offset + recd, block, bSecure) < 0)
               {
                  run = false;
                  break;
               }

               if (dst_port > 0)
               {
                  // write to uplink

                  int write = 2;
                  self->m_DataChn.send(dst_ip, dst_port, transid, (char*)&write, 4);
                  int response;
                  if ((self->m_DataChn.recv4(dst_ip, dst_port, transid, response) < 0) || (-1 == response))
                     break;

                  char req[16];
                  *(int64_t*)req = offset + recd;
                  *(int64_t*)(req + 8) = block;

                  if (self->m_DataChn.send(dst_ip, dst_port, transid, req, 16) < 0)
                     break;

                  self->m_DataChn.sendfile(dst_ip, dst_port, transid, fhandle, offset + recd, block);
               }

               recd += block;
               torecv -= block;
            }

            wb += recd;

            // update total received data size
            self->m_SlaveStat.updateIO(src_ip, size, (key == 0) ? 0 : 2);

            if (change != 1)
               change = 2;

            break;
         }

      case 5: // end session
         if (dst_port > 0)
         {
            // disconnet uplink
            self->m_DataChn.send(dst_ip, dst_port, transid, (char*)&cmd, 4);
            self->m_DataChn.recv4(dst_ip, dst_port, transid, cmd);
         }

         run = false;
         break;

      case 6: // read file path for local IO optimization
         self->m_DataChn.send(src_ip, src_port, transid, self->m_strHomeDir.c_str(), self->m_strHomeDir.length() + 1);
         break;

      default:
         break;
      }

      fhandle.close();
   }

   gettimeofday(&t2, 0);
   int duration = t2.tv_sec - t1.tv_sec;
   double avgRS = 0;
   double avgWS = 0;
   if (duration > 0)
   {
      avgRS = rb / duration * 8.0 / 1000000.0;
      avgWS = wb / duration * 8.0 / 1000000.0;
   }

   cout << "file server closed " << src_ip << " " << src_port << " " << avgRS << endl;

   char* tmp = new char[64 + sname.length()];
   sprintf(tmp, "file server closed ... %s %f %f.", sname.c_str(), avgRS, avgWS);
   self->m_SectorLog.insert(tmp);
   delete [] tmp;

   //report to master the task is completed
   self->report(master_ip, master_port, transid, sname, change);

   self->m_DataChn.send(src_ip, src_port, transid, (char*)&cmd, 4);

   if (key > 0)
      self->m_DataChn.remove(src_ip, src_port);

   return NULL;
}

void* Slave::copy(void* p)
{
   Slave* self = ((Param3*)p)->serv_instance;
   int transid = ((Param3*)p)->transid;
   time_t ts = ((Param3*)p)->timestamp;
   string src = ((Param3*)p)->src;
   string dst = ((Param3*)p)->dst;
   string master_ip = ((Param3*)p)->master_ip;
   int master_port = ((Param3*)p)->master_port;
   delete (Param3*)p;

   SNode tmp;
   if (self->m_pLocalFile->lookup(src.c_str(), tmp) >= 0)
   {
      //if file is local, copy directly
      self->createDir(dst.substr(0, dst.rfind('/')));
      string rhome = self->reviseSysCmdPath(self->m_strHomeDir);
      string rsrc = self->reviseSysCmdPath(src);
      string rdst = self->reviseSysCmdPath(dst);
      system(("cp " + rhome + src + " " + rhome + rdst).c_str());

      // if the file has been modified during the replication, remove this replica
      int type = (src == dst) ? 3 : 1;
      if (self->report(master_ip, master_port, transid, dst, type) < 0)
         system(("rm " + rhome + rdst).c_str());

      //utime: update timestamp according to the original copy
      utimbuf ut;
      ut.actime = ts;
      ut.modtime = ts;
      utime((rhome + rdst).c_str(), &ut);

      return NULL;
   }

   SectorMsg msg;
   msg.setType(110); // open the file
   msg.setKey(0);

   int32_t mode = 1;
   msg.setData(0, (char*)&mode, 4);
   int32_t localport = self->m_DataChn.getPort();
   msg.setData(4, (char*)&localport, 4);
   msg.setData(8, "\0", 1);
   msg.setData(72, src.c_str(), src.length() + 1);

   Address addr;
   self->m_Routing.lookup(src, addr);

   if (self->m_GMP.rpc(addr.m_strIP.c_str(), addr.m_iPort, &msg, &msg) < 0)
      return NULL;
   if (msg.getType() < 0)
      return NULL;

   string ip = msg.getData();
   int port = *(int*)(msg.getData() + 64);
   int session = *(int*)(msg.getData() + 68);

   int64_t size = *(int64_t*)(msg.getData() + 72);

   //cout << "rendezvous connect " << ip << " " << port << endl;
   if (self->m_DataChn.connect(ip, port) < 0)
      return NULL;

   // download command: 3
   int32_t cmd = 3;
   self->m_DataChn.send(ip, port, session, (char*)&cmd, 4);

   int response = -1;
   if ((self->m_DataChn.recv4(ip, port, session, response) < 0) || (-1 == response))
      return NULL;

   int64_t offset = 0;
   if (self->m_DataChn.send(ip, port, session, (char*)&offset, 8) < 0)
      return NULL;

   //copy to .tmp first, then move to real location
   self->createDir(string(".tmp") + dst.substr(0, dst.rfind('/')));

   fstream ofs;
   ofs.open((self->m_strHomeDir + ".tmp" + dst).c_str(), ios::out | ios::binary | ios::trunc);

   int64_t unit = 64000000; //send 64MB each time
   int64_t torecv = size;
   int64_t recd = 0;
   while (torecv > 0)
   {
      int64_t block = (torecv < unit) ? torecv : unit;
      if (self->m_DataChn.recvfile(ip, port, session, ofs, offset + recd, block) < 0)
         unlink((self->m_strHomeDir + ".tmp" + dst).c_str());

      recd += block;
      torecv -= block;
   }

   ofs.close();

   // update total received data size
   self->m_SlaveStat.updateIO(ip, size, 0);

   cmd = 5;
   self->m_DataChn.send(ip, port, session, (char*)&cmd, 4);
   self->m_DataChn.recv4(ip, port, session, cmd);

   //utime: update timestamp according to the original copy
   utimbuf ut;
   ut.actime = ts;
   ut.modtime = ts;
   utime((self->m_strHomeDir + ".tmp" + dst).c_str(), &ut);

   self->createDir(dst.substr(0, dst.rfind('/')));
   string rhome = self->reviseSysCmdPath(self->m_strHomeDir);
   string rfile = self->reviseSysCmdPath(dst);
   system(("mv " + rhome + ".tmp" + rfile + " " + rhome + rfile).c_str());

   // if the file has been modified during the replication, remove this replica
   int type = (src == dst) ? 3 : 1;
   if (self->report(master_ip, master_port, transid, dst, type) < 0)
      system(("rm " + rhome + rfile).c_str());

   return NULL;
}
