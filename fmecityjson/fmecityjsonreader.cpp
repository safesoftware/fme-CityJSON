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
#include <idonut.h>
#include <ipoint.h>
#include <ipolygon.h>
#include <imultisurface.h>
#include <ireader.h>
#include <iraster.h>
#include <ilibrary.h>

#include <sstream>
#include <iomanip>
#include <filesystem>

// These are initialized externally when a reader object is created so all
// methods in this file can assume they are ready to use.
IFMELogFile* FMECityJSONReader::gLogFile             = nullptr;
IFMEMappingFile* FMECityJSONReader::gMappingFile     = nullptr;
IFMECoordSysManager* FMECityJSONReader::gCoordSysMan = nullptr;
IFMESession* gFMESession                             = nullptr;

//===========================================================================
FME_Status fetchSchemaFeatures(IFMELogFile& logFile,
                               const std::string& schemaVersion,
                               std::map<std::string, IFMEFeature*>& schemaFeatures)
{
   // This will look in the official CityJSON specs
   // and pull out the correct schema information.

   // We know which major/minor version of the schemas we want.
   // Let's see if we can find it.

   std::string schemaDir = gFMESession->fmeHome();
   schemaDir += "/plugins/cityjson/" + schemaVersion + "/schemas/";

   // Here we would look in the FME install directory for all the schema
   // definitions, and create a schema feature for each FeatureType.
   //
   // In this case we know we are just looking for the minimized schema file only

   std::string schemafile = schemaDir + "cityjson.min.schema.json";

   // Open up the schema file.
   std::ifstream inputFile;
   inputFile.open(schemafile, std::ios::in);

   // Check that the file exists.
   if (!inputFile.good())
   {
      logFile.logMessageString("Unknown setting for CityJSON writer's starting schema.", FME_ERROR);
      logFile.logMessageString("Schema file does not exist", FME_ERROR);
      return FME_FAILURE;
   }

   // Reset the file stream and start reading.
   inputFile.seekg(0, std::ios::beg);
   inputFile.clear();
   json inputJSON;
   inputJSON = json::parse(inputFile);

   // Let's first read the metadata schema separately, as it is a bit different.

   // Create the schema feature
   IFMEFeature* schemaFeature(nullptr);
   schemaFeature = gFMESession->createFeature();
   // set the Feature Type
   schemaFeature->setFeatureType("Metadata");

   // Add the expected attribute names and types

   // Note: I don't know the best way to parse these json files, so forgive the
   // hacky way I pull out information.  Feel free to clean up this kind of code!
   json::iterator propIt = inputJSON.at("properties").at("metadata").at("properties").begin();
   const json::iterator propEnd = inputJSON.at("properties").at("metadata").at("properties").end();
   while (propIt != propEnd)
   {
      addAttributeNamesAndTypes(*schemaFeature, propIt.key(), propIt.value());
      propIt++;
   }

   // set the expected Geometry types.  You can have more than one
   schemaFeature->setAttribute("fme_geometry{0}", "fme_no_geom");

   // Drop the completed schema feature into the bucket for future use
   schemaFeatures["Metadata"] = schemaFeature; // gives up ownership
   schemaFeature              = nullptr;

   // Now, let's loop through all the CityObjects
   for (auto oneItem : inputJSON.at("properties").at("CityObjects").at("additionalProperties")["oneOf"])
   {
      // Create the schema feature
      schemaFeature = gFMESession->createFeature();

      // Make sure they all have a field for their Feature ID.
      schemaFeature->setSequencedAttribute("fid", "string");

      // Start off as blank.  We'll fill it in as we scrape info.
      std::string featureType;

      // Add the expected attribute names and types

      for (auto itemPart : oneItem["allOf"])
      {
         if (itemPart["allOf"].is_array())
         {
            for (auto itemSubPart : itemPart["allOf"])
            {
               addObjectProperties(itemSubPart, *schemaFeature, featureType);
            }
         }
         else
         {
            addObjectProperties(itemPart, *schemaFeature, featureType);
         }
      }

      // Maybe we didn't really read a FeatureType...
      if (featureType.empty())
      {
         gFMESession->destroyFeature(schemaFeature);
         schemaFeature = nullptr;
      }
      else
      {
         // set the Feature Type
         schemaFeature->setFeatureType(featureType.c_str());

         // set the expected Geometry types.  You can have more than one
         schemaFeature->setAttribute("fme_geometry{0}", "fme_no_geom");

         // Drop the completed schema feature into the bucket for future use
         schemaFeatures[featureType] = schemaFeature; // gives up ownership
         schemaFeature               = nullptr;
      }
   }

   return FME_SUCCESS;
}

void addAttributeNamesAndTypes(IFMEFeature& schemaFeature,
                               const std::string& attributeName,
                               json attributeValue)
{
   std::string attributeType = attributeValue["type"].dump();
   if (std::string("\"string\"") == attributeType)
   {
      // Simple string attribute case
      schemaFeature.setSequencedAttribute(attributeName.c_str(), "string");
   }
   else if (std::string("\"number\"") == attributeType)
   {
      // Simple number attribute case - but I'm just guessing at real64.  What should it map to?
      schemaFeature.setSequencedAttribute(attributeName.c_str(), "real64");
   }
   else if (std::string("\"integer\"") == attributeType)
   {
      // Simple integer attribute case - but I'm just guessing at int32.  What should it map to?
      schemaFeature.setSequencedAttribute(attributeName.c_str(), "int32");
   }
   else if (std::string("\"object\"") == attributeType)
   {
      // This is an attribute with sub-parts, so we typically mark this as separated by a "."
      json::iterator propIt        = attributeValue.at("properties").begin();
      const json::iterator propEnd = attributeValue.at("properties").end();

      while (propIt != propEnd)
      {
         addAttributeNamesAndTypes(schemaFeature, attributeName + "." + propIt.key(), propIt.value());
         propIt++;
      }
   }
   else if (std::string("null") == attributeType)
   {
      // This does not have an explicit type.  I'm not sure what to do.
      // I am kind of treating this as an "object" and doing the same case as above...
      for (auto const& entry : attributeValue.items())
      {
         if (entry.value().is_object())
         {
            addAttributeNamesAndTypes(schemaFeature, attributeName + "." + entry.key(), entry.value());
         }
      }
   }
   else if (std::string("\"array\"") == attributeType)
   {
      // We want to ignore the parent and children attributes, as they are already reserved as
      // special format attributes "cityjson_children" and "cityjson_parents"
      if ((std::string("children") != attributeName) && (std::string("parents") != attributeName))
      {
         // This is an array of values, so we typically use "list attributes" in FME to pass these around.
         addAttributeNamesAndTypes(schemaFeature, attributeName + "{}", attributeValue["items"]);
      }
   }
   else // we don't handle it yet
   {
      // do nothing?  Not sure what other cases may need to be handled.
   }
}

//===========================================================================
void addObjectProperties(json::value_type& itemPart, IFMEFeature& schemaFeature, std::string& featureType)
{
   json::iterator propIt        = itemPart.at("properties").begin();
   const json::iterator propEnd = itemPart.at("properties").end();
   while (propIt != propEnd)
   {
      if (propIt.key() == "attributes")
      {
         json::iterator propIt3 = itemPart.at("properties").at("attributes").at("properties").begin();
         const json::iterator propEnd3 =
            itemPart.at("properties").at("attributes").at("properties").end();

         while (propIt3 != propEnd3)
         {
            addAttributeNamesAndTypes(schemaFeature, propIt3.key(), propIt3.value());
            propIt3++;
         }
      }
      else if (propIt.key() == "type" && propIt.value()["enum"].is_array())
      {
         // Let's pick out what I *think* should be the Feature Type?
         featureType = propIt.value()["enum"][0].get<std::string>();
      }
      else
      {
         addAttributeNamesAndTypes(schemaFeature, propIt.key(), propIt.value());
      }
      propIt++;
   }
}

