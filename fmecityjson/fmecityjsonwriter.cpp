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
#include "fmecityjsonreader.h"


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
#include <iline.h>
#include <igeometryiterator.h>
#include <ilibrary.h>
#include <irastertools.h>

#include <typeinfo>
#include <iomanip>
#include <filesystem>

// These are initialized externally when a writer object is created so all
// methods in this file can assume they are ready to use.
IFMELogFile* FMECityJSONWriter::gLogFile = nullptr;
IFMEMappingFile* FMECityJSONWriter::gMappingFile = nullptr;
IFMECoordSysManager* FMECityJSONWriter::gCoordSysMan = nullptr;
extern IFMESession* gFMESession;

//===========================================================================
FME_Status fetchCityJSONTypes(IFMELogFile& logFile,
                              IFMEMappingFile& mappingFile,
                              const std::string& schemaVersion,
                              std::vector<std::string>& cityjsonTypes)
{
   // This will look in the official CityJSON specs
   // and pull out the correct "types" information.

   // We know which major/minor version of the schemas we want.
   // Let's see if we can find it.
   std::map<std::string, IFMEFeature*> schemaFeatures;
   FME_Status badLuck = fetchSchemaFeatures(logFile, schemaVersion, schemaFeatures);
   if (badLuck)
   {
      // TODO: Log some error message
      return FME_FAILURE;
   }

   // loop through the schema features and get the types only.
   // Maybe in the future we might want to keep the other information around,
   // but for now we can just toss it.
   for (auto& entry : schemaFeatures)
   {
      cityjsonTypes.push_back(entry.first);
      gFMESession->destroyFeature(entry.second); 
   }
   schemaFeatures.clear(); // we already deleted all the Features.

   return FME_SUCCESS;
}

