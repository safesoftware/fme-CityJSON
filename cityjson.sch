FEATURE_DEF Metadata           \
   geographicalExtent   string \
   referenceSystem      string \
   fme_geometry{0} cityjson_null

FEATURE_DEF Building                             \
   fid                                    string \
   lokaalid                               string \
   lv_publicatiedatum                     string \
   measuredHeight                         real64 \
   min-height-surface                     real64 \
   namespace                              string \
   plaatsingspunt                         string \
   plus_status                            string \
   relatievehoogteligging                 string \
   tekst                                  string \
   terminationdate                        string \
   bgt_status                             string \
   tijdstipregistratie                    string \
   bronhouder                             string \
   creationdate                           string \
   eindregistratie                        string \
   hoek                                   string \
   identificatiebagpnd                    string \
   identificatiebagvbohoogstehuisnummer   string \
   identificatiebagvbolaagstehuisnummer   string \
   inonderzoek                            string \
   fme_geometry{0} cityjson_solid

FEATURE_DEF GenericCityObject      \
   fid                      string \
   plus_status              string \
   plus_type                string \
   relatievehoogteligging   string \
   terminationdate          string \
   tijdstipregistratie      string \
   bgt_status               string \
   bgt_type                 string \
   bronhouder               string \
   creationdate             string \
   eindregistratie          string \
   inonderzoek              string \
   lokaalid                 string \
   lv_publicatiedatum       string \
   namespace                string \
   fme_geometry{0} cityjson_solid  \
   fme_geometry{1} cityjson_multisurface

FEATURE_DEF LandUse                      \
   fid                            string \
   onbegroeidterreindeeloptalud   string \
   plus_fysiekvoorkomen           string \
   plus_status                    string \
   relatievehoogteligging         string \
   terminationdate                string \
   tijdstipregistratie            string \
   bgt_fysiekvoorkomen            string \
   bgt_status                     string \
   bronhouder                     string \
   creationdate                   string \
   eindregistratie                string \
   inonderzoek                    string \
   lokaalid                       string \
   lv_publicatiedatum             string \
   namespace                      string \
   fme_geometry{0} cityjson_multisurface

FEATURE_DEF PlantCover                 \
   fid                          string \
   namespace                    string \
   plus_fysiekvoorkomen         string \
   plus_status                  string \
   relatievehoogteligging       string \
   terminationdate              string \
   tijdstipregistratie          string \
   begroeidterreindeeloptalud   string \
   bgt_status                   string \
   bronhouder                   string \
   class                        string \
   creationdate                 string \
   eindregistratie              string \
   inonderzoek                  string \
   lokaalid                     string \
   lv_publicatiedatum           string \
   fme_geometry{0} cityjson_multisurface

FEATURE_DEF Road                        \
   fid                           string \
   plus_functiewegdeel           string \
   plus_fysiekvoorkomenwegdeel   string \
   plus_status                   string \
   relatievehoogteligging        string \
   surfacematerial               string \
   terminationdate               string \
   tijdstipregistratie           string \
   wegdeeloptalud                string \
   bgt_status                    string \
   bronhouder                    string \
   creationdate                  string \
   eindregistratie               string \
   function                      string \
   inonderzoek                   string \
   lokaalid                      string \
   lv_publicatiedatum            string \
   namespace                     string \
   fme_geometry{0} cityjson_multisurface

FEATURE_DEF WaterBody              \
   fid                      string \
   plus_status              string \
   plus_type                string \
   relatievehoogteligging   string \
   terminationdate          string \
   tijdstipregistratie      string \
   bgt_status               string \
   bronhouder               string \
   class                    string \
   creationdate             string \
   eindregistratie          string \
   inonderzoek              string \
   lokaalid                 string \
   lv_publicatiedatum       string \
   namespace                string \
   fme_geometry{0} cityjson_multisurface