//===========================================================================
// Constructor
FMECityJSONReader::FMECityJSONReader(const char* readerTypeName, const char* readerKeyword)
   : readerTypeName_(readerTypeName),
     readerKeyword_(readerKeyword),
     dataset_(""),
     coordSys_(""),
     fmeGeometryTools_(nullptr),
     schemaScanDone_(false),
     schemaScanDoneMeta_(false),
     textureCoordUName_(nullptr),
     textureCoordVName_(nullptr),
     writerHelperMode_(false)
{
   textureCoordUName_  = gFMESession->createString();
   *textureCoordUName_ = kFME_texture_coordinate_u;
   textureCoordVName_  = gFMESession->createString();
   *textureCoordVName_ = kFME_texture_coordinate_v;
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
   dataset_          = datasetName;

   // There is one case, where the WRITER is calling this method to get help
   // in putting up the correct feature types.  Having this decided in code provides
   // the most flexibility.
   if (fetchWriterDirectives(parameters))
   {
      // Hey, we're not really being opened as a reader, but as a helper to the writer.
      // we can skip a lot of the stuff below.
      return FME_SUCCESS;
   }

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
      gLogFile->logMessageString("Input file does not exist", FME_ERROR);
      return FME_FAILURE;
   }

   // Reset the file stream and start reading.
   inputFile_.seekg(0, std::ios::beg);
   inputFile_.clear();

   inputJSON_ = json::parse(inputFile_);

   // Let's make sure we're parsing this correctly.
   if (inputJSON_.at("type").get<std::string>() != "CityJSON")
   {
      gLogFile->logMessageString("Not a CityJSON file", FME_ERROR);
      return FME_FAILURE;
   }

   // Let's make sure we are reading a version that we expect
   std::string supportedVersion = "1.0";
   if (inputJSON_.at("version").get<std::string>() < supportedVersion)
   {
      std::stringstream versionStream;
      versionStream << "Unsupported CityJSON version: " << inputJSON_.at("version").get<std::string>()
                    << ". Only the version " << supportedVersion << " or higher are supported.";
      gLogFile->logMessageString(versionStream.str().c_str(), FME_ERROR);
      return FME_FAILURE;
   }

   // Reads in the entire batch of vertices for this file.
   readVertexPool();

   // Read the mapping file parameters. Always do this, otherwise the parameters are not
   // recognized when the Reader is created in the Workspace, only when its executed.
   // We do this to get the LOD parameter, maybe others.
   readParametersDialog();

   // Scan the LODs in the file, and match to what the reader is requesting.
   scanLODs();

   readMetadata();

   readMaterials();

   readTextures();

   readTextureVertices();

   // These need to be read in after all the appearances/textures/materials have been populated.
   FME_Status badLuck = readGeometryDefinitions();
   if (FME_SUCCESS != badLuck) return badLuck;

   // Start by pointing to the first CityObject to read
   nextObject_     = inputJSON_.at("CityObjects").begin();
   skippedObjects_ = 0;

   return FME_SUCCESS;
}

//===========================================================================
void FMECityJSONReader::readTextureVertices()
{
   // Check for texture verrtices in the file
   try
   {
      json textureVertices = inputJSON_.at("appearance").at("vertices-texture");

      // Texture Vertices
      if (not textureVertices.is_null())
      {
         for (auto tvtx : textureVertices)
         {
            textureVertices_.emplace_back(tvtx[0], tvtx[1]);
         }
      }
   }
   catch (json::out_of_range& e)
   {
      gLogFile->logMessageString("The file does not contain any texture definitions.", FME_INFORM);
   }
}

//===========================================================================
void FMECityJSONReader::readTextures()
{
   // Check for textures in the file
   try
   {
      json textures = inputJSON_.at("appearance").at("textures");

      int nrTextures = distance(begin(textures), end(textures));
      for (int i = 0; i < nrTextures; i++)
      {
         // Get the "type"
         std::string rasterType;
         if (not textures[i]["type"].is_null())
         {
            std::string givenType = textures[i]["type"].get<std::string>();
            // These are the only expected types for now
            if (givenType == "PNG")
            {
               rasterType = "PNGRASTER";
            }
            else if (givenType == "JPG")
            {
               rasterType = "JPEG";
            }
         }

         // Get the "image"
         IFMERaster* raster(nullptr);
         if (not textures[i]["image"].is_null())
         {
            std::string imagePath    = textures[i]["image"].get<std::string>();
            std::string fullFileName = imagePath;

            // We've got to make the full path, if it is relative.
            // If it starts with "http" we know it's not relative.  If it can
            // be found as a full path we'll also assume it is not relative.
            if ((imagePath.rfind("http", 0) != 0) && (!std::filesystem::exists("imagePath")))
            {
               // Fix up the relative pathname so we can find it.
               // It is relative to the current dataset path.

               // This is really gross code here.  Should be a separate method, etc.
               // but I thought it would just be a good example starting point.
               fullFileName = dataset_;
               // TODO: I guess finding the directory the dataset is in may be tricky,
               // and different on Windows and Linux, etc.   This is quick and dirty.
               if (fullFileName.find_last_of("/") != std::string::npos)
               {
                  fullFileName.erase(fullFileName.find_last_of("/") + 1, std::string::npos);
               }
               if (fullFileName.find_last_of("\\") != std::string::npos)
               {
                  fullFileName.erase(fullFileName.find_last_of("\\") + 1, std::string::npos);
               }
               fullFileName += imagePath;
            }

            FME_Status badLuck = readRaster(fullFileName, raster, rasterType);
         }

         // Set the Raster on the texture.
         IFMETexture* tex = fmeGeometryTools_->createTexture();
         if (raster)
         {
            // Add the Raster to the FME Library
            FME_UInt32 rasterRef(0);
            FME_Status badLuck = gFMESession->getLibrary()->addRaster(rasterRef, raster);
            raster             = nullptr; // We no longer have ownership.
            tex->setRasterReference(rasterRef);
         }

         // Set the "borderColor"
         if (not textures[i]["borderColor"].is_null())
         {
            // Note: Alpha is not used here.
            tex->setBorderColor(textures[i]["borderColor"][0],
                                textures[i]["borderColor"][1],
                                textures[i]["borderColor"][2]);
         }

         // Set the "wrapMode"
         // Note that if you set the border colour after this, it will
         // change the texture mode to FME_TEXTURE_BORDER_FILL.
         if (not textures[i]["wrapMode"].is_null())
         {
            std::string wrapmode = textures[i]["wrapMode"].get<std::string>();
            if (wrapmode == "none")
            {
               tex->setTextureWrap(FME_TEXTURE_NONE);
            }
            else if (wrapmode == "wrap")
            {
               tex->setTextureWrap(FME_TEXTURE_REPEAT_BOTH);
            }
            else if (wrapmode == "mirror")
            {
               tex->setTextureWrap(FME_TEXTURE_MIRROR);
            }
            else if (wrapmode == "clamp")
            {
               tex->setTextureWrap(FME_TEXTURE_CLAMP_BOTH);
            }
            else if (wrapmode == "border")
            {
               tex->setTextureWrap(FME_TEXTURE_BORDER_FILL);
            }
         }

         // Set the "textureType"
         // I'm not sure how best to represent this in FME.

         // Add the Texture to the FME Library
         FME_UInt32 textureRef(0);
         FME_Status badLuck = gFMESession->getLibrary()->addTexture(textureRef, tex);
         tex                = nullptr; // We no longer have ownership.

         // Set the texture on a new Appearance
         IFMEAppearance* app = fmeGeometryTools_->createAppearance();
         app->setTextureReference(textureRef);

         // Add the Appearance to the FME Library
         FME_UInt32 appRef(0);
         badLuck = gFMESession->getLibrary()->addAppearance(appRef, app);
         app     = nullptr; // We no longer have ownership.

         // Add the appearance (with texture) reference to the lookup table
         texturesMap_.insert({i, appRef});
      }

      // TODO: What is the "default"?  (Not sure where to store this in FME yet.)
      if (inputJSON_.at("appearance").contains("default-theme-texture"))
      {
         defaultThemeTexture_ =
            inputJSON_.at("appearance").at("default-theme-texture").get<std::string>();
      }
   }
   catch (json::out_of_range& e)
   {
      gLogFile->logMessageString("The file does not contain any texture definitions.", FME_INFORM);
   }
}

//===========================================================================
void FMECityJSONReader::readMaterials()
{
   // Check for materials in the file
   try
   {
      IFMEString* fmeVal = gFMESession->createString();

      json materials = inputJSON_.at("appearance").at("materials");

      int nrMaterials = distance(begin(materials), end(materials));
      for (int i = 0; i < nrMaterials; i++)
      {
         FME_UInt32 materialRef(0);
         IFMEAppearance* app = fmeGeometryTools_->createAppearance();

         // Set the "name"
         if (not materials[i]["name"].is_null())
         {
            std::string mName = materials[i]["name"].get<std::string>();
            fmeVal->set(mName.c_str(), mName.length());
            app->setName(*fmeVal, "fme-system");
         }

         // Set the "ambientIntensity"
         // I'm not sure how the ambient Intensity matches to FME's ambient colour.
         // For now, I'll set all three colour values to be the same.
         if (not materials[i]["ambientIntensity"].is_null())
         {
            app->setColorAmbient(materials[i]["ambientIntensity"],
                                 materials[i]["ambientIntensity"],
                                 materials[i]["ambientIntensity"]);
         }

         // Set the "diffuseColor"
         if (not materials[i]["diffuseColor"].is_null())
         {
            app->setColorDiffuse(materials[i]["diffuseColor"][0],
                                 materials[i]["diffuseColor"][1],
                                 materials[i]["diffuseColor"][2]);
         }

         // Set the "emissiveColor"
         if (not materials[i]["emissiveColor"].is_null())
         {
            app->setColorEmissive(materials[i]["emissiveColor"][0],
                                  materials[i]["emissiveColor"][1],
                                  materials[i]["emissiveColor"][2]);
         }

         // Set the "specularColor"
         if (not materials[i]["specularColor"].is_null())
         {
            app->setColorSpecular(materials[i]["specularColor"][0],
                                  materials[i]["specularColor"][1],
                                  materials[i]["specularColor"][2]);
         }

         // Set the "shininess"
         if (not materials[i]["shininess"].is_null())
         {
            app->setShininess(materials[i]["shininess"]);
         }

         // Set the "transparency"
         if (not materials[i]["transparency"].is_null())
         {
            app->setAlpha(1.0 - materials[i]["transparency"]);
         }

         // Set the "isSmooth"
         // currently there is no easy way to support this in FME.  Not yet.

         // Add the Material to the FME Library
         FME_Status badLuck = gFMESession->getLibrary()->addAppearance(materialRef, app);
         app                = nullptr; // We no longer have ownership.

         // Add the material reference to the lookup table
         materialsMap_.insert({i, materialRef});
      }

      // What is the "default"?  (Not sure where to store this in FME yet.)
      if (inputJSON_.at("appearance").contains("default-theme-material"))
      {
         defaultThemeMaterial_ =
            inputJSON_.at("appearance").at("default-theme-material").get<std::string>();
      }

      gFMESession->destroyString(fmeVal);
   }
   catch (json::out_of_range& e)
   {
      gLogFile->logMessageString("The file does not contain any material definitions.", FME_INFORM);
   }
}

