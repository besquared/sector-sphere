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
   Yunhong Gu, last updated 07/30/2008
*****************************************************************************/

#ifndef __SPHERE_H__
#define __SPHERE_H__

#include <stdint.h>
#include <set>
#include <string>
#include <memobj.h>

class SInput
{
public:
   char* m_pcUnit;		// input data
   int m_iRows;			// number of records/rows
   int64_t* m_pllIndex;		// record index

   char* m_pcParam;		// parameter, NULL is no parameter
   int m_iPSize;		// size of the parameter, 0 if no parameter
};

class SOutput
{
public:
   char* m_pcResult;		// buffer to store the result
   int m_iBufSize;		// size of the physical buffer
   int m_iResSize;		// size of the result

   int64_t* m_pllIndex;		// record index of the result
   int m_iIndSize;		// size of the index structure (physical buffer size)
   int m_iRows;			// number of records/rows

   int* m_piBucketID;		// bucket ID

   int64_t m_llOffset;		// last data position (file offset) of the current processing
				// file processing only. starts with 0 and the last process should set this to -1.

   std::string m_strError;	// error text to be send back to client

public:
   int resizeResBuf(const int64_t& newsize);
   int resizeIdxBuf(const int64_t& newsize);
};

class SFile
{
public:
   std::string m_strHomeDir;		// Sector data home directory: constant
   std::string m_strLibDir;		// the directory that stores the library files available to the current process: constant
   std::string m_strTempDir;		// Sector temporary directory
   std::set<std::string> m_sstrFiles; 	// list of modified files

   MOMgmt* m_pInMemoryObjects;		// Handle to the in-memory objects management module
};

#endif
