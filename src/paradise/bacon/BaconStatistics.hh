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

    //Data Packet Related Statistics
    int packetsSent;
    int packetsForwarded;
    int packetsUnserved;
    int packetsLost;
    int chunksLost;
    int totalVehicles;
    int activeVehicles;
    int multimediaSentPackets;
    int multimediaUnservedPackets;
    int multimediaLostPackets;
    int multimediaLostChunks;
    int trafficSentPackets;
    int trafficUnservedPackets;
    int trafficLostPackets;
    int trafficLostChunks;
    int networkSentPackets;
    int networkUnservedPackets;
    int networkLostPackets;
    int networkLostChunks;
    int emergencySentPackets;
    int emergencyUnservedPackets;
    int emergencyLostPackets;
    int emergencyLostChunks;

    //Caching statistics
    int localCacheLateHits; //PIT RELATED LOCAL HITS
    int localCacheHits;
    int remoteCacheHits;
    int localCacheMisses;
    int remoteCacheMisses;
    int cacheReplacements;

    int serverCacheHits;
    int backloggedResponses;

    int createdInterests;
    int registeredInterests;
    int fulfilledInterests;

    std::map<std::string,int> locationMap;
    std::map<std::string,int> contentRequestMap;

    std::map<int,int> hopDistanceCountMap;
    //std::map<int,omnetpp::cOutVector> hopDistanceVectorMap;
    std::list<omnetpp::cOutVector*> hopDistanceVectorMap;

    const char * locationStatisticsFile;
    const char * contentPopularityStatisticsFile;

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

    omnetpp::cOutVector serverHitVect;

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

    void logPosition(double x, double y);               //Vehicles record their position every second of simulation
    void logContentRequest(std::string contentName);    //Recording the name of each item requested

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

    void increaseActiveVehicles();          //Increase the number of active Vehicles by 1 (new vehicle spawned)
    void decreaseActiveVehicles();          //Decrease the number of active Vehicles by 1 (vehicle "deleted" / journey complete)

    void increaseMultimediaMessagesSent();
    void increaseMultimediaMessagesUnserved();
    void increaseMultimediaMessagesLost();
    void increaseMultimediaChunksLost();
    void increaseTrafficMessagesSent();
    void increaseTrafficMessagesUnserved();
    void increaseTrafficMessagesLost();
    void increaseTrafficChunksLost();
    void increaseNetworkMessagesSent();
    void increaseNetworkMessagesUnserved();
    void increaseNetworkMessagesLost();
    void increaseNetworkChunksLost();
    void increaseEmergencyMessagesSent();
    void increaseEmergencyMessagesUnserved();
    void increaseEmergencyMessagesLost();
    void increaseEmergencyChunksLost();

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
