#ifndef FME_CITY_JSON_READER_H
#define FME_CITY_JSON_READER_H
/*=============================================================================

   Name     : fmecityjsonreader.h

   System   : FME Plug-in SDK

   Language : C++

   Purpose  : Declaration of FMECityJSONReader

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

#include <fmeread.h>
#include <fstream>
#include <string>
#include <map>

#include <fmefeat.h>
#include <igeometry.h>
#include <iaggregate.h>
#include <imultipoint.h>
#include <iline.h>
#include <imulticurve.h>
#include <iface.h>
#include <isurface.h>
#include <isurfaceiterator.h>
#include <imulticurve.h>
#include <imultisurface.h>
#include <icompositesurface.h>
#include <ibrepsolid.h>
#include <imultisolid.h>
#include <icompositesolid.h>

// #include <nlohmann/json.hpp>
#include "json.hpp"
// for convenience
using json = nlohmann::json;


// Forward declarations
class IFMEFeature;
class IFMELogFile;
class IFMEGeometryTools;

class FMECityJSONReader : public IFMEReader
{

public:

   // Please refer to the IFMEWriter documentation for more information in
   // FME_DEV_HOME/pluginbuilder/cpp/apidoc/classIFMEReader.html
   // -----------------------------------------------------------------------
   // Constructor
   // The readerTypeName is the name used in the following contexts:
   // - the name of the format's .db file in the formatsinfo folder
   // - the format short name for the format within the .db file
   // - FORMAT_NAME in the metafile
   // 
   // The readerKeyword is a unique identifier for this reader instance.
   // It is usually set by the READER_KEYWORD in the mapping file.
   // 
   // Since there can be multiple instances of a readerTypeName within
   // an FME session, the readerKeyword must be used when fetching 
   // context information from FME.
   FMECityJSONReader(const char* readerTypeName, const char* readerKeyword);

   // -----------------------------------------------------------------------
   // Destructor
   virtual ~FMECityJSONReader();

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
   // read()
   // adaptation of the CityJSON parser from https://github.com/tudelft3d/azul
   FME_Status read(IFMEFeature& feature, FME_Boolean& endOfFile) override;

   // -----------------------------------------------------------------------
   // readSchema()
   FME_Status readSchema(IFMEFeature& feature, FME_Boolean& endOfSchema) override;

   // -----------------------------------------------------------------------
   // Insert additional public methods here
   // -----------------------------------------------------------------------


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


private:

   // -----------------------------------------------------------------------
   // Copy constructor
   FMECityJSONReader(const FMECityJSONReader&);

   // -----------------------------------------------------------------------
   // Assignment operator
   FMECityJSONReader &operator=(const FMECityJSONReader&);

   // -----------------------------------------------------------------------
   // readParametersDialog
   //
   // This method doesn't do anything too interesting.  Its main purpose
   // is to illustrate how to read in the values entered in a parameters dialog.
   // In the Multi - Channel Format reader case, the values read from here
   // are logged. The values logged in the dataset are not used for anything
   // other than demonstration purposes.
   // A parameters dialog acts as the bridge between a mapping file and the
   // reader / writer.It should contain any configurable parameters
   // defined for a reader / writer.
   //
   // Please refer to the IFMEMappingFile documentation for more information in
   // FME_DEV_HOME/pluginbuilder/cpp/apidoc/classIFMEMappingFile.html
   void readParametersDialog();

   // -----------------------------------------------------------------------
   // Insert additional private methods here
   // -----------------------------------------------------------------------

   FME_Status readRaster(const std::string& fullFileName, FME_UInt32& appearanceReference, std::string readerToUse);

   // Parse a single Geometry of a CityObject
   IFMEGeometry *parseCityObjectGeometry(json::value_type &currentGeometry, std::vector<std::tuple<double, double, double>> &vertices);

   // Parse a Multi- or CompositeSolid
   template <typename MCSolid>
   void parseMultiCompositeSolid(MCSolid multiCompositeSolid, json::value_type &boundaries, json::value_type &semantics, std::vector<std::tuple<double, double, double>> &vertices);

   // Parse a Solid
   IFMEBRepSolid *parseSolid(json::value_type &boundaries, json::value_type &semantics, std::vector<std::tuple<double, double, double>> &vertices);

   // Parse a Multi- or CompositeSurface
   template <typename MCSurface>
   void parseMultiCompositeSurface(MCSurface multiCompositeSurface, json::value_type &boundaries, json::value_type &semantics, std::vector<std::tuple<double, double, double>> &vertices);

   // Parse a single Surface of the boundary
   IFMEFace *parseSurface(json::value_type surface, json::value_type semanticSurface, std::vector<std::tuple<double, double, double>> &vertices);

   // Parse a MultiLineString
   void parseMultiLineString(IFMEMultiCurve *mlinestring, json::value_type &boundaries, std::vector<std::tuple<double, double, double>> &vertices);

   // Parse a single Ring to an IFMELine
   void parseRings(std::vector<IFMELine *> *rings, json::value_type &boundary, std::vector<std::tuple<double, double, double>> &vertices);

   // Parse a single LineString
   static void parseLineString(IFMELine *line, json::value_type &boundary, std::vector<std::tuple<double, double, double>> &vertices);

   // Parse MultiPoint geometry
   void parseMultiPoint(IFMEMultiPoint *mpoint,
                        json::value_type &boundary,
                        std::vector<std::tuple<double, double, double>> &vertices);

   // Set the Level of Detail Trait on the geometry
   static void setTraitString(IFMEGeometry *geometry,
                              const std::string &traitName,
                              const std::string &traitValue);

   // Cast the geometry LoD to a string, even though the specs require a number.
   // Because strings are easier to compare than floats (in case of extended LoD).
   static std::string lodToString(json currentGeometry);

   // Data members

   // The value specified for the READER_TYPE in the mapping file.
   // It is also specified for the FORMAT_NAME in the metafile.
   // Initialized in the constructor to the value passed in by the FME.
   std::string readerTypeName_;

   // Typically the same as the readerTypeName_, but it could be different
   // if a value was specified for READER_KEYWORD in the FME mapping file.
   // Initialized in the constructor to the value passed in by the FME.
   std::string readerKeyword_;

   // Stores the full path of the dataset that the plug-in is reading from.
   // Initialized to an empty string in the constructor and set to the value
   // passed in by the FME in the open() method.
   std::string dataset_;

   // Stores the coordinate system of all the features in the file being read.
   // Initialized to an empty string. Not used in this implementation.
   std::string coordSys_;

   // A pointer to an IFMEGeometryTools object which is used to create and
   // manipulate geometries.
   IFMEGeometryTools* fmeGeometryTools_;

   // The parameters value used for reading the dataset.
   // For some formats, they have no need for parameters.
   std::string lodParam_;

   // -----------------------------------------------------------------------
   // Insert additional private data members here
   // -----------------------------------------------------------------------

   std::ifstream inputFile_;
   json inputJSON_;
   json::iterator nextObject_;
   std::vector<std::tuple<double, double, double>> vertices_;
   std::map<int, FME_UInt32> geomTemplateMap_;
   std::vector<std::string> lodInData_;

   bool schemaScanDone_;
   std::map<std::string, IFMEFeature*> schemaFeatures_;

   IFMEString* textureCoordUName_;
   IFMEString* textureCoordVName_;
};

#endif
