/*=============================================================================

   Name     : FMECityJSONWriter.cpp

   System   : FME Plug-in SDK

   Language : C++

   Purpose  : IFMEWriter method implementations

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
#include "fmecityjsonwriter.h"
#include "fmecityjsonpriv.h"
#include "fmecityjsongeometryvisitor.h"
#include "Point3.h"


#include <ifeature.h>
#include <ifeatvec.h>
#include <igeometry.h>
#include <igeometrytools.h>
#include <iface.h>
#include <ilogfile.h>
#include <isession.h>
#include <fmemap.h>
#include <vector>

#include <icompositesurface.h>
#include <isoliditerator.h>
#include <isurfaceiterator.h>
#include <imultisurface.h>
#include <imultiarea.h>
#include <inull.h>
#include <igeometryiterator.h>

#include <typeinfo>

// These are initialized externally when a writer object is created so all
// methods in this file can assume they are ready to use.
IFMELogFile* FMECityJSONWriter::gLogFile = nullptr;
IFMEMappingFile* FMECityJSONWriter::gMappingFile = nullptr;
IFMECoordSysManager* FMECityJSONWriter::gCoordSysMan = nullptr;
extern IFMESession* gFMESession;

// TODO: These should probably be populated from the shipped schema files, not hardcoded.
//       Maybe "Metadata" is added to this list because it is specific to the FME reader/writer
const std::vector<std::string> FMECityJSONWriter::cityjsonTypes_ = std::vector<std::string>(
   {
      "Building", 
      "BuildingPart",
      "BuildingInstallation",
      "Bridge",
      "BridgePart",
      "BridgeInstallation",
      "CityObjectGroup",
      "CityFurniture",
      "GenericCityObject",
      "LandUse",
      "Metadata",
      "PlantCover",
      "Railway",
      "Road",
      "SolitaryVegetationObject",
      "TINRelief",
      "TransportationSquare",
      "Tunnel",
      "TunnelPart",
      "TunnelInstallation",
      "WaterBody"
   }
);

//===========================================================================
// Constructor
FMECityJSONWriter::FMECityJSONWriter(const char* writerTypeName, const char* writerKeyword)
:
   writerTypeName_(writerTypeName),
   writerKeyword_(writerKeyword),
   dataset_(""),
   fmeGeometryTools_(nullptr),
   visitor_(nullptr),
   schemaFeatures_(nullptr)
{
}

//===========================================================================
// Destructor
FMECityJSONWriter::~FMECityJSONWriter()
{
   close();
}

//===========================================================================
// Open
FME_Status FMECityJSONWriter::open(const char* datasetName, const IFMEStringArray& parameters)
{
   gLogFile->logMessageString("Thank you for using CityJSON, the better encoding for the CityGML data model.");


   // get the .fmf parameters
   IFMEString* pv = gFMESession->createString();
   gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(), kSrcCompress, *pv);
   std::string s1 = pv->data();
   if (s1.compare("Yes") == 0)
      compress_ = true;
   else 
      compress_ = false;
   
   gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(), kSrcImportantDigits, *pv);
   important_digits_ = std::stoi(pv->data());

   //-- remove duplicate vertices?
   gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(), kSrcRemoveDuplicates, *pv);
   s1 = pv->data();
   remove_duplicates_ = false;
   if (s1.compare("Yes") == 0)
   {
    remove_duplicates_ = true;
   }
   gFMESession->destroyString(pv);

   // Perform setup steps before opening file for writing

   // Get geometry tools
   fmeGeometryTools_ = gFMESession->getGeometryTools();

   // Create visitor to visit feature geometries
   visitor_ = new FMECityJSONGeometryVisitor(fmeGeometryTools_, gFMESession);

   dataset_ = datasetName;

   // -----------------------------------------------------------------------
   // Add additional setup here
   // -----------------------------------------------------------------------
   
   // Log an opening writer message
   std::string msgOpeningWriter = kMsgOpeningWriter + dataset_;
   gLogFile->logMessageString(msgOpeningWriter.c_str());

   schemaFeatures_ = gFMESession->createFeatureVector();

   // Fetch all the schema features and add the DEF lines.
   fetchSchemaFeatures();

   // Write the schema information to the file. In this template,
   // since we are not writing to a file we will log the schema information
   // instead.

   for (FME_UInt32 i = 0; i < schemaFeatures_->entries(); i++)
   {
      IFMEFeature* schemaFeature = (*schemaFeatures_)(i);
      // gLogFile->logMessageString(schemaFeature->getFeatureType(), FME_WARN );
    
      IFMEStringArray* allatt = gFMESession->createStringArray();
      schemaFeature->getAllAttributeNames(*allatt);
      std::map<std::string, std::string> sa;
      for (FME_UInt32 i = 0; i < allatt->entries(); i++)
      {
         const char* t = allatt->elementAt(i)->data();
         IFMEString* s1 = gFMESession->createString();
         schemaFeature->getAttribute(t, *s1);
         std::string t2(s1->data());
         sa[std::string(t)] = t2;
         gFMESession->destroyString(s1);
      }
      std::string st(schemaFeature->getFeatureType());
      attrToWrite_[st] = sa;
      gLogFile->logFeature(*schemaFeature, FME_INFORM, 20);
      gFMESession->destroyStringArray(allatt);
   }



   // -----------------------------------------------------------------------
   // Open the dataset here
   // e.g. outputFile_.open(dataset_.c_str(), ios::out|ios::trunc);
   outputFile_.open(dataset_.c_str(), std::ios::out | std::ios::trunc );
   // Check that the file exists.
   if (!outputFile_.good())
   {
      // TODO: Should log a message
      return FME_FAILURE;
   }
   outputJSON_["type"] = "CityJSON";
   outputJSON_["version"] = "1.0";
   // outputJSON_["metadata"] = "is awesome";
   outputJSON_["CityObjects"] = json::object();
   outputJSON_["vertices"] = json::array();
   // -----------------------------------------------------------------------

   return FME_SUCCESS;
}

//===========================================================================
// Abort
FME_Status FMECityJSONWriter::abort()
{
   // -----------------------------------------------------------------------
   // Add any special actions to shut down a writer not finished writing
   // data. For example, if your format requires a footer at the end of a
   // file, write it here.
   // -----------------------------------------------------------------------

   close();
   return FME_SUCCESS;
}

//===========================================================================
// Close
FME_Status FMECityJSONWriter::close()
{
   // -----------------------------------------------------------------------
   // Perform any closing operations / cleanup here; e.g. close opened files
   // -----------------------------------------------------------------------
   // gLogFile->logMessageString("close() !!!", FME_WARN);

   
   if (vertices_.empty() == false)
   {
      outputJSON_["vertices"] = vertices_;
      //-- remove duplicates (and potentially compress/quantize the file)
      if (remove_duplicates_ == true)
      {
        duplicate_vertices();
        vertices_.clear();
      }
      else {
        outputJSON_["vertices"] = vertices_;
      }
      //-- write to the file
      outputFile_ << outputJSON_ << std::endl;
      // Log that the writer is done
      gLogFile->logMessageString((kMsgClosingWriter + dataset_).c_str());
   }
      

   // Delete the visitor
   if (visitor_)
   {
      delete visitor_;
   }
   visitor_ = nullptr;

   if (schemaFeatures_)
   {
      schemaFeatures_->clearAndDestroy();
   }
   schemaFeatures_ = nullptr;
   
   // close the file
   outputFile_.close();

   return FME_SUCCESS;
}

//===========================================================================
// Write
FME_Status FMECityJSONWriter::write(const IFMEFeature& feature)
{
   // Log the feature
   // gLogFile->logFeature(feature);

   // -----------------------------------------------------------------------
   // The feature type and the attributes can be extracted from the feature
   // at this point.
   // -----------------------------------------------------------------------

   // -----------------------------------------------------------------------
   // Perform your write operations here
   // -----------------------------------------------------------------------
   
   //-- check if the type is one of the allowed CityJSON type,
   //-- or an Extension (eg '+Shed')
   //-- FAILURE if not one of these
   std::string ft(feature.getFeatureType());
   auto it = std::find(cityjsonTypes_.begin(), cityjsonTypes_.end(), ft); 
   if (it == cityjsonTypes_.end()) 
   {
      if (ft[0] != '+')
      {
         gLogFile->logMessageString("CityJSON feature is not one of the CityJSON types (https://www.cityjson.org/specs/#cityjson-object) or an Extension ('+').", FME_WARN );
         return FME_FAILURE;
      }
   }

   // Handle Metadata features specially.
   // For now, we are basically ignoring it.
   if (ft == "Metadata")
   {
      IFMEString* rs = gFMESession->createString();
      // Look for whatever metadata is interesting
      feature.getAttribute("referenceSystem", *rs);
      // Use metadata as required
      // outputJSON_["metadata"] = "is awesome";
      gFMESession->destroyString(rs); rs = nullptr;

      return FME_SUCCESS;
   }

   //-- write fid for CityObject
   //-- FAILURE if not one of these
   IFMEString* s1 = gFMESession->createString();
   if (feature.getAttribute("fid", *s1) == FME_FALSE) 
   {
      gLogFile->logMessageString("CityJSON features must have an attribute named 'fid' to uniquely identify them.", FME_WARN );
      return FME_FAILURE;
   }
   
   gLogFile->logMessageString(*s1);
   // gLogFile->logMessageString(*s1, FME_WARN);

   outputJSON_["CityObjects"][s1->data()] = json::object();
   outputJSON_["CityObjects"][s1->data()]["type"] = ft;

   IFMEStringArray* allatt = gFMESession->createStringArray();
   outputJSON_["CityObjects"][s1->data()]["attributes"] = json::object();
   
   feature.getAllAttributeNames(*allatt);
   // feature.getSequencedAttributeList(*allatt);
   for (FME_UInt32 i = 0; i < allatt->entries(); i++)
   {
      const char* t = allatt->elementAt(i)->data();
      std::string ts(t);
      // gLogFile->logMessageString(ts.c_str());
      IFMEString* value = gFMESession->createString();
      feature.getAttribute(t, *value);
      FME_AttributeType ftype = feature.getAttributeType(t);
      // gLogFile->logMessageString(ts.c_str(), FME_WARN);
      if ( (ts != "fid") &&
           (ts != "cityjson_parents") &&  
           (ts != "cityjson_children") 
         )  
      {

         auto it = attrToWrite_[ft].find(ts);
         if (it == attrToWrite_[ft].end())
         {
            // gLogFile->logMessageString("not found");
            continue;
         }
         std::string wtype = it->second;


      //-- STRING & char & char(188) writing -----   
         if ( (wtype == "string") || (wtype == "char") || (wtype.substr(0, 4) == "char") )
         {
            if ( (ftype == FME_ATTR_INT8)   ||
                 (ftype == FME_ATTR_INT16)  ||                                  
                 (ftype == FME_ATTR_INT32)  ||                                  
                 (ftype == FME_ATTR_INT64)  ||                                  
                 (ftype == FME_ATTR_UINT8)  ||                                  
                 (ftype == FME_ATTR_UINT16) ||                                  
                 (ftype == FME_ATTR_UINT32) ||                                  
                 (ftype == FME_ATTR_UINT64) ||
                 (ftype == FME_ATTR_REAL32) ||
                 (ftype == FME_ATTR_REAL64) ||
                 (ftype == FME_ATTR_REAL80) ) 
            {
               outputJSON_["CityObjects"][s1->data()]["attributes"][t] = value->data();
            } 
            else if ( (ftype == FME_ATTR_STRING) || 
                      (ftype == FME_ATTR_ENCODED_STRING) 
                      ) {
               outputJSON_["CityObjects"][s1->data()]["attributes"][t] = value->data();
            }
            else if (ftype == FME_ATTR_BOOLEAN) {
               FME_Boolean b;
               if (feature.getBooleanAttribute(t, b) == FME_TRUE) {
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = "true";
               }
               else {
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = "false";
               }
            }
            else {
               std::stringstream ss;
               ss << "Attribute value type '" << wtype << "' is not allowed. Not written.";
               gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
            }
         }
      //-- INTEGER writing -----
         else if ( (wtype == "int16")  || 
                   (wtype == "int32")  ||
                   (wtype == "int64")  ||
                   (wtype == "uint8")  ||
                   (wtype == "uint16") ||
                   (wtype == "uint32") ||
                   (wtype == "uint64") ) 
         {
            if ( (ftype == FME_ATTR_INT8)   ||
                 (ftype == FME_ATTR_INT16)  ||                                  
                 (ftype == FME_ATTR_INT32)  ||                                  
                 (ftype == FME_ATTR_INT64)  ||                                  
                 (ftype == FME_ATTR_UINT8)  ||                                  
                 (ftype == FME_ATTR_UINT16) ||                                  
                 (ftype == FME_ATTR_UINT32) ||                                  
                 (ftype == FME_ATTR_UINT64) ||
                 (ftype == FME_ATTR_REAL32) ||
                 (ftype == FME_ATTR_REAL64) ||
                 (ftype == FME_ATTR_REAL80) ) 
            {
               long tmp = std::stol(value->data());
               outputJSON_["CityObjects"][s1->data()]["attributes"][t] = tmp;
            } 
            else if ( (ftype == FME_ATTR_STRING) || 
                      (ftype == FME_ATTR_ENCODED_STRING) ) 
            {
               try {
                  long tmp = std::stol(value->data());
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = tmp;
               }
               catch (const std::invalid_argument& ia) {
                  std::stringstream ss;
                  ss << "Attribute '" << ts << "' cannot be converted to integer, writing string.";
                  gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = value->data();
               }

            }
            else if (ftype == FME_ATTR_BOOLEAN) {
               FME_Boolean b;
               if (feature.getBooleanAttribute(t, b) == FME_TRUE) {
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = 1;
               }
               else {
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = 0;
               }
            }
            else {
               std::stringstream ss;
               ss << "Attribute value type '" << wtype << "' is not allowed. Not written.";
               gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
            }            
         }
      //-- FLOAT/DOUBLE/NUMBER writing -----
         else if ( (wtype == "number")  || 
                   (wtype == "real32")  ||
                   (wtype == "real64")  )
         {
            if ( (ftype == FME_ATTR_INT8)   ||
                 (ftype == FME_ATTR_INT16)  ||                                  
                 (ftype == FME_ATTR_INT32)  ||                                  
                 (ftype == FME_ATTR_INT64)  ||                                  
                 (ftype == FME_ATTR_UINT8)  ||                                  
                 (ftype == FME_ATTR_UINT16) ||                                  
                 (ftype == FME_ATTR_UINT32) ||                                  
                 (ftype == FME_ATTR_UINT64) ||
                 (ftype == FME_ATTR_REAL32) ||
                 (ftype == FME_ATTR_REAL64) ||
                 (ftype == FME_ATTR_REAL80) ) 
            {
               double tmp = std::stod(value->data());
               outputJSON_["CityObjects"][s1->data()]["attributes"][t] = tmp;
            } 
            else if ( (ftype == FME_ATTR_STRING) || 
                      (ftype == FME_ATTR_ENCODED_STRING) ) 
            {
               try {
                  double tmp = std::stod(value->data());
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = tmp;
               }
               catch (const std::invalid_argument& ia) {
                  std::stringstream ss;
                  ss << "Attribute '" << ts << "' cannot be converted to integer, writing string.";
                  gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = value->data();
               }

            }
            else if (ftype == FME_ATTR_BOOLEAN) {
               FME_Boolean b;
               if (feature.getBooleanAttribute(t, b) == FME_TRUE) {
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = 1.0;
               }
               else {
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = 0.0;
               }
            }
            else {
               std::stringstream ss;
               ss << "Attribute '" << wtype << "' is not allowed. Not written.";
               gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
            }            
         }
      //-- BOOLEAN writing -----
         else if (wtype == "logical") 
         {
            if ( (ftype == FME_ATTR_INT8)   ||
                 (ftype == FME_ATTR_INT16)  ||                                  
                 (ftype == FME_ATTR_INT32)  ||                                  
                 (ftype == FME_ATTR_INT64)  ||                                  
                 (ftype == FME_ATTR_UINT8)  ||                                  
                 (ftype == FME_ATTR_UINT16) ||                                  
                 (ftype == FME_ATTR_UINT32) ||                                  
                 (ftype == FME_ATTR_UINT64) ||
                 (ftype == FME_ATTR_REAL32) ||
                 (ftype == FME_ATTR_REAL64) ||
                 (ftype == FME_ATTR_REAL80) ) 
            {
               int tmp = std::stoi(value->data());
               if (tmp == 1)
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = true;
               else if (tmp == 0)
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = false;
               else
               {
                  std::stringstream ss;
                  ss << "Attribute '" << ts << "' cannot be converted to Boolean, writing string.";
                  gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = value->data();
               }

            } 
            else if (ftype == FME_ATTR_BOOLEAN) {
               FME_Boolean b;
               if (feature.getBooleanAttribute(t, b) == FME_TRUE) {
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = true;
               }
               else {
                  outputJSON_["CityObjects"][s1->data()]["attributes"][t] = false;
               }
            }
            else {
               std::stringstream ss;
               ss << "Attribute '" << ts << "' cannot be converted to Boolean. Not written.";
               gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
            }            
         }         
      //-- DATE/DATETIME writing -----
         else if ( (wtype == "date") || (wtype == "datetime") ) 
         {
            outputJSON_["CityObjects"][s1->data()]["attributes"][t] = value->data();
         }
      //-- OTHERS (TODO: not sure if they exist?)             
         else 
         {
            std::stringstream ss;
            ss << "Attribute value type '" << wtype << "' is not allowed. Not written.";
            gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
         }
      }
      gFMESession->destroyString(value);
   }
   gFMESession->destroyStringArray(allatt);
   // gLogFile->logMessageString("Done with attributes", FME_WARN);
   
   //-- cityjson_children
   IFMEStringArray* childrenValues = gFMESession->createStringArray();
   feature.getListAttribute("cityjson_children", *childrenValues);
   if (childrenValues->entries() > 0)
   outputJSON_["CityObjects"][s1->data()]["children"] = json::array();   
   for (FME_UInt32 i = 0; i < childrenValues->entries(); i++) {
      // TODO : test if children and parents are written as string
      outputJSON_["CityObjects"][s1->data()]["children"].push_back(childrenValues->elementAt(i)->data());
   }
   gFMESession->destroyStringArray(childrenValues);

   //-- cityjson_parents
   IFMEStringArray* parentValues = gFMESession->createStringArray();
   feature.getListAttribute("cityjson_parents", *parentValues);
   if (parentValues->entries() > 0)
   outputJSON_["CityObjects"][s1->data()]["parents"] = json::array();   
   for (FME_UInt32 i = 0; i < parentValues->entries(); i++) {
      outputJSON_["CityObjects"][s1->data()]["parents"].push_back(parentValues->elementAt(i)->data());
   }
   gFMESession->destroyStringArray(parentValues);

//-- GEOMETRIES -----

   //-- extract the geometries from the feature
   const IFMEGeometry* geometry = (const_cast<IFMEFeature&>(feature)).getGeometry();


   //-- do no process geometry if none, this is allowed in CityJSON
   //-- a CO without geometry still has to have an empty array "geomtry": []
   outputJSON_["CityObjects"][s1->data()]["geometry"] = json::array();
   FME_Boolean isgeomnull = geometry->canCastAs<IFMENull*>();
   if (isgeomnull == false)
   {

      //-- update the offset (for writing vertices in the global list of CityJSON)
      //-- in the visitor.
      (visitor_)->setVerticesOffset(vertices_.size());

      FME_Status badNews = geometry->acceptGeometryVisitorConst(*visitor_);
      if (badNews) {
         // There was an error in writing the geometry
         gLogFile->logMessageString(kMsgWriteError);
         return FME_FAILURE;
      }

      //-- fetch the LoD of the geometry
      IFMEString* slod = gFMESession->createString();
      slod->set("cityjson_lod", 12);
      IFMEString* stmp = gFMESession->createString();
      if (geometry->getTraitString(*slod, *stmp) == FME_FALSE)
      {
         std::stringstream ss;
         ss << "The '" << feature.getFeatureType() << "' feature will not be written because the geometry does not have a 'cityjson_lod' trait.";
         gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
         // The 'Building' feature with geometry type 'IFMEBRepSolid' will not be written because the geometry does not have a citygml_lod_name.
         return FME_FAILURE;
      }

      //-- fetch the JSON geometry from the visitor (FMECityJSONGeometryVisitor)
      json fgeomjson = (visitor_)->getGeomJSON();
      //-- TODO: write '2' or '2.0' is fine for the "lod"?
      fgeomjson["lod"] = atof(stmp->data());

      //-- write it to the JSON object
      // outputJSON_["CityObjects"][s1->data()]["geometry"] = json::array();
      if (!fgeomjson.empty()) {
         outputJSON_["CityObjects"][s1->data()]["geometry"].push_back(fgeomjson);
      }

      std::vector<std::vector<double>> vtmp = (visitor_)->getGeomVertices();
      vertices_.insert(vertices_.end(), vtmp.begin(), vtmp.end());
      // gLogFile->logMessageString("==> 3", FME_WARN);

      //-- reset the internal DS for one feature
      (visitor_)->reset();
   }

   return FME_SUCCESS;
}

//===========================================================================
// Fetch Schema Features
void FMECityJSONWriter::fetchSchemaFeatures()
{
   // Fetch all lines with the keyword "_DEF" from the mapping file because
   // those lines define the schema definition.
   IFMEStringArray* defLineList = gFMESession->createStringArray();
   if (gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(),
    "_DEF", *defLineList))
   {
      // defLineList is a list of the format :
      // [<FeatureType1>,<AttrName11>,<AttrType11>,...,<AttrName1N>,<AttrType1N>,
      // ...,
      // <FeatureTypeM>,<AttrNameM1>,<AttrTypeM1>,...,<AttrNameMN>,<AttrTypeMN>]
      // gLogFile->logMessageString("===", FME_WARN );
      logFMEStringArray(*defLineList);
      
      // We need to determine the feature type names for this writer.
      IFMEStringArray* featureTypes = gFMESession->createStringArray();
      IFMEString* fetchDefsOnly = gFMESession->createString();
      fetchDefsOnly->set("FETCH_DEFS_ONLY", 16);
      if (gMappingFile->fetchFeatureTypes(writerKeyword_.c_str(), writerTypeName_.c_str(),
       *defLineList, *fetchDefsOnly , *featureTypes))
      {
         // gLogFile->logMessageString("---", FME_WARN );
         logFMEStringArray(*featureTypes);
         std::vector<int> potentialFeatureTypeIndices;
         
         // Mark the indices where the feature type names are located in the defLineList.
         for (FME_UInt32 i = 0; i < defLineList->entries(); i++)
         {
            if (featureTypes->contains(defLineList->elementAt(i)->data()))
            {
               potentialFeatureTypeIndices.push_back(i);
            }
         }
         // Add one more index at last element +1 for easier checking later.
         potentialFeatureTypeIndices.push_back(defLineList->entries());

         // Find true feature types in the potential list.
         std::vector<int> featureTypeIndices;
         for (std::vector<int>::size_type i = 0; i < potentialFeatureTypeIndices.size() - 1; i++)
         {
            // Add the index if there are an even number of items between the two values.
            int parityBetweenIndices = (potentialFeatureTypeIndices.at(i + 1) - 
                                          potentialFeatureTypeIndices.at(i)) % 2;
            if (parityBetweenIndices == 1)
            {
               featureTypeIndices.push_back(potentialFeatureTypeIndices.at(i));
            }
         }
         // Add one more index at last element +1 for easier checking later.
         featureTypeIndices.push_back(defLineList->entries());

         // Now that we know the indices of the feature types in the defLineList, start
         // creating schema features through the items defined in that list.
         for (std::vector<int>::size_type i = 0; i < featureTypeIndices.size() - 1; i++)
         {
            IFMEStringArray * defLine = gFMESession->createStringArray();
            IFMEString* str = gFMESession->createString();
            defLineList->getElement(featureTypeIndices.at(i), *str);
            defLine->append(*str);
            // Grab the attribute names and types, and add them to the DEF line.
            int numAttr = (featureTypeIndices.at(i + 1) - featureTypeIndices.at(i)) / 2;
            for (int j = 0; j < numAttr; j++)
            {
               defLineList->getElement((featureTypeIndices.at(i) + (2 * j) + 1), *str);
               defLine->append(*str);
               defLineList->getElement((featureTypeIndices.at(i) + (2 * j) + 2), *str);
               defLine->append(*str);
            }
            // Store the DEF line to the schema.
            addDefLineToSchema(*defLine);
            gFMESession->destroyString(str);
            gFMESession->destroyStringArray(defLine);
         }
      }
      gFMESession->destroyString(fetchDefsOnly);
      gFMESession->destroyStringArray(featureTypes);
   }
   gFMESession->destroyStringArray(defLineList);
}

//===========================================================================
// Add DEF Line to the Schema Feature
void FMECityJSONWriter::addDefLineToSchema(const IFMEStringArray& parameters)
{
   // Get the feature type.
   const IFMEString* paramValue;
   IFMEFeature* schemaFeature = gFMESession->createFeature();

   paramValue = parameters.elementAt(0);

   // Set it on the schema feature.
   schemaFeature->setFeatureType(paramValue->data());

   gLogFile->logMessageString(paramValue->data());

   std::string attrName;
   std::string attrType;
   for (FME_UInt32 i = 1; i < parameters.entries(); i += 2)
   {
      // Grab the attribute name and type
      paramValue = parameters.elementAt(i);
      attrName = paramValue->data();

      paramValue = parameters.elementAt(i + 1);
      attrType = paramValue->data();
      // Add the attribute name and type pair to the schema feature.
      schemaFeature->setSequencedAttribute(attrName.c_str(), attrType.c_str());
      // gLogFile->logMessageString(attrName.c_str());
      // gLogFile->logMessageString(attrType.c_str());
   }
   schemaFeatures_->append(schemaFeature);
}

//===========================================================================
// Logs a IFMEStringArray
void FMECityJSONWriter::logFMEStringArray(IFMEStringArray& stringArray) 
{
   std::string sample = "";
   for (FME_UInt32 i = 0; i < stringArray.entries(); i++)
   {
      // Iterate through the String Array to compose a single string with the tokens.
      sample.append("\'");
      sample.append(stringArray.elementAt(i)->data(), stringArray.elementAt(i)->length());
      sample.append("\' ");
   }
   gLogFile->logMessageString(sample.c_str(), FME_INFORM);
}

int FMECityJSONWriter::duplicate_vertices() {
  gLogFile->logMessageString("Removing the duplicate vertices in the CityJSON object.");
  size_t inputsize = outputJSON_["vertices"].size();
  //-- find bbox
  double minx = 1e9;
  double miny = 1e9;
  double minz = 1e9;
  for (auto& v : outputJSON_["vertices"]) {
    if (v[0] < minx)
      minx = v[0];
    if (v[1] < miny)
      miny = v[1];
    if (v[2] < minz)
      minz = v[2];
  }
  //-- read points and translate now (if int)
  std::vector<Point3> vertices;
  for (auto& v : outputJSON_["vertices"]) {
    std::vector<double> t = v;
    Point3 tmp(t[0], t[1], t[2]);
    tmp.translate(-minx, -miny, -minz);
    vertices.push_back(tmp);
  }
  
  std::map<std::string,unsigned long> hash;
  std::vector<unsigned long> newids (vertices.size(), 0);
  std::vector<std::string> newvertices;
  unsigned long i = 0;
  for (auto& v : vertices) {
    std::string thekey = v.get_key(important_digits_);
    auto it = hash.find(thekey);
    if (it == hash.end()) {
      unsigned long newid = (unsigned long)(hash.size());
      newids[i] = newid;
      hash[thekey] = newid;
      newvertices.push_back(thekey);
    }
    else {
      newids[i] = it->second;
    }
    i++;
  }
  //-- update IDs for the faces
  update_to_new_ids(newids);
  
  if (compress_ == true) {
    gLogFile->logMessageString("Compressing the CityJSON file");
    //-- replace the vertices
    std::vector<std::array<int, 3>> vout;
    for (std::string& s : newvertices) {
      std::vector<std::string> ls;
      tokenize(s, ls);
      for (auto& each : ls) {
        std::size_t found = each.find(".");
        each.erase(found, 1);
      }
      // std::cout << ls[0] << std::endl;
      std::array<int,3> t;
      t[0] = std::stoi(ls[0]);
      t[1] = std::stoi(ls[1]);
      t[2] = std::stoi(ls[2]);
      vout.push_back(t);
    }
    outputJSON_["vertices"] = vout;
    double scalefactor = 1 / (pow(10, important_digits_));
    outputJSON_["transform"]["scale"] = {scalefactor, scalefactor, scalefactor};
    outputJSON_["transform"]["translate"] = {minx, miny, minz};
  }
  else {  //-- do not compress
    std::vector<std::array<double, 3>> vout;
    for (std::string& s : newvertices) {
      // gLogFile->logMessageString(s.c_str());
      std::vector<std::string> ls;
      tokenize(s, ls);
      std::array<double, 3> t;
      t[0] = minx + std::stod(ls[0]);
      t[1] = miny + std::stod(ls[1]);
      t[2] = minz + std::stod(ls[2]);
      vout.push_back(t);
    }  
    outputJSON_["vertices"] = vout;
  }
  return (inputsize - outputJSON_["vertices"].size());
}


void FMECityJSONWriter::tokenize(const std::string& str, std::vector<std::string>& tokens) {
  std::string::size_type lastPos = str.find_first_not_of(" ", 0);
  std::string::size_type pos     = str.find_first_of(" ", lastPos);
  while (std::string::npos != pos || std::string::npos != lastPos) {
    tokens.push_back(str.substr(lastPos, pos - lastPos));
    lastPos = str.find_first_not_of(" ", pos);
    pos = str.find_first_of(" ", lastPos);
  }
}

void FMECityJSONWriter::update_to_new_ids(std::vector<unsigned long> &newids) {
  for (auto& co : outputJSON_["CityObjects"]) {
    for (auto& g : co["geometry"]) {
      if (g["type"] == "GeometryInstance") {
        g["boundaries"][0] = newids[g["boundaries"][0]];
      }
      else if (g["type"] == "Solid") {
        for (auto& shell : g["boundaries"])
          for (auto& surface : shell)
            for (auto& ring : surface)
              for (auto& v : ring) 
                v = newids[v];
      }
      else if ( (g["type"] == "MultiSurface") || (g["type"] == "CompositeSurface") ) {
        for (auto& surface : g["boundaries"])
          for (auto& ring : surface)
            for (auto& v : ring)
              v = newids[v];  
      }
      else if ( (g["type"] == "MultiSolid") || (g["type"] == "CompositeSolid") ) {
        for (auto& solid : g["boundaries"])
          for (auto& shell : solid)
            for (auto& surface : shell)
              for (auto& ring : surface)
                for (auto& v : ring)
                  v = newids[v];
      }
    }
  }
}


