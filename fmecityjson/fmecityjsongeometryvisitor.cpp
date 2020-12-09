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
      {"TransportationSquare", {
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
                                                       int important_digits)
   :
   fmeGeometryTools_(geomTools),
   fmeSession_(session),
   remove_duplicates_(remove_duplicates),
   important_digits_(important_digits)
{
   logFile_ = session->logFile();
}

//===========================================================================
// Destructor.
FMECityJSONGeometryVisitor::~FMECityJSONGeometryVisitor()
{
   //------------------------------------------------------------------------
   // Perform any necessary cleanup
   //------------------------------------------------------------------------
}

json FMECityJSONGeometryVisitor::getGeomJSON()
{
   return outputgeom_;
}

const VertexPool& FMECityJSONGeometryVisitor::getGeomVertices()
{
   return vertices_;
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
   std::string message("CityJSON Semantic Surface Type '");
   message += featureType_;
   message += "' is not one of the CityJSON types(https://www.cityjson.org/specs/#semantic-surface-object) or an Extension ('+').";
   logFile_->logMessageString(message.c_str(), FME_WARN);
   return false;
}

void FMECityJSONGeometryVisitor::setFeatureType(std::string type) {
  featureType_ = type;
}

void FMECityJSONGeometryVisitor::setGeomType(std::string geomtype) {
  geomType_ = geomtype;
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

json FMECityJSONGeometryVisitor::replaceEmptySurface(std::vector<json> semanticSurface) {
   // replace array with only null values with a single null value
   if (semanticSurface.size() > 0) {
      return semanticSurface;
   }
   return std::vector<json>{nullptr};
}

void FMECityJSONGeometryVisitor::reset()
{
   outputgeom_.clear();
   surfaces_.clear();
   semanticValues_.clear();
   tmpRing_.clear();
   tmpFace_.clear();
   tmpMultiFace_.clear();
   tmpSolid_.clear();
   tmpMultiSolid_.clear();
}

// Converts a point into a string
std::string get_key(const FMECoord3D& vertex, int precision)
{
   char* buf = new char[200];
   std::stringstream ss;
   ss << "%." << precision << "f "
      << "%." << precision << "f "
      << "%." << precision << "f";
   std::sprintf(buf, ss.str().c_str(), vertex.x, vertex.y, vertex.z);
   std::string r(buf);
   return r;
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

void FMECityJSONGeometryVisitor::acceptVertex(const std::string& vertex_string)
{
   // Sadly, we need to calculate our bounds from the stringified vertex we
   // are using.  So back to float!
   std::vector<std::string> ls;
   tokenize(vertex_string, ls);
   double x = std::stod(ls[0]);
   double y = std::stod(ls[1]);
   double z = std::stod(ls[2]);

   if (!minx_ || x < minx_) minx_ = x;
   if (!maxx_ || x > maxx_) maxx_ = x;
   if (!miny_ || y < miny_) miny_ = y;
   if (!maxy_ || y > maxy_) maxy_ = y;
   if (!std::isnan(z)) // have z
   {
      if (!minz_ || z < minz_) minz_ = z;
      if (!maxz_ || z > maxz_) maxz_ = z;
   }

   vertices_.push_back({x, y, z});
}

// This will make sure we don't add any vertex twice.
unsigned long FMECityJSONGeometryVisitor::addVertex(const FMECoord3D& vertex)
{
   // This is the vertex, as a string, which we'll use.
   std::string vertex_string = get_key(vertex, important_digits_);

   // This will be the index in the vertex pool if it is new
   unsigned long index(vertices_.size());

   // A little more bookkeeping if we want to optimize the vertex pool
   // and not have duplicates.
   if (remove_duplicates_)
   {
      // Have we encountered this vertex before?
      auto [entry, vertexAdded] = vertexToIndex_.try_emplace(vertex_string, index);
      if (!vertexAdded) // We already have this in our vertex pool
      {
         index = entry->second;
      }
      else // We haven't seen this before, so insert it into the pool.
      {
         acceptVertex(vertex_string);

         // index is already set correctly.
      }
   }
   else
   {
      acceptVertex(vertex_string);

      // index is already set correctly.
   }
   return index;
}




//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitAggregate(const IFMEAggregate& aggregate)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("aggregate"));

   FME_Status badNews;

   // Create iterator to get all geometries
   //-- We can't write this out as an aggregate in CityJSON, so
   //   rather than fail, let's do the best we can - which is write
   //   them out as if they came in separately.
   IFMEGeometryIterator* iterator = aggregate.getIterator();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("aggregate geometry"));

      // re-visit aggregate geometries
      if (nullptr != iterator->getPart())
      {
         badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
         if (badNews)
         {
            // Destroy the iterator
            aggregate.destroyIterator(iterator);
            return FME_FAILURE;
         }
      }
   }
   // Done with the iterator, destroy it
   aggregate.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("aggregate"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPoint(const IFMEPoint& point)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("point"));

   tmpRing_.clear();
   unsigned long index = addVertex({point.getX(), point.getY(), point.getZ()});
   tmpRing_.push_back(index);

   outputgeom_ = json::object();
   outputgeom_["type"] = "MultiPoint";
   outputgeom_["boundaries"] = tmpRing_;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiPoint(const IFMEMultiPoint& multipoint)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi point"));

   FME_Status badNews;

   tmpRing_.clear();
   
   IFMEPointIterator* iterator = multipoint.getIterator();
   while (iterator->next())
   {
      const IFMEPoint* point = iterator->getPart();
      unsigned long index    = addVertex({point->getX(), point->getY(), point->getZ()});
      tmpRing_.push_back(index);
   }
   // We are done with the iterator, so destroy it
   multipoint.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi point"));

   outputgeom_ = json::object();
   outputgeom_["type"] = "MultiPoint";
   outputgeom_["boundaries"] = tmpRing_;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitArc(const IFMEArc& arc)
{
   // Determine what kind of arc we are visiting and visit it.
   switch (arc.optimalArcTypeRetrieval())
   {
      case FME_ARC_BY_CENTER_POINT:
      case FME_ARC_BY_CENTER_POINT_START_END:
      {
         return visitArcBCP(arc);
         break;
      }
      case FME_ARC_BY_BULGE:
      {
         return visitArcBB(arc);
         break;
      }
      case FME_ARC_BY_3_POINTS:
      {
         return visitArcB3P(arc);
         break;
      }
      default:
      {
         // If we come to this point we don't know which arc to read, so let's return fail.
         return FME_FAILURE;
      }
   }
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
FME_Status FMECityJSONGeometryVisitor::visitArcBCP(const IFMEArc& arc)
{
   FME_Status badNews;
   IFMEPoint* point = fmeGeometryTools_->createPoint();

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("arc by center point"));

   // Get the center point
   logDebugMessage(std::string(kMsgVisiting) + std::string("center point"));

   badNews = arc.getCenterPoint(*point);
   if (badNews)
   {
      point->destroy(); point = nullptr;
      return FME_FAILURE;
   }

   // re-visit points of the arc

   // re-visit the center point geometry
   badNews = point->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      // There was an error in visiting point.
      point->destroy(); point = nullptr;
      return FME_FAILURE;
   }

   if (arc.hasExplicitEndpoints())
   {
      // Get the start point
      logDebugMessage(std::string(kMsgVisiting) + std::string("start point"));

      badNews = arc.getStartPoint(*point);
      if (badNews)
      {
         point->destroy(); point = nullptr;
         return FME_FAILURE;
      }
      // re-visit the start point geometry
      badNews = point->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         point->destroy(); point = nullptr;
         return FME_FAILURE;
      }

      // Get the end point
      logDebugMessage(std::string(kMsgVisiting) + std::string("end point"));

      badNews = arc.getEndPoint(*point);
      if (badNews)
      {
         point->destroy(); point = nullptr;
         return FME_FAILURE;
      }
      // re-visit the end point geometry
      badNews = point->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         point->destroy(); point = nullptr;
         return FME_FAILURE;
      }
   }

   point->destroy(); point = nullptr;

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("arc by center point"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitArcBB(const IFMEArc& arc)
{
   FME_Status badNews;
   IFMEPoint* point = fmeGeometryTools_->createPoint();

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("arc by bulge"));

   // Get the start point
   logDebugMessage(std::string(kMsgVisiting) + std::string("start point"));

   badNews = arc.getStartPoint(*point);
   if (badNews)
   {
      point->destroy(); point = nullptr;
      return FME_FAILURE;
   }

   // re-visit points of the arc

   // re-visit the start point geometry
   badNews = point->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      point->destroy(); point = nullptr;
      return FME_FAILURE;
   }

   // Get the end point
   logDebugMessage(std::string(kMsgVisiting) + std::string("end point"));

   badNews = arc.getEndPoint(*point);
   if (badNews)
   {
      point->destroy(); point = nullptr;
      return FME_FAILURE;
   }
   // re-visit the end point geometry
   badNews = point->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      point->destroy(); point = nullptr;
      return FME_FAILURE;
   }

   // Get the bulge
   logDebugMessage(std::string(kMsgVisiting) + std::string("bulge"));

   FME_Real64 bulge;
   badNews = arc.getBulge(bulge);
   if (badNews)
   {
      // Bulge is not available because the arc is elliptical or close
      point->destroy(); point = nullptr;
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("arc by bulge"));

   point->destroy(); point = nullptr;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitArcB3P(const IFMEArc& arc)
{
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("arc by 3 points"));

   FME_Status badNews;

   IFMEPoint* startPoint = fmeGeometryTools_->createPoint();
   IFMEPoint* midPoint = fmeGeometryTools_->createPoint();
   IFMEPoint* endPoint = fmeGeometryTools_->createPoint();

   badNews = arc.getPropertiesAs3Points(*startPoint, *midPoint, *endPoint);
   if (badNews)
   {
      startPoint->destroy(); startPoint = nullptr;
      midPoint->destroy(); midPoint = nullptr;
      endPoint->destroy(); endPoint = nullptr;
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgVisiting) + std::string("start point"));

   // re-visit points of the arc

   // re-visit the start point geometry
   badNews = startPoint->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      startPoint->destroy(); startPoint = nullptr;
      midPoint->destroy(); midPoint = nullptr;
      endPoint->destroy(); endPoint = nullptr;
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgVisiting) + std::string("mid point"));

   // re-visit the mid point geometry
   badNews = midPoint->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      startPoint->destroy(); startPoint = nullptr;
      midPoint->destroy(); midPoint = nullptr;
      endPoint->destroy(); endPoint = nullptr;
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgVisiting) + std::string("end point"));

   // re-visit the end point geometry
   badNews = endPoint->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      startPoint->destroy(); startPoint = nullptr;
      midPoint->destroy(); midPoint = nullptr;
      endPoint->destroy(); endPoint = nullptr;
      return FME_FAILURE;
   }

   startPoint->destroy(); startPoint = nullptr;
   midPoint->destroy(); midPoint = nullptr;
   endPoint->destroy(); endPoint = nullptr;

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("arc by bulge"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitLine(const IFMELine& line)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("line"));
   logDebugMessage(geomType_);

   tmpRing_.clear();
   //-- special case for line, otherwise needs to be called each time this 
   //-- function is called, ie for each surface of a solid for instance...
   if (geomType_ == "line") {
      for (int i = 0; i < line.numPoints(); i++) {
         FMECoord3D point;
         line.getPointAt3D(i, point);
         unsigned long index = addVertex(point);
         tmpRing_.push_back(index);
      }
      tmpFace_.clear();
      tmpFace_.push_back(tmpRing_);
      outputgeom_ = json::object();
      outputgeom_["type"] = "MultiLineString";
      outputgeom_["boundaries"] = tmpFace_;
   } 
   else {
      for (int i = 0; i < line.numPoints()-1; i++) {
         FMECoord3D point;
         line.getPointAt3D(i, point);
         unsigned long index = addVertex(point);
         tmpRing_.push_back(index);
      }
   }
   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPath(const IFMEPath& path)
{
   FME_Status badNews;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("path"));

   // Create iterator to get all segments in the path
   IFMESegmentIterator* iterator = path.getIterator();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("segment"));

      // There is a segment to the path to add, re-visit the segment
      badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy the iterator and exit
         path.destroyIterator(iterator);
         return FME_FAILURE;
      }
   }
   // We are done with the iterator, so destroy it
   path.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("path"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiCurve(const IFMEMultiCurve& multicurve)
{
   FME_Status badNews;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi curve"));

   // Create an iterator to get the curves
   IFMECurveIterator* iterator = multicurve.getIterator();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("curve"));

      // re-visit the next curve
      badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy the iterator and return fail
         multicurve.destroyIterator(iterator);
         return FME_FAILURE;
      }
   }
   // Done visiting curves, destroy iterator
   multicurve.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi curve"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiArea(const IFMEMultiArea& multiarea)
{
   FME_Status badNews;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi area"));

   // Create iterator to visit all areas
   IFMEAreaIterator* iterator = multiarea.getIterator();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("area"));

      // re-visit areas
      badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator
         multiarea.destroyIterator(iterator);
         return FME_FAILURE;
      }
   }
   // Done with iterator, destroy it
   multiarea.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi area"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPolygon(const IFMEPolygon& polygon)
{
   FME_Status badNews;

   logDebugMessage(std::string(kMsgVisiting) + std::string("polygon"));

   const IFMECurve* boundary = polygon.getBoundaryAsCurve();
   if (boundary == nullptr)
   {
      // We need a boundary, return fail
      return FME_FAILURE;
   }
   // re-visit polygon curve geometry
   badNews = boundary->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitDonut(const IFMEDonut& donut)
{
   tmpFace_.clear();   
   FME_Status badNews;
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("donut"));

   // Get the outer boundary
   logDebugMessage(std::string(kMsgVisiting) + std::string("outer boundary"));

   const IFMEArea* outerBoundary = donut.getOuterBoundaryAsSimpleArea();
   if (outerBoundary == nullptr)
   {
      // We require an outer boundary, return fail
      return FME_FAILURE;
   }
   // re-visit the outer boundary
   badNews = outerBoundary->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }
   tmpFace_.push_back(tmpRing_);
   // tmpRing_.clear();

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
      tmpFace_.push_back(tmpRing_);
      // tmpRing_.clear();

   }
   // Done with iterator, destroy it
   donut.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("donut"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitText(const IFMEText& text)
{
   FME_Status badNews;

   logDebugMessage(std::string(kMsgVisiting) + std::string("text"));

   // This IFMEGeometryVisitorConst subclass does not consume the geometry which accepts it
   IFMEPoint* point = text.getLocationAsPoint();
   // re-visit location
   badNews = point->acceptGeometryVisitorConst(*this);
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
   FME_Status badNews;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi text"));

   // Create iterator to get all text geometries
   IFMETextIterator* iterator = multitext.getIterator();
   while (iterator->next())
   {
      logDebugMessage(std::string(kMsgVisiting) + std::string("text"));

      // re-visit next text geometry
      badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         multitext.destroyIterator(iterator);
         return FME_FAILURE;
      }
   }
   // Done with iterator, destroy it
   multitext.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi text"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitEllipse(const IFMEEllipse& ellipse)
{
   FME_Status badNews;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("ellipse"));

   const IFMEArc* boundary = ellipse.getBoundaryAsArc();
   if (boundary == nullptr)
   {
      // We require a boundary
      return FME_FAILURE;
   }
   // re-visit points of the ellipse
   badNews = boundary->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }

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
//
FME_Status FMECityJSONGeometryVisitor::visitFace(const IFMEFace& face)
{
   tmpFace_.clear();
   FME_Status badNews;
   logDebugMessage(std::string(kMsgVisiting) + std::string("Face"));

   const IFMEArea* area = face.getAsArea();
   if (area == nullptr)
   {
      // We require an area
      return FME_FAILURE;
   }
   
   // re-visit the boundary
   badNews = area->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }
   if (tmpRing_.size() > 0) {
     tmpFace_.push_back(tmpRing_);
     // tmpRing_.clear();
   }

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

               if (type == FME_ATTR_STRING) {
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

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitTriangleStrip(const IFMETriangleStrip& triangleStrip)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("triangle strip"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitTriangleFan(const IFMETriangleFan& triangleFan)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("triangle fan"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitBox(const IFMEBox& box)
{
   logDebugMessage(std::string(kMsgVisiting) + std::string("box"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitExtrusion(const IFMEExtrusion& extrusion)
{
   FME_Status badNews;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("extrusion"));

   // This is kind of taking a shortcut.  Many formats do not support Extrusion, so they convert it
   // first. Convert the IFMEExtrusion to a IFMEBRepSolid
   const IFMEBRepSolid* brepSolid = extrusion.getAsBRepSolid();

   logDebugMessage(std::string(kMsgVisiting) + std::string("extrusion as brep solid"));

   // re-visit the solid geometry
   badNews = brepSolid->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("extrusion"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitBRepSolid(const IFMEBRepSolid& brepSolid)
{
   tmpSolid_.clear();
   solidSemanticValues_.clear();
   FME_Status badNews;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("Solid"));

   // Get the outer surface
   logDebugMessage(std::string(kMsgVisiting) + std::string("outer surface"));
   const IFMESurface* outerSurface = brepSolid.getOuterSurface();
   if (outerSurface == nullptr)
   {
      // Need an outer surface
      return FME_FAILURE;
   }
   // re-visit the outer surface geometry
   badNews = outerSurface->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }
   tmpSolid_.push_back(tmpMultiFace_);
   solidSemanticValues_.push_back(replaceSemanticValues(semanticValues_));
   // tmpMultiFace_.clear();


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
      tmpSolid_.push_back(tmpMultiFace_);
      solidSemanticValues_.push_back(replaceSemanticValues(semanticValues_));
      // tmpMultiFace_.clear();

   }

   outputgeom_ = json::object();
   outputgeom_["type"] = "Solid";
   outputgeom_["boundaries"] = tmpSolid_;

   //-- store semantic surface information
   if (!semanticValues_.empty()) {
     outputgeom_["semantics"]["surfaces"] = replaceEmptySurface(surfaces_);
     outputgeom_["semantics"]["values"] = solidSemanticValues_;
   }

   // Done with the iterator
   brepSolid.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("boundary representation solid"));

   return FME_SUCCESS;
}

FME_Status FMECityJSONGeometryVisitor::visitCompositeSurfaceParts(const IFMECompositeSurface& compositeSurface)
{
   FME_Status badNews;
   IFMESurfaceIterator* iterator = compositeSurface.getIterator();
   while (iterator->next())
   {
      // Get the next surface
      const IFMESurface* surface = iterator->getPart();

      // Can't deal with multiple levels of nesting, so let's break that down.
      if (surface->canCastAs<const IFMECompositeSurface*>())
      {
         badNews = visitCompositeSurfaceParts(*(surface->castAs<const IFMECompositeSurface*>()));
         if (badNews) return FME_FAILURE;
      }
      else
      {
         logDebugMessage(std::string(kMsgVisiting) + std::string("surface"));

         // re-visit the surface geometry
         badNews = surface->acceptGeometryVisitorConst(*this);
         if (badNews)
         {
            // Destroy iterator before leaving
            compositeSurface.destroyIterator(iterator);
            return FME_FAILURE;
         }
         if (tmpFace_.size() > 0)
         {
            tmpMultiFace_.push_back(tmpFace_);
         }
      }
   }

   // Done with the iterator
   compositeSurface.destroyIterator(iterator);

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCompositeSurface(const IFMECompositeSurface& compositeSurface)
{
   tmpMultiFace_.clear();
   semanticValues_.clear();

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("CompositeSurface"));
   // Create an iterator to loop through all the surfaces this multi surface contains
   FME_Status badNews = visitCompositeSurfaceParts(compositeSurface);
   if (badNews) return FME_FAILURE;

   outputgeom_ = json::object();
   outputgeom_["type"] = "CompositeSurface";
   outputgeom_["boundaries"] = tmpMultiFace_;

   //-- store semantic surface information
   if (!semanticValues_.empty()) {
      outputgeom_["semantics"]["surfaces"] = surfaces_;
      outputgeom_["semantics"]["values"] = semanticValues_;
   }

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitRectangleFace(const IFMERectangleFace& rectangle)
{
   FME_Status badNews;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("rectangle face"));

   // This is kind of taking a shortcut.  Many formats do not support rectangle face, so they convert it
   // first. Convert the IFMERectangleFace to a IFMEFace
   IFMEFace* face = rectangle.getAsFaceCopy();

   logDebugMessage(std::string(kMsgVisiting) + std::string("rectangle face as face"));

   // re-visit the solid geometry
   badNews = face->acceptGeometryVisitorConst(*this);
   // Done with the face
   face->destroy(); face = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("rectangle face"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiSurface(const IFMEMultiSurface& multiSurface)
{
   tmpMultiFace_.clear();
   semanticValues_.clear();
   FME_Status badNews;
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi surface"));
      
   // Create an iterator to loop through all the surfaces this multi surface contains
   IFMESurfaceIterator* iterator = multiSurface.getIterator();
   while (iterator->next())
   {
      // Get the next surface
      const IFMESurface* surface = iterator->getPart();

      logDebugMessage(std::string(kMsgVisiting) + std::string("surface"));

      // re-visit the surface geometry
      badNews = surface->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator before leaving
         multiSurface.destroyIterator(iterator);
         return FME_FAILURE;
      }
      if (tmpFace_.size() > 0) {
         tmpMultiFace_.push_back(tmpFace_);
      }
   }

   outputgeom_ = json::object();
   outputgeom_["type"] = "MultiSurface";
   outputgeom_["boundaries"] = tmpMultiFace_;

   //-- store semantic surface information
   if (!semanticValues_.empty()) {
     outputgeom_["semantics"]["surfaces"] = surfaces_;
     outputgeom_["semantics"]["values"] = semanticValues_;
   }

   // Done with the iterator
   multiSurface.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi surface"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiSolid(const IFMEMultiSolid& multiSolid)
{
   tmpMultiSolid_.clear();
   multiSolidSemanticValues_.clear();

   FME_Status badNews;
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("multi solid"));

   // Create an iterator to loop through all the solids this multi solid contains
   IFMESolidIterator* iterator = multiSolid.getIterator();
   while (iterator->next())
   {
      // Get the next solid.
      const IFMESolid* solid = iterator->getPart();

      logDebugMessage(std::string(kMsgVisiting) + std::string("solid"));

      // re-visit the solid geometry
      badNews = solid->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator before leaving
         multiSolid.destroyIterator(iterator);
         return FME_FAILURE;
      }
      if (tmpSolid_.size() > 0) {
        tmpMultiSolid_.push_back(tmpSolid_);
        multiSolidSemanticValues_.push_back(solidSemanticValues_);
      }
   }

   outputgeom_ = json::object();
   outputgeom_["type"] = "MultiSolid";
   outputgeom_["boundaries"] = tmpMultiSolid_;

   //-- store semantic surface information
   if (!semanticValues_.empty()) {
      outputgeom_["semantics"]["surfaces"] = replaceEmptySurface(surfaces_);
      outputgeom_["semantics"]["values"] = multiSolidSemanticValues_;
   }

   // Done with the iterator
   multiSolid.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi solid"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCompositeSolid(const IFMECompositeSolid& compositeSolid)
{
   tmpMultiSolid_.clear();
   multiSolidSemanticValues_.clear();

   FME_Status badNews;
   logDebugMessage(std::string(kMsgStartVisiting) + std::string("CompositeSolid"));

   // Create an iterator to loop through all the solids this multi solid contains
   IFMESimpleSolidIterator* iterator = compositeSolid.getIterator();
   while (iterator->next())
   {
      // Get the next solid.
      const IFMESimpleSolid* solid = iterator->getPart();

      logDebugMessage(std::string(kMsgVisiting) + std::string("solid"));

      // re-visit the solid geometry
      badNews = solid->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator before leaving
         compositeSolid.destroyIterator(iterator);
         return FME_FAILURE;
      }
      if (tmpSolid_.size() > 0) {
        tmpMultiSolid_.push_back(tmpSolid_);
        multiSolidSemanticValues_.push_back(solidSemanticValues_);
      }
   }

   outputgeom_ = json::object();
   outputgeom_["type"] = "CompositeSolid";
   outputgeom_["boundaries"] = tmpMultiSolid_;

   //-- store semantic surface information
   if (!semanticValues_.empty()) {
      outputgeom_["semantics"]["surfaces"] = replaceEmptySurface(surfaces_);
      outputgeom_["semantics"]["values"] = multiSolidSemanticValues_;
   }

   // Done with the iterator
   compositeSolid.destroyIterator(iterator);

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("multi solid"));

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCSGSolid(const IFMECSGSolid& csgSolid)
{
   FME_Status badNews;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("CSG solid"));

   // This is kind of taking a shortcut.  Many formats do not support CSGSolid, so they convert it first.
   // Convert the IFMECSGSolid to either an IFMEMultiSolid, IFMEBRepSolid, or IFMENull.
   const IFMEGeometry* geomCSGSolid = csgSolid.evaluateCSG();

   logDebugMessage(std::string(kMsgVisiting) + std::string("CSG solid component"));

   // re-visit the solid geometry
   badNews = geomCSGSolid->acceptGeometryVisitorConst(*this);
   // Done with the geomCSGSolid
   geomCSGSolid->destroy(); geomCSGSolid = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }


   logDebugMessage(std::string(kMsgEndVisiting) + std::string("CSG solid"));
   
   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMesh( const IFMEMesh& mesh )
{
   FME_Status badNews;

   logDebugMessage(std::string(kMsgStartVisiting) + std::string("mesh"));

   // This is kind of taking a shortcut.  Many formats do not support Mesh, so they convert it
   // first. Convert the IFMEMesh to either an IFMECompositeSurface.
   const IFMECompositeSurface* geomCompositeSurface = mesh.getAsCompositeSurface();

   logDebugMessage(std::string(kMsgVisiting) + std::string("mesh as composite surface"));

   // re-visit the composite surface geometry
   badNews = geomCompositeSurface->acceptGeometryVisitorConst(*this);
   // Done with the geomCompositeSurface
   geomCompositeSurface->destroy();
   geomCompositeSurface = nullptr;
   if (badNews)
   {
      return FME_FAILURE;
   }

   logDebugMessage(std::string(kMsgEndVisiting) + std::string("mesh"));

   return FME_SUCCESS;
}

//=====================================================================
FME_Status FMECityJSONGeometryVisitor::visitPointCloud(const IFMEPointCloud& pointCloud) 
{
   //TODO: PR:26877 This method should be written.
   return FME_SUCCESS;
}

//=====================================================================
FME_Status FMECityJSONGeometryVisitor::visitFeatureTable(const IFMEFeatureTable& featureTable)
{
   return FME_SUCCESS;
}

//=====================================================================
FME_Status FMECityJSONGeometryVisitor::visitVoxelGrid(const IFMEVoxelGrid& /*voxelGrid*/)
{
   //TODO: [FMEENGINE-63998] This method should be written.
   return FME_SUCCESS;
}


