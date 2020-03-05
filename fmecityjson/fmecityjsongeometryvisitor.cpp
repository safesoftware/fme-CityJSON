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
FMECityJSONGeometryVisitor::FMECityJSONGeometryVisitor(const IFMEGeometryTools* geomTools, IFMESession* session)
:
   fmeGeometryTools_(geomTools),
   fmeSession_(session)
{
   offset_ = 0;
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

std::vector< std::vector< double > > FMECityJSONGeometryVisitor::getGeomVertices()
{
   return vertices_;
}


void FMECityJSONGeometryVisitor::setVerticesOffset(long unsigned offset)
{
   offset_ = offset;
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
         // FMECityJSONWriter::gLogFile->logMessageString(("comparing traits[i]: " + traits[i] + " &
         // trait: " + trait).c_str());
         if (traits[i] == trait)
         {
            return true;
         }
      }
   }
   std::string message("CityJSON Semantic Surface Type '");
   message += featureType_;
   message += "' is not one of the CityJSON types(https://www.cityjson.org/specs/#semantic-surface-object) or an Extension ('+').";
   FMECityJSONWriter::gLogFile->logMessageString(message.c_str(), FME_WARN);
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
   vertices_.clear();
   surfaces_.clear();
   semanticValues_.clear();
   tmpRing_.clear();
   tmpFace_.clear();
   tmpMultiFace_.clear();
   tmpSolid_.clear();
   tmpMultiSolid_.clear();
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitAggregate(const IFMEAggregate& aggregate)
{
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("aggregate")).c_str());

   FME_Status badNews;

   // Create iterator to get all geometries
   //-- TODO: ban all aggregate from being written in CityJSON? That would simplify things, the 
   //-- user is in charge of giving us "valid" CityJSON geometries. 
   //-- I guess that's the spirit of FME, innit?
   IFMEGeometryIterator* iterator = aggregate.getIterator();
   while (iterator->next())
   {
      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("aggregate geometry")).c_str());

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

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("aggregate")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPoint(const IFMEPoint& point)
{
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("point")).c_str());

   tmpRing_.clear();
   std::vector<double> v;
   v.push_back(point.getX());
   v.push_back(point.getY());
   v.push_back(point.getZ());
   unsigned long a = vertices_.size();
   tmpRing_.push_back(a + offset_);
   vertices_.push_back(v);

   outputgeom_ = json::object();
   outputgeom_["type"] = "MultiPoint";
   outputgeom_["boundaries"] = tmpRing_;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiPoint(const IFMEMultiPoint& multipoint)
{
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("multi point")).c_str());

   FME_Status badNews;

   tmpRing_.clear();
   
   IFMEPointIterator* iterator = multipoint.getIterator();
   while (iterator->next())
   {
      const IFMEPoint* point = iterator->getPart();
      std::vector<double> v;
      v.push_back(point->getX());
      v.push_back(point->getY());
      v.push_back(point->getZ());
      unsigned long a = vertices_.size();
      tmpRing_.push_back(a + offset_);
      vertices_.push_back(v);
   }
   // We are done with the iterator, so destroy it
   multipoint.destroyIterator(iterator);

   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("multi point")).c_str());
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
   if (arc.optimalArcTypeRetrieval() == FME_ARC_BY_CENTER_POINT ||
       arc.optimalArcTypeRetrieval() == FME_ARC_BY_CENTER_POINT_START_END)
   {
      return visitArcBCP(arc);
   }
   else if (arc.optimalArcTypeRetrieval() == FME_ARC_BY_BULGE)
   {
      return visitArcBB(arc);
   }
   else if (arc.optimalArcTypeRetrieval() == FME_ARC_BY_3_POINTS)
   {
      return visitArcB3P(arc);
   }

   // If we come to this point we don't know which arc to read, so let's return fail.
   return FME_FAILURE;
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

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("arc by center point")).c_str());

   // Get the center point
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("center point")).c_str());

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
      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("start point")).c_str());

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
      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("end point")).c_str());

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

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("arc by center point")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitArcBB(const IFMEArc& arc)
{
   FME_Status badNews;
   IFMEPoint* point = fmeGeometryTools_->createPoint();

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("arc by bulge")).c_str());

   // Get the start point
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("start point")).c_str());

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
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("end point")).c_str());

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
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("bulge")).c_str());

   FME_Real64 bulge;
   badNews = arc.getBulge(bulge);
   if (badNews)
   {
      // Bulge is not available because the arc is elliptical or close
      point->destroy(); point = nullptr;
      return FME_FAILURE;
   }

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("arc by bulge")).c_str());

   point->destroy(); point = nullptr;

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitArcB3P(const IFMEArc& arc)
{
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("arc by 3 points")).c_str());

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

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("start point")).c_str());

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

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("mid point")).c_str());

   // re-visit the mid point geometry
   badNews = midPoint->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      startPoint->destroy(); startPoint = nullptr;
      midPoint->destroy(); midPoint = nullptr;
      endPoint->destroy(); endPoint = nullptr;
      return FME_FAILURE;
   }

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("end point")).c_str());

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

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("arc by bulge")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitLine(const IFMELine& line)
{
   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("line")).c_str(), FME_WARN);
   // FMECityJSONWriter::gLogFile->logMessageString(geomType_.c_str());

   tmpRing_.clear();
   //-- special case for line, otherwise needs to be called each time this 
   //-- function is called, ie for each suface of a solid for instance...
   if (geomType_ == "line") {
      for (int i = 0; i < line.numPoints(); i++) {
         FMECoord3D coords;
         line.getPointAt3D(i, coords);
         std::vector< double > v;
         v.push_back(coords.x);
         v.push_back(coords.y);
         v.push_back(coords.z);
         unsigned long a = vertices_.size();
         tmpRing_.push_back(a + offset_);
         vertices_.push_back(v);
      }
      tmpFace_.clear();
      tmpFace_.push_back(tmpRing_);
      outputgeom_ = json::object();
      outputgeom_["type"] = "MultiLineString";
      outputgeom_["boundaries"] = tmpFace_;
   } 
   else {
      for (int i = 0; i < line.numPoints()-1; i++) {
         FMECoord3D coords;
         line.getPointAt3D(i, coords);
         std::vector< double > v;
         v.push_back(coords.x);
         v.push_back(coords.y);
         v.push_back(coords.z);
         unsigned long a = vertices_.size();
         tmpRing_.push_back(a + offset_);
         vertices_.push_back(v);
      }
   }
   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPath(const IFMEPath& path)
{
   FME_Status badNews;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("path")).c_str());

   // Create iterator to get all segments in the path
   IFMESegmentIterator* iterator = path.getIterator();
   while (iterator->next())
   {
      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("segment")).c_str());

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

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("path")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiCurve(const IFMEMultiCurve& multicurve)
{
   FME_Status badNews;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("multi curve")).c_str());

   // Create an iterator to get the curves
   IFMECurveIterator* iterator = multicurve.getIterator();
   while (iterator->next())
   {
      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("curve")).c_str());

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

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("multi curve")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiArea(const IFMEMultiArea& multiarea)
{
   FME_Status badNews;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("multi area")).c_str());

   // Create iterator to visit all areas
   IFMEAreaIterator* iterator = multiarea.getIterator();
   while (iterator->next())
   {
      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("area")).c_str());

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

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("multi area")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitPolygon(const IFMEPolygon& polygon)
{
   FME_Status badNews;

   //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("polygon")).c_str());

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
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("donut")).c_str());

   // Get the outer boundary
   //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("outer boundary")).c_str());

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
   //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("inner boundary")).c_str());

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

   //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("donut")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitText(const IFMEText& text)
{
   FME_Status badNews;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("text")).c_str());

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

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("multi text")).c_str());

   // Create iterator to get all text geometries
   IFMETextIterator* iterator = multitext.getIterator();
   while (iterator->next())
   {
      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("text")).c_str());

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

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("multi text")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitEllipse(const IFMEEllipse& ellipse)
{
   FME_Status badNews;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("ellipse")).c_str());

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
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("null")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitRaster(const IFMERaster& raster)
{
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("raster")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitFace(const IFMEFace& face)
{
   tmpFace_.clear();
   FME_Status badNews;
   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("Face")).c_str());

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
         //FMECityJSONWriter::gLogFile->logMessageString(std::to_string(traitNames->entries()).c_str());
         for (int i = 0; i < traitNames->entries(); i++) {
            //const IFMEString* traitName = traitNames->elementAt(i);
            std::string traitNameStr = traitNames->elementAt(i)->data();

            // filter cityjson specific traits
            if (traitNameStr.compare(0, 9, "cityjson_") != 0) {
               FME_AttributeType type = face.getTraitType(*traitNames->elementAt(i));
               //FMECityJSONWriter::gLogFile->logMessageString(("Found traitName with value: " + traitNameStr + " and type: " + std::to_string(type)).c_str());

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
                  FMECityJSONWriter::gLogFile->logMessageString(
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
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("triangle strip")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitTriangleFan(const IFMETriangleFan& triangleFan)
{
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("triangle fan")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitBox(const IFMEBox& box)
{
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("box")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitExtrusion(const IFMEExtrusion& extrusion)
{

   FMECityJSONWriter::gLogFile->logMessageString("IFMEExtrusion geometry is not allowed, use a GeometryCoercer.", FME_WARN);
   return FME_FAILURE;

   // FME_Status badNews;

   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("extrusion")).c_str());

   // // Get the base of the extrusion
   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("face")).c_str());

   // const IFMEFace* extrusionBase = extrusion.getBaseAsFace();
   // if (extrusionBase == nullptr)
   // {
   //    // A base is needed
   //    return FME_FAILURE;
   // }
   // // re-visit the base
   // badNews = extrusionBase->acceptGeometryVisitorConst(*this);
   // if (badNews)
   // {
   //    return FME_FAILURE;
   // }

   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("extrusion")).c_str());

   // return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitBRepSolid(const IFMEBRepSolid& brepSolid)
{
   tmpSolid_.clear();
   solidSemanticValues_.clear();
   FME_Status badNews;

   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("Solid")).c_str());

   // Get the outer surface
   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("outer surface")).c_str());
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
      // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("inner surface")).c_str());
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

   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("boundary representation solid")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCompositeSurface(const IFMECompositeSurface& compositeSurface)
{
   tmpMultiFace_.clear();
   semanticValues_.clear();

   FME_Status badNews;
   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("CompositeSurface")).c_str());
   // Create an iterator to loop through all the surfaces this multi surface contains
   IFMESurfaceIterator* iterator = compositeSurface.getIterator();
   while (iterator->next())
   {
      // Get the next surface
      const IFMESurface* surface = iterator->getPart();

      //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("surface")).c_str());

      // re-visit the surface geometry
      badNews = surface->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator before leaving
         compositeSurface.destroyIterator(iterator);
         return FME_FAILURE;
      }
      if (tmpFace_.size() > 0) {
        tmpMultiFace_.push_back(tmpFace_);
      }
   }

   outputgeom_ = json::object();
   outputgeom_["type"] = "CompositeSurface";
   outputgeom_["boundaries"] = tmpMultiFace_;

   //-- store semantic surface information
   if (!semanticValues_.empty()) {
      outputgeom_["semantics"]["surfaces"] = surfaces_;
      outputgeom_["semantics"]["values"] = semanticValues_;
   }

   // Done with the iterator
   compositeSurface.destroyIterator(iterator);

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitRectangleFace(const IFMERectangleFace& rectangle)
{
   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("rectangle face")).c_str());
   FMECityJSONWriter::gLogFile->logMessageString((std::string("rectangle face not supported")).c_str());
   return FME_FAILURE;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiSurface(const IFMEMultiSurface& multiSurface)
{
   tmpMultiFace_.clear();
   semanticValues_.clear();
   FME_Status badNews;
   //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("multi surface")).c_str());
      
   // Create an iterator to loop through all the surfaces this multi surface contains
   IFMESurfaceIterator* iterator = multiSurface.getIterator();
   while (iterator->next())
   {
      // Get the next surface
      const IFMESurface* surface = iterator->getPart();

      //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("surface")).c_str());

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

   //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("multi surface")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiSolid(const IFMEMultiSolid& multiSolid)
{
   tmpMultiSolid_.clear();
   multiSolidSemanticValues_.clear();

   FME_Status badNews;
   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("multi solid")).c_str());

   // Create an iterator to loop through all the solids this multi solid contains
   IFMESolidIterator* iterator = multiSolid.getIterator();
   while (iterator->next())
   {
      // Get the next solid.
      const IFMESolid* solid = iterator->getPart();

      // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("solid")).c_str());

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

   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("multi solid")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCompositeSolid(const IFMECompositeSolid& compositeSolid)
{
   tmpMultiSolid_.clear();
   multiSolidSemanticValues_.clear();

   FME_Status badNews;
   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("CompositeSolid")).c_str());

   // Create an iterator to loop through all the solids this multi solid contains
   IFMESimpleSolidIterator* iterator = compositeSolid.getIterator();
   while (iterator->next())
   {
      // Get the next solid.
      const IFMESimpleSolid* solid = iterator->getPart();

      // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("solid")).c_str());

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

   // FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("multi solid")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCSGSolid(const IFMECSGSolid& csgSolid)
{
   FME_Status badNews;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("CSG solid")).c_str());

   // This is kind of taking a shortcut.  Many formats do not support CSGSolid, so they convert it first.
   // Convert the IFMECSGSolid to either an IFMEMultiSolid, IFMEBRepSolid, or IFMENull.
   const IFMEGeometry* geomCSGSolid = csgSolid.evaluateCSG();

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("CSG solid component")).c_str());

   // re-visit the solid geometry
   badNews = geomCSGSolid->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      // Destroy the geomCSGSolid before returning
      geomCSGSolid->destroy(); geomCSGSolid = nullptr;
      return FME_FAILURE;
   }

   // Done with the geomCSGSolid
   geomCSGSolid->destroy(); geomCSGSolid = nullptr;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("CSG solid")).c_str());
   
   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMesh( const IFMEMesh& mesh )
{
   //TODO: 22012 This method should be written.
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
