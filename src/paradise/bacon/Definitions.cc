#include <paradise/bacon/Definitions.h>

const std::string MessageParameter::CONNECTION_ID       = "connection_id";          //
const std::string MessageParameter::TYPE                = "type";                   //Used by Data Transfers
const std::string MessageParameter::SEQUENCE_NUMBER     = "sequence_number";        //
const std::string MessageParameter::PEER_ID             = "peer_id";                //
const std::string MessageParameter::CLASS               = "class";                  //
const std::string MessageParameter::PREFIX              = "prefix";                 //
const std::string MessageParameter::STATUS              = "status";                 //
const std::string MessageParameter::PRIORITY            = "priority";               //
const std::string MessageParameter::POPULARITY          = "popularity";             //
const std::string MessageParameter::SIZE                = "size";                   //
const std::string MessageParameter::FREQUENCY           = "frequency";              //
const std::string MessageParameter::COORDINATES         = "coordinates";            //
const std::string MessageParameter::HOPS_LAST_CACHE     = "hops_last_cache";        //
const std::string MessageParameter::HOPS_DOWN           = "hops_down";              //
const std::string MessageParameter::HOPS_UP             = "hops_up";                //
const std::string MessageParameter::REQUESTS_AT_SOURCE  = "requests_at_source";     //
const std::string MessageParameter::CENTRALITY          = "centrality";             //
const std::string MessageParameter::LOAD                = "local_load";             //Parameter that stores local Load Perception


const std::string MessageClass::SELF_TIMER = "selfTimer";
const std::string MessageClass::INTEREST = "data_lookup";
const std::string MessageClass::INTEREST_REPLY = "data_lookup_reply";
const std::string MessageClass::INTEREST_ACCEPT = "data_lookup_accept";
const std::string MessageClass::INTEREST_CANCEL = "data_lookup_cancel";
const std::string MessageClass::UPDATE = "status_update";
const std::string MessageClass::DATA = "data";
const std::string MessageClass::DATA_COMPLETE = "data_complete";
const std::string MessageClass::DATA_MISSING = "data_missing";
const std::string MessageClass::DATA_INCLUDE = "data_include";
const std::string MessageClass::DATA_EXLUDE = "data_exclude";
const std::string MessageClass::BEACON = "beacon";
const std::string MessageClass::GPS_BEACON = "gps_beacon";
const std::string MessageClass::SELF_BEACON_TIMER = "selfBeaconTimer";