//===========================================================================
void FMECityJSONReader::readMetadata()
{
   // Check for metadata in the file
   try
   {
      metaObject_         = inputJSON_.at("metadata");
      schemaScanDoneMeta_ = false;

      // Scrape the coordinate system
      try
      {
         std::string inputCoordSys = metaObject_.at("referenceSystem").get<std::string>();
         // Looking to make the form EPSG:XXXX
         inputCoordSys = inputCoordSys.substr(inputCoordSys.find_first_of("EPSG"));
         if (inputCoordSys.find("::") != std::string::npos)
         {
            // In case of OGC URN 'urn:ogc:def:crs:EPSG::7415
            coordSys_ = inputCoordSys.erase(inputCoordSys.find_first_of(":"), 1);
         }
         else if (inputCoordSys.find(":") != std::string::npos)
         {
            // In case of legacy EPSG:7415
            coordSys_ = inputCoordSys;
         }
         else
         {
            gLogFile->logMessageString("Cannot parse EPSG code. Please provide the EPSG code as "
                                       "OGC URN, "
                                       "for example 'urn:ogc:def:crs:EPSG::7415'.",
                                       FME_WARN);
         }
         gLogFile->logMessageString(("Coordinate Reference System is set to " + coordSys_).c_str(),
                                    FME_INFORM);
      }
      catch (json::out_of_range& e)
      {
         gLogFile->logMessageString("Coordinate Reference System is not set in the file", FME_WARN);
      }
   }
   catch (json::out_of_range& e)
   {
      gLogFile->logMessageString("The file does not contain any metadata ('referenceSystem', "
                                 "'geographicalExtent' etc.)",
                                 FME_WARN);
      schemaScanDoneMeta_ = true;
   }
}

//===========================================================================
FME_Status FMECityJSONReader::readGeometryDefinitions()
{
   // Reading the Geometry Templates and adding them to IFMELibrary as geometry instances
   try
   {
      json templates         = inputJSON_.at("geometry-templates").at("templates");
      json verticesTemplates = inputJSON_.at("geometry-templates").at("vertices-templates");
      VertexPool3D verticesTemplatesVec;
      FME_MsgNum badLuck;

      for (auto vtx : verticesTemplates)
      {
         verticesTemplatesVec.emplace_back(vtx[0], vtx[1], vtx[2]);
      }
      int nrTemplates = distance(begin(templates), end(templates));
      for (int i = 0; i < nrTemplates; i++)
      {
         IFMEGeometry* geom = parseCityObjectGeometry(templates[i], verticesTemplatesVec);
         FME_UInt32 geomRef(0);
         badLuck = gFMESession->getLibrary()->addGeometryDefinition(geomRef, geom);
         if (badLuck)
         {
            std::string msg =
               "Not able to add geometry template #" + std::to_string(i) + " to IFMELibrary";
            gLogFile->logMessageString(msg.c_str(), FME_ERROR);
            return FME_FAILURE;
         }
         else
         {
            // Add the geometry instance reference to the lookup table
            geomTemplateMap_.insert({i, geomRef});
         }
      }
   }
   catch (json::out_of_range& e)
   {
   }

   return FME_SUCCESS;
}

//===========================================================================
void FMECityJSONReader::scanLODs()
{
   // Need to go through the whole file to extract the LoD of each geometry
   for (json::iterator it = inputJSON_.at("CityObjects").begin();
        it != inputJSON_.at("CityObjects").end();
        it++)
   {
      for (const auto& geometry : it.value().at("geometry"))
      {
         // These are maybe too many checks on the presence of an attribute. Ideally, the file would be
         //      validated for the schema before it goes into FME, so we can omit these checks.
         // Check which LoD is present in the data
         std::string lod;
         bool key_missing(false);
         try
         {
            geometry.at("lod");
            lod = lodToString(geometry);
         }
         catch (json::out_of_range& e)
         {
            try
            {
               int tId = geometry.at("template");
               lod     = lodToString(inputJSON_.at("geometry-templates").at("templates")[tId]);
            }
            catch (json::out_of_range& e)
            {
               lod         = "";
               key_missing = true;
            }
         }

         if (not lod.empty())
         {
            if (std::find(lodInData_.begin(), lodInData_.end(), lod) == lodInData_.end())
            {
               lodInData_.push_back(lod);
            }
         }
         else if (not key_missing)
         {
            gLogFile->logMessageString(
               ("The 'lod' attribute is empty in the geometry of the CityObject: " + it.key()).c_str(),
               FME_ERROR);
         }
         else
         {
            gLogFile->logMessageString(
               ("Did not find the 'lod' attribute in the geometry of the CityObject: " + it.key()).c_str(),
               FME_ERROR);
         }
      }
   }

   if (lodInData_.size() > 1)
   {
      std::stringstream lodMsg;
      lodMsg << "There are multiple Levels of Detail present in the CityJSON data: ";
      for (auto& l : lodInData_)
         lodMsg << l << ", ";
      std::string lodMsgStr = lodMsg.str();
      lodMsgStr.erase(lodMsgStr.end() - 2);
      gLogFile->logMessageString(lodMsgStr.c_str(), FME_INFORM);

      if (lodParam_.empty())
      {
         // The default LoD to read is the LoD of the first Geometry of the
         // first CityObject.
         std::string defaultMsg = "No value is set for the 'CityJSON Level of "
                                  "Detail' parameter. Defaulting to: " +
                                  lodInData_[0];
         gLogFile->logMessageString(defaultMsg.c_str(), FME_WARN);
         lodParam_ = lodInData_[0];
      }
      else if (std::find(lodInData_.begin(), lodInData_.end(), lodParam_) == lodInData_.end())
      {
         std::string defaultMsg = "The provided 'CityJSON Level of Detail' parameter value " +
                                  lodParam_ +
                                  " is not present in the data. Defaulting to: " + lodInData_[0];
         gLogFile->logMessageString(defaultMsg.c_str(), FME_WARN);
         lodParam_ = lodInData_[0];
      }
   }
   else if (lodInData_.size() == 1)
   {
      // In case there is only one LoD in the data, we ignore the Parameter
      // even if it is set.
      lodParam_ = lodInData_[0];
      gLogFile->logMessageString(("Level of Detail in file: " + lodParam_).c_str(), FME_INFORM);
   }
}

