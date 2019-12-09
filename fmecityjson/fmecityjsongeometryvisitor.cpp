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
   return onegeom_;
}

std::vector< std::vector< double > > FMECityJSONGeometryVisitor::getGeomVertices()
{
   return vertices_;
}

void FMECityJSONGeometryVisitor::reset()
{
   onegeom_.clear();
   vertices_.clear();
}


void FMECityJSONGeometryVisitor::setVerticesOffset(long unsigned offset)
{
   offset_ = offset;
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

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiPoint(const IFMEMultiPoint& multipoint)
{
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("multi point")).c_str());

   FME_Status badNews;

   // Create a point iterator
   IFMEPointIterator* iterator = multipoint.getIterator();
   while (iterator->next())
   {
      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("point")).c_str());

      // re-visit the next geometry
      badNews = iterator->getPart()->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         multipoint.destroyIterator(iterator);
         return FME_FAILURE;
      }
   }
   // We are done with the iterator, so destroy it
   multipoint.destroyIterator(iterator);

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("multi point")).c_str());

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
   //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("line")).c_str());
   for (int i = 0; i < line.numPoints()-1; i++) {
      FMECoord3D coords;
      line.getPointAt3D(i, coords);
      std::vector< double > v;
      v.push_back(coords.x);
      v.push_back(coords.y);
      v.push_back(coords.z);
      unsigned long a = vertices_.size();
      face_.push_back(a + offset_);
      vertices_.push_back(v);
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
   FME_Status badNews;
   //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("donut")).c_str());

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
   surface_.push_back(face_);
   face_.clear();

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
      surface_.push_back(face_);
      face_.clear();

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
   FME_Status badNews;

   //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("face")).c_str());

   const IFMEArea* area = face.getAsArea();
   if (area == nullptr)
   {
      // We require an area
      return FME_FAILURE;
   }
   //IFMEString *semType = FMECityJSONWriter::getSemanticSurfaceType(face);
   //FMECityJSONWriter::gLogFile->logMessageString(semType->data(), FME_WARN);
   
   // re-visit the boundary
   badNews = area->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      surface_.clear();
      return FME_FAILURE;
   }
   if (face_.size() > 0) {
     surface_.push_back(face_);
     face_.clear();
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
   FME_Status badNews;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("extrusion")).c_str());

   // Get the base of the extrusion
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("face")).c_str());

   const IFMEFace* extrusionBase = extrusion.getBaseAsFace();
   if (extrusionBase == nullptr)
   {
      // A base is needed
      return FME_FAILURE;
   }
   // re-visit the base
   badNews = extrusionBase->acceptGeometryVisitorConst(*this);
   if (badNews)
   {
      return FME_FAILURE;
   }

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("extrusion")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitBRepSolid(const IFMEBRepSolid& brepSolid)
{
   FME_Status badNews;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("boundary representation solid")).c_str());

   // Get the outer surface
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("outer surface")).c_str());

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

   // Create iterator to loop though all the inner surfaces
   IFMESurfaceIterator* iterator = brepSolid.getIterator();
   while (iterator->next())
   {
      // Get the next inner surface
      const IFMESurface* innerSurface = iterator->getPart();
     
      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("inner surface")).c_str());

      // re-visit the inner surface geometry
      badNews = innerSurface->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator before leaving
         brepSolid.destroyIterator(iterator);
         return FME_FAILURE;
      }
   }

   // Done with the iterator
   brepSolid.destroyIterator(iterator);

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("boundary representation solid")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCompositeSurface(const IFMECompositeSurface& compositeSurface)
{
   FME_Status badNews;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("composite surface")).c_str());

   // Create an iterator to loop through all the surfaces this composite contains
   IFMESurfaceIterator* iterator = compositeSurface.getIterator();
   while (iterator->next())
   {
      // Get the next surface
      const IFMESurface* surface = iterator->getPart();

      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("surface")).c_str());

      // re-visit the surface geometry
      badNews = surface->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy the iterator before leaving
         compositeSurface.destroyIterator(iterator);
         return FME_FAILURE;
      }
   }

   // Done with the iterator
   compositeSurface.destroyIterator(iterator);

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("composite surface")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitRectangleFace(const IFMERectangleFace& rectangle)
{
   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("rectangle face")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiSurface(const IFMEMultiSurface& multiSurface)
{
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
         face_.clear();
         surface_.clear();
         multisurface_.clear();
         return FME_FAILURE;
      }
      if (surface_.size() > 0) {
        multisurface_.push_back(surface_);
        surface_.clear();
      }
   }

   onegeom_ = json::object();
   onegeom_["type"] = "MultiSurface";
   onegeom_["boundaries"] = multisurface_;

   // Done with the iterator
   multiSurface.destroyIterator(iterator);
   face_.clear();
   surface_.clear();
   multisurface_.clear();

   //FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("multi surface")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitMultiSolid(const IFMEMultiSolid& multiSolid)
{
   FME_Status badNews;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("multi solid")).c_str());

   // Create an iterator to loop through all the solids this multi solid contains
   IFMESolidIterator* iterator = multiSolid.getIterator();
   while (iterator->next())
   {
      // Get the next solid.
      const IFMESolid* solid = iterator->getPart();

      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("solid")).c_str());

      // re-visit the solid geometry
      badNews = solid->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator before leaving
         multiSolid.destroyIterator(iterator);
         return FME_FAILURE;
      }
   }

   // Done with the iterator
   multiSolid.destroyIterator(iterator);

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("multi solid")).c_str());

   return FME_SUCCESS;
}

//=====================================================================
//
FME_Status FMECityJSONGeometryVisitor::visitCompositeSolid(const IFMECompositeSolid& compositeSolid)
{
   FME_Status badNews;

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgStartVisiting) + std::string("composite solid")).c_str());

   // Create an iterator to loop through all the simple solids this composite solid contains
   IFMESimpleSolidIterator* iterator = compositeSolid.getIterator();
   while (iterator->next())
   {
      // Get next simple solid
      const IFMESimpleSolid* simpleSolid = iterator->getPart();

      FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("simple solid")).c_str());

      // re-visit the simple solid geometry
      badNews = simpleSolid->acceptGeometryVisitorConst(*this);
      if (badNews)
      {
         // Destroy iterator before leaving
         compositeSolid.destroyIterator(iterator);
         return FME_FAILURE;
      }
   }

   // Done with the iterator
   compositeSolid.destroyIterator(iterator);

   FMECityJSONWriter::gLogFile->logMessageString((std::string(kMsgEndVisiting) + std::string("composite solid")).c_str());

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
