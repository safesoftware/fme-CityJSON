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
#include <optional>

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

#include <nlohmann/json.hpp>
// for convenience
using json = nlohmann::json;
using VertexPool3D = std::vector<std::tuple<double, double, double>>;
using VertexPool2D = std::vector<std::tuple<double, double>>;
using RefVec       = std::vector<json::value_type>;
using RefVec2      = std::vector<RefVec>;
using RefVec3      = std::vector<RefVec2>;
using RefVec4      = std::vector<RefVec3>;

// Forward declarations
class IFMEFeature;
class IFMELogFile;
class IFMEGeometryTools;
class IFMERaster;

// -----------------------------------------------------------------------
// Gather schema feature definitions, by looking in the official CityJSON specs
// and pull out the correct schema information.
FME_Status fetchSchemaFeatures(IFMELogFile& logFile,
                               const std::string& schemaVersion,
                               std::map<std::string, IFMEFeature*>& schemaFeatures);

// -----------------------------------------------------------------------
// Helper functions to recursively add nested attribute types to a schema feature.
void addAttributeNamesAndTypes(IFMEFeature& schemaFeature,
                               const std::string& attributeName,
                               json attributeValue);
void addObjectProperties(json::value_type& itemPart, IFMEFeature& schemaFeature, std::string& featureType);

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
   FMECityJSONReader& operator=(const FMECityJSONReader&);

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
   bool fetchWriterDirectives(const IFMEStringArray& parameters);

   // -----------------------------------------------------------------------
   // Insert additional private methods here
   // -----------------------------------------------------------------------

   void readVertexPool();

   void scanLODs();

   FME_Status readGeometryDefinitions();

   void readMetadata();

   void readMaterials();

   void readTextures();

   void readTextureVertices();

   FME_Status readRaster(const std::string& fullFileName, IFMERaster*& raster, std::string readerToUse);

   // Parse the attributes of a CityObject or metadata and assign it as attributes to the feature.
   // Takes an iterator over a json object. Also need to pass the end of the iterator to know when to stop.
   static void parseAttributes(IFMEFeature& feature, json::iterator& it, const json::iterator& _end);

   // Parse a single Geometry of a CityObject
   IFMEGeometry* parseCityObjectGeometry(json::value_type& currentGeometry, VertexPool3D& vertices);

   // Parse a Multi- or CompositeSolid
   template <typename MCSolid>
   void parseMultiCompositeSolid(MCSolid multiCompositeSolid,
                                 json::value_type& boundaries,
                                 json::value_type& semantics,
                                 std::vector<std::string>& textureThemes,
                                 RefVec4& textureRefsPerBoundaryPerShellPerSolid,
                                 json::value_type& materialRefs,
                                 VertexPool3D& vertices);

   // Parse a Solid
   IFMEBRepSolid* parseSolid(json::value_type& boundaries,
                             json::value_type& semantics,
                             json::value_type* semanticSrfVec2,
                             std::vector<std::string>& textureThemes,
                             RefVec3* textureRefsPerBoundaryPerShell,
                             json::value_type& materialRefs,
                             VertexPool3D& vertices);

   // Parse a Multi- or CompositeSurface
   template <typename MCSurface>
   void parseMultiCompositeSurface(MCSurface multiCompositeSurface,
                                   json::value_type& boundaries,
                                   json::value_type& semantics,
                                   json::value_type* semanticSrfVec,
                                   std::vector<std::string>& textureThemes,
                                   RefVec2* textureRefsPerBoundary,
                                   json::value_type& materialRefs,
                                   VertexPool3D& vertices);

   IFMEFace* createOneSurface(std::vector<std::string>& textureThemes,
                              RefVec* textureRefs,
                              json::value_type& materialRefs,
                              json::value_type& boundaries,
                              VertexPool3D& vertices,
                              json::value_type* semanticSrf);

   // Parse a single Surface of the boundary
   IFMEFace* parseSurfaceBoundaries(json::value_type& surface,
                                    VertexPool3D& vertices,
                                    std::vector<std::string>& textureThemes,
                                    RefVec* textureRefs);

   // parse the semantics and attach them to the surface.
   void parseSemantics(IFMEFace& face, json::value_type* semanticSurface);

   // parse the materials and attach them to the surface.
   void parseMaterials(IFMEFace& face, json::value_type& materialRef);

   // Parse a MultiLineString
   void parseMultiLineString(IFMEMultiCurve* mlinestring,
                             json::value_type& boundaries,
                             VertexPool3D& vertices);

   // Parse a single Ring to an IFMELine
   void parseRings(std::vector<IFMELine*>& rings,
                   std::vector<FME_UInt32>& appearanceRefs,
                   json::value_type& boundary,
                   VertexPool3D& vertices,
                   json::value_type& textureRefs);

   // Parse a single LineString
   void parseLineString(IFMELine* line,
                        std::optional<FME_UInt32>& appearanceRef,
                        json::value_type& boundary,
                        VertexPool3D& vertices,
                        json::value_type& textureRefs);

   // Parse MultiPoint geometry
   void parseMultiPoint(IFMEMultiPoint* mpoint,
                        json::value_type& boundary,
                        VertexPool3D& vertices);

   // Set the Level of Detail Trait on the geometry
   static void setTraitString(IFMEGeometry& geometry,
                              const std::string& traitName,
                              const std::string& traitValue);

   // Cast the geometry LoD to a string, even though the specs require a number.
   // Because strings are easier to compare than floats (in case of extended LoD).
   static std::string lodToString(json currentGeometry);

   // If we have N references per M boundaries, make a vector
   // of M entries, each with N values, rather than the inverse.
   // This is done 2 levels deep.
   void unrollReferences2(json::value_type& references,
                          json::value_type& boundaries,
                          RefVec2& refsPerBoundary);

   // If we have N references per M boundaries, make a vector
   // of M entries, each with N values, rather than the inverse.
   // This is done 3 levels deep.
   void unrollReferences3(json::value_type& references,
                          json::value_type& boundaries,
                          RefVec3& refsPerBoundaryPerShell);

   // If we have N references per M boundaries, make a vector
   // of M entries, each with N values, rather than the inverse.
   // This is done 4 levels deep.
   void unrollReferences4(json::value_type& references,
                          json::value_type& boundaries,
                          RefVec4& refsPerBoundaryPerShellperSolid);

   // -----------------------------------------------------------------------
   // If the reader is being used as a "helper" to the writer, to gather
   // schema feature definitions, these will look in the official CityJSON specs
   // and pull out the correct schema information.
   FME_Status fetchSchemaFeaturesForWriter();

   // -----------------------------------------------------------------------
   json::value_type* fetchSemanticsValues(json::value_type& semantics);

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
   // Initialized to an empty string.
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
   json metaObject_; // for storing the metadata object
   json::iterator nextObject_;
   int skippedObjects_;
   VertexPool3D vertices_;
   VertexPool2D textureVertices_;
   std::map<int, FME_UInt32> geomTemplateMap_;
   std::map<int, FME_UInt32> materialsMap_;
   std::string defaultThemeMaterial_;
   std::map<int, FME_UInt32> texturesMap_;
   std::string defaultThemeTexture_;
   std::vector<std::string> lodInData_;

   bool schemaScanDone_;
   bool schemaScanDoneMeta_;
   std::map<std::string, IFMEFeature*> schemaFeatures_;

   IFMEString* textureCoordUName_;
   IFMEString* textureCoordVName_;

   // These are when the reader is used as a "helper" to the writer
   bool writerHelperMode_;
   std::string writerStartingSchema_;
};

#endif
