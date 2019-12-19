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
#include <igeometryiterator.h>

#include <typeinfo>

// These are initialized externally when a writer object is created so all
// methods in this file can assume they are ready to use.
IFMELogFile* FMECityJSONWriter::gLogFile = nullptr;
IFMEMappingFile* FMECityJSONWriter::gMappingFile = nullptr;
IFMECoordSysManager* FMECityJSONWriter::gCoordSysMan = nullptr;
extern IFMESession* gFMESession;

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
   gLogFile->logMessageString(msgOpeningWriter.c_str(), FME_WARN);

   schemaFeatures_ = gFMESession->createFeatureVector();

   // Fetch all the schema features and add the DEF lines.
   fetchSchemaFeatures();

   gLogFile->logMessageString("@@@@@@@@@@@", FME_WARN);
   for (FME_UInt32 i = 0; i < schemaFeatures_->entries(); i++)
   {
      IFMEFeature* schemaFeature = (*schemaFeatures_)(i); 
      IFMEStringArray* sa = gFMESession->createStringArray();  
      schemaFeature->getSequencedAttributeList(*sa);
      for (FME_UInt32 i = 0; i < sa->entries(); i++)
      {
         const char* t = sa->elementAt(i)->data();
         gLogFile->logMessageString(t);
         FME_AttributeType type = schemaFeature->getAttributeType(t);
         if (type == FME_ATTR_STRING) {
            gLogFile->logMessageString("===STRING===", FME_WARN);
         } else {
            gLogFile->logMessageString("===SMTH-ELSE===", FME_WARN);
         }
      }
   }
   gLogFile->logMessageString("@@@@@@@@@@@", FME_WARN);


   // Write the schema information to the file. In this template,
   // since we are not writing to a file we will log the schema information
   // instead.

   for (FME_UInt32 i = 0; i < schemaFeatures_->entries(); i++)
   {
      IFMEFeature* schemaFeature = (*schemaFeatures_)(i);
      gLogFile->logMessageString(schemaFeature->getFeatureType(), FME_WARN );
    
      IFMEStringArray* allatt = gFMESession->createStringArray();
      schemaFeature->getAllAttributeNames(*allatt);
      std::set<std::string> sa;
      for (FME_UInt32 i = 0; i < allatt->entries(); i++)
      {
         const char* t = allatt->elementAt(i)->data();
         sa.insert(std::string(t));
         gLogFile->logMessageString(t, FME_WARN);

         std::string ts(t);
         IFMEString* value = gFMESession->createString();
         schemaFeature->getAttribute(t, *value);
         FME_AttributeType type = schemaFeature->getAttributeType(t);
         // gLogFile->logMessageString(type, FME_WARN);
         if (type == FME_ATTR_INT32) {
            gLogFile->logMessageString("===STRING===", FME_WARN);
         } else {
            gLogFile->logMessageString("===SMTH-ELSE===", FME_WARN);
         }
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

   outputJSON_["vertices"] = vertices_;
   // thepts.clear();
   vertices_.clear();
   
   outputFile_ << outputJSON_ << std::endl;

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
   
   // Log that the writer is done
   gLogFile->logMessageString((kMsgClosingWriter + dataset_).c_str());

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

   //-- write fid for CityObject
   //-- FAILURE if not one of these
   IFMEString* s1 = gFMESession->createString();
   if (feature.getAttribute("fid", *s1) == FME_FALSE) 
   {
      gLogFile->logMessageString("CityJSON features must have an attribute named 'fid' to uniquely identify them.", FME_WARN );
      return FME_FAILURE;
   }
   

   gLogFile->logMessageString(*s1, FME_WARN);
   outputJSON_["CityObjects"][s1->data()] = json::object();

   outputJSON_["CityObjects"][s1->data()]["type"] = ft;

   IFMEStringArray* allatt = gFMESession->createStringArray();
   outputJSON_["CityObjects"][s1->data()]["attributes"] = json::object();
   feature.getAllAttributeNames(*allatt);
   for (FME_UInt32 i = 0; i < allatt->entries(); i++)
   {
      const char* t = allatt->elementAt(i)->data();
      std::string ts(t);
      IFMEString* value = gFMESession->createString();
      feature.getAttribute(t, *value);
      FME_AttributeType type = feature.getAttributeType(t);
      if ( (ts != "fid") &&
           (ts != "cityjson_parents") &&  
           (ts != "cityjson_children") &&  
           (attrToWrite_[ft].count(ts) != 0)
         )  
      {
         // outputJSON_["CityObjects"][s1->data()]["attributes"][t] = value->data();
         if ( (type == FME_ATTR_STRING) || 
              (type == FME_ATTR_ENCODED_STRING) 
            ) {
            outputJSON_["CityObjects"][s1->data()]["attributes"][t] = value->data();
          }
          else if ( (type == FME_ATTR_INT8)  ||
                    (type == FME_ATTR_INT16) ||                                  
                    (type == FME_ATTR_INT32) ||                                  
                    (type == FME_ATTR_INT64) ||                                  
                    (type == FME_ATTR_UINT8)  ||                                  
                    (type == FME_ATTR_UINT16) ||                                  
                    (type == FME_ATTR_UINT32) ||                                  
                    (type == FME_ATTR_UINT64)
                  ) {
            outputJSON_["CityObjects"][s1->data()]["attributes"][t] = std::stoi(value->data());
          }
          else if ( (type == FME_ATTR_REAL32) ||
                    (type == FME_ATTR_REAL64) ||
                    (type == FME_ATTR_REAL80)
                  ) {
            outputJSON_["CityObjects"][s1->data()]["attributes"][t] = std::stod(value->data());
          }
          else if (type == FME_ATTR_BOOLEAN) {
            FME_Boolean b;
            if (feature.getBooleanAttribute(t, b) == FME_TRUE) {
               outputJSON_["CityObjects"][s1->data()]["attributes"][t] = true;
            }
            else {
               outputJSON_["CityObjects"][s1->data()]["attributes"][t] = false;
            }
         }
         else {
            std::string msg = "Attribute value type is not allowed, in '";
            msg.append(t);
            msg.append("'.");
            gLogFile->logMessageString(msg.c_str(), FME_WARN);
         }
      }
      gFMESession->destroyString(value);
   }
   gFMESession->destroyStringArray(allatt);
   
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

   //-- update the offset (for writing vertices in the global list of CityJSON)
   //-- in the visitor.
   (visitor_)->setVerticesOffset(vertices_.size());

   //-- extract the geometries from the feature
   const IFMEGeometry* geometry = (const_cast<IFMEFeature&>(feature)).getGeometry();

//======================================================
   // FME_GeometryType fmegeomtype = (const_cast<IFMEFeature&>(feature)).getGeometryType();
   // if fmegeomtype == 
   // std::string s = typeid(*geometry).name();
   // gLogFile->logMessageString(s.c_str(), FME_WARN);

   // if (dynamic_cast<const IFMEBRepSolid*>(geometry))
   // {
   //    gLogFile->logMessageString("I am a IFMEBRepSolid", FME_WARN);
   //    const IFMEBRepSolid* g = dynamic_cast<const IFMEBRepSolid*>(geometry);
   //    const IFMESurface* outerSurface = g->getOuterSurface();
   //    const IFMECompositeSurface* cs = dynamic_cast<const IFMECompositeSurface*>(outerSurface);
   //    IFMESurfaceIterator* iterator = cs->getIterator();
   //    while (iterator->next())
   //    {
   //       // Get the next surface
   //       const IFMESurface* surface = iterator->getPart();
   //       gLogFile->logMessageString((std::string(kMsgVisiting) + std::string("surface LEDOUX")).c_str());
   //    }

   // }
//======================================================

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
   outputJSON_["CityObjects"][s1->data()]["geometry"] = json::array();
   if (!fgeomjson.empty()) {
      outputJSON_["CityObjects"][s1->data()]["geometry"].push_back(fgeomjson);
   }

   std::vector<std::vector<double>> vtmp = (visitor_)->getGeomVertices();
   vertices_.insert(vertices_.end(), vtmp.begin(), vtmp.end());
   // gLogFile->logMessageString("==> 3", FME_WARN);

   //-- reset the internal DS for one feature
   (visitor_)->reset();

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
      gLogFile->logMessageString("===", FME_WARN );
      logFMEStringArray(*defLineList);
      
      // We need to determine the feature type names for this writer.
      IFMEStringArray* featureTypes = gFMESession->createStringArray();
      IFMEString* fetchDefsOnly = gFMESession->createString();
      fetchDefsOnly->set("FETCH_DEFS_ONLY", 16);
      if (gMappingFile->fetchFeatureTypes(writerKeyword_.c_str(), writerTypeName_.c_str(),
       *defLineList, *fetchDefsOnly , *featureTypes))
      {
         gLogFile->logMessageString("---", FME_WARN );
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
   gLogFile->logMessageString("00000000", FME_WARN);
   gLogFile->logMessageString(sample.c_str(), FME_INFORM);
   gLogFile->logMessageString("99999999", FME_WARN);
}

