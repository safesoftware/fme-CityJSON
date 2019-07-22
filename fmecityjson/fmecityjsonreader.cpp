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

   gLogFile->logMessageString("L144", FME_INFORM);
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
   gLogFile->logMessageString("L208", FME_INFORM);
   if (nextObject_ == inputJSON_.at("CityObjects").end())
   {
      endOfFile = FME_TRUE;
      return FME_SUCCESS;
   }

   // Set the feature type
   std::string featureType = nextObject_.value().at("type");
   feature.setFeatureType(featureType.c_str());

   // iterate through every attribute on this object.
   for (json::iterator it = nextObject_.value().at("attributes").begin(); it != nextObject_.value().at("attributes").end(); ++it) 
   {
       gLogFile->logMessageString("L222", FME_INFORM);
      const std::string& attributeName = it.key();
      const auto& attributeValue = it.value();
      // For now, I'm just guessing at the type of this attribute.
      // TODO: Something smarter really should be done here.
      feature.setAttribute(attributeName.c_str(), attributeValue.dump().c_str());
   }

   // Set the geometry

   // TODO: For now, I'm just going to see if the first geometry is a MultiSurface
   // and do that.  If it is anything else, it will stay as a NULL geometry.
   if (nextObject_.value().at("geometry")[0].at("type") == "MultiSurface")
   {
      std::string geometry = nextObject_.value().at("geometry")[0].dump();
      gLogFile->logMessageString("L236", FME_INFORM);
      gLogFile->logMessageString(geometry.c_str(), FME_INFORM);

      IFMEMultiSurface* ms = fmeGeometryTools_->createMultiSurface();

      // Loop through each Surface.
      for (int i(0); i < nextObject_.value().at("geometry")[0].at("boundaries").size(); ++i)
      {
         FME_UInt32 appearanceReference(0);
         json::value_type boundary = nextObject_.value().at("geometry")[0].at("boundaries")[i];
         gLogFile->logMessageString(typeid(boundary).name(), FME_INFORM);
        // N8nlohmann10basic_jsonISt3mapSt6vectorNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEblmdSaNS_14adl_serializerEEE

         // Let's build up this boundary.
         std::string boundaryString = boundary.dump();

         // add some coordinates
         IFMELine* line = fmeGeometryTools_->createLine();
         FMECityJSONReader::parseCityJSONRing(inputJSON_, line, boundary);

//         auto textureCoordinates = nextObject_.value().at("geometry")[0].at("texture").at("rgbTexture").at("values")[i];
//         std::string tcString = textureCoordinates.dump();
//         gLogFile->logMessageString(tcString.c_str(), FME_INFORM);
//
//         if (!textureCoordinates.empty())
//         {
//            // add some texture coordinates
//            FME_Real64 textureCoordsU[100];  // TODO: set size dynamically
//            FME_Real64 textureCoordsV[100];
//
//            for (auto & textureCoordinate : textureCoordinates)
//            {
//               std::string textureCoords = textureCoordinate.dump();
//               gLogFile->logMessageString(textureCoords.c_str(), FME_INFORM);
//
//               // We need to check for cases where it is "[null]"
//               if (textureCoordinate.size() > 1)
//               {
//                  for (int k(0); k < textureCoordinate.size(); ++k)
//                  {
//                     if (k==0)
//                     {
//                        // This is the index of the texture to use, I think.
//                        int textureFilenameIndex = textureCoordinate[k];
//                        std::string textureFilename = inputJSON_.at("appearance").at("textures")[textureFilenameIndex].at("image");
//
//                        // This is really gross code here.  Should be a separate method, etc.
//                        // but I thought it would just be a good example starting point.
//                        std::string fullFileName = dataset_;
//                        // TODO: I guess finding the directory the dataset is in may be tricky,
//                        // and different on Windows and Linux, etc.   This is quick and dirty.
//                        if (fullFileName.find_last_of("/") != std::string::npos)
//                        {
//                           fullFileName.erase(fullFileName.find_last_of("/")+1, std::string::npos);
//                        }
//                        if (fullFileName.find_last_of("\\") != std::string::npos)
//                        {
//                           fullFileName.erase(fullFileName.find_last_of("\\")+1, std::string::npos);
//                        }
//                        fullFileName += textureFilename;
//                        // TODO: We are using "GENERIC" to guess at what type the image is.  But
//                        // I think CityJSON knows the image type, so that hint could be passed in here
//                        // instead if we wished, I guess.
//                        badLuck = readRaster(fullFileName, appearanceReference, "GENERIC");
//                        if (badLuck != FME_SUCCESS) return badLuck;
//                     }
//                     else
//                     {
//                        // These are the indexes of the texture coordinates.
//                        int tcvertex = textureCoordinate[k];
//                        std::string vertexArray = inputJSON_.at("appearance").at("vertices-texture")[tcvertex].dump();
//                        gLogFile->logMessageString(vertexArray.c_str(), FME_INFORM);
//
//                        if (k-1 >= line->numPoints())
//                        {
//                           // There is a mismatch of # texture coordinates with real vertices.
//                           // TODO: Log some error message
//                           int bob(0);
//                        }
//                        else
//                        {
//                           badLuck = line->setNamedMeasureAt(*textureCoordUName_, k-1, inputJSON_.at("appearance").at("vertices-texture")[tcvertex][0]);
//                           if (badLuck != FME_SUCCESS) return badLuck;
//                           badLuck = line->setNamedMeasureAt(*textureCoordVName_, k-1, inputJSON_.at("appearance").at("vertices-texture")[tcvertex][1]);
//                           if (badLuck != FME_SUCCESS) return badLuck;
//                        }
//                     }
//                  }
//               }
//            }
//         }

         IFMEArea* area = fmeGeometryTools_->createSimpleAreaByCurve(line);
         IFMEFace* face = fmeGeometryTools_->createFaceByArea(area, FME_CLOSE_3D_EXTEND_MODE);

//         // For now we'll leave the face single-sided.
//         if (appearanceReference != 0) // Only set one if we found one.
//         {
//            face->setAppearanceReference(appearanceReference, FME_TRUE);
//         }

         // Here we could scan the CityJSON and see what optional GeometryName we could set.
         // Actually, the line, area, face, and ms could all have a GeometryName, if it is relevant.
         // Any FME geometry can have one.
         // TODO: For now, I'll just hardcode one.
         IFMEString* geometryName = gFMESession->createString();
         *geometryName = "WallSurface";
         face->setName(*geometryName, nullptr);
         gFMESession->destroyString(geometryName);
         
         // Here we could scan the CityJSON and see what optional GeometryName we could set
         ms->appendPart(face);
      }

      // Here we could scan the CityJSON and see what optional Geometry Traits we could set.
      // (A Geometry Trait is just an attribute stored at the geometry level, not at the top feature level.)
      // Actually, the line, area, face, and ms could all have Traits, if they are relevant.
      // Any FME geometry can have them.
      // TODO: For now, I'll just hardcode some.
      IFMEString* geometryTrait = gFMESession->createString();
      *geometryTrait = "Custom Float Value";
      ms->setTraitReal64(*geometryTrait, 42.0);
      *geometryTrait = "Custom Unsigned Integer Value";
      ms->setTraitUInt32(*geometryTrait, 1234);
      gFMESession->destroyString(geometryTrait);

      // Place the geometry on the feature
      feature.setGeometry(ms);
   }

   // Set the coordinate system
   feature.setCoordSys(coordSys_.c_str());

   // Log the feature
   gLogFile->logFeature(feature);

   ++nextObject_;

   endOfFile = FME_FALSE;
   return FME_SUCCESS;
}

