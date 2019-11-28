#ifndef FME_CITY_JSON_WRITER_H
#define FME_CITY_JSON_WRITER_H
/*=============================================================================

   Name     : fmecityjsonwriter.h

   System   : FME Plug-in SDK

   Language : C++

   Purpose  : Declaration of FMECityJSONWriter

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

#include <fmewrt.h>
#include <fstream>
#include <sstream>
#include <string>
#include <igeometry.h>

#include <json.hpp>
using json = nlohmann::json;

// Forward declarations
class IFMEFeature;
class IFMEFeatureVector;
class IFMELogFile;
class IFMEGeometryTools;
class FMECityJSONGeometryVisitor;

class FMECityJSONWriter : public IFMEWriter
{

public:

   // Please refer to the IFMEWriter documentation for more information in
   // FME_DEV_HOME/pluginbuilder/cpp/apidoc/classIFMEWriter.html
   // -----------------------------------------------------------------------
   // Constructor
   // The writerTypeName is the name used in the following contexts:
   // - the name of the format's .db file in the formatsinfo folder
   // - the format short name for the format within the .db file
   // - FORMAT_NAME in the metafile
   // 
   // The writerKeyword is a unique identifier for this writer instance.
   // It is usually set by the WRITER_KEYWORD in the mapping file.
   // 
   // Since there can be multiple instances of a writerTypeName within
   // an FME session, the writerKeyword must be used when fetching 
   // context information from FME.
   FMECityJSONWriter(const char* writerTypeName, const char* writerKeyword);

   // -----------------------------------------------------------------------
   // Destructor
   virtual ~FMECityJSONWriter();

   // -----------------------------------------------------------------------
   // open()
   FME_Status open(const char* datasetName, const IFMEStringArray& parameters) override;

   // -----------------------------------------------------------------------
   // abort()
   FME_Status abort() override;

   // -----------------------------------------------------------------------
   // close()
   FME_Status close() override;

   // -----------------------------------------------------------------------
   // The use of this method has been deprecated.
   FME_UInt32 id() const override { return 0; }

   // -----------------------------------------------------------------------
   // write()
   FME_Status write(const IFMEFeature& feature) override;

   // -----------------------------------------------------------------------
   // multiFileWriter()
   FME_Boolean multiFileWriter() const override { return FME_FALSE; }

   // -----------------------------------------------------------------------
   // Insert additional public methods here
   // -----------------------------------------------------------------------
   //static IFMEString* getSemanticSurfaceType(const IFMEFace& face);

   // Data members

   // A pointer to an IFMELogFile object that allows the plug-in to log messages
   // to the FME log file. Initialized externally after the plug-in object is created.
   static IFMELogFile* gLogFile;

   // A pointer to an IFMEMappingFile object that allows the plug-in to access
   // information from the mapping file. It is initialized externally after the
   // plug-in object is created.
   static IFMEMappingFile* gMappingFile;

   // A pointer to an IFMECoordSysManager object that allows the plug-in to retrieve
   // and define coordinate systems. It is initialized externally after the plug-in
   // object is created.
   static IFMECoordSysManager* gCoordSysMan;

   // -----------------------------------------------------------------------
   // Insert additional public data members here
   // -----------------------------------------------------------------------

   static json::value_type geometryJSON;
   static std::map<const FMECoord3D, unsigned long> vertices;

private:

   //---------------------------------------------------------------
   // Copy constructor
   FMECityJSONWriter(const FMECityJSONWriter&);

   //---------------------------------------------------------------
   // Assignment operator
   FMECityJSONWriter &operator=(const FMECityJSONWriter&);

   //---------------------------------------------------------------
   // Fetching features from the schema
   void fetchSchemaFeatures();

   //---------------------------------------------------------------
   // Adding a DEF line to the schema
   void addDefLineToSchema(const IFMEStringArray& parameters);

   //---------------------------------------------------------------
   // Convert the IFMEStringArray to be logged
   void logFMEStringArray(IFMEStringArray& stringArray);

   // -----------------------------------------------------------------------
   // Insert additional private methods here
   // -----------------------------------------------------------------------


   // Data members

   // The value specified for WRITER_TYPE in the mapping file. It is
   // also specified for the FORMAT_NAME in the metafile. Initialized
   // in the constructor to the value passed in by the FME.
   std::string writerTypeName_;

   // Typically the same as the writerTypeName_, but it could be different
   // if a value was specified for WRITER_KEYWORD in the FME mapping file.
   // Initialized in the constructor to the value passed in by the FME.
   std::string writerKeyword_;

   // The name of the dataset that the plug-in is writing to. Initialized
   // to an empty string in the constructor and set to the value passed in
   // by the FME in the open() method.
   std::string dataset_;

   // A pointer to an IFMEGeometryTools object which is used to create and
   // manipulate geometries.
   IFMEGeometryTools* fmeGeometryTools_;

   // Visitor variable.
   FMECityJSONGeometryVisitor* visitor_;

   // Represents the schema feature on advanced writing.
   IFMEFeatureVector* schemaFeatures_;

   // -----------------------------------------------------------------------
   // Insert additional private data members here
   // -----------------------------------------------------------------------

   std::ofstream outputFile_;
   json outputJSON_;
};

#endif