//===========================================================================
void FMECityJSONReader::readVertexPool()
{
   // Transform object
   std::vector<double> scale{1.0, 1.0, 1.0};
   std::vector<double> translation{0.0, 0.0, 0.0};
   try
   {
      json transformObject = inputJSON_.at("transform");
      gLogFile->logMessageString("Reading compressed CityJSON file.", FME_INFORM);
      scale.clear();
      for (double const s : transformObject.at("scale"))
      {
         scale.push_back(s);
      }
      translation.clear();
      for (double const t : transformObject.at("translate"))
      {
         translation.push_back(t);
      }
   }
   catch (json::out_of_range& e)
   {
      gLogFile->logMessageString("Reading uncompressed CityJSON file.", FME_INFORM);
   }

   // Vertices
   for (auto vtx : inputJSON_.at("vertices"))
   {
      double x = vtx[0];
      double y = vtx[1];
      double z = vtx[2];
      x        = scale[0] * x + translation[0];
      y        = scale[1] * y + translation[1];
      z        = scale[2] * z + translation[2];
      vertices_.emplace_back(x, y, z);
   }
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

   for (auto&& sf : schemaFeatures_)
   {
      gFMESession->destroyFeature(sf.second);
   }
   schemaFeatures_.clear();

   // shut the file
   inputFile_.close();

   gFMESession->destroyString(textureCoordUName_);
   textureCoordUName_ = nullptr;
   gFMESession->destroyString(textureCoordVName_);
   textureCoordVName_ = nullptr;

   // Log that the reader is done
   gLogFile->logMessageString((kMsgClosingReader + dataset_).c_str());
   gLogFile->logMessageString(("Skipped reading " + std::to_string(skippedObjects_) +
                               " features due to 'CityJSON Level of Detail' parameter setting")
                                 .c_str());

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

   // Set the coordinate system
   feature.setCoordSys(coordSys_.c_str());

   if (nextObject_ == inputJSON_.at("CityObjects").end() and metaObject_.empty())
   {
      endOfFile = FME_TRUE;
      return FME_SUCCESS;
   }

   if (not metaObject_.empty())
   {
      // reading the metadata into a feature
      feature.setFeatureType("Metadata");
      json::iterator metaIt          = metaObject_.begin();
      const json::iterator metaItEnd = metaObject_.end();
      parseAttributes(feature, metaIt, metaItEnd);

      metaObject_.clear();
      endOfFile = FME_FALSE;
      return FME_SUCCESS;
   }
   else
   {
      // Skipping CityObjects depending on their LoD
      {
         std::vector<bool> ignore_lod;
         std::string geometryLodValue;

         // CityObjects with empty geometries are always read
         if (nextObject_.value()["geometry"].empty()) ignore_lod.push_back(false);

         for (auto& geometry : nextObject_.value()["geometry"])
         {
            // Check if the whole feature should be ignored
            if (geometry.is_object())
            {
               geometryLodValue = lodToString(geometry);
               // Only ignore the feature if it is certain that the
               // required LoD (parmeter) != the LoD in the data.
               // All other cases (null, missing etc.) should be read.
               ignore_lod.push_back(not geometryLodValue.empty() and geometryLodValue != lodParam_);
            }
            else
               ignore_lod.push_back(false);
         }

         if (std::all_of(ignore_lod.begin(), ignore_lod.end(), [](bool i) { return i; }))
         {
            // We skip this object, because none of its geometries have the required LoD
            ++nextObject_;
            skippedObjects_++;
            endOfFile = FME_FALSE;
            return read(feature, endOfFile);
         }
      }

      // reading CityObjects into features
      std::string objectId = nextObject_.key();

      // Set the feature type
      std::string featureType = nextObject_.value().at("type").get<std::string>();
      feature.setFeatureType(featureType.c_str());

      // Set feature attributes
      feature.setAttribute("fid", objectId.c_str());

      json attributes;
      try
      {
         attributes = nextObject_.value().at("attributes");
      }
      catch (json::out_of_range& e)
      {
      }
      if (not attributes.empty())
      {
         json::iterator attrIt          = attributes.begin();
         const json::iterator attrItEnd = attributes.end();
         parseAttributes(feature, attrIt, attrItEnd);
      }
      // Set child and parent CityObjects as attributes. In FME we don't have/set an explicit object
      // hierarchy, but each feature is on the same level. Therefore we store the child-parent
      // relationships in attributes, for those who are interested in the hierarchies. Just like in
      // cityjson. I'm not adding the children and parents attributes to the schema, because its
      // better if they are hidden from the table view, since there can be many-many children for
      // each feature.
      if (not nextObject_.value()["children"].is_null() && not nextObject_.value()["children"].empty())
      {
         IFMEStringArray* children = gFMESession->createStringArray();
         for (std::string child : nextObject_.value()["children"])
         {
            children->append(child.c_str());
         }
         feature.setListAttributeNonSequenced("cityjson_children", *children);
         gFMESession->destroyStringArray(children);
      }

      if (not nextObject_.value()["parents"].is_null() && not nextObject_.value()["parents"].empty())
      {
         IFMEStringArray* parents = gFMESession->createStringArray();
         for (std::string parent : nextObject_.value()["parents"])
         {
            parents->append(parent.c_str());
         }
         feature.setListAttributeNonSequenced("cityjson_parents", *parents);
         gFMESession->destroyStringArray(parents);
      }

      // Set the geometry
      for (auto& geometry : nextObject_.value()["geometry"])
      {
         // Set the geometry for the feature
         IFMEGeometry* geom = parseCityObjectGeometry(geometry, vertices_);
         if (geom != nullptr)
         {
            feature.setGeometry(geom);
         }
      }

      ++nextObject_;

      endOfFile = FME_FALSE;
      return FME_SUCCESS;
   }
}

void FMECityJSONReader::parseAttributes(IFMEFeature& feature,
                                        json::iterator& it,
                                        const json::iterator& _end)
{
   while (it != _end)
   {
      const std::string& attributeName = it.key();
      if (it.value().is_string())
      {
         std::string attributeValue = it.value().get<std::string>();
         feature.setAttribute(attributeName.c_str(), attributeValue.c_str());
      }
      else if (it.value().is_number_float())
      {
         FME_Real64 attributeValue = it.value();
         feature.setAttribute(attributeName.c_str(), attributeValue);
      }
      else if (it.value().is_number_integer())
      {
         FME_Int32 attributeValue = it.value();
         feature.setAttribute(attributeName.c_str(), attributeValue);
      }
      else if (it.value().is_boolean())
      {
         if (it.value())
            feature.setBooleanAttribute(attributeName.c_str(), FME_TRUE);
         else
            feature.setBooleanAttribute(attributeName.c_str(), FME_FALSE);
      }
      else
      {
         // value is object or array
         std::string attributeValue = it.value().dump();
         feature.setAttribute(attributeName.c_str(), attributeValue.c_str());
      }

      it++;
   }
}

IFMEGeometry* FMECityJSONReader::parseCityObjectGeometry(json::value_type& currentGeometry,
                                                         VertexPool3D& vertices)
{
   if (currentGeometry.is_object())
   {
      std::string geometryType, geometryLodValue;
      std::string geometryLodName = "cityjson_lod"; // geometry Trait name
      json::value_type boundaries = currentGeometry.at("boundaries");
      json::value_type semantics  = currentGeometry["semantics"];

      // Does this have any texture data attached?
      json::value_type textures = currentGeometry["texture"];
      std::vector<std::string> textureThemes;
      if (not textures.is_null())
      {
         for (json::iterator it = textures.begin(); it != textures.end(); ++it)
         {
            textureThemes.push_back(it.key());
         }
      }

      // Does this have any material data attached?
      json::value_type materials = currentGeometry["material"];
      std::vector<std::string> materialNames;
      if (not materials.is_null())
      {
         for (json::iterator it = materials.begin(); it != materials.end(); ++it)
         {
            materialNames.push_back(it.key());
         }
      }

      // geometry type and level of detail
      geometryType = currentGeometry.at("type").get<std::string>();
      if (geometryType != "GeometryInstance")
      {
         geometryLodValue = lodToString(currentGeometry);
      }
      else
      {
         int tId          = currentGeometry.at("template");
         geometryLodValue = lodToString(inputJSON_.at("geometry-templates").at("templates")[tId]);
      }

      if (not geometryType.empty())
      {
         if (geometryLodValue == lodParam_)
         {
            if (geometryType == "MultiPoint")
            {
               IFMEMultiPoint* mpoint = fmeGeometryTools_->createMultiPoint();
               parseMultiPoint(mpoint, boundaries, vertices);
               return mpoint;
            }
            else if (geometryType == "MultiLineString")
            {
               IFMEMultiCurve* mlinestring = fmeGeometryTools_->createMultiCurve();
               parseMultiLineString(mlinestring, boundaries, vertices);
               return mlinestring;
            }
            else if (geometryType == "MultiSurface")
            {
               RefVec2 textureRefsPerBoundary;
               unrollReferences2(textures, boundaries, textureRefsPerBoundary);
               RefVec2 materialRefsPerBoundary;
               unrollReferences2(materials, boundaries, materialRefsPerBoundary);

               // Does this have any semantic data?
               json::value_type* semanticSrfVec = fetchSemanticsValues(semantics);

               IFMEMultiSurface* msurface = fmeGeometryTools_->createMultiSurface();
               parseMultiCompositeSurface(msurface, boundaries, semantics, semanticSrfVec, textureThemes, &textureRefsPerBoundary, materialNames, &materialRefsPerBoundary, vertices);
               // Set the Level of Detail Trait on the geometry
               setTraitString(*msurface, geometryLodName, geometryLodValue);
               // Append the geometry to the FME feature
               return msurface;
            }
            else if (geometryType == "CompositeSurface")
            {
               
               // Does this have any texture data attached?
               RefVec2 textureRefsPerBoundary;
               unrollReferences2(textures, boundaries, textureRefsPerBoundary);
               RefVec2 materialRefsPerBoundary;
               unrollReferences2(materials, boundaries, materialRefsPerBoundary);

               // Does this have any semantic data?
               json::value_type* semanticSrfVec = fetchSemanticsValues(semantics);

               IFMECompositeSurface* csurface = fmeGeometryTools_->createCompositeSurface();
               parseMultiCompositeSurface(csurface, boundaries, semantics, semanticSrfVec, textureThemes, &textureRefsPerBoundary, materialNames, &materialRefsPerBoundary, vertices);
               setTraitString(*csurface, geometryLodName, geometryLodValue);
               return csurface;
            }
            else if (geometryType == "Solid")
            {
               RefVec3 textureRefsPerBoundaryPerShell;
               unrollReferences3(textures, boundaries, textureRefsPerBoundaryPerShell);
               RefVec3 materialRefsPerBoundaryPerShell;
               unrollReferences3(materials, boundaries, materialRefsPerBoundaryPerShell);

               // Does this have any semantic data?
               json::value_type* semanticSrfVec2 = fetchSemanticsValues(semantics);

               IFMEBRepSolid* BSolid =
                  parseSolid(boundaries, semantics, semanticSrfVec2, textureThemes, &textureRefsPerBoundaryPerShell, materialNames, &materialRefsPerBoundaryPerShell, vertices);
               setTraitString(*BSolid, geometryLodName, geometryLodValue);
               return BSolid;
            }
            else if (geometryType == "MultiSolid")
            {
               RefVec4 textureRefsPerBoundaryPerShellPerSolid;
               unrollReferences4(textures, boundaries, textureRefsPerBoundaryPerShellPerSolid);
               RefVec4 materialRefsPerBoundaryPerShellPerSolid;
               unrollReferences4(materials, boundaries, materialRefsPerBoundaryPerShellPerSolid);

               IFMEMultiSolid* msolid = fmeGeometryTools_->createMultiSolid();
               parseMultiCompositeSolid(msolid, boundaries, semantics, textureThemes, textureRefsPerBoundaryPerShellPerSolid, materialNames, materialRefsPerBoundaryPerShellPerSolid, vertices);
               setTraitString(*msolid, geometryLodName, geometryLodValue);
               return msolid;
            }
            else if (geometryType == "CompositeSolid")
            {
               RefVec4 textureRefsPerBoundaryPerShellPerSolid;
               unrollReferences4(textures, boundaries, textureRefsPerBoundaryPerShellPerSolid);
               RefVec4 materialRefsPerBoundaryPerShellPerSolid;
               unrollReferences4(materials, boundaries, materialRefsPerBoundaryPerShellPerSolid);

               IFMECompositeSolid* csolid = fmeGeometryTools_->createCompositeSolid();
               parseMultiCompositeSolid(csolid, boundaries, semantics, textureThemes, textureRefsPerBoundaryPerShellPerSolid, materialNames, materialRefsPerBoundaryPerShellPerSolid, vertices);
               setTraitString(*csolid, geometryLodName, geometryLodValue);
               return csolid;
            }
            else if (geometryType == "GeometryInstance")
            {
               IFMEAggregate* ginst = fmeGeometryTools_->createAggregate();
               int templ            = currentGeometry.at("template");
               FME_UInt32 geomRef   = geomTemplateMap_[templ];
               ginst->setGeometryDefinitionReference(geomRef);
               int vtx      = currentGeometry.at("boundaries")[0];
               FME_Real64 x = std::get<0>(vertices_[vtx]);
               FME_Real64 y = std::get<1>(vertices_[vtx]);
               FME_Real64 z = std::get<2>(vertices_[vtx]);
               ginst->setGeometryInstanceLocalOrigin(x, y, z);
               json::array_t tm   = currentGeometry.at("transformationMatrix");
               FME_Real64 m[3][4] = {{tm[0], tm[1], tm[2], tm[3]},
                                     {tm[4], tm[5], tm[6], tm[7]},
                                     {tm[8], tm[9], tm[10], tm[11]}};
               ginst->setGeometryInstanceMatrix(m);
               return ginst;
            }
            else
            {
               gLogFile->logMessageString(("Unknown geometry type " + geometryType).c_str(), FME_WARN);
               return nullptr;
            }
         }
      }
      else
      {
         gLogFile->logMessageString("CityObject Geometry type is not set", FME_WARN);
         return nullptr;
      }
   }
   return nullptr;
}


