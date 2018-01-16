/*
 * BaconLibrary.hh
 *
 *  Created on: oct 01, 2016
 *      Author: felipe
 */

#ifndef BACONSTRUCTURES_H
#define BACONSTRUCTURES_H

////////////////////////////////////////////////////////////////////
//  INCLUDES (NOT A CARE IN THE WORLD ABOUT DECOUPLING CODE)
////////////////////////////////////////////////////////////////////

#include <veins/modules/application/ieee80211p/BaseWaveApplLayer.h>
#include <veins/modules/mobility/traci/TraCIMobility.h>
#include <veins/modules/world/annotations/AnnotationManager.h>
//#include <veins/modules/messages/WaveShortMessage_m.h>

#include <omnetpp.h>
#include <omnetpp/cmodule.h>
#include <omnetpp/cconfiguration.h>
#include <omnetpp/cenvir.h>

//#include <osgEarth/MapNode>
//#include <osgEarthAnnotation/CircleNode>
//#include <osgEarthAnnotation/FeatureNode>
//#include <osgEarthUtil/ObjectLocator>
//#include "OsgEarthScene.h"

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <cstring>
#include <math.h>
#include <list>
#include <ctime>
#include <algorithm>
#include <iomanip>
#include <sys/types.h>
#include <sys/stat.h>

using Veins::TraCIMobility;
using Veins::AnnotationManager;


////////////////////////////////////////////////////////////////////
//  POLICY & OTHER SIMULATION RELATED EVALUATION PARAMETERS
////////////////////////////////////////////////////////////////////

enum NodeRole {
    SERVER = 0,         //Servers do not make requests but have all content
    CLIENT = 1,         //Clients request packets but do not have full libraries
    MULE = 2            //Mules only retransmit packets but do not initiate requests nor have libraries from the get-go
};

enum CacheDistributionModel {
    ALL_EQUAL = 0,
    WAVE_THIRDS = 1,
    HALF_PRIORITY = 2
};

enum CacheSection {
    GENERIC = 0,        //Generic All-purpose section of cache, used if no policy is applied
    PRIORITY = 1,       //Priority only cache, where popular and other "important" items are placed
    PERSONAL = 2,       //Local Requests only Cache portion
    FRIEND = 3,         //Friend-cache portion, used for coordination with nodes that have been neighbors for longer periods of time
    STRANGER = 4        //Cache portion used for requests sent from strangers
};

enum LocationCorrelationModel {
    NONE = 0,           //No correlation model
    GRID = 1,           //GRID based rank shift         (Corrupts the overall popularity distribution)
    SAW = 2,            //Weird Saw-based Rank Shift    (From the fucked-up paper)
    NORMAL = 3,         //Normal function based shift   (??)
    BUCKET = 4,         //Bucket-based content placing  (Distribute content into locations available in the map)
    TWITTER = 5,        //Data Trace based model        (Data is loaded from twitter)
    GPS =   6           //GPS
};

enum class AccessRestrictionPolicy {
    NO_RESTRICTION = 0,
    FORWARD_50 = 1,
    ADD_DELAY = 2,
    FORWARD_AND_DELAY = 3,
    LIMIT_HOP = 4,
};

enum CacheInNetworkCoordPolicy {
    NEVER = 0,                      //NEVER KEEP COPY (Nodes never keep copy of messages sent in their downstream)
    LCE = 1,                        //LEAVE COPY EVERYWHERE (All nodes keep a copy of the cached object, even those that are not part of the route)
    LCD = 2,                        //LEAVE COPY DOWN (Only leave copy in direct downstream - Only the first node in the downstream path keeps a copy)
    MCD = 3,                        //MOVE COPY DOWN (Upon complete hop transmission, sender deletes copy. Ultimately, only 1 copy is kept in the downstream)
    PROB = 4,                       //COPY WITH PROBABILITY (Probability function with probability of copy P)
    RC_ONE = 5,                     //RANDOM COPY ONE (Request is copied once (randomly) along returning path)
    DISTANCE = 6,                   //DISTANCE BASED PROBABILISTIC (Probability is inversely proportional to the distance from the requester and this node)
    PROB30p = 7,                    //30% chance of caching
    PROB70p = 8,                    //70% chance of caching
    LOC_PROB = 10,                  //Local Probability Estimation
    LOC_PROB_MIN = 11,              //Local Probability Estimation Filtering for Minimum Value
    LOC_PROB_MAX = 12,              //Local Probability Estimation Filtering for the Maximum Value

