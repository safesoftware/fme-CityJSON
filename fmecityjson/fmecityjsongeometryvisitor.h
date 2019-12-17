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

#include <vector>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class IFMESession;

// This returns a string that contains the geometry of a feature passed in.
class FMECityJSONGeometryVisitor : public IFMEGeometryVisitorConst
{
public:

   //---------------------------------------------------------------------
   // Constructor.
   FMECityJSONGeometryVisitor(const IFMEGeometryTools* geomTools, IFMESession* session);

   //---------------------------------------------------------------------
   // Destructor.
   ~FMECityJSONGeometryVisitor();

   //---------------------------------------------------------------------
   // Return version.
   FME_Int32 getVersion() const override
   {
      return kGeometryVisitorVersion; // This constant defined in the parent's header file
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
   FME_Status visitOrientedArc(const IFMEOrientedArc & orientedArc) override;

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
   // get the JSON object for the geometry (without the "lod")
   json getGeomJSON();

   //----------------------------------------------------------------------
   // get the array of vertices for the geometry
   std::vector< std::vector< double > > getGeomVertices();

   //----------------------------------------------------------------------
   // get the array of semantics for the geometry
   json getSemantics();

   //----------------------------------------------------------------------
   // set an offset for the indices used by the geometry, since in CityJSON
   // all the indices are global
   void setVerticesOffset(long unsigned offset);

   //----------------------------------------------------------------------
   void setOutputGeometryType(int level);

   //----------------------------------------------------------------------
   // reset the variables vertices_ and onegeom_ so that a new geometry
   // can be written
   void reset();


private:

   //---------------------------------------------------------------
   // Copy constructor
   FMECityJSONGeometryVisitor (const FMECityJSONGeometryVisitor&);

   //---------------------------------------------------------------
   // Assignment operator
   FMECityJSONGeometryVisitor& operator=(const FMECityJSONGeometryVisitor&);


   //---------------------------------------------------------------------
   // The IFMEArc geometry object passed in here is an arc by center point.
   // This function logs the values of that IFMEArc.
   FME_Status visitArcBCP(const IFMEArc& arc);

   //---------------------------------------------------------------------
   // The IFMEArc geometry object passed in here is an arc by bulge.
   // This function logs the values of that IFMEArc.
   FME_Status visitArcBB(const IFMEArc& arc);

   //---------------------------------------------------------------------
   // The IFMEArc geometry object passed in here is an arc by 3 points.
   // This function logs the values of that IFMEArc.
   FME_Status visitArcB3P(const IFMEArc& arc);


   // The fmeGeometryTools member stores a pointer to an IFMEGeometryTools
   // object that is used to create IFMEGeometries.
   const IFMEGeometryTools* fmeGeometryTools_;

   // The fmeSession_ member stores a pointer to an IFMESession object
   // which performs the services on the FME Objects.
   IFMESession* fmeSession_;

   //---------- private data members

   long unsigned offset_;

   std::vector< std::vector< double > > vertices_;
   
   json outputgeom_;

   json semantics_;
   
   std::vector< unsigned long > tmpRing_;                                                                   //-- level 1
   std::vector< std::vector< unsigned long> > tmpFace_;                                                     //-- level 2
   std::vector< std::vector< std::vector< unsigned long > > > tmpMultiFace_;                                //-- level 3
   std::vector< std::vector< std::vector< std::vector< unsigned long > > > > tmpSolid_;                     //-- level 4
   std::vector< std::vector< std::vector< std::vector< std::vector< unsigned long> > > > > tmpMultiSolid_;  //-- level 5

};

#endif
