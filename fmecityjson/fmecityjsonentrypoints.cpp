/*=============================================================================

   Name     : fmecityjsonentrypoints.cpp

   System   : FME Plug-in SDK

   Language : C++

   Purpose  : Entry points for the DLL

         Copyright (c) 1994 - 2018, Safe Software Inc. All rights reserved.

   Redistribution and use of this sample code in source and binary forms, with 
   or without modification, are permitted provided that the following 
   conditions are met:
   * Redistributions of source code must retain the above copyright notice, 
     this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright notice, 
     this list of conditions and the following disclaimer in the documentation 
     and/or other materials provided with the distribution.

   THIS SAMPLE CODE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SAMPLE CODE, EVEN IF 
   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

// Include Files
#include "fmecityjsonreader.h"
#include "fmecityjsonwriter.h"

#ifdef WIN32
#include <Windows.h>
#include <sstream>
#endif

#include <fmedllexport.h>

//=====================================================================
// Forward Declarations
class IFMESession;

//=====================================================================
// Global Variables
extern IFMESession* gFMESession;

//=====================================================================
// This method is used to reliably get a session for use by this
// plug-in.
//
FME_DLLEXPORT_C void FME_acceptSession(IFMESession* fmeSession)
{
   // This is a pointer only and does not take ownership.
   gFMESession = fmeSession;
}

//=====================================================================
// This method returns the version of the FME API that this plug-in
// was compiled with. The value returned here is a constant defined in
// fmetypes.h and should not be changed.
//
// If this plug-in is compiled with a beta build of FME, compatibility
// issues may arise with subsequent builds of FME. To prevent crashes,
// FME will not load a plug-in compiled against a beta if there is a
// mismatch in the FME API version. To resolve this issue, re-compile
// this plug-in with the version of FME that will run this plug-in.
//
// It is recommended that this plug-in is compiled with a release
// version of FME before being distributed for use. As long as this
// plug-in is compiled with a release version of FME, there should
// be no compatibility issues when running this plug-in on any
// subsequent build of FME.
//
// A plug-in compiled with a release version of FME may generate
// a warning when used with subsequent builds of FME, but the plug-in
// will still operate correctly.
//
FME_DLLEXPORT_C const char* FME_apiVersion()
{
   return kFMEDevKitVersion;
}

//=====================================================================
// This method is called by the FME to initialize the plug-in.
//
FME_DLLEXPORT_C FME_MsgNum FME_initialize(IFMEServiceManager& serviceManager)
{
   // For internationalization -- allows various encoding formats
   #ifdef WIN32
      FME_UInt32 codePage = GetACP();

      // Set the locale according to the current code-page
      std::ostringstream os;
      os << codePage;
      std::string codePageString = "." + os.str();
      char* locale = setlocale(LC_CTYPE, codePageString.c_str());
   #endif

   return FME_SUCCESS;
}


// --------------------------------------------------------------------
// Reader methods -- remove the following two methods if you are not
// implementing a reader
// --------------------------------------------------------------------

//=====================================================================
// This method is called by the FME when the reader is required. It
// instantiates an object that implements the IFMEReader interface and
// returns a pointer to that object as an output parameter. Note that
// this pointer is not "remembered" by this method; once the FME has
// finished using the object, it will pass the pointer to
// FME_destroyReader() so the reader object can be deleted.
//
FME_DLLEXPORT_C FME_MsgNum FME_createReader( IFMELogFile& logFile,
                                       IFMEMappingFile& mappingFile,
                                       IFMECoordSysManager& coordSysMan,
                                       IFMEReader*& reader,
                                       const char* readerTypeName,
                                       const char* readerKeyword )
{
   reader = new FMECityJSONReader(readerTypeName, readerKeyword);

   FMECityJSONReader::gLogFile      = &logFile;     // create pointer to log file
   FMECityJSONReader::gMappingFile  = &mappingFile; // create pointer to mapping file
   FMECityJSONReader::gCoordSysMan  = &coordSysMan; // create pointer to coordinate system manager
   
   return FME_SUCCESS;
}

//=====================================================================
// This method is called by the FME when the reader is no longer
// required. It deletes the reader object pointed to by the input
// parameter.
//
FME_DLLEXPORT_C FME_MsgNum FME_destroyReader(IFMEReader*& reader)
{
   delete reader;
   reader = nullptr;
   return FME_SUCCESS;
}


// --------------------------------------------------------------------
// Writer methods -- remove the following two methods if you are not
// implementing a writer
// --------------------------------------------------------------------

//=====================================================================
// This method is called by the FME when the writer is required. It
// instantiates an object that implements the IFMEWriter interface and
// returns a pointer to that object as an output parameter. Note that
// this pointer is not "remembered" by this method; once the FME has
// finished using the object, it will pass the pointer to
// FME_destroyWriter() so the reader object can be deleted.
//
FME_DLLEXPORT_C FME_MsgNum FME_createWriter( IFMELogFile& logFile,
                                       IFMEMappingFile& mappingFile,
                                       IFMECoordSysManager& coordSysMan,
                                       IFMEWriter*& writer,
                                       const char* writerTypeName,
                                       const char* writerKeyword )
{
   writer = new FMECityJSONWriter(writerTypeName, writerKeyword);

   FMECityJSONWriter::gLogFile      = &logFile;     // create pointer to log file
   FMECityJSONWriter::gMappingFile  = &mappingFile; // create pointer to mapping file
   FMECityJSONWriter::gCoordSysMan  = &coordSysMan; // create pointer to coordinate system manager
   
   return FME_SUCCESS;
}

//=====================================================================
// This method is called by the FME when the writer is no longer
// required. It deletes the writer object pointed to by the input
// parameter.
//
FME_DLLEXPORT_C FME_MsgNum FME_destroyWriter(IFMEWriter*& writer)
{
   delete writer;
   writer = nullptr;
   return FME_SUCCESS;
}

