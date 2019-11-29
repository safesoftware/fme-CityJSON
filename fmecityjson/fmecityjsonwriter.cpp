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

// These are initialized externally when a writer object is created so all
// methods in this file can assume they are ready to use.
IFMELogFile* FMECityJSONWriter::gLogFile = nullptr;
IFMEMappingFile* FMECityJSONWriter::gMappingFile = nullptr;
IFMECoordSysManager* FMECityJSONWriter::gCoordSysMan = nullptr;
extern IFMESession* gFMESession;

json::value_type FMECityJSONWriter::geometryJSON;
std::map<const FMECoord3D, unsigned long> FMECityJSONWriter::vertices;
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
   // gLogFile->logMessageString("$$$$ constructor", FME_WARN );
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
   gLogFile->logMessageString("$$$$ open()", FME_WARN );

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

   // Write the schema information to the file. In this template,
   // since we are not writing to a file we will log the schema information
   // instead.
   gLogFile->logMessageString("=====", FME_WARN );

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
      }
      std::string st(schemaFeature->getFeatureType());
      attrToWrite_[st] = sa;
      gLogFile->logFeature(*schemaFeature, FME_INFORM, 20);
   }
   gLogFile->logMessageString("=====", FME_WARN );


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
   outputJSON_["metadata"] = "is awesome";
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
   // gLogFile->logMessageString("8888", FME_WARN);
   // gLogFile->logMessageString(std::to_string(attrToWrite_.size()).c_str(), FME_WARN);

   // for (auto s: attrToWrite_["Building"])
   // {
   //    gLogFile->logMessageString(s.c_str(), FME_WARN);      
   // }


   // -----------------------------------------------------------------------
   // Perform any closing operations / cleanup here; e.g. close opened files
   // -----------------------------------------------------------------------

   // write vertex list
  std::vector<std::vector<double>> thepts;
  thepts.resize(vertices.size());
  for (auto& p : vertices)
    thepts[p.second] = { p.first.x, p.first.y, p.first.z };
  vertices.clear();

   outputJSON_["vertices"] = thepts;
   thepts.clear();
   
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
   gLogFile->logMessageString("$$$$ write()", FME_WARN );
   // Log the feature
   // gLogFile->logFeature(feature);

   // -----------------------------------------------------------------------
   // The feature type and the attributes can be extracted from the feature
   // at this point.
   // -----------------------------------------------------------------------

   // -----------------------------------------------------------------------
   // Perform your write operations here
   // -----------------------------------------------------------------------
   // write fid for CityObject
   IFMEString* s1 = gFMESession->createString();
   if (feature.getAttribute("fid", *s1) == FME_TRUE) 
   {
      gLogFile->logMessageString(*s1, FME_WARN);
      outputJSON_["CityObjects"][s1->data()] = json::object();

      std::string ft(feature.getFeatureType());
      outputJSON_["CityObjects"][s1->data()]["type"] = ft;

      IFMEStringArray* allatt = gFMESession->createStringArray();
      outputJSON_["CityObjects"][s1->data()]["attributes"] = json::object();
      feature.getAllAttributeNames(*allatt);
      for (FME_UInt32 i = 0; i < allatt->entries(); i++)
      {
         const char* t = allatt->elementAt(i)->data();
         std::string ts(t);
         if (attrToWrite_[ft].count(ts) != 0)
         {
            IFMEString* tmps = gFMESession->createString();
            feature.getAttribute(t, *tmps);
            outputJSON_["CityObjects"][s1->data()]["attributes"][t] = tmps->data();
         }
      }
      
      //-- cityjson_children
      IFMEStringArray* childrenValues = gFMESession->createStringArray();
      feature.getListAttribute("cityjson_children", *childrenValues);
      if (childrenValues->entries() > 0)
      outputJSON_["CityObjects"][s1->data()]["children"] = json::array();   
      for (FME_UInt32 i = 0; i < childrenValues->entries(); i++) {
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

    }


   // Extract the geometry from the feature
   const IFMEGeometry* geometry = (const_cast<IFMEFeature&>(feature)).getGeometry();
   FME_Status badNews = geometry->acceptGeometryVisitorConst(*visitor_);
   if (badNews) {
     // There was an error in writing the geometry
     gLogFile->logMessageString(kMsgWriteError);
     return FME_FAILURE;
   }
   outputJSON_["CityObjects"][s1->data()]["geometry"] = json::array();
   if (!geometryJSON.empty()) {
     outputJSON_["CityObjects"][s1->data()]["geometry"].push_back(geometryJSON);
   }

   //TODO: handle lod trait
   IFMEStringArray *names = gFMESession->createStringArray();
   geometry->getTraitNames(*names);
   for (int i = 0; i < names->entries(); i++) {
     //gLogFile->logMessageString(names->elementAt(i)->data(), FME_WARN);
   }

   //TODO: Check if geometry exists, if not skip writing bounds
   // write gepgraphical extent
   if (!outputJSON_["CityObjects"][s1->data()]["geometry"].empty()) {
     FME_Real64 minx, miny, minz, maxx, maxy, maxz;
     feature.boundingCube(minx, maxx, miny, maxy, minz, maxz);
     //gLogFile->logMessageString(std::to_string(minx).c_str(), FME_WARN);
     outputJSON_["CityObjects"][s1->data()]["geographicalExtent"] = { minx, miny, minz, maxx, maxy, maxz };
   }
   return FME_SUCCESS;
}

//===========================================================================
// Fetch Schema Features
void FMECityJSONWriter::fetchSchemaFeatures()
{
   gLogFile->logMessageString("$$$$ fetchSchemaFeatures()", FME_WARN );
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
      logFMEStringArray(*defLineList);
      
      // We need to determine the feature type names for this writer.
      IFMEStringArray* featureTypes = gFMESession->createStringArray();
      IFMEString* fetchDefsOnly = gFMESession->createString();
      fetchDefsOnly->set("FETCH_DEFS_ONLY", 16);
      if (gMappingFile->fetchFeatureTypes(writerKeyword_.c_str(), writerTypeName_.c_str(),
       *defLineList, *fetchDefsOnly , *featureTypes))
      {
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
   gLogFile->logMessageString(sample.c_str(), FME_INFORM);
}