//===========================================================================
// Constructor
FMECityJSONWriter::FMECityJSONWriter(const char* writerTypeName, const char* writerKeyword)
:
   writerTypeName_(writerTypeName),
   writerKeyword_(writerKeyword),
   dataset_(""),
   fmeGeometryTools_(nullptr),
   visitor_(nullptr),
   schemaFeatures_(nullptr),
   alreadyLoggedMissingFid_(false),
   nextGoodFidCount_(1),
   alreadyLoggedMissingLod_(false),
   remove_duplicates_(false),
   compress_(false),
   important_digits_(9),
   pretty_print_(false),
   indent_size_(2),
   indent_characters_tabs_(false),
   uniqueFilenameCounter_(1)
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

   //-- compress?
   gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(), kSrcCompress, *pv);
   std::string s1 = pv->data();
   if (s1.compare("Yes") == 0)
      compress_ = true;
   else 
      compress_ = false;
   
   //-- important decimal digits?
   gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(), kSrcImportantDigits, *pv);
   important_digits_ = std::stoi(pv->data());

   //-- indent size?
   FME_Int32 indent_size(indent_size_);
   if (FME_TRUE == gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(), kSrcIndentSize, indent_size))
   {
      indent_size_ = indent_size;
   }

   //-- indent characters?
   gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(), kSrcIndentCharacters, *pv);
   s1 = pv->data();
   indent_characters_tabs_ = false;
   if (s1.compare("Tabs") == 0)
   {
      indent_characters_tabs_ = true;
   }

   //-- pretty print?
   gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(), kSrcPrettyPrint, *pv);
   s1 = pv->data();
   pretty_print_ = false;
   if (s1.compare("Yes") == 0)
   {
      pretty_print_ = true;
   }

   //-- remove duplicate vertices?
   gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(), kSrcRemoveDuplicates, *pv);
   s1 = pv->data();
   remove_duplicates_ = false;
   if (s1.compare("Yes") == 0)
   {
      remove_duplicates_ = true;
   }

   //-- output version?
   gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(), kSrcCityjsonVersion, *pv);
   cityjson_version_ = pv->data();
   // Fixing a bug we had in early workspaces where users set this wrong and
   // we really know what they meant.
   if (cityjson_version_ == "1.0")
   {
      cityjson_version_ = "1.0.1";
   }
   
   //-- texture output format?
   gMappingFile->fetchWithPrefix(writerKeyword_.c_str(), writerTypeName_.c_str(), kSrcPreferredTextureFormat, *pv);
   preferredTextureFormat_ = pv->data();
   if (preferredTextureFormat_ == "Auto")
   {
      preferredTextureFormat_ = "";
   }

   gFMESession->destroyString(pv);

   // Perform setup steps before opening file for writing

   // Get geometry tools
   fmeGeometryTools_ = gFMESession->getGeometryTools();

   // Create visitor to visit feature geometries
   visitor_ = new FMECityJSONGeometryVisitor(
      fmeGeometryTools_, gFMESession, remove_duplicates_, important_digits_, textureRefsToCJIndex_);

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
      //gLogFile->logFeature(*schemaFeature, FME_INFORM, 20);
      gFMESession->destroyStringArray(allatt);
   }

   // Let's populate the possible feature types that are valid for our version
   // of CityJSON
   FME_Status badLuck = fetchCityJSONTypes(*gLogFile,
                                           *gMappingFile,
                                           cityjson_version_,
                                           cityjsonTypes_);
   if (badLuck)
   {
      // TODO: Log some error message
      return FME_FAILURE;
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
   outputJSON_["version"] = cityjson_version_;

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

   // Let's write out any vertices we have accumulated from the geometries we
   // have already created.
   std::optional<double> minx, miny, minz, maxx, maxy, maxz;
   if (visitor_)
   {
      const VertexPool& vtmp = (visitor_)->getGeomVertices();
      vertices_.insert(vertices_.end(), vtmp.begin(), vtmp.end());
      visitor_->getGeomBounds(minx, miny, minz, maxx, maxy, maxz);
   }

   if (!vertices_.empty())
   {
      // Let's update the metadata for the bounds of the actual data.
      // We may have no vertices or it may all be 2D.  Cover those odd cases.
      if (minx && miny && maxx && maxy)
      {
         std::vector<double> bounds;

         // not sure if data can be 2D, but let's code it up like it is possible.
         if (!minz || !maxz)
         {
            bounds.push_back(*minx);
            bounds.push_back(*miny);
            bounds.push_back(*maxx);
            bounds.push_back(*maxy);
         }
         else
         {
            bounds.push_back(*minx);
            bounds.push_back(*miny);
            bounds.push_back(*minz);
            bounds.push_back(*maxx);
            bounds.push_back(*maxy);
            bounds.push_back(*maxz);
         }

         outputJSON_["metadata"]["geographicalExtent"] = json::array();
         outputJSON_["metadata"]["geographicalExtent"] = bounds;
      }

      // Output the actual vertices
      outputJSON_["vertices"] = json::array();
      outputJSON_["vertices"] = vertices_;
      //-- compress/quantize the file
      if (compress_ )
      {
         compressAndOutputVertices(*minx, *miny, *minz);
      }
      else
      {
         // Just output them as they are.
         outputJSON_["vertices"] = vertices_;
      }
      vertices_.clear();
   }

   // Write out the appearances
   FME_Status badLuck = outputAppearances();
   if (badLuck != FME_SUCCESS) return badLuck;

   //-- write to the file
   if (!outputJSON_.is_null())
   {
      if (pretty_print_)
      {
         if (indent_characters_tabs_)
         {
            outputFile_ << std::setw(indent_size_) << std::setfill('\t') << outputJSON_ << std::endl;
         }
         else
         {
            outputFile_ << std::setw(indent_size_) << outputJSON_ << std::endl;
         }
      }
      else
      {
         outputFile_ << outputJSON_ << std::endl;
      }

      // Log that the writer is done
      gLogFile->logMessageString((kMsgClosingWriter + dataset_).c_str());
   }
   outputJSON_.clear();

   // Delete the visitor
   if (visitor_)
   {
      delete visitor_;
   }
   visitor_ = nullptr;

   if (schemaFeatures_)
   {
      schemaFeatures_->clearAndDestroy();
      gFMESession->destroyFeatureVector(schemaFeatures_);
   }
   schemaFeatures_ = nullptr;
   
   // close the file
   outputFile_.close();

   for (auto& [key, value]: writers_)
   {
      IFMEUniversalWriter* writer = value;
      writer->close();
      gFMESession->destroyWriter(writer);
   }
   writers_.clear();

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

   // Coordinate system of all the features in the file being written.
   // (We assume all features passed in share the same
   // coordinate system.)
   IFMEString* csFME = gFMESession->createString();
   feature.getCoordSys(*csFME);
   if (csFME->length() > 0)
   {
      std::string csval(csFME->data(), csFME->length());
      outputJSON_["metadata"]["referenceSystem"] = csval;
   }
   gFMESession->destroyString(csFME);
   csFME = nullptr;

   // Handle Metadata features specially.
   if (ft == "Metadata")
   {
      return handleMetadataFeature(feature);
   }

   //--------------------------------------------------------------------
   //-- write fid for CityObject
   //  This is a bit of a tricky situation.  CityJSON requires a unique
   //  'fid' ID on each and every feature.  We cannot continue if we do not
   //  have this.  But we want to be as helpful as possible to users who may
   //  not really know what they are doing or how to fix things as well.
   //  So we are going to take this lenient, hybrid approach.
   //  1) If a feature has a 'fid', we'll try and use it.  We will
   //     check for already used values and warn if it is a duplicate.
   //     If we have a duplicate fid, we'll continue on
   //     as if it did not have one set.
   //  2) If a featured does not have a 'fid', we'll warn and auto-generate
   //     one ourselves.  A quick way of making sure we get a new unique value is
   //     to just remember the max fid we have seen so far and increment it by one.
   // Note: If users put good, clean fid values on the features themselves, #1
   //       Will be quick and easy.  If users do not put any fid values on themselves,
   //       #2 will be quick and easy (and sensible).  The rough part is if folks start
   //       sending in features with duplicate fids or with some existing and some missing.
   //       We will warn a lot, and try to patch things up, but it might get messy.
   //       I think this is a good compromise, and I hope the latter case is not common.
   IFMEString* fidsFME = gFMESession->createString();
   std::string fids;
   if (feature.getAttribute("fid", *fidsFME) == FME_TRUE)
   {
      fids.assign(fidsFME->data(), fidsFME->length());
      // Case 1 - we got a fid.  Let's see if it is unique.

      if (usedFids_.insert(fids).second)
      {
         // We found a nice, unique fid.
         // Let's just parse it a bit to see if it would clash with the
         // unique set we _would_ be building if we ever need to generate one.
         if (fids.compare(0, kGeneratedFidPrefix.size(), kGeneratedFidPrefix) == 0)
         {
            // Well, it starts the same... 
            std::string suffix = fids.substr(kGeneratedFidPrefix.size());
            int countVal = std::stoi(suffix); // Will return 0 if it is not an integer
            if (countVal > 0)
            {
               // Yikes!  It fits exactly the format of our generated fids!
               // Let's reset the latest seen count to this number so we don't make
               // duplicates.
               nextGoodFidCount_ = std::max(countVal+1, nextGoodFidCount_);
            }
         }
      }
      else
      {
         // Ah, it is a duplicate.  Gotta get a fresh one.
         std::string errorMsg = "CityJSON features must have an attribute named 'fid' to uniquely identify them.  Duplicate value '" +
                                 fids + "' found.  Generating a unique fid' instead and continuing.";
         gLogFile->logMessageString(errorMsg.c_str(), FME_WARN);

         generateUniqueFID(fids);
      }
   }
   else
   {
      // We don't have any 'fid'.  :(
      // Let's only log this message once.
      if (!alreadyLoggedMissingFid_)
      {
         gLogFile->logMessageString("CityJSON features must have an attribute named 'fid' to uniquely identify them.  Generating a unique fid' and continuing.", FME_WARN);
         alreadyLoggedMissingFid_ = true;
      }
      generateUniqueFID(fids);
   }
   gFMESession->destroyString(fidsFME); fidsFME = nullptr;

   //--------------------------------------------------------------------

   if (!outputJSON_["CityObjects"].is_object())
   {
      outputJSON_["CityObjects"] = json::object();
   }

   //gLogFile->logMessageString(*fidsFME);

   outputJSON_["CityObjects"][fids] = json::object();
   outputJSON_["CityObjects"][fids]["type"] = ft;
   //-- set FeatureType in visitor for surface semantics
   visitor_->setFeatureType(ft);

   IFMEStringArray* allatt = gFMESession->createStringArray();
   outputJSON_["CityObjects"][fids]["attributes"] = json::object();
   
   feature.getAllAttributeNames(*allatt);
   // feature.getSequencedAttributeList(*allatt);
   for (FME_UInt32 i = 0; i < allatt->entries(); i++)
   {
      const IFMEString* tFME = allatt->elementAt(i);
      std::string ts(tFME->data(), tFME->length());
      // gLogFile->logMessageString(ts.c_str());
      IFMEString* valueFME = gFMESession->createString();
      feature.getAttribute(*tFME, *valueFME);
      std::string val(valueFME->data(), valueFME->length());
      FME_AttributeType ftype = feature.getAttributeType(*tFME);
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
               outputJSON_["CityObjects"][fids]["attributes"][ts] = val;
            } 
            else if ( (ftype == FME_ATTR_STRING) || 
                      (ftype == FME_ATTR_ENCODED_STRING) 
                      ) {
               outputJSON_["CityObjects"][fids]["attributes"][ts] = val;
            }
            else if (ftype == FME_ATTR_BOOLEAN) {
               FME_Boolean b;
               if (feature.getBooleanAttribute(*tFME, b) == FME_TRUE) {
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = "true";
               }
               else {
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = "false";
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
               long tmp = std::stol(val);
               outputJSON_["CityObjects"][fids]["attributes"][ts] = tmp;
            } 
            else if ( (ftype == FME_ATTR_STRING) || 
                      (ftype == FME_ATTR_ENCODED_STRING) ) 
            {
               try {
                  long tmp = std::stol(val);
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = tmp;
               }
               catch (const std::invalid_argument& ia) {
                  std::stringstream ss;
                  ss << "Attribute '" << ts << "' cannot be converted to integer, writing string.";
                  gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = val;
               }

            }
            else if (ftype == FME_ATTR_BOOLEAN) {
               FME_Boolean b;
               if (feature.getBooleanAttribute(*tFME, b) == FME_TRUE) {
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = 1;
               }
               else {
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = 0;
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
               double tmp = std::stod(val);
               outputJSON_["CityObjects"][fids]["attributes"][ts] = tmp;
            } 
            else if ( (ftype == FME_ATTR_STRING) || 
                      (ftype == FME_ATTR_ENCODED_STRING) ) 
            {
               try {
                  double tmp = std::stod(val);
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = tmp;
               }
               catch (const std::invalid_argument& ia) {
                  std::stringstream ss;
                  ss << "Attribute '" << ts << "' cannot be converted to integer, writing string.";
                  gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = val;
               }

            }
            else if (ftype == FME_ATTR_BOOLEAN) {
               FME_Boolean b;
               if (feature.getBooleanAttribute(*tFME, b) == FME_TRUE) {
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = 1.0;
               }
               else {
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = 0.0;
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
               int tmp = std::stoi(val);
               if (tmp == 1)
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = true;
               else if (tmp == 0)
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = false;
               else
               {
                  std::stringstream ss;
                  ss << "Attribute '" << ts << "' cannot be converted to Boolean, writing string.";
                  gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = val;
               }

            } 
            else if (ftype == FME_ATTR_BOOLEAN) {
               FME_Boolean b;
               if (feature.getBooleanAttribute(*tFME, b) == FME_TRUE) {
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = true;
               }
               else {
                  outputJSON_["CityObjects"][fids]["attributes"][ts] = false;
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
            outputJSON_["CityObjects"][fids]["attributes"][ts] = val;
         }
      //-- OTHERS (TODO: not sure if they exist?)             
         else 
         {
            std::stringstream ss;
            ss << "Attribute value type '" << wtype << "' is not allowed. Not written.";
            gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
         }
      }
      gFMESession->destroyString(valueFME);
   }
   gFMESession->destroyStringArray(allatt);
   // gLogFile->logMessageString("Done with attributes", FME_WARN);
   
   //-- cityjson_children
   IFMEStringArray* childrenValues = gFMESession->createStringArray();
   feature.getListAttribute("cityjson_children", *childrenValues);
   if (childrenValues->entries() > 0)
   {
      outputJSON_["CityObjects"][fids]["children"] = json::array();   
   }
   for (FME_UInt32 i = 0; i < childrenValues->entries(); i++) {
      // TODO : test if children and parents are written as string
      std::string oneVal(childrenValues->elementAt(i)->data(),childrenValues->elementAt(i)->length());
      outputJSON_["CityObjects"][fids]["children"].push_back(oneVal);
   }
   gFMESession->destroyStringArray(childrenValues);

   //-- cityjson_parents
   IFMEStringArray* parentValues = gFMESession->createStringArray();
   feature.getListAttribute("cityjson_parents", *parentValues);
   if (parentValues->entries() > 0)
   {
      outputJSON_["CityObjects"][fids]["parents"] = json::array();
   }
   for (FME_UInt32 i = 0; i < parentValues->entries(); i++) {
      std::string oneVal(parentValues->elementAt(i)->data(), parentValues->elementAt(i)->length());
      outputJSON_["CityObjects"][fids]["parents"].push_back(oneVal);
   }
   gFMESession->destroyStringArray(parentValues);

//-- GEOMETRIES -----

   //-- extract the geometries from the feature
   const IFMEGeometry* geometry = (const_cast<IFMEFeature&>(feature)).getGeometry();


   //-- do no process geometry if none, this is allowed in CityJSON
   //-- a CO without geometry still has to have an empty array "geometry": []
   outputJSON_["CityObjects"][fids]["geometry"] = json::array();
   FME_Boolean isgeomnull = geometry->canCastAs<IFMENull*>();
   if (isgeomnull == false)
   {
      FME_Status badNews = geometry->acceptGeometryVisitorConst(*visitor_);
      if (badNews) {
         // There was an error in writing the geometry
         gLogFile->logMessageString(kMsgWriteError);
         return FME_FAILURE;
      }

      //-- fetch the LoD of the geometry
      IFMEString* slod = gFMESession->createString();
      slod->set("cityjson_lod", 12);
      IFMEString* stmpFME = gFMESession->createString();
      bool notValidLOD(geometry->getTraitString(*slod, *stmpFME) == FME_FALSE);

      // Maybe we got one, but it was not a valid number.
      double lodAsDouble(2);
      try
      {
         lodAsDouble = std::stod({stmpFME->data(), stmpFME->length()});
      }
      catch (const std::invalid_argument&)
      {
         notValidLOD = true;
      }
      catch (const std::out_of_range&)
      {
         notValidLOD = true;
      }                                                                              

      if (notValidLOD)
      {
         // Let's only log this message once.
         if (!alreadyLoggedMissingLod_)
         {
            std::stringstream ss;
            ss << "The '" << feature.getFeatureType() << "' does not have the required cityjson_lod' trait.  Assuming a LOD of '2' and continuing.";
            gLogFile->logMessageString(ss.str().c_str(), FME_WARN);
            alreadyLoggedMissingLod_ = true;
         }
         lodAsDouble = 2;
      }
      gFMESession->destroyString(stmpFME); stmpFME = nullptr;
      gFMESession->destroyString(slod); slod = nullptr;

      //-- fetch the JSON geometry from the visitor (FMECityJSONGeometryVisitor)
      json fgeomjson = (visitor_)->getGeomJSON();
      //-- TODO: write '2' or '2.0' is fine for the "lod"?
      fgeomjson["lod"] = lodAsDouble;

      //-- write it to the JSON object
      // outputJSON_["CityObjects"][s1->data()]["geometry"] = json::array();
      if (!fgeomjson.empty()) {
         outputJSON_["CityObjects"][fids]["geometry"].push_back(fgeomjson);
      }

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
      //logFMEStringArray(*defLineList);
      
      // We need to determine the feature type names for this writer.
      IFMEStringArray* featureTypes = gFMESession->createStringArray();
      IFMEString* fetchDefsOnly = gFMESession->createString();
      fetchDefsOnly->set("FETCH_DEFS_ONLY", 15);
      if (gMappingFile->fetchFeatureTypes(writerKeyword_.c_str(), writerTypeName_.c_str(),
       *defLineList, *fetchDefsOnly , *featureTypes))
      {
         // gLogFile->logMessageString("---", FME_WARN );
         //logFMEStringArray(*featureTypes);
         std::vector<int> potentialFeatureTypeIndices;
         
         // Mark the indices where the feature type names are located in the defLineList.
         for (FME_UInt32 i = 0; i < defLineList->entries(); i++)
         {
            if (featureTypes->contains(*defLineList->elementAt(i)))
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

   //gLogFile->logMessageString(paramValue->data());

   for (FME_UInt32 i = 1; i < parameters.entries(); i += 2)
   {
      // Grab the attribute name and type
      // Add the attribute name and type pair to the schema feature.
      schemaFeature->setEncodedSequencedAttribute(*parameters.elementAt(i),
                                                  *parameters.elementAt(i + 1),
                                                  "fme-system");
      // gLogFile->logMessageString(attrName.c_str());
      // gLogFile->logMessageString(attrType.c_str());
   }
   schemaFeatures_->append(schemaFeature);
}

//===========================================================================
FME_Status FMECityJSONWriter::handleMetadataFeature(const IFMEFeature& feature)
{
   // TODO:  Right now we will consume as many metadata features as are
   // passed in.  Each one will take their values and overwrite the last.
   // I'm not sure if this is good policy, or if we should reject more than
   // one coming in, or give warning or error messages if we get more than one.

   IFMEString* tempAttr = gFMESession->createString();

   // Look for whatever metadata is interesting
   // https://www.cityjson.org/specs/1.0.1/#metadata

   // TODO: we should probably scrape off metadata based on the
   // version number we are writing.
   //if (cityjson_version_ == "1.0.1")
   {
      // Let's ignore the reference system for now.
      // We take this off of the clues given to the writer, 
      // not from the metadata features.
      //feature.getAttribute("referenceSystem", *tempAttr);

      // Let's ignore the geographicalExtent, as we will
      // use the data's true bounding box as we calculate from
      // the input features instead.
      //feature.getAttribute("geographicalExtent", *tempAttr);

      // This is a simple string, and we do no checking
      if (FME_TRUE == feature.getAttribute("geographicLocation", *tempAttr))
      {
         std::string glval(tempAttr->data(), tempAttr->length());
         outputJSON_["metadata"]["geographicLocation"] = glval;
      }

      // This is a simple string, and we do no checking
      if (FME_TRUE == feature.getAttribute("datasetTopicCategory", *tempAttr))
      {
         std::string dtcval(tempAttr->data(), tempAttr->length());
         outputJSON_["metadata"]["datasetTopicCategory"] = dtcval;
      }

      // TODO: this has not yet been implemented.  I have not seen an example
      // of this in data, so it is hard to test currently without it.
      //feature.getAttribute("lineage", *tempAttr);
   }

   // clean up
   gFMESession->destroyString(tempAttr);
   tempAttr = nullptr;

   return FME_SUCCESS;
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

//===========================================================================
void FMECityJSONWriter::compressAndOutputVertices(double minx, double miny, double minz)
{
   gLogFile->logMessageString("Compressing/quantizing vertices in the CityJSON object.");

   // We are passed in the offset.  We calculate the scaling factor
   double scalefactor = 1 / (pow(10, important_digits_));

   std::vector<std::array<long long, 3>> vout;
   for (auto v : vertices_)
   {
      long long newx = round((std::get<0>(v) - minx) / scalefactor);
      long long newy = round((std::get<1>(v) - miny) / scalefactor);
      long long newz = round((std::get<2>(v) - minz) / scalefactor);
      vout.push_back({newx, newy, newz});
   }
   outputJSON_["vertices"]               = vout;
   outputJSON_["transform"]["scale"]     = {scalefactor, scalefactor, scalefactor};
   outputJSON_["transform"]["translate"] = {minx, miny, minz};
}

//===========================================================================
void FMECityJSONWriter::generateUniqueFID(std::string& fids)
{
   fids = kGeneratedFidPrefix + std::to_string(nextGoodFidCount_);
   nextGoodFidCount_++;
}

//===========================================================================
FME_Status FMECityJSONWriter::writeRaster(FME_UInt32 rasterReference,
                                          const std::string& fileBaseNameSuggestion,
                                          const std::string& texturesRelativeDir,
                                          const std::string& outputDir,
                                          std::string& fileName,
                                          std::string& fileType)
{
   IFMERaster* raster = gFMESession->getLibrary()->getRasterCopy(rasterReference);

   // This is a bit confusing, so I'll put a note here:
   // - fileType: suggestions of formats, from the GUI, are PNG/JPEG
   // - flieType: strings returned to be used in the CityJSON file PNG/JPG (note, no "E")
   // - writer keywords to use are PNGRASTER/JPEG

   // If they are "suggesting" a type we don't support, we just will ignore their
   // suggestion.
   if ((fileType != "PNG") && (fileType != "JPEG"))
   {
      fileType = "";
   }

   // Get the original format name, if we can.
   IFMEString* originalFormatName = gFMESession->createString();
   raster->getSourceFormatName(*originalFormatName);
   std::string ofn(originalFormatName->data(), originalFormatName->length());
   gFMESession->destroyString(originalFormatName);

   // If no fileType is suggested, let's try to keep what we have.
   if (fileType.empty())
   {
      if (ofn == "JPEG")
      {
         // Let's pick the type that it already is
         fileType = "JPG";
      }
      else
      {
         // If we still don't know what type to write out, let's just pick an arbitrary one
         fileType = "PNG";
      }
   }
   else if (fileType == "JPEG")
   {
      fileType = "JPG";
   }

   // JPEG doesn't like to write palettes, have Alpha band, etc. so we must resolve them here.
   // no need to fix up a raster that is not changing format.
   if ((ofn != "JPEG") && (fileType == "JPG"))
   {
      gFMESession->getRasterTools()->resolvePalettes(raster);
      gFMESession->getRasterTools()->convertInterpretation(
         FME_REINTERPRET_MODE_RASTER, FME_INTERPRETATION_RGB24, raster, nullptr);
   }
   else if ((ofn != "PNGRASTER") && (fileType == "PNG"))
   {
      gFMESession->getRasterTools()->convertInterpretation(
         FME_REINTERPRET_MODE_RASTER, FME_INTERPRETATION_RGBA32, raster, nullptr);
   }

   if (rasterRefsToFileNames_.find(rasterReference) != rasterRefsToFileNames_.end())
   {
      std::string writtenName = rasterRefsToFileNames_[rasterReference];
      if (writtenName.empty())
      {
         // Storing a null string indicates a failure to write the file.
         fileName = "missing_raster";
         return FME_FAILURE;
      }
      else
      {
         fileName = writtenName;
         return FME_SUCCESS;
      }
   }

   // We want silent logging during the writing of a texture file.
   FME_Boolean oldSilentMode = gLogFile->getSilent();
   gLogFile->silent(FME_TRUE); // don't forget to set this back to what it was before...

      // Figure out if we can use original file basename or if we need to use the suggestion.
   std::string basename("texture"); // the default if there is no name

   // The source file name may be null if the raster is newly created.
   IFMEString* sfn = gFMESession->createString();
   raster->getSourceDataset(*sfn);
   std::string origFileName = {sfn->data(), sfn->length()};
   gFMESession->destroyString(sfn);

   // Always use the source basName, if possible.
   std::string sourceBN = std::filesystem::path(origFileName).stem().string();
   if (!sourceBN.empty())
   {
      basename = sourceBN;
   }
   else if (!fileBaseNameSuggestion.empty())
   {
      basename = fileBaseNameSuggestion;
   }

   // We need to write this raster out using an
   // FME writer. Let's use the format indicated.
   std::string format = (fileType == "JPG") ? "JPEG" : "PNGRASTER";
   fileName.clear();
   FME_Status badLuck = writeWithWriter(raster, basename, format, outputDir, fileName);
   raster = nullptr;

   rasterRefsToFileNames_[rasterReference] = fileName;

   gLogFile->silent(oldSilentMode);

   return badLuck;
}

//------------------------------------------------------------------------------
FME_Status FMECityJSONWriter::writeWithWriter(IFMERaster*& raster,
                                              const std::string& basename,
                                              const std::string& format,
                                              const std::string& outputDir,
                                              std::string& outputFilename)
{
   // Get the correct writer.
   IFMEUniversalWriter* writer = nullptr;
   auto it = writers_.find(format);
   if (it != writers_.end())
   {
      writer = it->second;
   }
   else
   {
      // We haven't tried to make a writer for this format yet. Do it now.
      writer = gFMESession->createWriter(format.c_str(), nullptr);

      // Open this writer on our directory.
      IFMEStringArray* directives = gFMESession->createStringArray();

      FME_MsgNum msgNum = writer->open(outputDir.c_str(), *directives);
      gFMESession->destroyStringArray(directives);
      if (msgNum != FME_SUCCESS)
      {
         raster->destroy();
         raster = nullptr;
         gFMESession->destroyWriter(writer);
         return FME_FAILURE;
      }

      // Place the writer in our dictionary.
      writers_[format] = writer;
      extensions_[format] = (format == "JPEG") ? ".jpg" : ".png";
   }

   // Make a temporary feature.
   IFMEFeature* feature = gFMESession->createFeature();
   feature->setGeometry(raster);
   raster = nullptr;

   // Make a unique name.
   outputFilename = getUniqueFilename(basename, extensions_[format]);
   feature->setFeatureType(std::filesystem::path(outputFilename).stem().string().c_str());

   // Do the writing.
   FME_MsgNum msgNum = writer->write(*feature);
   gFMESession->destroyFeature(feature);

   if (msgNum != FME_SUCCESS)
   {
      return FME_FAILURE;
   }

   return FME_SUCCESS;
}

std::string FMECityJSONWriter::getUniqueFilename(const std::string& basename,
                                                 const std::string& extension)
{
   // This is the one we want - if it does not clash with a previous one.
   std::string fileName = basename + extension;

   // We have to alter the name if we found we used it.
   if (std::find_if(std::begin(rasterRefsToFileNames_), std::end(rasterRefsToFileNames_), [&](auto&& p){ return p.second == fileName; }) != std::end(rasterRefsToFileNames_))
   {
      //basename += "_x";
      fileName = basename + "_" + std::to_string(uniqueFilenameCounter_++) + extension;
   }

   return fileName;
}

//===========================================================================
FME_Status FMECityJSONWriter::outputAppearances()
{
   if (!textureRefsToCJIndex_.empty())
   {
      // we need to know what order to write out the texture References
      std::map<int, FME_UInt32> CJIndexToTexRef;
      for (auto& refIndex : textureRefsToCJIndex_)
      {
         CJIndexToTexRef[refIndex.second] = refIndex.first;
      }

      // Get textures directory name and location
      std::string texturesRelativeDir; // no trailing slash
      std::string texturesFullDir;     // no trailing slash

      texturesRelativeDir = std::filesystem::path(dataset_).stem().string() + "_textures";
      texturesFullDir     = std::filesystem::path(dataset_).parent_path().string() + '/' + texturesRelativeDir;

      json allTextures;
      for (auto& indexRef : CJIndexToTexRef)
      {
         FME_UInt32 textureRef = indexRef.second;

         // Get the Raster reference from the texture.
         IFMETexture* tex = gFMESession->getLibrary()->getTextureCopy(textureRef);

         std::string fileName;
         std::string fileType(preferredTextureFormat_);
         FME_UInt32 rasterRef(0);
         if (FME_FALSE == tex->getRasterReference(rasterRef))
         {
            // We shouldn't get here.  We have got no raster to write
            // but we still need to fill an entry in the texture array.
            fileName = "missing_raster";
            fileType = "PNG";
         }
         else
         {
            // We'll get back the name and type that was written out
            // Let's try to write it out with the original filename if we can,
            // but if not, start with "texture.png" etc.
            FME_Status badLuck = writeRaster(
               rasterRef, "texture", texturesRelativeDir, texturesFullDir, fileName, fileType);
            if (badLuck != FME_SUCCESS) return badLuck;
         }

         json textureJSON;
         textureJSON["image"] = texturesRelativeDir + '/' + fileName;
         textureJSON["type"] = fileType;

         FME_TextureWrap wrapStyle;
         tex->getTextureWrap(wrapStyle);
         switch (wrapStyle)
         {
            case FME_TEXTURE_REPEAT_BOTH:
            case FME_TEXTURE_CLAMP_U_REPEAT_V: // Can't really represent this so we'll pick "wrap"
            case FME_TEXTURE_REPEAT_U_CLAMP_V: // Can't really represent this so we'll pick "wrap"
               textureJSON["wrapMode"] = "wrap";
               break;
            case FME_TEXTURE_CLAMP_BOTH:
               textureJSON["wrapMode"] = "clamp";
               break;
            case FME_TEXTURE_MIRROR:
               textureJSON["wrapMode"] = "mirror";
               break;
            case FME_TEXTURE_BORDER_FILL:
               textureJSON["wrapMode"] = "border";
               break;
            case FME_TEXTURE_NONE:
            default: 
               textureJSON["wrapMode"] = "none";
               break;
         }

         // TODO: figure out how we should support "textureType"
         //textureJSON["textureType"] = "unknown";

         FME_Real64 r, g, b;
         if (FME_TRUE == tex->getBorderColor(r, g, b))
         {
            std::vector<double> rgba{ r, g, b, 1.0 };
            textureJSON["borderColor"] = rgba;
         }

         gFMESession->getGeometryTools()->destroyTexture(tex); tex = nullptr;

         // Add this to our array
         allTextures += textureJSON;
      }
      
      if (!allTextures.is_null())
      {
         outputJSON_["appearance"]["textures"] = allTextures;
      }

      // We're done with this
      textureRefsToCJIndex_.clear();
   }
   return FME_SUCCESS;
}