json::value_type* FMECityJSONReader::fetchSemanticsValues(json::value_type& semantics)
{
   if ((not semantics.is_null()) && (not semantics["values"].is_null()))
   {
      return &semantics["values"];
   }
   return nullptr;
}

template <typename MCSolid>
void FMECityJSONReader::parseMultiCompositeSolid(MCSolid multiCompositeSolid,
                                                 json::value_type& boundaries,
                                                 json::value_type& semantics,
                                                 std::vector<std::string>& textureThemes,
                                                 RefVec4& textureRefsPerBoundaryPerShellPerSolid,
                                                 std::vector<std::string>& materialNames,
                                                 RefVec4& materialRefsPerBoundaryPerShellPerSolid,
                                                 VertexPool3D& vertices)
{
   int nrSolids = distance(begin(boundaries), end(boundaries));
   for (int i = 0; i < nrSolids; i++)
   {
      json::value_type& shellBoundaries = boundaries[i];

      // Does this have any semantic data?
      json::value_type* semanticSrfVec2 = fetchSemanticsValues(semantics);

      // Does this have any texture data attached?
      RefVec3* textureRefsPerBoundaryPerShell = textureRefsPerBoundaryPerShellPerSolid.empty() ?
                                                   nullptr :
                                                   &textureRefsPerBoundaryPerShellPerSolid[i];

      // Does this have any texture data attached?
      RefVec3* materialRefsPerBoundaryPerShell = materialRefsPerBoundaryPerShellPerSolid.empty() ?
                                                    nullptr :
                                                    &materialRefsPerBoundaryPerShellPerSolid[i];

      IFMEBRepSolid* BSolid = parseSolid(shellBoundaries,
                                         semantics,
                                         semanticSrfVec2,
                                         textureThemes,
                                         textureRefsPerBoundaryPerShell,
                                         materialNames,
                                         materialRefsPerBoundaryPerShell,
                                         vertices);

      multiCompositeSolid->appendPart(BSolid);
   }
}

IFMEBRepSolid* FMECityJSONReader::parseSolid(json::value_type& boundaries,
                                             json::value_type& semantics,
                                             json::value_type* semanticSrfVec2,
                                             std::vector<std::string>& textureThemes,
                                             RefVec3* textureRefsPerBoundaryPerShell,
                                             std::vector<std::string>& materialNames,
                                             RefVec3* materialRefsPerBoundaryPerShell,
                                             VertexPool3D& vertices)
{
   IFMEBRepSolid* BSolid(nullptr);
   IFMECompositeSurface* outerSurface = fmeGeometryTools_->createCompositeSurface();
   IFMECompositeSurface* innerSurface(nullptr);

   int nrShells = distance(begin(boundaries), end(boundaries));
   for (int i = 0; i < nrShells; i++)
   {
      json::value_type& surfaceBoundaries = boundaries[i];

      // Does this have any texture data attached?
      RefVec2* textureRefsPerBoundary =
         (!textureRefsPerBoundaryPerShell || textureRefsPerBoundaryPerShell->empty()) ?
            nullptr :
            &(*textureRefsPerBoundaryPerShell)[i];

      // Does this have any material data attached?
      RefVec2* materialRefsPerBoundary =
         (!materialRefsPerBoundaryPerShell || materialRefsPerBoundaryPerShell->empty()) ?
            nullptr :
            &(*materialRefsPerBoundaryPerShell)[i];

      // Inner shells/surfaces do not have semantics
      json::value_type* semanticSrfVec(nullptr);
      if (i == 0)
      {
         // Does this have any semantic data?
         if (semanticSrfVec2 && (not(*semanticSrfVec2)[i].is_null()))
         {
            semanticSrfVec = &(*semanticSrfVec2)[i];
         }
      }
      else
      {
         // Set up the inner surface we're building
         innerSurface = fmeGeometryTools_->createCompositeSurface();
      }

      // First let's build the outer surfaces, then the inner ones.
      IFMECompositeSurface* surfaceToBuild = (i == 0) ? outerSurface : innerSurface;

      // put together the composite surface
      parseMultiCompositeSurface(surfaceToBuild,
                                 surfaceBoundaries,
                                 semantics,
                                 semanticSrfVec,
                                 textureThemes,
                                 textureRefsPerBoundary,
                                 materialNames,
                                 materialRefsPerBoundary,
                                 vertices);

      if (i == 0)
      {
         BSolid       = fmeGeometryTools_->createBRepSolidBySurface(outerSurface);
         outerSurface = nullptr; // we gave up ownership
      }
      else
      {
         BSolid->addInnerSurface(innerSurface);
      }
   }

   // Let's see if we had some empty set of shells and return an empty brepsolid.
   if (!BSolid)
   {
      BSolid = fmeGeometryTools_->createBRepSolidBySurface(outerSurface);
      outerSurface = nullptr; // we gave up ownership
   }

   return BSolid;
}

