/*=============================================================================

   Name     : geometryvisitor.cpp

   System   : FME Plug-in SDK

   Language : C++

   Purpose  : Declaration of FMECityJSONGeometryVisitor

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

#include "fmecityjsongeometryvisitor.h"
#include "fmecityjsonpriv.h"
#include "fmecityjsonwriter.h"

#include <fmetypes.h>
#include <iaggregate.h>
#include <iarc.h>
#include <iorientedarc.h>
#include <iclothoid.h>
#include <iarea.h>
#include <iareaiterator.h>
#include <ibox.h>
#include <ibrepsolid.h>
#include <icompositesolid.h>
#include <icompositesurface.h>
#include <icsgsolid.h>
#include <icurveiterator.h>
#include <idonut.h>
#include <iellipse.h>
#include <iextrusion.h>
#include <iface.h>
#include <igeometryiterator.h>
#include <iline.h>
#include <ilogfile.h>
#include <imesh.h>
#include <imultiarea.h>
#include <imulticurve.h>
#include <imultipoint.h>
#include <imultisolid.h>
#include <imultisurface.h>
#include <imultitext.h>
#include <ipath.h>
#include <ipoint.h>
#include <ipointiterator.h>
#include <ipolygon.h>
#include <iraster.h>
#include <irectangleface.h>
#include <isegmentiterator.h>
#include <isession.h>
#include <isimpleareaiterator.h>
#include <isimplesoliditerator.h>
#include <isoliditerator.h>
#include <isurfaceiterator.h>
#include <itext.h>
#include <itextiterator.h>
#include <itrianglefan.h>
#include <itrianglestrip.h>
#include <ilibrary.h>

#include <string>

const std::map< std::string, std::vector< std::string > > FMECityJSONGeometryVisitor::semancticsTypes_ = std::map< std::string, std::vector< std::string > >(
   {
      {"Building", {
         "RoofSurface",
         "GroundSurface",
         "WallSurface",
         "ClosureSurface",
         "OuterCeilingSurface",
         "OuterFloorSurface",
         "Window",
         "Door"}},
      {"BuildingPart", {
         "RoofSurface",
         "GroundSurface",
         "WallSurface",
         "ClosureSurface",
         "OuterCeilingSurface",
         "OuterFloorSurface",
         "Window",
         "Door"}},
      {"BuildingInstallation", {
         "RoofSurface",
         "GroundSurface",
         "WallSurface",
         "ClosureSurface",
         "OuterCeilingSurface",
         "OuterFloorSurface",
         "Window",
         "Door"}},
      {"Railway", {
         "TrafficArea",
         "AuxiliaryTrafficArea"
      }},
      {"Road", {
         "TrafficArea",
         "AuxiliaryTrafficArea"
      }},
      {"TransportSquare", {
         "TrafficArea",
         "AuxiliaryTrafficArea"
      }},
      {"WaterBody", {
         "WaterSurface",
         "WaterGroundSurface",
         "WaterClosureSurface"
      }}
   }
);

//===========================================================================
// Constructor.
FMECityJSONGeometryVisitor::FMECityJSONGeometryVisitor(const IFMEGeometryTools* geomTools,
                                                       IFMESession* session,
                                                       bool remove_duplicates,
                                                       int important_digits,
                                                       std::map<FME_UInt32, int>& textureRefsToCJIndex,
                                                       std::map<MaterialInfo, int>& materialInfoToCJIndex)
   :
   fmeGeometryTools_(geomTools),
   fmeSession_(session),
   remove_duplicates_(remove_duplicates),
   important_digits_(important_digits),
   skipLastPointOnLine_(false),
   textureRefsToCJIndex_(textureRefsToCJIndex),
   materialInfoToCJIndex_(materialInfoToCJIndex)
{
   logFile_ = session->logFile();
   uCoordDesc_ = session->createString();
   uCoordDesc_->set(kFME_texture_coordinate_u, (FME_UInt32)strlen(kFME_texture_coordinate_u));
   vCoordDesc_ = session->createString();
   vCoordDesc_->set(kFME_texture_coordinate_v, (FME_UInt32)strlen(kFME_texture_coordinate_v));
}

//===========================================================================
// Destructor.
FMECityJSONGeometryVisitor::~FMECityJSONGeometryVisitor()
{
   //------------------------------------------------------------------------
   // Perform any necessary cleanup
   //------------------------------------------------------------------------

   fmeSession_->destroyString(uCoordDesc_); uCoordDesc_ = nullptr;
   fmeSession_->destroyString(vCoordDesc_); vCoordDesc_ = nullptr;
}

json FMECityJSONGeometryVisitor::getGeomJSON()
{
   json retVal = outputgeom_;
   outputgeom_.clear();
   return retVal;
}

json FMECityJSONGeometryVisitor::getTexCoordsJSON()
{
   json retVal;
   // This holds an array of strings, but each string is a (nested) JSON array.
   if (!textureCoords_.empty())
   {
      retVal = json::array();
      for (auto& oneElement : textureCoords_)
      {
         retVal.push_back(json::parse(oneElement));
      }
   }
   textureCoords_.clear();
   return retVal;
}

void FMECityJSONGeometryVisitor::takeWorkingBoundaries(json& jsonArray,
                                                       json& jsonTCArray,
                                                       json& jsonMaterialRefs)
{
   jsonArray = workingBoundary_;
   workingBoundary_.clear();

   // Do we need to gather up the textures?
   jsonTCArray = workingTexCoords_;
   workingTexCoords_.clear();

   // Do we need to gather up the materials?
   jsonMaterialRefs = workingMaterialRefs_;
   workingMaterialRefs_.clear();
}

void FMECityJSONGeometryVisitor::takeWorkingBoundaries_1Deep(json& jsonArray,
                                                             json& jsonTCArray,
                                                             json& jsonMaterialRefs)
{
   // We need to handle multi surfaces, etc differently, as they will have
   // another level of hierarchy we need to drop.  CityJSON does not allow
   // nesting in the same way FME can.
   json jsonArray2;
   json jsonTCArray2;
   json jsonMaterialRefs2;
   takeWorkingBoundaries(jsonArray2, jsonTCArray2, jsonMaterialRefs2);
   jsonArray.insert(jsonArray.end(), jsonArray2.begin(), jsonArray2.end());

   // Do we need to gather up the textures?
   jsonTCArray.insert(jsonTCArray.end(), jsonTCArray2.begin(), jsonTCArray2.end());

   // Do we need to gather up the materials?
   jsonMaterialRefs.insert(jsonMaterialRefs.end(), jsonMaterialRefs2.begin(), jsonMaterialRefs2.end());
}

void FMECityJSONGeometryVisitor::addWorkingBoundaries(json& jsonArray,
                                                      json& jsonTCArray,
                                                      json& jsonMaterialRefs)
{
   json jsonArray2;
   json jsonTCArray2;
   json jsonMaterialRefs2;
   takeWorkingBoundaries(jsonArray2, jsonTCArray2, jsonMaterialRefs2);
   jsonArray.push_back(jsonArray2);

   // Do we need to gather up the textures?
   jsonTCArray.push_back(jsonTCArray2);

   // Do we need to gather up the materials?
   jsonMaterialRefs.push_back(jsonMaterialRefs2);
}

void FMECityJSONGeometryVisitor::addWorkingBoundaries_1Deep(json& jsonArray,
                                                            json& jsonTCArray,
                                                            json& jsonMaterialRefs)
{
   json jsonArray2;
   json jsonTCArray2;
   json jsonMaterialRefs2;
   takeWorkingBoundaries(jsonArray2, jsonTCArray2, jsonMaterialRefs2);
   jsonArray.insert(jsonArray.end(), jsonArray2.begin(), jsonArray2.end());

   // Do we need to gather up the textures?
   jsonTCArray.insert(jsonTCArray.end(), jsonTCArray2.begin(), jsonTCArray2.end());

   // Do we need to gather up the materials?
   jsonMaterialRefs.insert(jsonMaterialRefs.end(), jsonMaterialRefs2.begin(), jsonMaterialRefs2.end());
}

const VertexPool& FMECityJSONGeometryVisitor::getGeomVertices()
{
   return vertices_;
}

const TexCoordPool& FMECityJSONGeometryVisitor::getTextureCoords()
{
   return textureCoords_;
}

void FMECityJSONGeometryVisitor::getGeomBounds(std::optional<double>& minx,
                                               std::optional<double>& miny,
                                               std::optional<double>& minz,
                                               std::optional<double>& maxx,
                                               std::optional<double>& maxy,
                                               std::optional<double>& maxz)
{
   minx = minx_;
   miny = miny_;
   minz = minz_;
   maxx = maxx_;
   maxy = maxy_;
   maxz = maxz_;
}

json FMECityJSONGeometryVisitor::getTemplateJSON()
{
   json result = json::object();
   if (!templateGeoms_.empty())
   {
      result["templates"] = templateGeoms_;
      result["vertices-templates"] = templateVertices_;
   }
   return result;
}

bool FMECityJSONGeometryVisitor::semanticTypeAllowed(std::string trait)
{
   if (trait.compare(0, 1, "+") == 0) {
      return true;
   }
   auto iter = semancticsTypes_.find(featureType_);
   if (iter != semancticsTypes_.end())
   {
      std::vector<std::string> traits = iter->second;
      for (int i = 0; i < traits.size(); i++)
      {
         // logDebugMessage("comparing traits[i]: " + traits[i] + " & trait: " + trait);
         if (traits[i] == trait)
         {
            return true;
         }
      }
   }

   // Let's see that we don't log too much
   // we make up a key based on the feature type and semantics
   std::string logKey = featureType_ + trait;
   if (limitLogging_[logKey]++ < 3)
   {
      std::string message("CityJSON Semantic of '");
      message += trait;
      message += "' is not valid for Surface Type '";
      message += featureType_;
      message += "'.  Consult the official CityJSON types(https://www.cityjson.org/specs/#semantic-surface-object) or an Extension ('+').";
      logFile_->logMessageString(message.c_str(), FME_WARN);
   }

   return false;
}

void FMECityJSONGeometryVisitor::setFeatureType(std::string type) {
  featureType_ = type;
}

json FMECityJSONGeometryVisitor::replaceSemanticValues(std::vector<json> semanticValues) {
   // replace array with only null values with a single null value
   for (int i = 0; i < semanticValues.size(); i++) {
      if (!semanticValues[i].is_null()) {
         return semanticValues;
      }
   }
   return nullptr;
}

// Converts a value into a string
std::string get_key(FME_Real64 val, int precision)
{
   char buf[200];
   std::stringstream ss;
   ss << "%." << precision << "f";
   std::sprintf(buf, ss.str().c_str(), val);
   std::string r(buf);

   // Pretty it up a bit if it has a decimal place (remove trailing zeros)
   if (r.find('.') != std::string::npos)
   {
      // Remove trailing 0s
      r = r.substr(0, r.find_last_not_of('0') + 1);
      // If the decimal point is now the last character, remove that as well
      if (r.find('.') == r.size() - 1)
      {
         r = r.substr(0, r.size() - 1);
      }
   }

   return r;
}

// Converts a point into a string
std::string get_key(const FMECoord3D& vertex, int precision)
{
   return (get_key(vertex.x, precision) + ' ' + 
           get_key(vertex.y, precision) + ' ' +
           get_key(vertex.z, precision));
}

std::string get_key(const FMECoord2D& vertex, int precision)
{
   // we put commas in this key, as it will be used to make JSON later.
   return (get_key(vertex.x, precision) + ", " +
           get_key(vertex.y, precision));
}


void tokenize(const std::string& str, std::vector<std::string>& tokens)
{
   std::string::size_type lastPos = str.find_first_not_of(" ", 0);
   std::string::size_type pos     = str.find_first_of(" ", lastPos);
   while (std::string::npos != pos || std::string::npos != lastPos)
   {
      tokens.push_back(str.substr(lastPos, pos - lastPos));
      lastPos = str.find_first_not_of(" ", pos);
      pos     = str.find_first_of(" ", lastPos);
   }
}

void FMECityJSONGeometryVisitor::acceptVertex(const std::string& vertex_string, 
                                              VertexPool& vertices, // out
                                              const bool updateBounds)
{
   // Sadly, we need to calculate our bounds from the stringified vertex we
   // are using.  So back to float!
   std::vector<std::string> ls;
   tokenize(vertex_string, ls);
   double x = std::stod(ls[0]);
   double y = std::stod(ls[1]);
   double z = std::stod(ls[2]);

   if (updateBounds)
   {
      if (!minx_ || x < minx_) minx_ = x;
      if (!maxx_ || x > maxx_) maxx_ = x;
      if (!miny_ || y < miny_) miny_ = y;
      if (!maxy_ || y > maxy_) maxy_ = y;
      if (!std::isnan(z)) // have z
      {
         if (!minz_ || z < minz_) minz_ = z;
         if (!maxz_ || z > maxz_) maxz_ = z;
      }
   }

   vertices.push_back({x, y, z});
}

// This will make sure we don't add any vertex twice.
unsigned long FMECityJSONGeometryVisitor::addVertex(const FMECoord3D& vertex)
{
   // This is the vertex, as a string, which we'll use.
   std::string vertex_string = get_key(vertex, important_digits_);

   // If we're inside a template geom the bounds should not be updated
   // and the vertices should go into a separate vertex pool
   auto& vertices = insideTemplateGeom_ ? templateVertices_ : vertices_;
   auto& vertexToIndex = insideTemplateGeom_ ? templateVertexToIndex_ : vertexToIndex_;
   const bool updateBounds = !insideTemplateGeom_;

   // This will be the index in the vertex pool if it is new
   unsigned long index(vertices.size());

   // A little more bookkeeping if we want to optimize the vertex pool
   // and not have duplicates.
   if (remove_duplicates_)
   {
      // Have we encountered this vertex before?
      auto [entry, vertexAdded] = vertexToIndex.try_emplace(vertex_string, index);
      if (!vertexAdded) // We already have this in our vertex pool
      {
         index = entry->second;
      }
      else // We haven't seen this before, so insert it into the pool.
      {
         acceptVertex(vertex_string, vertices, updateBounds);

         // index is already set correctly.
      }
   }
   else
   {
      acceptVertex(vertex_string, vertices, updateBounds);

      // index is already set correctly.
   }
   return index;
}

// This will make sure we don't add any texture coord twice.
unsigned long FMECityJSONGeometryVisitor::addTextureCoord(const FMECoord2D& texcoord)
{
   // This is the vertex, as a string, which we'll use.
   std::string texcoord_key = get_key(texcoord, important_digits_);

   // This will be the index in the vertex pool if it is new
   unsigned long index(textureCoords_.size());

   // A little more bookkeeping if we want to optimize the texture coordinate pool
   // and not have duplicates.

   // Have we encountered this texture coordinate before?
   auto [entry, texCoordAdded] = textureCoordToIndex_.try_emplace(texcoord_key, index);
   if (!texCoordAdded) // We already have this in our texture coordinate pool
   {
      index = entry->second;
   }
   else // We haven't seen this before, so insert it into the pool.
   {
      textureCoords_.push_back('[' + texcoord_key + ']');

      // index is already set correctly.
   }

   return index;
}

//=====================================================================
int FMECityJSONGeometryVisitor::getMaterialRefFromAppearance(const IFMEAppearance* app)
{
   MaterialInfo mi;

   bool gotNonDefaults(false);

   // Get the "ambientIntensity"
   // "ambientIntensity" specifies how much ambient light from light sources this surface shall
   // reflect. Ambient light is omnidirectional and depends only on the number of light sources, not
   // their  positions with respect to the surface.
   // https://www.web3d.org/documents/specifications/19775-1/V3.2/Part01/components/shape.html#Material
   // FME_ambientColour = cityJSON_ambientIntensity × cityJSON_diffuseColor.
   FME_Real64 r_ambient;
   FME_Real64 g_ambient;
   FME_Real64 b_ambient;
   if (FME_TRUE == app->getColorAmbient(r_ambient, g_ambient, b_ambient))
   {
      gotNonDefaults = true;

      FME_Real64 r_diffuse;
      FME_Real64 g_diffuse;
      FME_Real64 b_diffuse;
      if (FME_TRUE != app->getColorDiffuse(r_diffuse, g_diffuse, b_diffuse))
      {
         // Ick.  There are lots of possible solutions if we don't have a diffuse color specified.
         // We're going to assume an average diffuse color of 0.5, 0.5, 0.5, and pretend it was specified.
         r_diffuse = 0.5;
         g_diffuse = 0.5;
         b_diffuse = 0.5;
      }

      // We'll use a non-zero element to compute our ambientIntensity.
      // All of the non-zero elements should result in the same solution for ambientIntensity
      // (assuming infinite precision arithmetic).
      if (r_diffuse != 0)
      {
         std::get<1>(mi) = r_ambient / r_diffuse;
      }
      else if (g_diffuse != 0)
      {
         std::get<1>(mi) = g_ambient / g_diffuse;
      }
      else if (b_diffuse != 0)
      {
         std::get<1>(mi) = b_ambient / b_diffuse;
      }
      else
      {
         // Undetermined.  Hopefully the data is self-consistent, and everything is zero.
         // We'll arbitrarily choose a mid-range value for ambient intensity
         std::get<1>(mi) = 0.5;
      }
   }

   // Get the "diffuseColor"
   FME_Real64 r_diffuse;
   FME_Real64 g_diffuse;
   FME_Real64 b_diffuse;
   if (FME_TRUE == app->getColorDiffuse(r_diffuse, g_diffuse, b_diffuse))
   {
      std::get<2>(mi) = r_diffuse;
      std::get<3>(mi) = g_diffuse;
      std::get<4>(mi) = b_diffuse;
      gotNonDefaults = true;
   }

   // Get the "emissiveColor"
   FME_Real64 r_emissive;
   FME_Real64 g_emissive;
   FME_Real64 b_emissive;
   if (FME_TRUE == app->getColorEmissive(r_emissive, g_emissive, b_emissive))
   {
      std::get<5>(mi) = r_emissive;
      std::get<6>(mi) = g_emissive;
      std::get<7>(mi) = b_emissive;
      gotNonDefaults = true;
   }

   // Get the "specularColor"
   FME_Real64 r_specular;
   FME_Real64 g_specular;
   FME_Real64 b_specular;
   if (FME_TRUE == app->getColorSpecular(r_specular, g_specular, b_specular))
   {
      std::get<8>(mi) = r_specular;
      std::get<9>(mi) = g_specular;
      std::get<10>(mi) = b_specular;
      gotNonDefaults = true;
   }

   // Get the "shininess"
   FME_Real64 shininess;
   if (FME_TRUE == app->getShininess(shininess))
   {
      std::get<11>(mi) = shininess;
      gotNonDefaults = true;
   }

   // Get the "transparency"
   FME_Real64 alpha;
   if (FME_TRUE == app->getAlpha(alpha))
   {
      gotNonDefaults = true;
      std::get<12>(mi) = 1.0 - alpha; // convert from Alpha to transparency
   }

   // Get the "isSmooth"
   // currently there is no easy way to support this in FME.  Not yet.
 
   int cityJSONMaterialIndex(-1);

   // Get the name
   if (gotNonDefaults)
   {
      IFMEString* name = fmeSession_->createString();
      if (FME_TRUE == app->getName(*name, nullptr))
      {
         std::get<0>(mi) = {name->data(), name->length()};
      }
      fmeSession_->destroyString(name);

      // We've got a material.
      // One we've never seen before?
      auto refIndex = materialInfoToCJIndex_.find(mi);
      if (refIndex == materialInfoToCJIndex_.end())
      {
         // Storing an increasing number means we'll get the right
         // index later.
         cityJSONMaterialIndex      = materialInfoToCJIndex_.size();
         materialInfoToCJIndex_[mi] = cityJSONMaterialIndex;
      }
      else
      {
         cityJSONMaterialIndex = refIndex->second;
      }

   }

   return cityJSONMaterialIndex;
}

//=====================================================================
//
bool FMECityJSONGeometryVisitor::claimTopLevel(const std::string& type)
{
   if (outputgeom_.empty())
   {
      outputgeom_ = json::object();
      outputgeom_["type"] = type;
      return true;
   }
   else
   {
      return false;

   }
}

// Do we have at least one item that is not null?
bool containsNonNull(const json& inputData)
{
   if (not inputData.is_array())
   {
      return not inputData.is_null();
   }
   for (auto& id : inputData)
   {
      if (containsNonNull(id))
      {
         return true;
      }
   }
   return false;
}

//=====================================================================
//
void FMECityJSONGeometryVisitor::completedGeometry(bool topLevel,
                                                   const json& boundary,
                                                   const json& texCoords,
                                                   const json& materialRefs)
{
   if (topLevel)
   {
      outputgeom_["boundaries"] = boundary;
      if (!texCoords.empty() && !textureRefsToCJIndex_.empty()) // only if we've actually found a texture.
      {
         outputgeom_["texture"]["default_theme"]["values"] = texCoords;
      }

      if (containsNonNull(materialRefs) && !materialInfoToCJIndex_.empty()) // only if we've actually found a material.
      {
         outputgeom_["material"]["default_theme"]["values"] = materialRefs;
      }

      //-- write it to the JSON object
      if (!outputgeom_.empty())
      {
         //-- TODO: write '2' or '2.0' is fine for the "lod"?
         outputgeom_["lod"] = lodAsDouble_;

         if (insideTemplateGeom_)
         {
            templateGeoms_.push_back(outputgeom_);
         }
         else if (outputgeoms_ != nullptr)
         {
            outputgeoms_->push_back(outputgeom_);
         }
      }

      outputgeom_.clear();
      surfaces_.clear();
      semanticValues_.clear();
      workingBoundary_.clear();
      workingTexCoords_.clear();
      workingMaterialRefs_.clear();
   }
   else
   {
      workingBoundary_ = boundary;
      if (!texCoords.is_null())
      {
         workingTexCoords_ = texCoords;
      }
      if (!materialRefs.is_null())
      {
         workingMaterialRefs_ = materialRefs;
      }
   }
}


//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitAggregate(const IFMEAggregate& aggregate)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("aggregate"));

   // CityJSON must explicitly set texture references on each level of the
   // hierarchy, so we must resolve any inheritance that might exist.
   const FME_UInt32 oldParentAppearanceRef = updateParentAppearanceReference(aggregate);

   // Check whether this is a geometry instance that can be written as a template
   FME_UInt32 geometryDefinitionReference = 0;
   IFMEGeometry* geometryDefinition = nullptr;
   if (!insideTemplateGeom_ && parentAppearanceRef_ == 0)
   {
      if (aggregate.getGeometryDefinitionReference(geometryDefinitionReference))
      {
         geometryDefinition = fmeSession_->getLibrary()->getGeometryDefinitionCopy(geometryDefinitionReference);
         // Templates in CityJSON may not contain aggregates
         if (geometryDefinition->canCastAs<IFMEAggregate*>())
         {
            fmeGeometryTools_->destroyGeometry(geometryDefinition);
            geometryDefinition = nullptr;
         }
      }
   }

   if (geometryDefinition != nullptr)
   {
      std::size_t templateIndex = templateGeoms_.size();
      const auto insertResult = gdReferenceToTemplateIndex_.try_emplace(geometryDefinitionReference, templateIndex);
      if (insertResult.second)
      {
         // This is the first time this geometry definition has been seen. Visit the geometry definition
         insideTemplateGeom_ = true;
         const FME_Status badLuck = geometryDefinition->acceptGeometryVisitorConst(*this);
         fmeGeometryTools_->destroyGeometry(geometryDefinition); geometryDefinition = nullptr;
         insideTemplateGeom_ = false;
         if (badLuck) return badLuck;
      }
      else
      {
         // This geometry definition has already been visited. Let's just use that template index.
         templateIndex = insertResult.first->second;
      }

      FMECoord3D origin;
      if (!aggregate.getGeometryInstanceLocalOrigin(origin.x, origin.y, origin.z))
      {
         return FME_FAILURE;
      }

      FME_Real64 matrix[3][4];
      if (!aggregate.getGeometryInstanceMatrix(matrix))
      {
         matrix[0][0] = 1;
         matrix[0][1] = 0;
         matrix[0][2] = 0;
         matrix[0][3] = 0;
         matrix[1][0] = 0;
         matrix[1][1] = 1;
         matrix[1][2] = 0;
         matrix[1][3] = 0;
         matrix[2][0] = 0;
         matrix[2][1] = 0;
         matrix[2][2] = 1;
         matrix[2][3] = 0;
      }

      const bool topLevel = claimTopLevel("GeometryInstance");
      if (!topLevel)
      {
         return FME_FAILURE;
      }
      
      outputgeom_["template"] = templateIndex;
      outputgeom_["transformationMatrix"] = 
         {matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3],
          matrix[1][0], matrix[1][1], matrix[1][2], matrix[1][3],
          matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3],
          0.0, 0.0, 0.0, 1.0};

      completedGeometry(topLevel, { addVertex(origin) }, {}, {});
   }
   else
   {
      // Visit all the parts in order. Each geometry part will become a separate
      // geometry json object in outputgeoms_
      for (FME_UInt32 i = 0; i < aggregate.numParts(); ++i)
      {
         const FME_Status badLuck = aggregate.getPartAt(i)->acceptGeometryVisitorConst(*this);
         if (badLuck) return badLuck;
      }
   }

   parentAppearanceRef_ = oldParentAppearanceRef;

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("aggregate"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPoint(const IFMEPoint& point)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("point"));

   bool topLevel = claimTopLevel("MultiPoint");

   unsigned long index = addVertex({point.getX(), point.getY(), point.getZ()});

   // We have to make an array of only one!
   auto jsonArray = json::array();
   jsonArray.push_back(index);

   completedGeometry(topLevel, jsonArray, {}, {});

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("point"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiPoint(const IFMEMultiPoint& multipoint)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi point"));

   bool topLevel = claimTopLevel("MultiPoint");

   // Create iterator to get all point geometries
   IFMEPointIterator* iterator = multipoint.getIterator();
   auto jsonArray = json::array();
   json jsonTCArray = json::array();
   json jsonMaterialRefs = json::array();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("point"));

      // re-visit points
      FME_Status badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator
         multipoint.destroyIterator(iterator);
         return FME_FAILURE;
      }
      addWorkingBoundaries(jsonArray, jsonTCArray, jsonMaterialRefs);
   }
   // We are done with the iterator, so destroy it
   multipoint.destroyIterator(iterator);

   completedGeometry(topLevel, jsonArray, jsonTCArray, jsonMaterialRefs);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi point"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitArc(const IFMEArc& arc)
{
   IFMELine* asLine  = arc.getAsLine();
   FME_Status result = visitLine(*asLine);
   fmeGeometryTools_->destroyGeometry(asLine);
   return result;
}

//-----------------------------------------------------------------------------
FME_Status FMECityJSONGeometryVisitor::visitOrientedArc(const IFMEOrientedArc & orientedArc)
{
   IFMELine* asLine = orientedArc.getAsLine();
   FME_Status result = visitLine(*asLine);
   fmeGeometryTools_->destroyGeometry(asLine);
   return result;
}

//-----------------------------------------------------------------------------
FME_Status FMECityJSONGeometryVisitor::visitClothoid(const IFMEClothoid& clothoid)
{
   IFMELine* asLine = clothoid.getAsLine();
   FME_Status result = visitLine(*asLine);
   fmeGeometryTools_->destroyGeometry(asLine);
   return result;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitLine(const IFMELine& line)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("line"));

   bool topLevel = claimTopLevel("MultiLineString");

   // Do we need to skip the last point?
   int skip = skipLastPointOnLine_ ? 1 : 0;

   auto jsonArray = json::array();
   for (int i = 0; i < line.numPoints()-skip; i++)
   {
      FMECoord3D point;
      line.getPointAt3D(i, point);
      unsigned long index = addVertex(point);
      jsonArray.push_back(index);
   }

   // If we are at the top level, just passed in a Line, we must convert this into
   // a MultiLineString, as CityJSON cannot store lines by themselves.
   if (topLevel)
   {
      auto jsonArray2 = json::array();
      jsonArray2.push_back(jsonArray);
      jsonArray = jsonArray2;
   }

   // Do we need to gather up the textures?
   auto jsonTCArray = json::array();

   FME_Real64* uCoords = new FME_Real64[line.numPoints()];
   FME_Real64* vCoords = new FME_Real64[line.numPoints()];

   if ((FME_SUCCESS == line.getNamedMeasureValues(*uCoordDesc_, uCoords)) &&
       (FME_SUCCESS == line.getNamedMeasureValues(*vCoordDesc_, vCoords)))
   {
      // The index to the texture is first.
      jsonTCArray.push_back(nextTextRef_);
      for (FME_UInt32 i = 0; i < line.numPoints() - skip; i++)
      {
         FMECoord2D uvCoord(uCoords[i], vCoords[i]);
         unsigned long index = addTextureCoord(uvCoord);
         jsonTCArray.push_back(index);
      }
   }
   else
   {
      jsonTCArray.push_back(nullptr);
   }

   delete [] uCoords; uCoords = nullptr;
   delete [] vCoords; vCoords = nullptr;

   completedGeometry(topLevel, jsonArray, jsonTCArray, {});

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("line"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPath(const IFMEPath& path)
{
   IFMELine* asLine  = path.getAsLine();
   FME_Status result = visitLine(*asLine);
   fmeGeometryTools_->destroyGeometry(asLine);
   return result;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiCurve(const IFMEMultiCurve& multicurve)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi curve"));

   bool topLevel = claimTopLevel("MultiLineString");

   // Create an iterator to get the curves
   IFMECurveIterator* iterator = multicurve.getIterator();
   auto jsonArray = json::array();
   json jsonTCArray = json::array();
   json jsonMaterialRefs = json::array();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("curve"));

      // re-visit the next curve
      FME_Status badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy the iterator and return fail
         multicurve.destroyIterator(iterator);
         return FME_FAILURE;
      }
      addWorkingBoundaries(jsonArray, jsonTCArray, jsonMaterialRefs);
   }
   // Done visiting curves, destroy iterator
   multicurve.destroyIterator(iterator);

   completedGeometry(topLevel, jsonArray, jsonTCArray, jsonMaterialRefs);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi curve"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiArea(const IFMEMultiArea& multiarea)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi area"));

   bool topLevel = claimTopLevel("MultiLineString");

   // Create iterator to visit all areas
   IFMEAreaIterator* iterator = multiarea.getIterator();
   auto jsonArray = json::array();
   json jsonTCArray = json::array();
   json jsonMaterialRefs = json::array();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("area"));

      // re-visit areas
      FME_Status badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator
         multiarea.destroyIterator(iterator);
         return FME_FAILURE;
      }
      takeWorkingBoundaries_1Deep(jsonArray, jsonTCArray, jsonMaterialRefs);
   }
   // Done with iterator, destroy it
   multiarea.destroyIterator(iterator);

   completedGeometry(topLevel, jsonArray, jsonTCArray, jsonMaterialRefs);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi area"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPolygon(const IFMEPolygon& polygon)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("polygon"));

   bool topLevel = claimTopLevel("MultiLineString");

   const IFMECurve* boundary = polygon.getBoundaryAsCurve();
   if (boundary == nullptr)
   {
      // We need a boundary, return fail
      return FME_FAILURE;
   }
   // re-visit polygon curve geometry
   auto jsonArray = json::array();
   json jsonTCArray = json::array();
   json unused = json::array();
   FME_Status badNews = boundary->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }
   addWorkingBoundaries(jsonArray, jsonTCArray, unused);
   completedGeometry(topLevel, jsonArray, jsonTCArray, {});

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("polygon"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitDonut(const IFMEDonut& donut)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("donut"));

   // Get the outer boundary
   logDebugMessage(std::string(kMsgVisiting) + std::string("outer boundary"));

   bool topLevel = claimTopLevel("MultiLineString");

   const IFMEArea* outerBoundary = donut.getOuterBoundaryAsSimpleArea();
   if (outerBoundary == nullptr)
   {
      // We require an outer boundary, return fail
      return FME_FAILURE;
   }
   // re-visit the outer boundary
   FME_Status badNews = outerBoundary->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }
   // We need to handle donut areas differently, as they will have
   // another level of hierarchy we need to drop.  CityJSON does not allow
   // nesting in the same way FME can.
   auto jsonArray = json::array();
   json jsonTCArray = json::array();
   json unused = json::array();
   takeWorkingBoundaries_1Deep(jsonArray, jsonTCArray, unused);

   // Get the inner boundary
   logDebugMessage(std::string(kMsgVisiting) + std::string("inner boundary"));

   IFMESimpleAreaIterator* iterator = donut.getIterator();
   while (iterator->next())
   {
      // re-visit the inner boundary
      badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         donut.destroyIterator(iterator);
         return FME_FAILURE;
      }
      // We need to handle donut areas differently, as they will have
      // another level of hierarchy we need to drop.  CityJSON does not allow
      // nesting in the same way FME can.
      takeWorkingBoundaries_1Deep(jsonArray, jsonTCArray, unused);
   }
   // Done with iterator, destroy it
   donut.destroyIterator(iterator);

   completedGeometry(topLevel, jsonArray, jsonTCArray, {});

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("donut"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitText(const IFMEText& text)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("text"));

   // This IFMEGeometryVisitorConst subclass does not consume the geometry which accepts it
   IFMEPoint* point = text.getLocationAsPoint();
   // re-visit location
   FME_Status badNews = visitPoint(*point);
   point->destroy(); point = nullptr;

   if (badNews)
   {
      return FME_FAILURE;
   }
   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiText(const IFMEMultiText& multitext)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi text"));

   bool topLevel = claimTopLevel("MultiPoint");

   // Create iterator to get all text geometries
   IFMETextIterator* iterator = multitext.getIterator();
   auto jsonArray = json::array();
   json jsonTCArray = json::array();
   json jsonMaterialRefs = json::array();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("text"));

      // re-visit next text geometry
      FME_Status badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         multitext.destroyIterator(iterator);
         return FME_FAILURE;
      }
      addWorkingBoundaries(jsonArray, jsonTCArray, jsonMaterialRefs);
   }
   // Done with iterator, destroy it
   multitext.destroyIterator(iterator);

   completedGeometry(topLevel, jsonArray, jsonTCArray, jsonMaterialRefs);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi text"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitEllipse(const IFMEEllipse& ellipse)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("ellipse"));

   const IFMEArc* boundary = ellipse.getBoundaryAsArc();
   if (boundary == nullptr)
   {
      // We require a boundary
      return FME_FAILURE;
   }
   // re-visit points of the ellipse
   FME_Status badNews = boundary->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("ellipse"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitFace(const IFMEFace& face)
{
   skipLastPointOnLine_ = true;

   logDebugMessage(std::string(kMsgVisiting) + std::string("Face"));

   const IFMEArea* area = face.getAsArea();
   if (area == nullptr)
   {
      // We require an area
      return FME_FAILURE;
   }
   
   // Let's deal with appearances, if we have any.
   // Note: we only look at the front, because CityJSON does not
   // have the ability to store back textures.
   FME_UInt32 frontAppRef(0);
   int cityJSONTexIndex(-1);
   int cityJSONMaterialIndex(-1);
   if (face.getAppearanceReference(frontAppRef, FME_TRUE) == FME_TRUE)
   {
      if (frontAppRef == 0 && parentAppearanceRef_ > 0)
      {
         frontAppRef = parentAppearanceRef_;
      }

      // Is this appearance a texture or a material?
      IFMEAppearance* app = fmeSession_->getLibrary()->getAppearanceCopy(frontAppRef);
      if (app) // if the appRef was "0" or "-1" we don't expect a reference
      {
         FME_UInt32 texRef(0);
         if (FME_TRUE == app->getTextureReference(texRef))
         {
            // We've got a texture.
            // One we've never seen before?
            auto refIndex = textureRefsToCJIndex_.find(texRef);
            if (refIndex == textureRefsToCJIndex_.end())
            {
               // Storing an increasing number means we'll get the right
               // index later.
               cityJSONTexIndex = textureRefsToCJIndex_.size();
               textureRefsToCJIndex_[texRef] = cityJSONTexIndex;
            }
            else
            {
               cityJSONTexIndex = refIndex->second;
            }
         }

         // Do we have enough non-default info here to mean we have a "material'?
         cityJSONMaterialIndex = getMaterialRefFromAppearance(app);

         fmeGeometryTools_->destroyAppearance(app); app = nullptr;
      }
   }

   // For children of mine, this is the texture index to use
   nextTextRef_= cityJSONTexIndex;

   bool topLevel = claimTopLevel("CompositeSurface");

   // re-visit the boundary
   auto jsonArray = json::array();
   json jsonTCArray = json::array();
   json jsonMaterialRefs = json::array();
   FME_Status badNews = area->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }
   takeWorkingBoundaries(jsonArray, jsonTCArray, jsonMaterialRefs);

   if (cityJSONMaterialIndex < 0)
   {
      jsonMaterialRefs.push_back(nullptr);
   }
   else
   {
      jsonMaterialRefs.push_back(cityJSONMaterialIndex);
   }

   // For a Face, we must convert this into 
   // a CompositeSurface, as CityJSON cannot store faces by themselves.
   // Adding one layer of "nesting"...
   auto jsonArray2 = json::array();
   jsonArray2.push_back(jsonArray);
   jsonArray = jsonArray2;
   auto jsonTCArray2 = json::array();
   jsonTCArray2.push_back(jsonTCArray);
   jsonTCArray = jsonTCArray2;

   //-- fetch the semantic surface type of the geometry
   // Check if the semantics type is allowed
   // this needs the cityjson type to be known, pass cityjson type to constructor
   // and allow string to start with a '+'
   if (face.hasName()) {
      IFMEString* type = fmeSession_->createString();
      face.getName(*type, nullptr);

      if (semanticTypeAllowed(type->data())) {
         json surfaceSemantics;
         surfaceSemantics["type"] = type->data();
         fmeSession_->destroyString(type);

         //-- fetch the semantic surface traits of the geometry
         IFMEStringArray* traitNames = fmeSession_->createStringArray();
         face.getTraitNames(*traitNames);
         logDebugMessage(std::to_string(traitNames->entries()));
         for (int i = 0; i < traitNames->entries(); i++) {
            std::string traitNameStr = traitNames->elementAt(i)->data();

            // filter cityjson specific traits
            if (traitNameStr.compare(0, 9, "cityjson_") != 0) {
               FME_AttributeType type = face.getTraitType(*traitNames->elementAt(i));
               logDebugMessage("Found traitName with value: " + traitNameStr + " and type: " + std::to_string(type));

               if (type == FME_ATTR_STRING || type == FME_ATTR_ENCODED_STRING) {
                  IFMEString *geometryTrait = fmeSession_->createString();
                  face.getTraitString(*traitNames->elementAt(i), *geometryTrait);
                  surfaceSemantics[traitNameStr] = geometryTrait->data();
                  fmeSession_->destroyString(geometryTrait);
               }
               else if (type == FME_ATTR_REAL64) {
                  FME_Real64 geometryTrait;
                  face.getTraitReal64(*traitNames->elementAt(i), geometryTrait);
                  surfaceSemantics[traitNameStr] = geometryTrait;
               }
               else if (type == FME_ATTR_INT64) {
                  FME_Int64 geometryTrait;
                  face.getTraitInt64(*traitNames->elementAt(i), geometryTrait);
                  surfaceSemantics[traitNameStr] = geometryTrait;
               }
               else if (type == FME_ATTR_BOOLEAN) {
                  FME_Boolean geometryTrait;
                  face.getTraitBoolean(*traitNames->elementAt(i), geometryTrait);
                  if (geometryTrait == FME_FALSE) {
                     surfaceSemantics[traitNameStr] = false;
                  }
                  else {
                     surfaceSemantics[traitNameStr] = true;
                  }
               }
               else {
                  logFile_->logMessageString(
                     ("Semantic Surface attribute type '" + std::to_string(type) + "' is not allowed.").c_str(),
                     FME_WARN);
               }
            }
         }

         //-- De-duplicate surface semantics and keep correct number of semantic to store in values
         //-- Take into account not only semantics type since type can be same with different attribute values
         //-- Can use == to compare two json objects, comparison works on nested values of objects
         //-- check if semantic surface description exists
         int surfaceIdx = -1;
         for (int i = 0; i < surfaces_.size(); i++) {
            if (surfaces_[i] == surfaceSemantics) {
               surfaceIdx = i;
            }
         }
         if (surfaceIdx != -1) { // store value for existing semantic surface
            semanticValues_.push_back(surfaceIdx);
         }
         else { // store new semantic surface and its value 
            surfaces_.push_back(surfaceSemantics);
            semanticValues_.push_back(surfaces_.size() - 1);
         }
         fmeSession_->destroyStringArray(traitNames);
      }
      else {
         semanticValues_.push_back(nullptr);
      }
   }
   else {
      semanticValues_.push_back(nullptr);
   }

   completedGeometry(topLevel, jsonArray, jsonTCArray, jsonMaterialRefs);

   skipLastPointOnLine_ = false;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitTriangleStrip(const IFMETriangleStrip& triangleStrip)
{
   skipLastPointOnLine_ = true;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("triangle strip"));

   // This is kind of taking a shortcut.  Many formats do not support triangle fan, so they convert
   // it first. Convert the IFMETriangleStrip to a mesh, then to IFMECompositeSurface
   IFMEMesh* mesh = fmeGeometryTools_->createTriangulatedMeshFromGeometry(triangleStrip);
   IFMECompositeSurface* geomCompositeSurface = mesh->getAsCompositeSurface();

   logDebugMessage(std::string(kMsgVisiting) + std::string("triangle strip as composite surface"));

   // re-visit the solid geometry
   FME_Status badNews = visitCompositeSurface(*geomCompositeSurface);
   // Done with the geomCompositeSurface and mesh
   geomCompositeSurface->destroy(); geomCompositeSurface = nullptr;
   mesh->destroy(); mesh = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("triangle strip"));

   skipLastPointOnLine_ = false;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitTriangleFan(const IFMETriangleFan& triangleFan)
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("triangle fan"));

   // This is kind of taking a shortcut.  Many formats do not support triangle fan, so they convert it
   // first. Convert the IFMETriangleFan to a mesh, then to IFMECompositeSurface
   IFMEMesh* mesh = fmeGeometryTools_->createTriangulatedMeshFromGeometry(triangleFan);
   IFMECompositeSurface* geomCompositeSurface = mesh->getAsCompositeSurface();

   logDebugMessage(std::string(kMsgVisiting) + std::string("triangle fan as composite surface"));

   // re-visit the solid geometry
   FME_Status badNews = visitCompositeSurface(*geomCompositeSurface);
   // Done with the geomCompositeSurface and mesh
   geomCompositeSurface->destroy(); geomCompositeSurface = nullptr;
   mesh->destroy(); mesh = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("triangle fan"));

   skipLastPointOnLine_ = false;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitBox(const IFMEBox& box)
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("box"));

   // This is kind of taking a shortcut.  Many formats do not support Box, so they convert it
   // first. Convert the IFMEBox to a IFMEBRepSolid
   const IFMEBRepSolid* brepSolid = box.getAsBRepSolid(); // we don't own this.

   logDebugMessage(std::string(kMsgVisiting) + std::string("box as brep solid"));

   // re-visit the solid geometry
   FME_Status badNews = visitBRepSolid(*brepSolid);
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("box"));

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitExtrusion(const IFMEExtrusion& extrusion)
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("extrusion"));

   // This is kind of taking a shortcut.  Many formats do not support Extrusion, so they convert it
   // first. Convert the IFMEExtrusion to a IFMEBRepSolid
   const IFMEBRepSolid* brepSolid = extrusion.getAsBRepSolid(); // we don't own this.

   logDebugMessage(std::string(kMsgVisiting) + std::string("extrusion as brep solid"));

   // re-visit the solid geometry
   FME_Status badNews = visitBRepSolid(*brepSolid);
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("extrusion"));

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitBRepSolid(const IFMEBRepSolid& brepSolid)
{
   // CityJSON must explicitly set texture references on each level of the
   // hierarchy, so we must resolve any inheritance that might exist.
   const FME_UInt32 oldParentAppearanceRef = updateParentAppearanceReference(brepSolid);

   skipLastPointOnLine_ = true; 

   solidSemanticValues_.clear();

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("Solid"));

   bool topLevel = claimTopLevel("Solid");

   // Get the outer surface
   logDebugMessage(std::string(kMsgVisiting) + std::string("outer surface"));
   const IFMESurface* outerSurface = brepSolid.getOuterSurface();
   if (outerSurface == nullptr)
   {
      // Need an outer surface
      return FME_FAILURE;
   }

   // re-visit the outer surface geometry
   FME_Status badNews = outerSurface->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }
   // We need to handle donut solids differently, as they will have
   // another level of hierarchy we need to drop.  CityJSON does not allow
   // nesting in the same way FME can.
   auto jsonArray = json::array();
   json jsonTCArray = json::array();
   json jsonMaterialRefs = json::array();

   // Let's get the list of surfaces that make up this solid:
   addWorkingBoundaries(jsonArray, jsonTCArray, jsonMaterialRefs);
   solidSemanticValues_.push_back(replaceSemanticValues(semanticValues_));

   // Create iterator to loop though all the inner surfaces
   IFMESurfaceIterator* iterator = brepSolid.getIterator();
   while (iterator->next())
   {
      // Get the next inner surface
      const IFMESurface* innerSurface = iterator->getPart();
      logDebugMessage(std::string(kMsgVisiting) + std::string("inner surface"));
      // re-visit the inner surface geometry
      badNews = innerSurface->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator before leaving
         brepSolid.destroyIterator(iterator);
         return FME_FAILURE;
      }

      // Let's get the list of surfaces that make up this solid:
      addWorkingBoundaries(jsonArray, jsonTCArray, jsonMaterialRefs);

      solidSemanticValues_.push_back(replaceSemanticValues(semanticValues_));
   }

   //-- store semantic surface information
   if (!surfaces_.empty()) {
     outputgeom_["semantics"]["surfaces"] = surfaces_;
     outputgeom_["semantics"]["values"] = solidSemanticValues_;
   }

   completedGeometry(topLevel, jsonArray, jsonTCArray, jsonMaterialRefs);

   // Done with the iterator
   brepSolid.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("boundary representation solid"));

   skipLastPointOnLine_ = false; 

   parentAppearanceRef_ = oldParentAppearanceRef;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCompositeSurfaceParts(
   const IFMECompositeSurface& compositeSurface, json& jsonArray, json& jsonTCArray, json& jsonMaterialRefs)
{
   skipLastPointOnLine_ = true; 

   IFMESurfaceIterator* iterator = compositeSurface.getIterator();
   while (iterator->next())
   {
      // Get the next surface
      const IFMESurface* surface = iterator->getPart();

      // Can't deal with multiple levels of nesting, so let's break that down.
      if (surface->canCastAs<const IFMECompositeSurface*>())
      {
         FME_Status badNews = visitCompositeSurfaceParts(*(surface->castAs<const IFMECompositeSurface*>()), jsonArray, jsonTCArray, jsonMaterialRefs);
         if (badNews) return FME_FAILURE;
      }
      else
      {
         logDebugMessage(std::string(kMsgVisiting) + std::string("surface"));

         // re-visit the surface geometry
         FME_Status badNews = surface->acceptGeometryVisitorConst(*this);
         if (badNews)
         {
            // Destroy iterator before leaving
            compositeSurface.destroyIterator(iterator);
            return FME_FAILURE;
         }
         addWorkingBoundaries_1Deep(jsonArray, jsonTCArray, jsonMaterialRefs);
      }
   }

   // Done with the iterator
   compositeSurface.destroyIterator(iterator);

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCompositeSurface(const IFMECompositeSurface& compositeSurface)
{
   // CityJSON must explicitly set texture references on each level of the
   // hierarchy, so we must resolve any inheritance that might exist.
   const FME_UInt32 oldParentAppearanceRef = updateParentAppearanceReference(compositeSurface);

   skipLastPointOnLine_ = true; 

   semanticValues_.clear();

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("CompositeSurface"));

   bool topLevel = claimTopLevel("CompositeSurface");

   // Create an iterator to loop through all the surfaces this multi surface contains
   auto jsonArray = json::array();
   json jsonTCArray = json::array();
   json jsonMaterialRefs = json::array();
   FME_Status badNews = visitCompositeSurfaceParts(compositeSurface, jsonArray, jsonTCArray, jsonMaterialRefs);
   if (badNews) return FME_FAILURE;

   //-- store semantic surface information
   if (!surfaces_.empty()) {
      outputgeom_["semantics"]["surfaces"] = surfaces_;
      outputgeom_["semantics"]["values"] = semanticValues_;
   }

   completedGeometry(topLevel, jsonArray, jsonTCArray, jsonMaterialRefs);

   skipLastPointOnLine_ = false; 

   parentAppearanceRef_ = oldParentAppearanceRef;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitRectangleFace(const IFMERectangleFace& rectangle)
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("rectangle face"));

   // This is kind of taking a shortcut.  Many formats do not support rectangle face, so they convert it
   // first. Convert the IFMERectangleFace to a IFMEFace
   IFMEFace* face = rectangle.getAsFaceCopy();

   logDebugMessage(std::string(kMsgVisiting) + std::string("rectangle face as face"));

   // re-visit the solid geometry
   FME_Status badNews = visitFace(*face);
   // Done with the face
   face->destroy(); face = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("rectangle face"));

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiSurface(const IFMEMultiSurface& multiSurface)
{
   // CityJSON must explicitly set texture references on each level of the
   // hierarchy, so we must resolve any inheritance that might exist.
   const FME_UInt32 oldParentAppearanceRef = updateParentAppearanceReference(multiSurface);

   skipLastPointOnLine_ = true; 

   semanticValues_.clear();

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi surface"));
      
   bool topLevel = claimTopLevel("MultiSurface");

   // Create an iterator to loop through all the surfaces this multi surface contains
   IFMESurfaceIterator* iterator = multiSurface.getIterator();
   auto jsonArray = json::array();
   json jsonTCArray = json::array();
   json jsonMaterialRefs = json::array();
   while (iterator->next())
   {
      // Get the next surface
      const IFMESurface* surface = iterator->getPart();

      logDebugMessage(std::string(kMsgVisiting) + std::string("surface"));

      // re-visit the surface geometry
      FME_Status badNews = surface->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator before leaving
         multiSurface.destroyIterator(iterator);
         return FME_FAILURE;
      }

      takeWorkingBoundaries_1Deep(jsonArray, jsonTCArray, jsonMaterialRefs);
   }

   //-- store semantic surface information
   if (!surfaces_.empty()) {
     outputgeom_["semantics"]["surfaces"] = surfaces_;
     outputgeom_["semantics"]["values"] = semanticValues_;
   }

   completedGeometry(topLevel, jsonArray, jsonTCArray, jsonMaterialRefs);

   // Done with the iterator
   multiSurface.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi surface"));

   skipLastPointOnLine_ = false; 

   parentAppearanceRef_ = oldParentAppearanceRef;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiSolid(const IFMEMultiSolid& multiSolid)
{
   // Use some shared code.
   return visitCompositeOrMultiSolid(multiSolid, "MultiSolid");
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCompositeSolid(const IFMECompositeSolid& compositeSolid)
{
   // Use some shared code.
   return visitCompositeOrMultiSolid(compositeSolid, "CompositeSolid");
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCSGSolid(const IFMECSGSolid& csgSolid)
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("CSG solid"));

   // This is kind of taking a shortcut.  Many formats do not support CSGSolid, so they convert it first.
   // Convert the IFMECSGSolid to either an IFMEMultiSolid, IFMEBRepSolid, or IFMENull.
   const IFMEGeometry* geomCSGSolid = csgSolid.evaluateCSG();

   logDebugMessage(std::string(kMsgVisiting) + std::string("CSG solid component"));

   // re-visit the solid geometry
   FME_Status badNews = geomCSGSolid->acceptGeometryVisitorConst(*this);
   // Done with the geomCSGSolid
   geomCSGSolid->destroy(); geomCSGSolid = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("CSG solid"));
   
   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMesh( const IFMEMesh& mesh )
{
   skipLastPointOnLine_ = true; 

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("mesh"));

   // This is kind of taking a shortcut.  Many formats do not support Mesh, so they convert it
   // first. Convert the IFMEMesh to either an IFMECompositeSurface.
   IFMECompositeSurface* geomCompositeSurface = mesh.getAsCompositeSurface();

   logDebugMessage(std::string(kMsgVisiting) + std::string("mesh as composite surface"));

   // re-visit the composite surface geometry
   FME_Status badNews = visitCompositeSurface(*geomCompositeSurface);
   // Done with the geomCompositeSurface
   geomCompositeSurface->destroy(); geomCompositeSurface = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("mesh"));

   skipLastPointOnLine_ = false; 

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitNull(const IFMENull& fmeNull)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("null"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitRaster(const IFMERaster& raster)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("raster"));

   return FME_SUCCESS;
}

//=====================================================================
FME_Status FMECityJSONGeometryVisitor::visitPointCloud(const IFMEPointCloud& pointCloud) 
{
   //TODO: PR:26877 This method should be written.
   logDebugMessage(std::string(kMsgVisiting) + std::string("pointCloud"));
   return FME_SUCCESS;
}

//=====================================================================
FME_Status FMECityJSONGeometryVisitor::visitFeatureTable(const IFMEFeatureTable& featureTable)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("featureTable"));
   return FME_SUCCESS;
}

//=====================================================================
FME_Status FMECityJSONGeometryVisitor::visitVoxelGrid(const IFMEVoxelGrid& /*voxelGrid*/)
{
   //TODO: [FMEENGINE-63998] This method should be written.
   logDebugMessage(std::string(kMsgVisiting) + std::string("voxel"));
   return FME_SUCCESS;
}

//=====================================================================
FME_Status FMECityJSONGeometryVisitor::visitPipe(const IFMEPipe& /*pipe*/)
{
   // TODO: [FMEENGINE-63998] This method should be written.
   logDebugMessage(std::string(kMsgVisiting) + std::string("pipe"));
   return FME_SUCCESS;
}


