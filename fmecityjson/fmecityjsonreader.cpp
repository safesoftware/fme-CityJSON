/*=============================================================================

   Name     : fmecityjsonreader.cpp

   System   : FME Plug-in SDK

   Language : C++

   Purpose  : IFMEReader method implementations

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
#include "fmecityjsonpriv.h"

#include <igeometrytools.h>
#include <ilogfile.h>
#include <fmemap.h>
#include <isession.h>
#include <ifeature.h>

#include <iarea.h>
#include <isimplearea.h>
#include <iface.h>
#include <isurface.h>
#include <imultisurface.h>
#include <ireader.h>
#include <iraster.h>
#include <ilibrary.h>

#include <typeinfo>
#include <sstream>
#include <iomanip>

// These are initialized externally when a reader object is created so all
// methods in this file can assume they are ready to use.
IFMELogFile* FMECityJSONReader::gLogFile = nullptr;
IFMEMappingFile* FMECityJSONReader::gMappingFile = nullptr;
IFMECoordSysManager* FMECityJSONReader::gCoordSysMan = nullptr;
IFMESession* gFMESession = nullptr;

//===========================================================================
// Constructor
FMECityJSONReader::FMECityJSONReader(const char* readerTypeName, const char* readerKeyword)
   :
   readerTypeName_(readerTypeName),
   readerKeyword_(readerKeyword),
   dataset_(""),
   coordSys_(""),
   fmeGeometryTools_(nullptr),
   schemaScanDone_(false)
{
}

//===========================================================================
// Destructor
FMECityJSONReader::~FMECityJSONReader()
{
   close();
}

//===========================================================================
// Open
FME_Status FMECityJSONReader::open(const char* datasetName, const IFMEStringArray& parameters)
{
   // Get geometry tools
   fmeGeometryTools_ = gFMESession->getGeometryTools();
   dataset_ = datasetName;

   textureCoordUName_ = gFMESession->createString();
   *textureCoordUName_ = kFME_texture_coordinate_u;
   textureCoordVName_ = gFMESession->createString();
   *textureCoordVName_ = kFME_texture_coordinate_v;

   // -----------------------------------------------------------------------
   // Add additional setup here
   // -----------------------------------------------------------------------

   // Log an opening reader message
   gLogFile->logMessageString((kMsgOpeningReader + dataset_).c_str());

   // -----------------------------------------------------------------------
   // Open the dataset here, e.g. inputFile.open(dataSetName, ios::in);
   // -----------------------------------------------------------------------

   // Open up the data file.
   inputFile_.open(dataset_, std::ios::in);

   // Check that the file exists.
   if (!inputFile_.good())
   {
      // TODO: Should log a message
      return FME_FAILURE;
   }

   // Reset the file stream and start reading.
   inputFile_.seekg(0, std::ios::beg);
   inputFile_.clear();

   inputJSON_ = json::parse(inputFile_);

   // Let's make sure we're parsing this correctly.
   if (inputJSON_.at("type") != "CityJSON")
   {
       gLogFile->logMessageString("Not a CityJSON file", FME_ERROR);
      return FME_FAILURE;
   }
   if (inputJSON_.at("version") < "1.0")
   {
      gLogFile->logMessageString("Unsupported CityJSON version", FME_ERROR);
      return FME_FAILURE;
   }

   // Scrape the coordinate system
   // TODO: try/catch?
   std::string inputCoordSys = inputJSON_.at("metadata").at("referenceSystem");
   // TODO: Should log a message

   // Looking to make the form EPSG:XXXX
   inputCoordSys = inputCoordSys.substr(inputCoordSys.find_first_of("EPSG"));
   coordSys_ = inputCoordSys.erase(inputCoordSys.find_first_of(":"), 1);

  // Transform object
  std::vector<double> scale{1.0, 1.0, 1.0};
  std::vector<double> translation{0.0, 0.0, 0.0};
  try {
    json transformObject = inputJSON_.at("transform");
    scale.clear();
    for (double const s: transformObject.at("scale")) {
      scale.push_back(s);
    }
    translation.clear();
    for (double const t: transformObject.at("translate")) {
      translation.push_back(t);
    }
  }
  catch (json::out_of_range& e) {
    gLogFile->logMessageString("CityJSON file is not transformed",FME_INFORM);
  }

  // Vertices
  for (auto vtx: inputJSON_.at("vertices")) {
    double x = vtx[0];
    double y = vtx[1];
    double z = vtx[2];
    x = scale[0] * x + translation[0];
    y = scale[1] * y + translation[1];
    z = scale[2] * z +translation[2];
    vertices_.emplace_back(x, y, z);
  }

   // start by pointing to the first object to read
   nextObject_ = inputJSON_.at("CityObjects").begin();

   // Read the mapping file parameters if there is one specified.
   if (parameters.entries() < 1)
   {
      // We are in "open to read data features" mode.
      readParametersDialog();
   }

   return FME_SUCCESS;
}

//===========================================================================
// Abort
FME_Status FMECityJSONReader::abort()
{
   // -----------------------------------------------------------------------
   // Add any special actions to shut down a reader not finished reading
   // data; e.g. log a message or send an email. 
   // -----------------------------------------------------------------------

   close();
   return FME_SUCCESS;
}

//===========================================================================
// Close
FME_Status FMECityJSONReader::close()
{
   // -----------------------------------------------------------------------
   // Perform any closing operations / cleanup here; e.g. close opened files
   // -----------------------------------------------------------------------

   for (auto&& sf : schemaFeatures_) { 
      gFMESession->destroyFeature(sf.second); }
   schemaFeatures_.clear();

   // shut the file
   inputFile_.close();

   gFMESession->destroyString(textureCoordUName_);
   textureCoordUName_ = nullptr;
   gFMESession->destroyString(textureCoordVName_);
   textureCoordVName_ = nullptr;

   // Log that the reader is done
   gLogFile->logMessageString((kMsgClosingReader + dataset_).c_str());

   return FME_SUCCESS;
}

//===========================================================================
// Read
FME_Status FMECityJSONReader::read(IFMEFeature& feature, FME_Boolean& endOfFile)
{
   FME_Status badLuck(FME_SUCCESS);
   // -----------------------------------------------------------------------
   // Perform read actions here
   // -----------------------------------------------------------------------

   // TODO: I'm not sure how you want to set this up, but one way is to
   // have a class iterator that is progressively going through the CityJSON
   // file.  I'll use that method as a starting example.
   if (nextObject_ == inputJSON_.at("CityObjects").end())
   {
      endOfFile = FME_TRUE;
      return FME_SUCCESS;
   }

   std::string objectId = nextObject_.key();
   gLogFile->logMessageString(("Parsing CityObject: " + objectId).c_str(),FME_INFORM);

   // Set the feature type
   std::string featureType = nextObject_.value().at("type");
   feature.setFeatureType(featureType.c_str());

   // iterate through every attribute on this object.
   for (json::iterator it = nextObject_.value().at("attributes").begin(); it != nextObject_.value().at("attributes").end(); ++it)
   {
      const std::string& attributeName = it.key();
      const auto& attributeValue = it.value();
      // For now, I'm just guessing at the type of this attribute.
      // TODO: Something smarter really should be done here.
      feature.setAttribute(attributeName.c_str(), attributeValue.dump().c_str());
   }

   // Set the geometry
   for (auto &geometry: nextObject_.value()["geometry"])
   {
     // TODO: Can FME handle multiple geometries for the same object? Lod 1 an lod2? How?
     // Set the geometry for the feature
     FMECityJSONReader::parseCityJSONObjectGeometry(feature, geometry);
   }
  // Set the coordinate system
  feature.setCoordSys(coordSys_.c_str());

   ++nextObject_;

   endOfFile = FME_FALSE;
   return FME_SUCCESS;
}

void FMECityJSONReader::parseCityJSONObjectGeometry(
    IFMEFeature &feature, json::value_type &currentGeometry) {

  if (currentGeometry.is_object()) {
    std::string geometryType, geometryLodValue;
    std::string geometryLodName = "Level of Detail"; // geometry Trait name
    json::array_t boundaries = currentGeometry.at("boundaries");
    json::array_t semanticSurfaces = currentGeometry.at("semantics").at("surfaces");
    json::array_t semanticValues = currentGeometry.at("semantics").at("values");

    // geometry type and level of detail
    geometryType = currentGeometry.at("type");
    if (currentGeometry.at("lod").is_number_integer())
      geometryLodValue = std::to_string(int(currentGeometry.at("lod")));
    else if (currentGeometry.at("lod").is_number_float()) {
      // We want the LoD as string, even though CityJSON specs currently
      // prescribe a number
      std::stringstream stream;
      stream << std::fixed << std::setprecision(1)
             << float(currentGeometry.at("lod"));
      geometryLodValue = stream.str();
    } else {
      geometryLodValue = currentGeometry.at("lod");
      gLogFile->logMessageString(
          ("parseCityJSONObjectGeometry; geometryType: " + geometryType + "; geometryLodValue: " + geometryLodValue).c_str(),
          FME_INFORM);
    }

    // TODO: get "template"
    // TODO: get "transformationMatrix"

    if (!geometryType.empty()) {
      if (geometryType == "MultiPoint") {
        gLogFile->logMessageString(
            "Geometry type 'MultiPoint' is not supported yet", FME_WARN);
      }
      else if (geometryType == "LineString") {
        gLogFile->logMessageString(
            "Geometry type 'LineString' is not supported yet", FME_WARN);
      }
      else if (geometryType == "MultiSurface") {
        IFMEMultiSurface *msurface = fmeGeometryTools_->createMultiSurface();
        FMECityJSONReader::parseMultiCompositeSurface(msurface, boundaries, semanticValues, semanticSurfaces);
        // Set the Level of Detail Trait on the geometry
        FMECityJSONReader::setTraitString(msurface, geometryLodName, geometryLodValue);
        // Append the geometry to the FME feature
        feature.setGeometry(msurface);
      }
      else if (geometryType == "CompositeSurface") {
        IFMECompositeSurface *csurface = fmeGeometryTools_->createCompositeSurface();
        FMECityJSONReader::parseMultiCompositeSurface(csurface, boundaries, semanticValues, semanticSurfaces);
        FMECityJSONReader::setTraitString(csurface, geometryLodName, geometryLodValue);
        feature.setGeometry(csurface);
      }
      else if (geometryType == "Solid") {
        IFMEBRepSolid* BSolid = FMECityJSONReader::parseSolid(boundaries, semanticValues, semanticSurfaces);
        FMECityJSONReader::setTraitString(BSolid, geometryLodName, geometryLodValue);
        feature.setGeometry(BSolid);
      }
      else if (geometryType == "MultiSolid") {
        IFMEMultiSolid *msolid = fmeGeometryTools_->createMultiSolid();
        FMECityJSONReader::parseMultiCompositeSolid(msolid, boundaries, semanticValues, semanticSurfaces);
        FMECityJSONReader::setTraitString(msolid, geometryLodName, geometryLodValue);
        feature.setGeometry(msolid);
      }
      else if (geometryType == "CompositeSolid") {
        IFMECompositeSolid *csolid = fmeGeometryTools_->createCompositeSolid();
        FMECityJSONReader::parseMultiCompositeSolid(csolid, boundaries, semanticValues, semanticSurfaces);
        FMECityJSONReader::setTraitString(csolid, geometryLodName, geometryLodValue);
        feature.setGeometry(csolid);
      }
      else if (geometryType == "GeometryInstance") {
        gLogFile->logMessageString(
            "Geometry type 'GeometryInstance' is not supported yet", FME_WARN);
      }
      else {
        gLogFile->logMessageString(
            ("Unknown geometry type " + geometryType).c_str(), FME_WARN);
      }
    }
    else {
      gLogFile->logMessageString("CityObject Geometry type is not set",
                                 FME_WARN);
    }
  }
}


template <typename MCSolid>
void FMECityJSONReader::parseMultiCompositeSolid(MCSolid multiCompositeSolid, json::array_t& boundaries,
                                                 json::array_t& semanticValues, json::array_t& semanticSurfaces)
{
  int nrSolids = distance(begin(boundaries), end(boundaries));
  for (int i=0; i < nrSolids; i++) {
    IFMECompositeSurface *outerSurface = fmeGeometryTools_->createCompositeSurface();
    // TODO: this will be problematic with inner shells
    int nrShells = distance(begin(boundaries[i]), end(boundaries[i]));
    for (int j = 0; j < nrShells; j++) {
      int nrSurfaces = distance(begin(boundaries[i][j]), end(boundaries[i][j]));
      for (int k = 0; k < nrSurfaces; k++) {
        json::value_type semanticSrf;
        if (not semanticValues[i][j][k].is_null())
        {
          int semanticIdx = semanticValues[i][j][k];
          semanticSrf = semanticSurfaces[semanticIdx];
        }
        IFMEFace *face = FMECityJSONReader::parseCityJSONPolygon(boundaries[i][j][k], semanticSrf);
        outerSurface->appendPart(face);
      }
    }
    IFMEBRepSolid *BSolid = fmeGeometryTools_->createBRepSolidBySurface(outerSurface);
    multiCompositeSolid->appendPart(BSolid);
  }
}

IFMEBRepSolid* FMECityJSONReader::parseSolid(json::array_t& boundaries, json::array_t& semanticValues,
                                             json::array_t& semanticSurfaces)
{
  // TODO: this will be problematic with inner shells
  IFMECompositeSurface *outerSurface = fmeGeometryTools_->createCompositeSurface();
  int nrShells = distance(begin(boundaries), end(boundaries));
  for (int i=0; i < nrShells; i++) {
    int nrSurfaces = distance(begin(boundaries[i]), end(boundaries[i]));
    for (int j=0; j < nrSurfaces; j++) {
      json::value_type semanticSrf;
      if (not semanticValues[i][j].is_null()) {
        int semanticIdx = semanticValues[i][j];
        semanticSrf = semanticSurfaces[semanticIdx];
      }
      IFMEFace *face = FMECityJSONReader::parseCityJSONPolygon(boundaries[i][j], semanticSrf);
      outerSurface->appendPart(face);
    }
  }
  IFMEBRepSolid *BSolid = fmeGeometryTools_->createBRepSolidBySurface(outerSurface);
  return BSolid;
}

template <typename MCSurface>
void FMECityJSONReader::parseMultiCompositeSurface(MCSurface multiCompositeSurface, json::array_t& boundaries,
                                                   json::array_t& semanticValues, json::array_t& semanticSurfaces)
{
  int nrSurfaces = distance(begin(boundaries), end(boundaries));
  for (int i=0; i < nrSurfaces; i++) {
    json::value_type semanticSrf;
    if (not semanticValues[i].is_null()) {
      int semanticIdx = semanticValues[i];
      semanticSrf = semanticSurfaces[semanticIdx];
    }
    IFMEFace *face = FMECityJSONReader::parseCityJSONPolygon(boundaries[i], semanticSrf);
    multiCompositeSurface->appendPart(face);
  }
}

IFMEFace* FMECityJSONReader::parseCityJSONPolygon(json::value_type surface, json::value_type semanticSurface)
{
  IFMELine *line = fmeGeometryTools_->createLine();
  FMECityJSONReader::parseCityJSONRing(line, surface);

  // TODO: Create the appearance for the face here. See:
  // https://github.com/safesoftware/fme-CityJSON/blob/c203e92bd06a9e6c0cb25a7fb7be8c182a63675e/fmecityjson/fmecityjsonreader.cpp#L271-L341

  IFMEArea *area = fmeGeometryTools_->createSimpleAreaByCurve(line);
  IFMEFace *face = fmeGeometryTools_->createFaceByArea(area, FME_CLOSE_3D_EXTEND_MODE);

  // TODO: Set the appearance for the face here. See:
  // https://github.com/safesoftware/fme-CityJSON/blob/c203e92bd06a9e6c0cb25a7fb7be8c182a63675e/fmecityjson/fmecityjsonreader.cpp#L346-L350

  // TODO: How to handle childrent and parent semantics?
  // Setting semantics
  if (not semanticSurface.is_null()) {
    IFMEString *geometryName = gFMESession->createString();
    geometryName->set(semanticSurface.at("type").dump().c_str(), semanticSurface.at("type").dump().length());
    face->setName(*geometryName, nullptr);
    gFMESession->destroyString(geometryName);

    for (json::iterator it = semanticSurface.begin(); it != semanticSurface.end(); it++) {
      if (it.key() != "type" && it.key() != "children" && it.key() != "parent") {
        if (it.value().is_string()) {
          FMECityJSONReader::setTraitString(face, it.key(), it.value());
        }
        else if (it.value().is_number_float()) {
          IFMEString* geometryTrait = gFMESession->createString();
          geometryTrait->set(it.key().c_str(), it.key().length());
          face->setTraitReal64(*geometryTrait, it.value());
          gFMESession->destroyString(geometryTrait);
        }
        else if (it.value().is_number_integer()) {
          IFMEString* geometryTrait = gFMESession->createString();
          geometryTrait->set(it.key().c_str(), it.key().length());
          face->setTraitInt64(*geometryTrait, it.value());
          gFMESession->destroyString(geometryTrait);
        }
        else if (it.value().is_boolean()) {
          IFMEString* geometryTrait = gFMESession->createString();
          geometryTrait->set(it.key().c_str(), it.key().length());
          if (it.value()) face->setTraitBoolean(*geometryTrait, FME_TRUE);
          else face->setTraitBoolean(*geometryTrait, FME_FALSE);
          gFMESession->destroyString(geometryTrait);
        }
        else {
          std::string val = it.value().type_name();
          gLogFile->logMessageString(("Semantic Surface attribute type '" + val + "' is not allowed.").c_str(),
                                     FME_WARN);
        }
      }
    }
  }

  return face;
}

void FMECityJSONReader::parseCityJSONRing(IFMELine *line,
                                          json::value_type &boundary) {
  for (json::iterator it = boundary.begin(); it != boundary.end(); ++it) {
    for (int vertex : it.value()) {
      line->appendPointXYZ(std::get<0>(vertices_[vertex]),
                           std::get<1>(vertices_[vertex]),
                           std::get<2>(vertices_[vertex]));
    }
  }
}

void FMECityJSONReader::setTraitString(IFMEGeometry *geometry,
                                       const std::string &traitName,
                                       const std::string &traitValue) {
  IFMEString* geometryTrait = gFMESession->createString();
  geometryTrait->set(traitName.c_str(), traitName.length());

  IFMEString* value = gFMESession->createString();
  value->set(traitValue.c_str(), traitValue.length());

  geometry->setTraitString(*geometryTrait, *value);

  gFMESession->destroyString(geometryTrait);
  gFMESession->destroyString(value);
}


//===========================================================================
// readSchema
FME_Status FMECityJSONReader::readSchema(IFMEFeature& feature, FME_Boolean& endOfSchema)
{
   // In the variable schema reader case, this is where the feature schemas are read

   // The purpose of this method is to read 'schema features', which are descriptions
   // of what types of 'data features' this will produce for the given dataset.

   // For some formats, these schema features are easy to produce, such as looking
   // at database column types, or metadata in file headers.

   // I think in CityJSON's case, we need to do a full parse/scan of the file to
   // determine all the schema features.  If this is true, the easiest thing to do is
   // scan it once, store up the schema features, and return them one at a time
   // when asked.

   if (!schemaScanDone_)
   {
      // iterate through every object in the file.
      for (auto& cityObject: inputJSON_.at("CityObjects"))
      {
         // I'm not sure exactly what types of features this reader will
         // produce, so this is just a wild guess as an example.
         std::string object = cityObject.dump();

         // Let's find out what we will be using as the "feature_type", and
         // group the schema features by that.  I'll pick the field "type".

         std::string featureType = cityObject.at("type");

         // Let's see if we already have seen a feature of this 'type'.
         // If not, create a new schema feature.  If we have, just add to it I guess.
         auto schemaFeature = schemaFeatures_.find(featureType);
         IFMEFeature* sf(nullptr);
         if (schemaFeature == schemaFeatures_.end())
         {
            sf = gFMESession->createFeature();
            sf->setFeatureType(featureType.c_str());
            gLogFile->logMessageString(featureType.c_str(), FME_INFORM);
            schemaFeatures_[featureType] = sf; // gives up ownership
         }
         else
         {
             gLogFile->logMessageString("schemaFeature != schemaFeatures_.end()", FME_INFORM);
            sf = schemaFeature->second;
         }

         // iterate through every attribute on this object.
         for (json::iterator it = cityObject.at("attributes").begin(); it != cityObject.at("attributes").end(); ++it) 
         {
            const std::string& attributeName = it.key();
            const auto& attributeValue = it.value();
            // For now, I'm just guessing at the type of this attribute.
            // TODO: Something smarter really should be done here.
            // The value here must be something found in the left hand
            // column of the ATTR_TYPE_MAP line in the metafile 'fmecityjson.fmf'
            std::string attributeType = "string"; // could be string, real64, uint32, logical, char, date, time, etc.

            // Schema feature attributes need to be set with setSequencedAttribute()
            // to preserve the order of attributes.
            sf->setSequencedAttribute(attributeName.c_str(), attributeType.c_str());
         }


         // Here we add to the schema feature all the possible geometries of the
         // feature type.  Arc and ellipse geometries require that you also set
         // fme_geomattr on them.  Setting the fme_geomattr is required for
         // backwards compatible with writers that only support classic geometry.

         // TODO: I will leave it for future work to iterate over all the
         // geometries, map the JSON types to the types needed here, and properly
         // accumulate all possible geometry types per schema feature, avoiding duplicates.

         // The value here must be something found in the left hand
         // column of the GEOM_MAP line in the metafile 'fmecityjson.fmf'

         // But the type you'd scan for is this, I think:
         std::string geometryType = cityObject.at("geometry")[0].at("type").dump();
         gLogFile->logMessageString(geometryType.c_str(), FME_INFORM);

         // For now, I'll just hard code that each schema feature type 
         // may have many possible geometry types.
         sf->setAttribute("fme_geometry{0}", "cityjson_point");

         sf->setAttribute("fme_geometry{1}", "cityjson_linestring");

         sf->setAttribute("fme_geometry{2}", "cityjson_multilinestring");

         sf->setAttribute("fme_geometry{3}", "cityjson_polygon");

         sf->setAttribute("fme_geometry{4}", "cityjson_text");
         sf->setAttribute("fme_geometry{4}.fme_text_string", "string");
         sf->setAttribute("fme_geometry{4}.fme_text_size", "number(31,15)");

         sf->setAttribute("fme_geometry{5}", "cityjson_multi_text");
         sf->setAttribute("fme_geometry{5}.fme_text_string", "string");
         sf->setAttribute("fme_geometry{5}.fme_text_size", "number(31,15)");

         sf->setAttribute("fme_geometry{6}", "cityjson_collection");

         sf->setAttribute("fme_geometry{7}", "cityjson_null");

         sf->setAttribute("fme_geometry{8}", "cityjson_surface");

         sf->setAttribute("fme_geometry{9}", "cityjson_multisurface");

         sf->setAttribute("fme_geometry{10}", "cityjson_compositesurface");

         sf->setAttribute("fme_geometry{11}", "cityjson_solid");

         sf->setAttribute("fme_geometry{12}", "cityjson_multisolid");

         sf->setAttribute("fme_geometry{13}", "cityjson_compositesolid");
      }

      schemaScanDone_ = true;
   }

   if (schemaFeatures_.empty())
   {
       gLogFile->logMessageString("schemaFeatures_.empty()", FME_INFORM);
      endOfSchema = FME_TRUE;
      return FME_SUCCESS;
   }

   // Let's take one schema feature off our waiting list
   IFMEFeature* schemaFeature = schemaFeatures_.begin()->second;
   schemaFeatures_.erase(schemaFeatures_.begin());
   // Too bad the API forces us to make a copy.
   schemaFeature->clone(feature);
   gFMESession->destroyFeature(schemaFeature);

   endOfSchema = FME_FALSE;
   return FME_SUCCESS;
}

//===========================================================================
// readParameterDialog
void FMECityJSONReader::readParametersDialog()
{
   IFMEString* paramValue = gFMESession->createString();
   if (gMappingFile->fetchWithPrefix(readerKeyword_.c_str(), readerTypeName_.c_str(), kSrcCityJSONParamTag, *paramValue)) 
   {
      // A parameter value has been found, so set the values.
      cityJsonParameters_ = paramValue->data();

      // Let's log to the user that a parameter value has been specified.
      std::string paramMsg = (kCityJSONParamTag + cityJsonParameters_).c_str();
      gLogFile->logMessageString("Let's log to the user that a parameter value has been specified.", FME_INFORM);
      gLogFile->logMessageString(paramMsg.c_str(), FME_INFORM);
   }
   else
   {
      // Log that no parameter value was entered.
      gLogFile->logMessageString("Log that no parameter value was entered", FME_INFORM);
      gLogFile->logMessageString(kMsgNoCityJSONParam, FME_INFORM);
   }
   gFMESession->destroyString(paramValue);
}

FME_Status FMECityJSONReader::readRaster(const std::string& fullFileName, FME_UInt32& appearanceReference, std::string readerToUse)
{
   IFMEUniversalReader* newReader(nullptr);
   if (readerToUse.length() == 0)
   {
      readerToUse = "GENERIC";
   }

   newReader = gFMESession->createReader(readerToUse.c_str(), FME_FALSE, nullptr);
   if (!newReader)
   {
      // TODO: Log some error message
      return FME_FAILURE;
   }

   // Now let's make a raster out of this file.

   IFMEStringArray* parameters = gFMESession->createStringArray();
   FME_MsgNum badLuck = newReader->open(fullFileName.data(), *parameters);
   gFMESession->destroyStringArray(parameters);
   if (badLuck)
   {
      // TODO: Log some error message
      return FME_FAILURE;
   }
   FME_Boolean endOfFile = FME_FALSE;
   IFMEFeature* textureFeature = gFMESession->createFeature();

   if (nullptr == textureFeature)
   {
      // TODO: Log some error message
      return FME_FAILURE;
   }

   badLuck = newReader->read(*textureFeature, endOfFile);
   if (badLuck)
   {
      // TODO: Log some error message
      return FME_FAILURE;
   }

   IFMEGeometry* geom = textureFeature->removeGeometry();

   if (nullptr == geom || !geom->canCastAs<IFMERaster*>())
   {
      // TODO: Log some error message
      return FME_FAILURE;
   }

   IFMERaster* raster =geom->castAs<IFMERaster*>();

   //Close the reader and *ignore* any errors.
   badLuck = newReader->close();

   // clean up
   gFMESession->destroyFeature(textureFeature); textureFeature = nullptr;
   gFMESession->destroyReader(newReader); newReader = nullptr;

   // Let's stick this raster into the FME Library so we can refer to it by reference.
   // TODO: Really, we should only add each once, and share it among all features that refer to it.
   FME_UInt32 rasterRef(0);
   badLuck = gFMESession->getLibrary()->addRaster(rasterRef, raster);
   if (badLuck)
   {
      // TODO: Log some error message
      return FME_FAILURE;
   }

   IFMETexture* texture = fmeGeometryTools_->createTexture();
   texture->setRasterReference(rasterRef);

   // Let's stick this texture into the FME Library so we can refer to it by reference.
   // TODO: Really, we should only add each once, and share it among all features that refer to it.
   FME_UInt32 textureRef(0);
   badLuck = gFMESession->getLibrary()->addTexture(textureRef, texture);
   if (badLuck)
   {
      // TODO: Log some error message
      return FME_FAILURE;
   }

   IFMEAppearance* appearance = fmeGeometryTools_->createAppearance();
   appearance->setTextureReference(textureRef);

   // Let's stick this appearance into the FME Library so we can refer to it by reference.
   // TODO: Really, we should only add each once, and share it among all features that refer to it.
   badLuck = gFMESession->getLibrary()->addAppearance(appearanceReference, appearance);
   if (badLuck)
   {
      // TODO: Log some error message
      return FME_FAILURE;
   }

   return FME_SUCCESS;
}