template <typename MCSurface>
void FMECityJSONReader::parseMultiCompositeSurface(MCSurface multiCompositeSurface,
                                                   json::value_type& boundaries,
                                                   json::value_type& semantics,
                                                   json::value_type* semanticSrfVec,
                                                   std::vector<std::string>& textureThemes,
                                                   RefVec2* textureRefsPerBoundary,
                                                   std::vector<std::string>& materialNames,
                                                   RefVec2* materialRefsPerBoundary,
                                                   VertexPool3D& vertices)
{
   int nrSurfaces = distance(begin(boundaries), end(boundaries));
   for (int i = 0; i < nrSurfaces; i++)
   {
      // Does this have any texture data attached?
      RefVec* textureRefs = (!textureRefsPerBoundary || textureRefsPerBoundary->empty()) ?
                               nullptr :
                               &(*textureRefsPerBoundary)[i];

      // Does this have any material data attached?
      RefVec* materialRefs = (!materialRefsPerBoundary || materialRefsPerBoundary->empty()) ?
                                nullptr :
                                &(*materialRefsPerBoundary)[i];

      // Does this have any semantic data?
      json::value_type* semanticSrf = (!semanticSrfVec || (*semanticSrfVec)[i].is_null()) ?
                                         nullptr :
                                         &semantics["surfaces"][int((*semanticSrfVec)[i])];

      IFMEFace* face = createOneSurface(textureThemes,
                                        textureRefs,
                                        materialNames,
                                        materialRefs,
                                        boundaries[i],
                                        vertices,
                                        semanticSrf);

      multiCompositeSurface->appendPart(face);
   }
}

IFMEFace* FMECityJSONReader::createOneSurface(std::vector<std::string>& textureThemes,
                                              RefVec* textureRefs,
                                              std::vector<std::string>& materialNames,
                                              RefVec* materialRefs,
                                              json::value_type& boundaries,
                                              VertexPool3D& vertices,
                                              json::value_type* semanticSrf)
{
   IFMEFace* face = parseSurfaceBoundaries(boundaries, vertices, textureThemes, textureRefs);

   // Add traits onto the face.
   parseSemantics(*face, semanticSrf);

   // Add materials to the face
   parseMaterials(*face, materialNames, materialRefs);

   return face;
}

IFMEFace* FMECityJSONReader::parseSurfaceBoundaries(json::value_type& surface,
                                                    VertexPool3D& vertices,
                                                    std::vector<std::string>& textureThemes,
                                                    RefVec* textureRefs)
{
   json::value_type textureRefToUse(json{nullptr});
   std::string themeToUse;
   if (textureRefs)
   {
      // TODO: I guess here we could use the textureThemes to decide how to attach them, or which to
      // use.  For now I think FME can only store one.
      int themeNumToUse(0); // <-- arbitrary choice

      // Make sure we don't extend beyond the size of what is passed in.
      if (textureThemes.size() > themeNumToUse)
      {
         themeToUse = textureThemes[themeNumToUse];
      }
      if (textureRefs->size() > themeNumToUse)
      {
         textureRefToUse = (*textureRefs)[themeNumToUse];
      }
   }

   // TODO: "themeToUse" is not put on the feature or geometry anywhere.
   //       Is this really not needed?  Maybe it is only used in some
   //       future where the user has input on which to use.

   std::vector<IFMELine*> rings;
   std::vector<FME_UInt32> appearanceRefs;
   parseRings(rings, appearanceRefs, surface, vertices, textureRefToUse);
   IFMELine* outerRing = rings[0];

   IFMEArea* area = fmeGeometryTools_->createSimpleAreaByCurve(outerRing);
   IFMEFace* face = fmeGeometryTools_->createFaceByArea(area, FME_CLOSE_3D_EXTEND_MODE);
   if (rings.size() > 1)
   {
      for (auto it = rings.cbegin() + 1; it != rings.cend(); ++it)
      {
         face->addInnerBoundaryCurve(*it, FME_CLOSE_3D_EXTEND_MODE);
      }
   }

   // Set the texture/appearance
   if (not appearanceRefs.empty())
   {
      // TODO: I'm not sure if this texture should be on both sides.  I'll just do the "front" for now.
      // TODO: I'm not sure if rings can refer to different appearances, but if so,
      //       which would we use?  I am assuming they are all the same and pick the
      //       appearance from the outer ring "0".
      face->setAppearanceReference(appearanceRefs[0], FME_TRUE);
      face->deleteSide(FME_FALSE); // make the back face not exist, "transparent".
   }

   return face;
}

void FMECityJSONReader::parseSemantics(IFMEFace& face, json::value_type* semanticSurface)
{
   // Setting semantics
   if (semanticSurface && (not semanticSurface->is_null()))
   {
      IFMEString* geometryName = gFMESession->createString();
      std::string semType      = semanticSurface->at("type").get<std::string>();
      geometryName->set(semType.c_str(), semType.length());
      face.setName(*geometryName, nullptr);
      gFMESession->destroyString(geometryName);

      for (json::iterator it = semanticSurface->begin(); it != semanticSurface->end(); it++)
      {
         if (it.key() == "children" || it.key() == "parent")
         {
            // We ignore Semantic Surface hierarchies and the 'children' and 'parent' tag is
            // discarded, because we don't have a way to handle Semantic Surface hierarchies (eg.
            // Door is a child of a WallSurface).
            gLogFile->logMessageString("Semantic Surface hierarchy (children, parent) is discarded",
                                       FME_WARN);
         }
         else if (it.key() != "type")
         {
            if (it.value().is_string())
            {
               setTraitString(face, it.key(), it.value());
            }
            else if (it.value().is_number_float())
            {
               IFMEString* geometryTrait = gFMESession->createString();
               geometryTrait->set(it.key().c_str(), it.key().length());
               face.setTraitReal64(*geometryTrait, it.value());
               gFMESession->destroyString(geometryTrait);
            }
            else if (it.value().is_number_integer())
            {
               IFMEString* geometryTrait = gFMESession->createString();
               geometryTrait->set(it.key().c_str(), it.key().length());
               face.setTraitInt64(*geometryTrait, it.value());
               gFMESession->destroyString(geometryTrait);
            }
            else if (it.value().is_boolean())
            {
               IFMEString* geometryTrait = gFMESession->createString();
               geometryTrait->set(it.key().c_str(), it.key().length());
               if (it.value())
                  face.setTraitBoolean(*geometryTrait, FME_TRUE);
               else
                  face.setTraitBoolean(*geometryTrait, FME_FALSE);
               gFMESession->destroyString(geometryTrait);
            }
            else
            {
               std::string val = it.value().type_name();
               gLogFile->logMessageString(
                  ("Semantic Surface attribute type '" + val + "' is not allowed.").c_str(), FME_WARN);
            }
         }
      }
   }
}

void FMECityJSONReader::parseMaterials(IFMEFace& face,
                                       std::vector<std::string>& materialNames,
                                       RefVec* materialRefs)
{
   json::value_type materialRefToUse(json{nullptr});
   std::string nameToUse;
   if (materialRefs)
   {
      // TODO: I guess here we could use the materialNames to decide how to attach them, or which to
      // use.  For now I think FME can only store one.
      int nameNumToUse(0); // <-- arbitrary choice

      // Make sure we don't extend beyond the size of what is passed in.
      if (materialNames.size() > nameNumToUse)
      {
         nameToUse = materialNames[nameNumToUse];
      }
      if (materialRefs->size() > nameNumToUse)
      {
         materialRefToUse = (*materialRefs)[nameNumToUse];
      }

      // TODO: "nameToUse" is not put on the feature or geometry anywhere.
      //       Is this really not needed?  Maybe it is only used in some
      //       future where the user has input on which to use.

      if (not materialRefToUse.is_null())
      {
         FME_UInt32 fmeMatRef = materialsMap_[materialRefToUse];

         // TODO: I'm not sure if this material should be on both sides.  I'll just do the "front" for now.
         face.setAppearanceReference(fmeMatRef, FME_TRUE);
         face.deleteSide(FME_FALSE); // make the back face not exist, "transparent".
      }
   }
}

void FMECityJSONReader::parseMultiLineString(IFMEMultiCurve* mlinestring,
                                             json::value_type& boundaries,
                                             VertexPool3D& vertices)
{
   for (auto& linestring : boundaries)
   {
      IFMELine* line = fmeGeometryTools_->createLine();
      std::optional<FME_UInt32> unusedRef;
      parseLineString(line, unusedRef, linestring, vertices, json{nullptr});
      mlinestring->appendPart(line);
   }
}

void FMECityJSONReader::parseRings(std::vector<IFMELine*>& rings,
                                   std::vector<FME_UInt32>& appearanceRefs,
                                   json::value_type& boundary,
                                   VertexPool3D& vertices,
                                   json::value_type& textureRefs)
{
   int nrRings = distance(begin(boundary), end(boundary));
   for (int i = 0; i < nrRings; i++)
   {
      IFMELine* line = fmeGeometryTools_->createLine();
      std::optional<FME_UInt32> appearanceRef;
      parseLineString(line, appearanceRef, boundary[i], vertices, textureRefs[i]);
      rings.push_back(line);
      if (appearanceRef)
      {
         appearanceRefs.push_back(*appearanceRef);
      }
   }
}