    GLOB_PROB = 15,                 //Same as Loc_Prob but with Global Knowledge
    GLOB_PROB_MIN = 16,             //Same as Loc_Prob_Min but with Global Knowledge
    GLOB_PROB_MAX = 17              //Same as Loc_Prob_Max but with Global Knowledge
};

enum CacheReplacementPolicy {
    RANDOM = 0,                     //RANDOM CACHE REPLACEMENT
    LRU = 1,                        //LEAST RECENTLY USED
    LFU = 2,                        //LEAST FREQUENTLY USED
    FIFO = 3,                       //FIRST IN FIRST OUT
    FILO = 4,                       //FIRST IN LAST OUT
    BIG = 5,                        //BIGGEST ITEM
    MULT_FIRST = 6,                 //REPLACE multimedia objects first (LRU within group)
    GPS_FIRST = 7,                  //REPLACE GPS dependent data first (LRU within group)
    ///Something///
    GOD_POPULARITY = 10,            //God-like Global Knowledge based Coordination
    //FREQ_POPULARITY = 20,           //Coordinate Frequency observation based popularity Replacement
};


////////////////////////////////////////////////////////////////////
//  CONTENT RELATED DEFINITIONS
////////////////////////////////////////////////////////////////////

enum class ContentClass {
    BEACON = 0,
    MULTIMEDIA = 1,
    TRAFFIC = 2,
    NETWORK = 3,
    EMERGENCY_SERVICE = 4,
    GPS_DATA = 5
};

enum class ContentPriority {
    PRIORITY_EMERGENCY = 0,
    PRIORITY_HIGH = 1,
    PRIORITY_MEDIUM = 2,
    PRIORITY_LOW = 3,
    PRIORITY_PRIVATE = 4
};

enum class ContentStatus {
    UNSERVED,
    LOCAL,
    UNAVAILABLE,
    AVAILABLE,
    STALE,
    PARCIAL,
    PRIVATE,
    LIVE_FEED,
    SERVED
};

//Generic object description
struct Content_t {
    //std::string contentPrefix;              //String name representation of content prefix
    ContentClass contentClass;              //Type of Content
    long contentIndex;                      //Content Index within its class
    ContentPriority priority;               //Content Class Specific Ranking (1 = top rank, highest probability)
    long popularityRanking;                 //Content Popularity
    long contentSize;                       //Content Size in Bytes
};

struct CachedContent_t {
    Content_t* referenceObject;             //NOTE: This is a reference and not an extension to save memory
    simtime_t lastAccessTime;               //Time in which object was obtained
    long useCount;                          //Number of times content was "requested" while in a Library
    ContentStatus contentStatus;            //Status of Content
    //simtime_t expireTime;                 //Time in which object will become Stale
};

//Variation of a general content object used by Clients to track status of object requests
struct PendingContent_t : CachedContent_t {
    int pendingID;                          //ID for pending request
    simtime_t requestTime;                  //Time in which content was requested
    simtime_t fullfillTime;                 //Time in which content was fulfilled
};


//Used to store knowledge of server-correlated GPS frequency spikes
struct OverheardGPSObject_t {
    std::string contentPrefix;              //String name representation of content prefix
    ContentClass contentClass;              //Type of Content       TODO: This might be redundant!
    double referenceCount;                  //Number of requests this object has had in the current list (sliding window column) - We keep this as a double value to make our calculations later easier
    double referenceOriginCount;            //Number of advertisements that lead to the current Reference Count
};

//List of overheard GPS Stuff
struct OverheardGPSObjectList_t {
    std::list<OverheardGPSObject_t> gpsList;//
    long simTime;                               //Time of the list
};

////////////////////////////////////////////////////////////////////
//  CONNECTION & NETWORK RELATED DEFINITIONS
////////////////////////////////////////////////////////////////////

struct RequestReplyAttempt_t {
    int requestID;
    int providerID;
    int clientID;
    simtime_t responseTime;
};

enum ConnectionStatus {
    ERROR = 0,                  //SOME WEIRD ERROR HAPPENED
    IDLE = 10,                  //NOT DOING ANYTHING

