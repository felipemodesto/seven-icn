/*
 * BaconStatistics.hh
 *
 *  Created on: Mar 11, 2016
 *      Author: felipe
 */

#ifndef BACONSTATISTICS_H
#define BACONSTATISTICS_H


#include <paradise/bacon/Definitions.h>
#include <paradise/bacon/GlobalLibrary.h>
class GlobalLibrary;

using namespace omnetpp;
using namespace std;

class Statistics : public omnetpp::cSimpleModule {

protected:

    Statistics* stats;
    GlobalLibrary* library;
    cMessage *clockTimerMessage;      //Self message sent in timer

    //Data Collection Configuration Setup
    clock_t begin = clock();
    double statisticsStartTime;     //Collection Start Time
    double statisticsStopTime;      //Collection Stop Time
    bool hasStarted;
    bool hasStopped;

    int lastSecond;

    bool collectingStatistics;
    bool collectingPositions;
    bool collectingRequestNames;
    bool collectingLoad;
    bool collectingNeighborhood;

    long int unviableRequests;      //Requests that were not initiated because the client was busy or something similar

    //Data Packet Related Statistics
    long int packetsSent;           //Requests fulfilled by remote node
    long int packetsSelfServed;     //Requests fulfilled locally
    long int packetsForwarded;      //Requests forwarded (is not restrict to complete return path)
    long int packetsUnserved;       //Requests that did not get a response
    long int packetsLost;           //Requests whose response failed after a content server was found
    long int packetsFallenback;     //Requests fulfilled by resorting to infrastructure
    long int chunksSent;
    long int chunksLost;
    long int totalVehicles;
    long int activeVehicles;
    long int multimediaSentPackets;
    long int multimediaUnservedPackets;
    long int multimediaLostPackets;
    long int multimediaLostChunks;
    long int trafficSentPackets;
    long int trafficUnservedPackets;
    long int trafficLostPackets;
    long int trafficLostChunks;
    long int networkSentPackets;
    long int networkUnservedPackets;
    long int networkLostPackets;
    long int networkLostChunks;
    long int emergencySentPackets;
    long int emergencyUnservedPackets;
    long int emergencyLostPackets;
    long int emergencyLostChunks;
    //long int fallbackOutsourcedRequests;

    //Caching statistics
    long int localCacheLateHits;            //PIT RELATED LOCAL HITS
    long int localCacheHits;                //
    long int remoteCacheHits;               //
    long int localCacheMisses;              //
    long int remoteCacheMisses;             //
    long int cacheReplacements;             //

    //Location-dependent statistics logging
    long int gpsAvailableFromLocation;      //Hits due to vehicle being in the correct location
    long int gpsProvisoningAttempts;        //
    long int gpsCacheHits;                  //Preemptive GPS Packets stored
    long int gpsCacheReplacements;          //
    long int gpsPreemptiveRequests;         //Special Preemptive caching requests made based on disclosed made based on server advertisement

    long int serverCacheHits;
    long int backloggedResponses;

    long int createdInterests;
    long int registeredInterests;
    long int fulfilledInterests;

    long int requestsStarted;

    double lastContactTime;
    double lastParticipationlength;

    //Latency related results.
    //Note: Technically because the values are low, we shouldn't extrapolate the upperbound of the variables but lets keep this in mind
    long int totalTransmissionCount;
    long int completeTransmissionCount;
    long int incompleteTransmissionCount;
    double totalTransmissionDelay;
    double completeTransmissionDelay;
    double incompleteTranmissionDelay;

    std::map<std::string,int> vehicleLocationMap;
    std::map<std::string,int> contentRequestLocationMap;
    std::map<std::string,int> contentNameFrequencyMap;
    std::map<double,int> contactDurationMap;
    std::map<double,int> participationLengthMap;

    std::map<int,int> hopDistanceCountMap;
    std::list<omnetpp::cOutVector*> hopDistanceVectorMap;

    std::list<LoadAtTime_t> instantLoadList;
    std::list<LoadAtTime_t> averageLoadList;

    std::list<RequestReplyAttempt_t*> openRequestList;

    string simulationDirectoryFolder;
    string simulationPrefix;
    string requestLocationStatsFile;
    string hopcountFile;
    string locationStatisticsFile;
    string participationLengthStatsFile;
    string neighborhoodStatisticsFile;
    string contentPopularityStatisticsFile;
    string networkInstantLoadStatisticsFile;
    string networkAverageLoadStatisticsFile;
    string generalStatisticsFile;