void FMECityJSONReader::parseLineString(IFMELine* line,
                                        std::optional<FME_UInt32>& appearanceRef,
                                        json::value_type& boundary,
                                        VertexPool3D& vertices,
                                        json::value_type& textureRefs)
{
   // the textureRefs include one reference to the texture plus all the vertexcoord references
   // so we should make sure it all matches up.
   bool useTexCoords = ((boundary.size() + 1) == textureRefs.size());
   int vertexCoordIndex(0);
   for (json::iterator it = boundary.begin(); it != boundary.end(); it++)
   {
      for (int vertex : it.value())
      {
         if (useTexCoords && (vertexCoordIndex == 0))
         {
            appearanceRef = texturesMap_[textureRefs[0]]; // texture reference is the first one.
         }
         vertexCoordIndex++;
         IFMEPoint* point = fmeGeometryTools_->createPointXYZ(std::get<0>(vertices[vertex]),
                                                              std::get<1>(vertices[vertex]),
                                                              std::get<2>(vertices[vertex]));

         if (useTexCoords)
         {
            // Some datasets do not have the texture coordinates they claim to need.
            int uvRef = textureRefs[vertexCoordIndex];
            if (uvRef < textureVertices_.size())
            {
               point->setNamedMeasure(*textureCoordUName_, std::get<0>(textureVertices_[uvRef]));
               point->setNamedMeasure(*textureCoordVName_, std::get<1>(textureVertices_[uvRef]));
            }
            else
            {
               // TODO: log a warning here that some texture coordinates are missing from the file.
            }
         }

         line->appendPoint(point);
         point = nullptr; // We no longer own this.
      }
   }
}

void FMECityJSONReader::parseMultiPoint(IFMEMultiPoint* mpoint,
                                        json::value_type& boundary,
                                        VertexPool3D& vertices)
{
   for (json::iterator it = boundary.begin(); it != boundary.end(); it++)
   {
      for (int vertex : it.value())
      {
         IFMEPoint* point = fmeGeometryTools_->createPointXYZ(std::get<0>(vertices[vertex]),
                                                              std::get<1>(vertices[vertex]),
                                                              std::get<2>(vertices[vertex]));
         mpoint->appendPart(point);
      }
   }
}

void FMECityJSONReader::setTraitString(IFMEGeometry& geometry,
                                       const std::string& traitName,
                                       const std::string& traitValue)
{
   IFMEString* geometryTrait = gFMESession->createString();
   geometryTrait->set(traitName.c_str(), traitName.length());

   IFMEString* value = gFMESession->createString();
   value->set(traitValue.c_str(), traitValue.length());

   geometry.setTraitString(*geometryTrait, *value);

   gFMESession->destroyString(geometryTrait);
   gFMESession->destroyString(value);
}

