#ifndef FME_CITY_JSON_PRIV_H
#define FME_CITY_JSON_PRIV_H

/*=============================================================================

   Name     : fmecityjsonpriv.h

   System   : FME Plug-in SDK

   Language : C++

   Purpose  : Constants for reader / writer messages used in logging

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

//-------------------------------------------------------------------------
// For messages used in logging. Modify as necessary.
//-------------------------------------------------------------------------

const static char* const kMsgOpeningReader = "Opening reader on dataset ";
const static char* const kMsgClosingReader = "Closing reader on dataset ";

const static char* const kMsgOpeningWriter = "Opening writer on dataset ";
const static char* const kMsgClosingWriter = "Closing writer on dataset ";
const static char* const kMsgWriteError    = "Error writing geometry";

const static char* const kMsgStartVisiting = "Starting visit to geometry type ";
const static char* const kMsgVisiting      = "Visiting geometry type ";
const static char* const kMsgEndVisiting   = "Finishing visit to geometry type ";

const static char* const kLodParamTag      = "'CityJSON Level of Detail' parameter value: ";
const static char* const kSrcLodParamTag   = "_LOD";
const static char* const kMsgNoLodParam    = "'CityJSON Level of Detail' parameter value is not set";

const static char* const kSrcCityjsonVersion  = "_CITYJSON_VERSION";
const static char* const kSrcRemoveDuplicates = "_REMOVE_DUPLICATES";
const static char* const kSrcCompress         = "_USE_COMPRESSION";
const static char* const kSrcImportantDigits  = "_IMPORTANT_DIGITS";
const static char* const kSrcIndentSize       = "_INDENT_SIZE";
const static char* const kSrcIndentCharacters = "_INDENT_CHARACTERS";
const static char* const kSrcPrettyPrint      = "_PRETTY_PRINT";
const static char* const kSrcPreferredTextureFormat = "_TEXTURE_OUTPUT_FORMAT";

// Used when the writer is looking for schema features from the reader
const static char* const kCityJSON_FME_DIRECTION    = "FME_DIRECTION";
const static char* const kCityJSON_FME_DESTINATION  = "DESTINATION";
const static char* const kCityJSON_CITYJSON_STARTING_SCHEMA = "CITYJSON_STARTING_SCHEMA";


#endif