    omnetpp::cLongHistogram packetsSentHist;
    omnetpp::cLongHistogram packetsForwardedHist;
    omnetpp::cLongHistogram packetsUnservedHist;
    omnetpp::cLongHistogram packetsLostHist;
    omnetpp::cLongHistogram chunksLostHist;
    omnetpp::cLongHistogram serverBusyHist;
    omnetpp::cLongHistogram contentUnavailableHist;
    omnetpp::cLongHistogram hopCountHist;
    omnetpp::cLongHistogram duplicateRequestHist;
    omnetpp::cLongHistogram requestsPerConnectionHist;

    omnetpp::cOutVector fallbackOutsourcedRequestsVect;
    omnetpp::cOutVector distanceFromTweetVect;
    omnetpp::cOutVector unviableRequestsVect;
    omnetpp::cOutVector participationLengthVect;
    omnetpp::cOutVector neighborCountVect;
    omnetpp::cOutVector serverHitVect;
    omnetpp::cOutVector requestsStartedVect;
    omnetpp::cOutVector packetsSentVect;
    omnetpp::cOutVector packetsSelfServVect;
    omnetpp::cOutVector packetsForwardedVect;
    omnetpp::cOutVector packetsUnservedVect;
    omnetpp::cOutVector packetsFallenbackVect;
    omnetpp::cOutVector packetsLostVect;
    omnetpp::cOutVector chunksLostVect;
    omnetpp::cOutVector chunksSentVect;
    omnetpp::cOutVector activeVehiclesVect;
    omnetpp::cOutVector multimediaSentPacketVect;
    omnetpp::cOutVector multimediaUnservedPacketVect;
    omnetpp::cOutVector multimediaLostPacketVect;
    omnetpp::cOutVector multimediaLostChunkVect;
    omnetpp::cOutVector trafficSentPacketVect;
    omnetpp::cOutVector trafficUnservedPacketVect;
    omnetpp::cOutVector trafficLostPacketVect;
    omnetpp::cOutVector trafficLostChunkVect;
    omnetpp::cOutVector networkSentPacketVect;
    omnetpp::cOutVector networkUnservedPacketVect;
    omnetpp::cOutVector networkLostPacketVect;
    omnetpp::cOutVector networkLostChunkVect;
    omnetpp::cOutVector emergencySentPacketVect;
    omnetpp::cOutVector emergencyUnservedPacketVect;
    omnetpp::cOutVector emergencyLostPacketVect;
    omnetpp::cOutVector emergencyLostChunkVect;

    omnetpp::cOutVector localLateCacheHitVect;
    omnetpp::cOutVector localCacheHitVect;
    omnetpp::cOutVector remoteCacheHitVect;
    omnetpp::cOutVector localCacheMissVect;
    omnetpp::cOutVector remoteCacheMissVect;
    omnetpp::cOutVector cacheReplacementVect;
    omnetpp::cOutVector gpsCacheReplacementVect;

    omnetpp::cOutVector overallTranmissionDelayVect;
    omnetpp::cOutVector completeTranmissionDelayVect;
    omnetpp::cOutVector incompleteTranmissionDelayVect;

    omnetpp::cOutVector backloggedClientResponseVect;

    omnetpp::cOutVector statisticsTimekeepingVect;

    omnetpp::cOutVector createdInterestVect;
    omnetpp::cOutVector registeredInterestVect;
    omnetpp::cOutVector fulfilledInterestVect;

protected:
    void handleMessage(cMessage *msg);

public:
    virtual ~Statistics();
    virtual void initialize(int stage);
    virtual void finish();

    virtual int numInitStages () const {
        return 1;
    }

    void keepTime();

    bool allowedToRun();
    bool shouldRecordData();

    double getStartTime();
    double getStopTime();

    bool recordingPosition();
    bool recordingStatistics();

    void startStatistics();                  //Soft Class Initializer
    void stopStatistics();                   //Soft Class Destructor

    string getSimulationDirectory();
    string getSimulationPrefix();

    void logInstantLoad(int node, double load);
    void logAverageLoad(int node, double load);

    void logPosition(double x, double y);                                   //Vehicles record their position every second of simulation
    void logContentRequest(std::string contentName, bool shouldCount, double x, double y);    //Recording the name of each item requested

    void logParticipationDuration(double participationLength);
    void logContactDuration(double contactDuration);

    void logProvisionAttempt(int providerID, int requestID);

