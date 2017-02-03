/*
 * BaconLibrary.hh
 *
 *  Created on: oct 01, 2016
 *      Author: felipe
 */

#ifndef BACONSTRUCTURES_H
#define BACONSTRUCTURES_H

#include <omnetpp.h>
#include <omnetpp/cmodule.h>

#include <osgEarth/MapNode>
#include <osgEarthAnnotation/CircleNode>
#include <osgEarthAnnotation/FeatureNode>
#include <osgEarthUtil/ObjectLocator>
#include "OsgEarthScene.h"

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <cstring>
#include <math.h>
#include <list>
#include <ctime>

//#include <paradise/bacon/BaconLibrary.hh>
//#include <paradise/bacon/BaconServiceManager.hh>
//#include <paradise/bacon/BaconStatistics.hh>

//#include <paradise/bacon/BaconStatistics.hh>
//class BaconStatistics;

#include <veins/modules/mobility/traci/TraCIMobility.h>
#include <veins/modules/world/annotations/AnnotationManager.h>

#include <veins/modules/application/ieee80211p/BaseWaveApplLayer.h>
#include <veins/modules/messages/WaveShortMessage_m.h>


using Veins::TraCIMobility;
using Veins::AnnotationManager;

enum class ContentClass         {MULTIMEDIA = 1, TRAFFIC = 2, NETWORK = 3, EMERGENCY_SERVICE = 4};
enum class ContentPopularity    {MANDATORY, POPULAR, AVERAGE, UNPOPULAR, PRIVATE};
enum class ContentStatus        {UNSERVED, LOCAL, UNAVAILABLE, AVAILABLE, STALE, PARCIAL, PRIVATE, LIVE_FEED, SERVED};

enum CacheCoordinationPolicy {
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

    GOD_POPULARITY = 10,            //God-like Global Knowledge based Coordination
    FREQ_POPULARITY = 20,           //Coordinate Frequency observation based popularity Replacement
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
    DONE_AVAILABLE = 65,
    DONE_UNAVAILABLE = 66,      //CONTENT UNAVAILABLE (NO REPLY TO CLIENT)
    DONE_NO_DATA = 67,          //CONNECTION CREATED BUT NO DATA RECEIVED
    DONE_PARTIAL = 68,          //PARTIAL DATA RECEIVED
    DONE_MAX = 69,
};

class MessageClass {
    public:
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

//Information Structure that contains the "description" of a piece of content
struct Content_t {
    ContentClass contentClass;              //Type of Content
    ContentStatus contentStatus;            //Status of Content
    long popularityRanking;                 //Content Class Specific Ranking (1 = top rank, highest probability)
    long contentSize;                       //Content Size in Bytes
    long useCount;                          //Number of times content was "requested" while in a Library
    std::string contentPrefix;              //String name representation of content prefix
    simtime_t lastAccessTime;               //Time in which object was obtained
    //simtime_t expireTime;                 //Time in which object will become Stale
};

struct PendingContent_t : Content_t {
    int pendingID;                          //ID for pending request
    simtime_t requestTime;                  //Time in which content was requested
    simtime_t fullfillTime;                 //Time in which content was fulfilled
};

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

class MessageParameter {
public:
    static const std::string CONNECTION_ID;     //"Unique" message ID
    static const std::string PEER_ID;           //Peer ID
    static const std::string TYPE;              //Content Request Type (Used during Message Exchange)
    static const std::string SEQUENCE_NUMBER;   //Offset of chunk during message exchange
    static const std::string CLASS;             //Content Class / Type  : String matches enum ContentClass
    static const std::string PREFIX;            //Prefix in Naming Scheme
    static const std::string STATUS;            //Status of Content in Response : String matches enum ContentStatus
    static const std::string PRIORITY;          //Priority of Content : String matches enum ContentPopularity
    static const std::string POPULARITY;        //Popularity of Content : String matches enum ContentPopularity
    static const std::string SIZE;              //Total object size
    static const std::string COORDINATES;       //Vehicle Location in TraCIMobility XYZ coordinates
    static const std::string HOPS_LAST_CACHE;   //Calculated Hops since last Cache
    static const std::string HOPS_DOWN;         //Calculated Hop Count for current transmission Downstream
    static const std::string HOPS_UP;           //Calculated Hop Count for current transmission Upstream
    static const std::string REQUESTS_AT_SOURCE;//Number of requests at source
};

//Information structure that contains properties of a Category Type
struct ContentCategoryDistribution_t {
    ContentClass category;
    long averageByteSize;                   //Average size of elements in category
    long averageCount;                      //Average number of items
    long averagePriority;                   //Priority of packets in category (1-100)
    ContentPopularity averagePopularity;    //Average Popularity of items
};

struct Interest_t {
    std::string interestPrefix;                             //prefix of the content request
    simtime_t lastTimeRequested;                            //Time the request was made
    int totalTimesRequested;                                //Number of times the given contest was requested locally
    Content_t* contentReference;                            //Reference to Content Object
    Connection_t* providingConnection;                      //Connection that
    std::vector<int> pendingConnections;                    //Clients that have requested the content
};

#endif