void FMECityJSONReader::parseCityJSONRing(json& inputJSON_, IFMELine* line, json::value_type& boundary) {
  std::string bdr = boundary.dump();
  gLogFile->logMessageString(("parseCityJSONRing input boundary: " + bdr).c_str(), FME_INFORM);
//  [[49,50,51,52,53,54]]
  for (json::iterator it = boundary.begin(); it != boundary.end(); ++it)
  {
    std::string coords = it.value().dump();
    for(int vertex : it.value())
    {
      std::string vertexArray = inputJSON_.at("vertices")[vertex].dump();

      line->appendPointXYZ(inputJSON_.at("vertices")[vertex][0],
                           inputJSON_.at("vertices")[vertex][1],
                           inputJSON_.at("vertices")[vertex][2]);
    }
  }
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
         gLogFile->logMessageString("L416", FME_INFORM);
         gLogFile->logMessageString(object.c_str(), FME_INFORM);

         // Let's find out what we will be using as the "feature_type", and
         // group the schema features by that.  I'll pick the field "type".

         std::string featureType = cityObject.at("type");

         // Let's see if we already have seen a feature of this 'type'.
         // If not, create a new schema feature.  If we have, just add to it I guess.
         auto schemaFeature = schemaFeatures_.find(featureType);
         IFMEFeature* sf(nullptr);
         if (schemaFeature == schemaFeatures_.end())
         {
             gLogFile->logMessageString("schemaFeature == schemaFeatures_.end()", FME_INFORM);
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
             gLogFile->logMessageString("attributes iterator", FME_INFORM);
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