    void increasePacketsSelfServed(int myId, int requestID);            //Increase number of Packets Sent to self by 1
    void increasePacketsSelfServed(int x,int myId, int requestID);      //Increase number of Packets Sent to self by X
    void increasePacketsSent(int myId, int requestID);                  //Increase number of Packets Sent by 1
    void increasePacketsSent(int x,int myId, int requestID);            //Increase number of Packets Sent by X
    void increasePacketsForwarded(int myId, int requestID);             //Increase number of Packets Forwarded by 1
    void increasePacketsForwarded(int x,int myId, int requestID);       //Increase number of Packets Forwarded by X
    void increasePacketsUnserved(int myId, int requestID);              //Increase number of Packets that were not served by 1
    void increasePacketsUnserved(int x,int myId, int requestID);        //Increase number of Packets that were not served by X
    void increasePacketsLost(int myId, int requestID);                  //Increase number of Packets Lost by 1
    void increasePacketsLost(int x,int myId, int requestID);            //Increase number of Packets Lost by X
    void increaseChunksLost(int myId, int requestID);                   //Increase number of Chunks Lost by 1
    void increaseChunksLost(int x,int myId, int requestID);             //Increase number of Chunks Lost by X
    void increaseChunksSent(int myId, int requestID);                   //Increase number of Chunks Sent by 1
    void increaseChunksSent(int x,int myId, int requestID);             //Increase number of Chunks Sent by X
    void increasePacketsFallenBack(long int x,int myId, int requestID);
    void increasePacketsFallenBack(int myId, int requestID);

    void increaseServerCacheHits();
    void logDistanceFromTweet(double distance);

    bool increaseActiveVehicles(int vehicleId); //Increase the number of active Vehicles by 1 (new vehicle spawned)
    bool decreaseActiveVehicles(int vehicleId); //Decrease the number of active Vehicles by 1 (vehicle "deleted" / journey complete)

    void increaseGPSMessagesPreSent();
    void increaseMessagesSent(ContentClass cClass);
    void increaseMultimediaMessagesSent();
    void increaseTrafficMessagesSent();
    void increaseNetworkMessagesSent();
    void increaseEmergencyMessagesSent();

    void increaseMessagesLost(ContentClass cClass);
    void increaseMultimediaMessagesLost();
    void increaseTrafficMessagesLost();
    void increaseNetworkMessagesLost();
    void increaseEmergencyMessagesLost();

    void increaseMessagesUnserved(ContentClass cClass);
    void increaseTrafficMessagesUnserved();
    void increaseMultimediaMessagesUnserved();
    void increaseNetworkMessagesUnserved();
    void increaseEmergencyMessagesUnserved();


    void increaseChunkLost(ContentClass cClass);
    void increaseTrafficChunksLost();
    void increaseNetworkChunksLost();
    void increaseEmergencyChunksLost();
    void increaseMultimediaChunksLost();

    void increaseLocalLateCacheHits();
    void increaseLocalCacheHits();
    void increaseRemoteCacheHits();
    void increaseLocalCacheMisses();
    void increaseRemoteCacheMisses();
    void increaseCacheReplacements();

    void addcompleteTransmissionDelay(double delay);
    void addincompleteTransmissionDelay(double delay);
    void increaseHopCountResult(int hopCount);

    void increaseCreatedInterests();
    void increaseRegisteredInterests();
    void increaseFulfilledInterests();

    void increasedUnviableRequests();
    void increasedBackloggedResponses();

    //GPS / Location Correlation Stuff
    void increaseNodeAtGPSLocationCacheHits();
    void increasePLCPreemptiveCacheRequests();
    void increasePLCCacheHits();
    void increasePLCCacheReplacements();
    void increasePLCProvisioningAttempts();

    //Location-dependent statistics logging
    //long int gpsProvisoningAttempts;
    //long int gpsCacheHits;                //Preemptive GPS Packets stored
    //long int gpsCacheReplacements;
    //long int gpsPreemptiveRequests;

    //HISTOGRAMS
    void setPacketsSent(int x);             //Set the number of packets sent by a vehicle (only called on vehicle destructor)
    void setPacketsUnserved(int x);         //Set the number of packets not served to a vehicle (only called on vehicle destructor)
    void setPacketsLost(int x);             //Set the number of packets lost (vehicle receiving) (only called on vehicle destructor)
    void setChunksLost(int x);              //Set the number of chunks lost (vehicle receiving) (only called on vehicle destructor)
    void setServerBusy(int x);              //Set the number of responses with "server busy" (only called on vehicle destructor)
    void setContentUnavailable(int x);      //Set the number of responses with "Content Unavailable" (only called on vehicle destructor)

    void logInterestForConnection(int connection);
    void logDuplicateMessagesForConnection(int duplicates,int connection);

    void setHopsCount(int hops);            //Set the number of messages with a given hop count
    void setHopsCount(int hops, int count); //Set the number of messages with a given hop count

};

#endif /* BACONSTATISTICS_H */
