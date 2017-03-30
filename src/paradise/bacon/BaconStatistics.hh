/*
 * BaconStatistics.hh
 *
 *  Created on: Mar 11, 2016
 *      Author: felipe
 */

#ifndef BACONSTATISTICS_H
#define BACONSTATISTICS_H


#include <paradise/bacon/BaconStructures.hh>
//#include <BaconStructures.hh>
#include <paradise/bacon/BaconLibrary.hh>
class BaconLibrary;

using namespace omnetpp;
using namespace std;

class BaconStatistics : public omnetpp::cSimpleModule {

protected:

    BaconStatistics* stats;
    BaconLibrary* library;
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

    //Data Packet Related Statistics
    long int packetsSent;
    long int packetsForwarded;
    long int packetsUnserved;
    long int packetsLost;
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

    //Caching statistics
    long int localCacheLateHits; //PIT RELATED LOCAL HITS
    long int localCacheHits;
    long int remoteCacheHits;
    long int localCacheMisses;
    long int remoteCacheMisses;
    long int cacheReplacements;

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

    std::map<std::string,int> locationMap;
    std::map<std::string,int> contentRequestMap;
    std::map<double,int> contactDurationMap;
    std::map<double,int> participationLengthMap;

    std::map<int,int> hopDistanceCountMap;
    //std::map<int,omnetpp::cOutVector> hopDistanceVectorMap;
    std::list<omnetpp::cOutVector*> hopDistanceVectorMap;

    std::list<LoadAtTime_t> instantLoadList;
    std::list<LoadAtTime_t> averageLoadList;

    const char * locationStatisticsFile;
    const char * participationLengthStatsFile;
    const char * neighborhoodStatisticsFile;
    const char * contentPopularityStatisticsFile;
    const char * networkInstantLoadStatisticsFile;
    const char * networkAverageLoadStatisticsFile;
    const char * generalStatisticsFile;

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

    omnetpp::cOutVector participationLengthVect;
    omnetpp::cOutVector neighborCountVect;
    omnetpp::cOutVector serverHitVect;
    omnetpp::cOutVector requestsStartedVect;
    omnetpp::cOutVector packetsSentVect;
    omnetpp::cOutVector packetsForwardedVect;
    omnetpp::cOutVector packetsUnservedVect;
    omnetpp::cOutVector packetsLostVect;
    omnetpp::cOutVector chunksLostVect;
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

    void logInstantLoad(int node, double load);
    void logAverageLoad(int node, double load);

    void logPosition(double x, double y);                                   //Vehicles record their position every second of simulation
    void logContentRequest(std::string contentName, bool shouldCount);    //Recording the name of each item requested

    void logParticipationDuration(double participationLength);
    void logContactDuration(double contactDuration);

    void increasePacketsSent();                 //Increase number of Packets Sent by 1
    void increasePacketsSent(int x);            //Increase number of Packets Sent by X
    void increasePacketsForwarded();            //Increase number of Packets Forwarded by 1
    void increasePacketsForwarded(int x);       //Increase number of Packets Forwarded by X
    void increasePacketsUnserved();             //Increase number of Packets that were not served by 1
    void increasePacketsUnserved(int x);        //Increase number of Packets that were not served by X
    void increasePacketsLost();                 //Increase number of Packets Lost by 1
    void increasePacketsLost(int x);            //Increase number of Packets Lost by X
    void increaseChunksLost();                  //Increase number of Chunks Lost by 1
    void increaseChunksLost(int x);             //Increase number of Chunks Lost by X

    void increaseServerCacheHits();

    bool increaseActiveVehicles(int vehicleId); //Increase the number of active Vehicles by 1 (new vehicle spawned)
    bool decreaseActiveVehicles(int vehicleId); //Decrease the number of active Vehicles by 1 (vehicle "deleted" / journey complete)

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

    void increasedBackloggedResponses();

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
