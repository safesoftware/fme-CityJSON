#ifndef FME_CITY_JSON_GEOMETRY_VISITOR_H
#define FME_CITY_JSON_GEOMETRY_VISITOR_H
/*=============================================================================

   Name     : fmecityjsongeometryvisitor.h

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

#include <igeometrytools.h>
#include <igeometryvisitor.h>
#include "fmecityjsonpriv.h"
#include <isolid.h>

#include <vector>
#include <optional>

#include <nlohmann/json.hpp>
using json = nlohmann::json;
using VertexPool = std::vector<std::tuple<double, double, double>>;
using TexCoordPool = std::vector<std::string>;

// I use a tuple here, so it can be easily used as a key to a std::map, for uniqueness.
using MaterialInfo = std::tuple<
   std::optional<std::string>, // matName
   std::optional<FME_Real64>, // ambient intensity
   std::optional<FME_Real64>, std::optional<FME_Real64>, std::optional<FME_Real64>, // Diffuse r,g,b
   std::optional<FME_Real64>, std::optional<FME_Real64>, std::optional<FME_Real64>, // Emissive r,g,b
   std::optional<FME_Real64>, std::optional<FME_Real64>, std::optional<FME_Real64>, // Specular r,g,b
   std::optional<FME_Real64>, // shininess
   std::optional<FME_Real64> // transparency
                             // isSmooth ?
>;


class IFMEVoxelGrid;
class IFMEPipe;
class IFMESession;

// This class fills outputgeoms_ with json geometry objects at the specified LOD
// NOTE: reset() must be called before the visitor visits any geometries
class FMECityJSONGeometryVisitor : public IFMEGeometryVisitorConst
{
public:

   //---------------------------------------------------------------------
   // Constructor.
   FMECityJSONGeometryVisitor(const IFMEGeometryTools* geomTools,
                              IFMESession* session,
                              bool remove_duplicates,
                              int important_digits,
                              std::map<FME_UInt32, int>& textureRefsToCJIndex,
                              std::map<MaterialInfo, int>& materialInfoToCJIndex);

   //---------------------------------------------------------------------
   // Destructor.
   ~FMECityJSONGeometryVisitor();

   //---------------------------------------------------------------------
   // Return version.
   FME_Int32 getVersion() const override
   {
      return kGeometryVisitorVersion; // This constant defined in the parent's header file
   }

   //----------------------------------------------------------------------
   // reset the variables outputgeoms_ and lodAsDouble_ so that a new geometry
   // can be written. Must be called before a geometry is visited.
   void reset(json& outputgeoms, const double lodAsDouble)
   {
      outputgeoms_ = &outputgeoms;
      lodAsDouble_ = lodAsDouble;
   }

   //---------------------------------------------------------------------
   // Visitor logs values of the passed in IFMEAggregate geometry object.
   FME_Status visitAggregate(const IFMEAggregate& aggregate) override;

   //---------------------------------------------------------------------
   // Visitor logs values of the passed in IFMEPoint geometry object.
   FME_Status visitPoint(const IFMEPoint& point) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEMultiPoint geometry object.
   FME_Status visitMultiPoint(const IFMEMultiPoint& multipoint) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEArc geometry object.
   FME_Status visitArc(const IFMEArc& arc) override;

   //----------------------------------------------------------------------
   FME_Status visitOrientedArc(const IFMEOrientedArc& orientedArc) override;

   //----------------------------------------------------------------------
   FME_Status visitClothoid(const IFMEClothoid& clothoid) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMELine geometry object.
   FME_Status visitLine(const IFMELine& line) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEPath geometry object.
   FME_Status visitPath(const IFMEPath& path) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEMultiCurve geometry object.
   FME_Status visitMultiCurve(const IFMEMultiCurve& multicurve) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEMultiArea geometry object.
   FME_Status visitMultiArea(const IFMEMultiArea& multiarea) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEPolygon geometry object.
   FME_Status visitPolygon(const IFMEPolygon& polygon) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEDonut geometry object.
   FME_Status visitDonut(const IFMEDonut& donut) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEText geometry object.
   FME_Status visitText(const IFMEText& text) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEMultiText geometry object.
   FME_Status visitMultiText(const IFMEMultiText& multitext) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEEllipse geometry object.
   FME_Status visitEllipse(const IFMEEllipse& ellipse) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMENull geometry object.
   FME_Status visitNull(const IFMENull& fmeNull) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMERaster geometry object.
   FME_Status visitRaster(const IFMERaster& raster) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEFace geometry object.
   FME_Status visitFace(const IFMEFace& face) override;

   //---------------------------------------------------------------------
   // Visitor creates a string representing the values of the passed
   // in IFMETriangleStrip geometry object.  It then assigns the string to
   // the "geomString_" data member.
   FME_Status visitTriangleStrip(const IFMETriangleStrip& triangleStrip) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMETriangleFan geometry object.
   FME_Status visitTriangleFan(const IFMETriangleFan& triangleFan) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEBox geometry object.
   FME_Status visitBox(const IFMEBox& box) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEExtrusion geometry object.
   FME_Status visitExtrusion(const IFMEExtrusion& extrusion) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEBRepSolid geometry object.
   FME_Status visitBRepSolid(const IFMEBRepSolid& brepSolid) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMECompositeSurface geometry object.
   FME_Status visitCompositeSurface(const IFMECompositeSurface& compositeSurface) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMERectangleFace geometry object.
   FME_Status visitRectangleFace(const IFMERectangleFace& rectangle) override;

   //---------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEMultiSurface geometry object.
   FME_Status visitMultiSurface(const IFMEMultiSurface& multiSurface) override;

   //---------------------------------------------------------------------
   // Visitor clogs the values of the passed in IFMEMultiSolid geometry object.
   FME_Status visitMultiSolid(const IFMEMultiSolid& multiSolid) override;

   //----------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMECompositeSolid geometry object.
   FME_Status visitCompositeSolid(const IFMECompositeSolid& compositeSolid) override;

   //----------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMECSGSolid geometry object.
   // The string created does not represent a true IFMECSGSolid, instead it
   // represents the IFMEMultiSolid, IFMEBRepSolid, or IFMENull equivalent to it.
   FME_Status visitCSGSolid(const IFMECSGSolid& csgSolid) override;

   //----------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEMesh geometry object.
   FME_Status visitMesh(const IFMEMesh& mesh) override;

   //----------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEPointCloud geometry object.
   FME_Status visitPointCloud(const IFMEPointCloud& pointCloud) override;

   //----------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEFeatureTable geometry object.
   FME_Status visitFeatureTable(const IFMEFeatureTable& featureTable) override;

   //----------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEVoxelGrid geometry object.
   FME_Status visitVoxelGrid(const IFMEVoxelGrid& voxelGrid);

   //----------------------------------------------------------------------
   // Visitor logs the values of the passed in IFMEPipe geometry object.
   FME_Status visitPipe(const IFMEPipe& pipe);

   //----------------------------------------------------------------------
   // get the JSON object for the geometry (without the "lod")
   json getGeomJSON();
   json getTexCoordsJSON();

   //----------------------------------------------------------------------
   // get the array of vertices for the geometry
   const VertexPool& getGeomVertices();
   const TexCoordPool& getTextureCoords();

   //----------------------------------------------------------------------
   // get bounds of vertices for the geometry
   void getGeomBounds(std::optional<double>& minx,
                      std::optional<double>& miny,
                      std::optional<double>& minz,
                      std::optional<double>& maxx,
                      std::optional<double>& maxy,
                      std::optional<double>& maxz);

   //----------------------------------------------------------------------
   json getTemplateJSON();

   //----------------------------------------------------------------------
   // check if the surface semantics type is allowed for this CityObjectType
   bool semanticTypeAllowed(std::string trait);

   //----------------------------------------------------------------------
   // set the CityObjectType of the feature
   void setFeatureType(std::string type);

   //----------------------------------------------------------------------
   // replace array with null values to single null value
   json replaceSemanticValues(std::vector<json> semanticValues);

   //----------------------------------------------------------------------
private:

   //---------------------------------------------------------------
   // Copy constructor
   FMECityJSONGeometryVisitor(const FMECityJSONGeometryVisitor&);

   //---------------------------------------------------------------
   // Assignment operator
   FMECityJSONGeometryVisitor& operator=(const FMECityJSONGeometryVisitor&);

   //---------------------------------------------------------------------
   // We can't have nested composite surfaces, so we need to flatten
   // them down to one level of hierarchy.
   FME_Status visitCompositeSurfaceParts(const IFMECompositeSurface& compositeSurface,
                                         json& jsonArray,
                                         json& jsonTCArray,
                                         json& jsonMaterialRefs);

   //---------------------------------------------------------------------
   // The vertex is added to the vertex pool.  It will not add duplicates.
   // The index of the vertex in the pool is returned.
   unsigned long addVertex(const FMECoord3D& vertex);
   void acceptVertex(const std::string& vertex_string, VertexPool& output, bool updateBounds);
   unsigned long addTextureCoord(const FMECoord2D& texcoord);

   //---------------------------------------------------------------------
   int getMaterialRefFromAppearance(const IFMEAppearance* app);

   //---------------------------------------------------------------------
   // This allows easy access to turn on/off debug logging throughout this class.
   void logDebugMessage(const std::string& message)
   {
      // You could just comment this in/out to control messages.
      //logFile_->logMessageString(message.c_str());
   }
   //----------------------------------------------------------------------
   // get the JSON object for the boundary that is "in progress"  
   // likely from the last visit call.
   void takeWorkingBoundaries(json& jsonArray, json& jsonTCArray, json& jsonMaterialRefs);
   void takeWorkingBoundaries_1Deep(json& jsonArray, json& jsonTCArray, json& jsonMaterialRefs);
   void addWorkingBoundaries(json& jsonArray, json& jsonTCArray, json& jsonMaterialRefs);
   void addWorkingBoundaries_1Deep(json& jsonArray, json& jsonTCArray, json& jsonMaterialRefs);

   //----------------------------------------------------------------------
   // If we are the first in a hierarchy, let's put out "header" info about
   // our type, etc.  If someone's already started, we know we are just a sub-part
   // and we'll do nothing.  We return if we are the top level or not.
   bool claimTopLevel(const std::string& type);

   //----------------------------------------------------------------------
   // If we are the first in a hierarchy, let's put out "boundary" and "semantic" info.
   // If we know we are just a sub-part we'll store our info away for it to use later.
   void completedGeometry(bool topLevel,
                          const json& boundary,
                          const json& texCoords,
                          const json& materialRefs);

   //----------------------------------------------------------------------
   template <class T>
   FME_UInt32 updateParentAppearanceReference(const T& geom)
   {
      const FME_UInt32 result = parentAppearanceRef_;

      FME_UInt32 appearanceRef = 0;
      if (geom.getAppearanceReference(appearanceRef, FME_TRUE) && appearanceRef != 0)
      {
         parentAppearanceRef_ = appearanceRef;
      }

      return result;
   }

   //----------------------------------------------------------------------
   //
   template <class T>
   FME_Status visitCompositeOrMultiSolid(const T& compositeOrMultiSolid, const std::string& typeAsString)
   {
      // CityJSON must explicitly set texture references on each level of the
      // hierarchy, so we must resolve any inheritance that might exist.
      const FME_UInt32 oldParentAppearanceRef = updateParentAppearanceReference(compositeOrMultiSolid);

      skipLastPointOnLine_ = true; 

      multiSolidSemanticValues_.clear();

      logDebugMessage(std::string(kMsgStartVisiting) + typeAsString);

      bool topLevel = claimTopLevel(typeAsString);

      // Create an iterator to loop through all the solids this multi solid contains
      auto* iterator = compositeOrMultiSolid.getIterator();
      auto jsonArray = json::array();
      json jsonTCArray = json::array();
      json jsonMaterialRefs = json::array();
      while (iterator->next())
      {
         // Get the next solid.
         const IFMESolid* solid = iterator->getPart();

         logDebugMessage(std::string(kMsgVisiting) + std::string("solid"));

         // re-visit the solid geometry
         FME_Status badNews = solid->acceptGeometryVisitorConst(*this);
         if (badNews)
         {
            // Destroy iterator before leaving
            compositeOrMultiSolid.destroyIterator(iterator);
            return FME_FAILURE;
         }

         // Add the solid info to our boundaries.
         //
         // We need to handle composite solids differently, as they will have
         // another level of hierarchy we need to drop.  CityJSON does not allow
         // nesting in the same way FME can.
         if (solid->canCastAs<const IFMECompositeSolid*>())
         {
            addWorkingBoundaries_1Deep(jsonArray, jsonTCArray, jsonMaterialRefs);
            multiSolidSemanticValues_.insert(multiSolidSemanticValues_.end(), solidSemanticValues_.begin(), solidSemanticValues_.end());
         }
         else
         {
            // Just a regular single solid.
            addWorkingBoundaries(jsonArray, jsonTCArray, jsonMaterialRefs);
            multiSolidSemanticValues_.push_back(solidSemanticValues_);
         }
      }

      //-- store semantic surface information
      if (!surfaces_.empty())
      {
         outputgeom_["semantics"]["surfaces"] = surfaces_;
         outputgeom_["semantics"]["values"]   = multiSolidSemanticValues_;
      }

      completedGeometry(topLevel, jsonArray, jsonTCArray, jsonMaterialRefs);

      // Done with the iterator
      compositeOrMultiSolid.destroyIterator(iterator);

      logDebugMessage(std::string(kMsgEndVisiting) + typeAsString);

      skipLastPointOnLine_ = false; 

      parentAppearanceRef_ = oldParentAppearanceRef;

      return FME_SUCCESS;
   }



   // The fmeGeometryTools member stores a pointer to an IFMEGeometryTools
   // object that is used to create IFMEGeometries.
   const IFMEGeometryTools* fmeGeometryTools_;

   // The fmeSession_ member stores a pointer to an IFMESession object
   // which performs the services on the FME Objects.
   IFMESession* fmeSession_;
   IFMELogFile* logFile_;

   //---------- private data members

   json* outputgeoms_ = nullptr;
   double lodAsDouble_ = FME_NAN;

   std::string featureType_;
   json outputgeom_;
   json workingBoundary_;
   json workingTexCoords_;
   json workingMaterialRefs_;
   bool remove_duplicates_; 
   int important_digits_; 

   // Let's track things so we don't log so much.
   std::map<std::string, int> limitLogging_;

   // in CityJSON, the surfaces don't duplicate the last point on closed rings.
   bool skipLastPointOnLine_;

   //-- semantics of surfaces
   std::vector< json > surfaces_; //-- all the surfaces (which are json object; same ones are merged)
   std::vector< json > semanticValues_; //-- values for MultiSurfaces and CompositeSurfaces
   std::vector< json > solidSemanticValues_;
   std::vector< json > multiSolidSemanticValues_;
   //-- possible types; always possible to have '+MySemantics' with the '+'
   static const std::map< std::string, std::vector< std::string > > semancticsTypes_; 

   // Keeping track of textures in appearances
   std::map<FME_UInt32, int>& textureRefsToCJIndex_;

   // Keeping track of materials in appearances
   std::map<MaterialInfo, int>& materialInfoToCJIndex_;

   // Maps a vertex to a specific index in the vertex pool.
   std::unordered_map<std::string, unsigned long> vertexToIndex_;
   VertexPool vertices_;
   std::optional<double> minx_, miny_, minz_, maxx_, maxy_, maxz_;

   // Maps a texture coordinate to a specific index in the textCoord pool
   std::unordered_map<std::string, unsigned long> textureCoordToIndex_;
   TexCoordPool textureCoords_;
   // If we need texture coordinates from a parent object, set this
   // up high so the line down below knows which ref to use.
   int nextTextRef_;

   // Saved once so we don't need to make them over and over
   IFMEString* uCoordDesc_;
   IFMEString* vCoordDesc_;

   // CityJSON must explicitly set texture references on each level of the hierarchy, so we keep
   // track of the parent's appearance reference to be able to resolve appearance inheritance
   FME_UInt32 parentAppearanceRef_ = 0;

   // Keep track of whether this visitor is currently visiting a geometry definition
   // (a.k.a CityJSON template geometry). The template geometries and template vertices
   // must go to tempateGeoms_ and templateVertices_ instead of outputgeoms_ and vertices_
   // respectively.
   bool insideTemplateGeom_ = false;
   json templateGeoms_ = json::array();
   std::unordered_map<std::string, unsigned long> templateVertexToIndex_;
   VertexPool templateVertices_;
   std::unordered_map<FME_UInt32, std::size_t> gdReferenceToTemplateIndex_;
};

#endif