std::string FMECityJSONReader::lodToString(json currentGeometry)
{
   json::value_type lod;
   try
   {
      lod = currentGeometry.at("lod");
   }
   catch (json::out_of_range& e)
   {
      return "";
   }
   if (lod.is_number_integer())
   {
      return std::to_string(int(lod));
   }
   else if (lod.is_number_float())
   {
      // We want the LoD as string, even though CityJSON specs currently
      // prescribe a number
      std::stringstream stream;
      stream << std::fixed << std::setprecision(1) << float(lod);
      return stream.str();
   }
   else if (lod.is_null())
   {
      return "";
   }
   else if (lod.is_string())
   {
      std::string lod_ = lod.get<std::string>();
      transform(lod_.begin(), lod_.end(), lod_.begin(), ::tolower);
      if (lod_ == "null")
      {
         return "";
      }
      else
      {
         return lod_;
      }
   }
   else
   {
      gLogFile->logMessageString("Unknown type for 'lod'", FME_ERROR);
      return "";
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

   // There is one special case - when the reader is started to help out the writer and
   // provide it with a set of default schemas.  In this case we don't want to scan for
   // them from an input dataset, but from the official specs instead.
   if (writerHelperMode_ and not schemaScanDoneMeta_)
   {
      schemaScanDoneMeta_ = true; // don't need to do this
      schemaScanDone_     = true; // don't need to do this

      FME_Status badLuck = fetchSchemaFeaturesForWriter();
      if (FME_SUCCESS != badLuck) return badLuck;
   }

   // Create a feature for the metadata
   if (not schemaScanDoneMeta_)
   {
      try
      {
         // try to catch early
         json metadata = inputJSON_.at("metadata");

         std::string featureType = "Metadata";
         auto schemaFeature      = schemaFeatures_.find(featureType);
         IFMEFeature* sf(nullptr);
         if (schemaFeature == schemaFeatures_.end())
         {
            sf = gFMESession->createFeature();
            sf->setFeatureType(featureType.c_str());
            schemaFeatures_[featureType] = sf; // gives up ownership
         }
         else
         {
            sf = schemaFeature->second;
         }

         {
            std::string attributeName = "fme_geometry{0}";
            sf->setAttribute(attributeName.c_str(), "fme_no_geom");
         }

         // Go trough the metadata and add as attributes
         for (json::iterator it = metadata.begin(); it != metadata.end(); it++)
         {
            const std::string& attributeName = it.key();
            std::string attributeType        = "string";
            sf->setSequencedAttribute(attributeName.c_str(), attributeType.c_str());
         }
      }
      catch (json::out_of_range& e)
      {
      }

      schemaScanDoneMeta_ = true;
   }

   if (not schemaScanDone_ and schemaScanDoneMeta_)
   {
      // iterate through every object in the file.
      for (auto& cityObject : inputJSON_.at("CityObjects"))
      {
         // I'm not sure exactly what types of features this reader will
         // produce, so this is just a wild guess as an example.

         // Let's find out what we will be using as the "feature_type", and
         // group the schema features by that.  I'll pick the field "type".
         std::string featureType = cityObject.at("type").get<std::string>();

         // Let's see if we already have seen a feature of this 'type'.
         // If not, create a new schema feature.  If we have, just add to it I guess.
         auto schemaFeature = schemaFeatures_.find(featureType);
         IFMEFeature* sf(nullptr);
         if (schemaFeature == schemaFeatures_.end())
         {
            sf = gFMESession->createFeature();
            sf->setFeatureType(featureType.c_str());
            schemaFeatures_[featureType] = sf; // gives up ownership
         }
         else
         {
            sf = schemaFeature->second;
         }

         // Set the feature ID attribute
         // Schema feature attributes need to be set with setSequencedAttribute()
         // to preserve the order of attributes.
         {
            const std::string attributeName = "fid";
            std::string attributeType       = "string";
            sf->setSequencedAttribute(attributeName.c_str(), attributeType.c_str());
         }

         // iterate through every attribute on this object.
         if (not cityObject["attributes"].is_null())
         {
            for (json::iterator it = cityObject.at("attributes").begin();
                 it != cityObject.at("attributes").end();
                 ++it)
            {
               const std::string& attributeName = it.key();
               // The value here must be something found in the left hand
               // column of the ATTR_TYPE_MAP line in the metafile 'fmecityjson.fmf'
               // could be string, real64, uint32, logical, char, date, time, etc.

               if (it.value().is_string())
               {
                  std::string attributeType = "string";
                  // Schema feature attributes need to be set with setSequencedAttribute()
                  // to preserve the order of attributes.
                  sf->setSequencedAttribute(attributeName.c_str(), attributeType.c_str());
               }
               else if (it.value().is_number_float())
               {
                  std::string attributeType = "real64";
                  sf->setSequencedAttribute(attributeName.c_str(), attributeType.c_str());
               }
               else if (it.value().is_number_integer())
               {
                  std::string attributeType = "int32";
                  sf->setSequencedAttribute(attributeName.c_str(), attributeType.c_str());
               }
               else if (it.value().is_boolean())
               {
                  std::string attributeType = "logical";
                  sf->setSequencedAttribute(attributeName.c_str(), attributeType.c_str());
               }
               else
               {
                  std::string msg = "Attribute value type '";
                  msg.append(it.value().type_name());
                  msg.append("' is not allowed, in '");
                  msg.append(attributeName);
                  msg.append("'.");
                  gLogFile->logMessageString(msg.c_str(), FME_WARN);
               }
            }
         }

         // Here we add to the schema feature all the possible geometries of the
         // feature type.  Arc and ellipse geometries require that you also set
         // fme_geomattr on them.  Setting the fme_geomattr is required for
         // backwards compatible with writers that only support classic geometry.

         // The value here must be something found in the left hand
         // column of the GEOM_MAP line in the metafile 'fmecityjson.fmf'
         int nrGeometries = distance(begin(cityObject.at("geometry")), end(cityObject.at("geometry")));
         if (nrGeometries == 0)
         {
            gLogFile->logMessageString("Empty geometry for CityObject", FME_WARN);
            std::string attributeName = "fme_geometry{0}";
            sf->setAttribute(attributeName.c_str(), "fme_no_geom");
         }
         else
         {
            for (int i = 0; i < nrGeometries; i++)
            {
               std::string attributeName = "fme_geometry{" + std::to_string(i) + "}";
               std::string type = cityObject.at("geometry")[i].at("type").get<std::string>();
               if (type == "GeometryInstance")
               {
                  int tId = cityObject.at("geometry")[i].at("template");
                  type = inputJSON_["geometry-templates"]["templates"][tId]["type"].get<std::string>();
               }

               // Set the geometry types from the data
               if (type == "MultiPoint")
               {
                  sf->setAttribute(attributeName.c_str(), "fme_point");
               }
               else if (type == "MultiLineString")
               {
                  sf->setAttribute(attributeName.c_str(), "fme_line");
               }
               else if ((type == "MultiSurface") || (type == "CompositeSurface"))
               {
                  sf->setAttribute(attributeName.c_str(), "fme_surface");
               }
               else if ((type == "Solid") || (type == "MultiSolid") || (type == "CompositeSolid"))
               {
                  sf->setAttribute(attributeName.c_str(), "fme_solid");
               }
               else
               {
                  gLogFile->logMessageString(("No match for geometry type " + type).c_str(), FME_WARN);
                  sf->setAttribute(attributeName.c_str(), "fme_no_geom");
               }
            }
         }
      }

      schemaScanDone_ = true;
   }

   if (schemaFeatures_.empty())
   {
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
FME_Status FMECityJSONReader::fetchSchemaFeaturesForWriter()
{
   // The reader is being used as a "helper" to the writer, to gather
   // schema feature definitions, this will look in the official CityJSON specs
   // and pull out the correct schema information.

   // We know which setting they have for requesting the starting schemas.
   if (writerStartingSchema_ == "none")
   {
      // OK, they just want to start off blank.  I guess they will build their own in Workbench.
      return FME_SUCCESS;
   }

   return fetchSchemaFeatures(*gLogFile, writerStartingSchema_, schemaFeatures_);
}

//===========================================================================
// readParameterDialog
void FMECityJSONReader::readParametersDialog()
{
   IFMEString* paramValue = gFMESession->createString();

   if (gMappingFile->fetchWithPrefix(
          readerKeyword_.c_str(), readerTypeName_.c_str(), kSrcLodParamTag, *paramValue))
   {
      // A parameter value has been found, so set the values.
      lodParam_ = paramValue->data();

      // Let's log to the user that a parameter value has been specified.
      std::string paramMsg = (kLodParamTag + lodParam_).c_str();
      gLogFile->logMessageString(paramMsg.c_str(), FME_INFORM);
   }
   else
   {
      // Log that no parameter value was entered.
      gLogFile->logMessageString(kMsgNoLodParam, FME_INFORM);
   }
   gFMESession->destroyString(paramValue);
}

//=========================================================================
bool FMECityJSONReader::fetchWriterDirectives(const IFMEStringArray& parameters)
{
   // first array component is the dataset, subsequent are keyword name/value pairs
   const FME_UInt32 entries = parameters.entries();

   // we want at least one name value pair
   if (entries < 3) return false;

   bool foundSpecialWriterFlag(false);
   for (FME_UInt32 i = 2; i < entries; i += 2)
   {
      const IFMEString* keyword = parameters.elementAt(i - 1);

      if (0 == strcmp(keyword->data(), kCityJSON_FME_DIRECTION))
      {
         // If the reader is being opened in order to create schema features for the writer
         // schema, then the core will have set the FME_DIRECTION parameter to DESTINATION.
         const IFMEString* keyword2 = parameters.elementAt(i);

         foundSpecialWriterFlag =
            (0 == strcmp(parameters.elementAt(i)->data(), kCityJSON_FME_DESTINATION));
      }
   }

   // See if we are just in the regular reader mode
   if (!foundSpecialWriterFlag)
   {
      writerHelperMode_ = false;
      return false;
   }

   // Need to read in the writer settings relevant to reading schemas.
   IFMEString* paramValue = gFMESession->createString();

   // These lines are good for debugging to see all the things we could fetch from gMappingFile
   /*
   gMappingFile->startIteration();
   IFMEStringArray* aRow = gFMESession->createStringArray();
   while (FME_TRUE == gMappingFile->nextLine(*aRow))
   {
      for (int j = 0; j < aRow->entries(); j++)
      {
         gLogFile->logMessageString(aRow->elementAt(j)->data(), FME_INFORM);
      }
      gLogFile->logMessageString("----------------", FME_INFORM);
   }
   gFMESession->destroyStringArray(aRow);
   */

   if (gMappingFile->fetchWithPrefix(readerKeyword_.c_str(),
                                     readerTypeName_.c_str(),
                                     kCityJSON_CITYJSON_STARTING_SCHEMA,
                                     *paramValue))
   {
      // A parameter value has been found, so set the values.
      writerStartingSchema_ = paramValue->data();

      // Let's log to the user that a parameter value has been specified.
      // TODO: this could be cleaned up.
      std::string paramMsg =
         (kCityJSON_CITYJSON_STARTING_SCHEMA + std::string(" ") + writerStartingSchema_).c_str();
      gLogFile->logMessageString(paramMsg.c_str(), FME_INFORM);

      // Now we know we are in the special writer-helper-mode
      writerHelperMode_ = true;
   }
   else
   {
      // Log that no parameter value was entered.
      // TODO: this could be cleaned up.
      gLogFile->logMessageString(kMsgNoLodParam, FME_INFORM);
      writerHelperMode_ = false;
   }
   gFMESession->destroyString(paramValue);

   return writerHelperMode_;
}

FME_Status FMECityJSONReader::readRaster(const std::string& fullFileName,
                                         IFMERaster*& raster,
                                         std::string readerToUse)
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
   FME_MsgNum badLuck          = newReader->open(fullFileName.data(), *parameters);
   gFMESession->destroyStringArray(parameters);
   if (badLuck)
   {
      // TODO: Log some error message
      return FME_FAILURE;
   }
   FME_Boolean endOfFile       = FME_FALSE;
   IFMEFeature* textureFeature = gFMESession->createFeature();

   badLuck = newReader->read(*textureFeature, endOfFile);
   if (badLuck)
   {
      // TODO: Log some error message
      return FME_FAILURE;
   }

   IFMEGeometry* geom = textureFeature->removeGeometry();

   if (!geom->canCastAs<IFMERaster*>())
   {
      // TODO: Log some warning message
      raster = nullptr;
   }
   else
   {
      // This is what we'll return
      raster = geom->castAs<IFMERaster*>();
   }

   // Close the reader and *ignore* any errors.
   badLuck = newReader->close();

   // clean up
   gFMESession->destroyFeature(textureFeature);
   textureFeature = nullptr;
   gFMESession->destroyReader(newReader);
   newReader = nullptr;

   return FME_SUCCESS;
}

void FMECityJSONReader::unrollReferences2(json::value_type& references,
                                          json::value_type boundaries,
                                          RefVec2& refsPerBoundary)
{
   // Does this have any texture data attached?
   if (not references.is_null())
   {
      int nrSurfaces = distance(begin(boundaries), end(boundaries));
      for (int i = 0; i < nrSurfaces; i++)
      {
         RefVec refs;
         for (json::iterator it = references.begin(); it != references.end(); ++it)
         {
            refs.push_back(it.value()["values"][i]);
         }
         refsPerBoundary.push_back(refs);
      }
   }
}

void FMECityJSONReader::unrollReferences3(json::value_type& references,
                                          json::value_type boundaries,
                                          RefVec3& refsPerBoundaryPerShell)
{
   // Does this have any texture data attached?
   if (not references.is_null())
   {
      int nrShells = distance(begin(boundaries), end(boundaries));
      for (int i = 0; i < nrShells; i++)
      {
         RefVec2 refsPerBoundary;
         int nrSurfaces = distance(begin(boundaries[i]), end(boundaries[i]));
         for (int j = 0; j < nrSurfaces; j++)
         {
            RefVec refs;
            for (json::iterator it = references.begin(); it != references.end(); ++it)
            {
               refs.push_back(it.value()["values"][i][j]);
            }
            refsPerBoundary.push_back(refs);
         }
         refsPerBoundaryPerShell.push_back(refsPerBoundary);
      }
   }
}

void FMECityJSONReader::unrollReferences4(json::value_type& references,
                                          json::value_type boundaries,
                                          RefVec4& refsPerBoundaryPerShellperSolid)
{
   // Does this have any texture data attached?
   if (not references.is_null())
   {
      int nrSolids = distance(begin(boundaries), end(boundaries));
      for (int i = 0; i < nrSolids; i++)
      {
         RefVec3 refsPerBoundaryPerShell;
         int nrShells = distance(begin(boundaries[i]), end(boundaries[i]));
         for (int j = 0; j < nrShells; j++)
         {
            RefVec2 refsPerBoundary;
            int nrSurfaces = distance(begin(boundaries[i][j]), end(boundaries[i][j]));
            for (int k = 0; k < nrSurfaces; k++)
            {
               RefVec rRefs;
               for (json::iterator it = references.begin(); it != references.end(); ++it)
               {
                  rRefs.push_back(it.value()["values"][i][j][k]);
               }
               refsPerBoundary.push_back(rRefs);
            }
            refsPerBoundaryPerShell.push_back(refsPerBoundary);
         }
         refsPerBoundaryPerShellperSolid.push_back(refsPerBoundaryPerShell);
      }
   }
}