    WAITING_MIN = 20,           //WAITING CONNECTION STATUS LOWER BOUND
    //WAITING_FOR_CACHE = 21,   //WAITING FOR LOCAL CACHE RESPONSE  (NEVER USE, THIS IS ISTANTANEOUS NOW!)
    WAITING_FOR_NETWORK = 22,   //CONTENT IS NOT AVAILABLE LOCALLY AND WE ARE AWAITING FOR NETWORK RESPONSE
    WAITING_FOR_ACCEPT = 23,    //CONTENT WAS FOUND, WAITING FOR CLIENT APPROVAL
    //WAITING_FOR_BACKOFF = 24, //WAITING FOR BACKOFF TIMER TO REPLY FOR CONTENT REQUEST
    WAITING_FOR_CONTENT = 25,   //ACCEPTED CONTENT, WAITING FOR CONTENT DELIVERY (DON'T USE)
    WAITING_MAX = 29,           //WAITING CONNECTION STATUS UPPER BOUND

    TRANSFER_MIN = 30,
    TRANSFER_TRANSFERING = 31,
    TRANSFER_FORWARDING = 32,
    TRANSFER_WAITING_ACK = 33,
    TRANSFER_MAX = 39,

    RECEIVE_MIN = 40,
    RECEIVE_CLIENT = 41,
    //RECEIVE_FORWARDING = 42,
    RECEIVE_MAX = 49,

    DONE_MIN = 60,
    DONE_RECEIVED = 61,
    DONE_NO_CLIENT_REPLY = 62,  //CLIENT HAS NOT REPLIED TO CONTENT TRANSFER PROPOSAL
    DONE_PROVIDED = 63,         //PROVIDED CONTENT TO CLIENT (SERVER ONLY)
    DONE_REJECTED = 64,         //CLIENT ACTIVELY REJECTED CONNECTIVITY (GOT CONTENT FROM SOMEWHERE ELSE?)
    DONE_AVAILABLE = 65,        //CONTENT IS AVAILABLE ATM
    DONE_UNAVAILABLE = 66,      //CONTENT UNAVAILABLE (NO REPLY TO CLIENT)
    DONE_NO_DATA = 67,          //CONNECTION CREATED BUT NO DATA RECEIVED
    DONE_PARTIAL = 68,          //PARTIAL DATA RECEIVED
    DONE_NETWORK_BUSY = 69,     //NETWORK STATUS PREVENTED THIS REQUEST FROM TAKING PLACE
    DONE_FALLBACK = 70,         //CONTENT DELIVERY FAILED, RECEIVED VIA Infrastructure Fallback
    DONE_MAX = 79,
};

enum NetworkLoadStatus {
    EMERGENCY_LOAD = -1,
    LOW_LOAD = 0,
    MEDIUM_LOAD = 50,
    HIGH_LOAD = 80
};

class MessageClass {
    public:
        static const std::string SELF_TIMER;        //General self-timer
        static const std::string INTEREST;          //Content Lookup
        static const std::string INTEREST_REPLY;    //Content Lookup Reply
        static const std::string INTEREST_ACCEPT;   //Content Lookup Reply
        static const std::string INTEREST_CANCEL;   //Content Lookup Reply
        static const std::string UPDATE;            //Status Update Request (Network Information?)
        static const std::string DATA;              //Generic Data message used by Veins Wave Class
        static const std::string DATA_MISSING;      //Data Missing Message Reply
        static const std::string DATA_COMPLETE;     //Data Transmission Completed Message Reply
        static const std::string DATA_INCLUDE;      //Cache Data Inclusion Message
        static const std::string DATA_EXLUDE;       //Cache Data Exclusion Message
        static const std::string BEACON;            //Generic Data message used by Veins Wave Class
        static const std::string GPS_BEACON;        //GPS Beacons used to share information of items
        static const std::string SELF_BEACON_TIMER; //
};

/**
 * Interface to be implemented by mobile nodes to be able to
 * register in ChannelController.
 */
class IMobileNode
{
  public:
    virtual double getX() const  = 0;
    virtual double getY() const  = 0;
    virtual double getLatitude() const  = 0;
    virtual double getLongitude() const = 0;
    virtual double getTxRange() const  = 0;
};

struct LoadAtTime_t {
    int vehicleId;
    simtime_t simTime;
    double loadValue;
};

//Neighbor Representation
struct Neighbor_t {
    long neighborID;
    Coord neighborPosition;
    simtime_t firstContact;
    simtime_t lastContact;
    long neighborCentrality;        //Number of neighbors neighbor reported having
    NetworkLoadStatus load;
};

//
struct NetworkPacket_t {
    simtime_t arrival;
    long bitSize;
    ContentClass type;
};

//
struct Connection_t {
    long requestID;                                         //Unique ID assigned to 1st Node in Connection
    long peerID;                                            //ID of peer this connection
    Coord peerPosition;                                     //Position of the peer node we're connecting to

    int upstreamHopCount;                                   //Number of hops in upstream at current node
    int downstreamHopCount;                                 //Number of hops in downstream at current node
    int downstreamCacheDistance;                            //Number of Hops since object was last cached (cache operations affect this!)

    Content_t* requestedContent;                            //Reference to the Requested Content Object

    std::string requestPrefix;                              //prefix of the content request
    simtime_t requestTime;                                  //Time the request was made

    int remoteHopUseCount;                                  //Number of uses of object at remote note as notified by serving peer

    //bool replyFromPIT;                                    //If connection reply came after checking PIT

    ContentClass contentClass;                              //Content Request CLASS, according to ContentClass list in BaconLibrary
    int obtainedSize;                                       //(Size) Number of chunks of content received (Packet-wise)
    int lostSize;                                           //(Size) Number of chunks of content Lost (Packet-wise)
    int attempts;                                           //Number of attempts since last "communication response/success"

    bool * chunkStatusList;                                 //List (size of contentSize) with status of each chunk
    WaveShortMessage* pendingMessage;                       //Pending Message (for cancelation / transmission timers)
    ConnectionStatus connectionStatus;                      //Connection Status
};

//
class MessageParameter {
public:
    static const std::string CONNECTION_ID;     //"Unique" message ID
    static const std::string PEER_ID;           //Peer ID
    static const std::string TYPE;              //Content Request Type (Used during Message Exchange)
    static const std::string SEQUENCE_NUMBER;   //Offset of chunk during message exchange
    static const std::string CLASS;             //Content Class / Type  : String matches enum ContentClass
    static const std::string INDEX;             //Content Index within its class (for simulation ease we use numbers instead of human readable strings)
    static const std::string PREFIX;            //Prefix in Naming Scheme (Class + Index)
    static const std::string STATUS;            //Status of Content in Response : String matches enum ContentStatus
    static const std::string PRIORITY;          //Priority of Content : String matches enum ContentPopularity
    static const std::string POPULARITY;        //Popularity of Content : String matches enum ContentPopularity
    static const std::string SIZE;              //Total object size
    static const std::string FREQUENCY;         //Frequency of item (either for popularity or something similar)
    static const std::string NEIGHBORS;         //Number of neighbors (Used for neighbor count sharing and neighbor awareness and participation)
    static const std::string COORDINATES;       //Vehicle Location in TraCIMobility XYZ coordinates
    static const std::string HOPS_LAST_CACHE;   //Calculated Hops since last Cache
    static const std::string HOPS_DOWN;         //Calculated Hop Count for current transmission Downstream
    static const std::string HOPS_UP;           //Calculated Hop Count for current transmission Upstream
    static const std::string REQUESTS_AT_SOURCE;//Number of requests at source
    static const std::string CENTRALITY;        //Degree of Centrality (Neighborhood Size)
    static const std::string LOAD;              //Load Perception
};

//Information structure that contains properties of a Category Type
struct ContentCategoryDistribution_t {
    ContentClass category;
    long byteSize;                          //Average size of elements in category
    long count;                             //Average number of items
    ContentPriority priority;               //Priority of packets in category
};

struct Interest_t {
    std::string interestPrefix;                             //prefix of the content request
    simtime_t lastTimeRequested;                            //Time the request was made
    int totalTimesRequested;                                //Number of times the given contest was requested locally
    Content_t* contentReference;                            //Reference to Content Object
    Connection_t* providingConnection;                      //Connection that
    std::vector<int> pendingConnections;                    //Clients that have requested the content
};

struct LocationRequest_t {
    double x;
    double y;
    simtime_t time;
    Content_t* object;
};

inline std::string loadFile (const std::string& path) {
  std::ostringstream buf; std::ifstream input (path.c_str()); buf << input.rdbuf(); return buf.str();
}

#endif
