/*
 * BaconStatistics.cc
 *
 *  https://omnetpp.org/pmwiki/index.php?n=Main.CollectingGlobalStatistics
 *
 *  Created on: Mar 11, 2016
 *      Author: felipe
 */

#include <paradise/bacon/BaconStatistics.hh>

Define_Module(BaconStatistics);

//Initialization Function
void BaconStatistics::initialize(int stage) {
    //Initializing
    if (stage == 0) {
        hasStarted = false;
        hasStopped = false;
        lastSecond = 0;

        collectingStatistics = par("collectingStatistics").boolValue();
        collectingRequestNames = par("collectingRequestNames").boolValue();
        collectingPositions = par("collectingPositions").boolValue();
        statisticsStartTime = par("statisticsStartTime").doubleValue();
        statisticsStopTime = par("statisticsStopTime").doubleValue();

        locationStatisticsFile = par("locationStatisticsFile").stringValue();
        contentPopularityStatisticsFile = par("contentNameStatisticsFile").stringValue();
        networkInstantLoadStatisticsFile = par("networkInstantLoadStatisticsFile").stringValue();
        networkAverageLoadStatisticsFile = par("networkAverageLoadStatisticsFile").stringValue();
        generalStatisticsFile = par("generalStatisticsFile").stringValue();

        startStatistics();
    }
}

//Finalization Function (not a destructor!)
void BaconStatistics::finish() {
    stopStatistics();
}

//
bool BaconStatistics::allowedToRun() {
    if (statisticsStartTime < omnetpp::simTime().dbl() && omnetpp::simTime().dbl() < statisticsStopTime ) return true;
    return false;
}

//Getter for Simulation Start Time
double BaconStatistics::getStartTime() {
    return statisticsStartTime;
}

//Getter for Simulation Stop Time
double BaconStatistics::getStopTime() {
    return statisticsStopTime;
}

//Getter for Simulation Control parameter related to statistics collection
bool BaconStatistics::recordingStatistics() {
    return collectingStatistics;
}

//Getter for Simulation Control parameter related to position tracking
bool BaconStatistics::recordingPosition() {
    return collectingPositions;
}

//
void BaconStatistics::keepTime() {
    int newTime = static_cast<int>(simTime().dbl());
    if (lastSecond < newTime) {
        lastSecond = newTime;

        clock_t end = clock();
        double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
        begin = end;

        std::cout << "(St) Current Time is: " << lastSecond << " \t CPU Time: " << round(elapsed_secs) << " sec(s)\n";
        std::cout.flush();

        if (hasStarted && !hasStopped) statisticsTimekeepingVect.record( (lastSecond - statisticsStartTime) / (double) (statisticsStopTime - statisticsStartTime) );
    }
}

//
bool BaconStatistics::shouldRecordData() {
    keepTime();

    if (!allowedToRun()) return false;
    if (!collectingStatistics and !collectingPositions) return false;
    if (!hasStarted || hasStopped) return false;
    return true;
}

//
void BaconStatistics::startStatistics() {
    if (hasStarted || hasStopped || !collectingStatistics) return;

    hasStarted = false;
    hasStopped = false;

    packetsSent = 0;
    packetsForwarded = 0;
    packetsUnserved = 0;
    packetsLost = 0;
    chunksLost = 0;
    totalVehicles = 0;
    activeVehicles = 0;
    multimediaSentPackets = 0;
    multimediaUnservedPackets = 0;
    multimediaLostPackets = 0;
    multimediaLostChunks = 0;
    trafficSentPackets = 0;
    trafficUnservedPackets = 0;
    trafficLostPackets = 0;
    trafficLostChunks = 0;
    networkSentPackets = 0;
    networkUnservedPackets = 0;
    networkLostPackets = 0;
    networkLostChunks = 0;
    emergencySentPackets = 0;
    emergencyUnservedPackets = 0;
    emergencyLostPackets = 0;
    emergencyLostChunks = 0;

    localCacheLateHits = 0;
    localCacheHits = 0;
    remoteCacheHits = 0;
    localCacheMisses = 0;
    remoteCacheMisses = 0;
    cacheReplacements = 0;

	serverCacheHits = 0;
	backloggedResponses = 0;

    createdInterests = 0;
    registeredInterests = 0;
    fulfilledInterests = 0;

    packetsSentHist.setName("PacketsSent");
    packetsForwardedHist.setName("PacketsForwarded");
    packetsUnservedHist.setName("PacketsUnserved");
    packetsLostHist.setName("PacketsLost");
    chunksLostHist.setName("ChunksLost");
    serverBusyHist.setName("ServerBusy");
    contentUnavailableHist.setName("ContentUnavailable");

    packetsSentVect.setName("SentPackets");
    packetsForwardedVect.setName("ForwardedPackets");
    packetsUnservedVect.setName("UnservedPackets");
    packetsLostVect.setName("LostPackets");
    chunksLostVect.setName("LostChunks");
    activeVehiclesVect.setName("ActiveVehicles");
    multimediaSentPacketVect.setName("MultimediaSentPackets");
    multimediaUnservedPacketVect.setName("MultimediaUnservedPackets");
    multimediaLostPacketVect.setName("MultimediaLostPackets");
    multimediaLostChunkVect.setName("MultimediaLostChunks");
    trafficSentPacketVect.setName("TrafficSentPackets");
    trafficUnservedPacketVect.setName("TrafficUnservedPackets");
    trafficLostPacketVect.setName("TrafficLostPackets");
    trafficLostChunkVect.setName("TrafficLostChunks");
    networkSentPacketVect.setName("NetworkSentPackets");
    networkUnservedPacketVect.setName("NetworkUnservedPackets");
    networkLostPacketVect.setName("NetworkLostPackets");
    networkLostChunkVect.setName("NetworkLostChunks");
    emergencySentPacketVect.setName("EmergencySentPackets");
    emergencyUnservedPacketVect.setName("EmergencyUnservedPackets");
    emergencyLostPacketVect.setName("EmergencyLostPackets");
    emergencyLostChunkVect.setName("EmergencyLostChunks");

    serverHitVect.setName("ServerCacheHits");
    backloggedClientResponseVect.setName("BackloggedResponses");

    createdInterestVect.setName("InterestsCreated");
    registeredInterestVect.setName("InterestsRegistered");
    fulfilledInterestVect.setName("InterestsFulfilled");

    localLateCacheHitVect.setName("LocalLateCacheHits");
    localCacheHitVect.setName("LocalCacheHits");
    remoteCacheHitVect.setName("RemoteCacheHits");
    localCacheMissVect.setName("LocalCacheMisses");
    remoteCacheMissVect.setName("RemoteCacheMisses");
    cacheReplacementVect.setName("CacheReplacements");
    hopCountHist.setName("HopCount");
    duplicateRequestHist.setName("DuplicateRequests");
    requestsPerConnectionHist.setName("InterestCount");

    overallTranmissionDelayVect.setName("CommunicationLatency");
    completeTranmissionDelayVect.setName("CompleteCommunicationLatency");
    incompleteTranmissionDelayVect.setName("FailedCommunicationLatency");

    statisticsTimekeepingVect.setName("CompletionPercentage");

    packetsSentVect.record(0);
    packetsUnservedVect.record(0);
    packetsLostVect.record(0);
    chunksLostVect.record(0);
    activeVehiclesVect.record(0);
    multimediaSentPacketVect.record(0);
    multimediaUnservedPacketVect.record(0);
    multimediaLostPacketVect.record(0);
    multimediaLostChunkVect.record(0);
    trafficSentPacketVect.record(0);
    trafficUnservedPacketVect.record(0);
    trafficLostPacketVect.record(0);
    trafficLostChunkVect.record(0);
    networkSentPacketVect.record(0);
    networkUnservedPacketVect.record(0);
    networkLostPacketVect.record(0);
    networkLostChunkVect.record(0);
    emergencySentPacketVect.record(0);
    emergencyUnservedPacketVect.record(0);
    emergencyLostPacketVect.record(0);
    emergencyLostChunkVect.record(0);

    serverHitVect.record(0);
    backloggedClientResponseVect.record(0);

    createdInterestVect.record(0);
    registeredInterestVect.record(0);
    fulfilledInterestVect.record(0);

    localLateCacheHitVect.record(0);
    localCacheHitVect.record(0);
    remoteCacheHitVect.record(0);
    localCacheMissVect.record(0);
    remoteCacheMissVect.record(0);
    cacheReplacementVect.record(0);

    hasStarted = true;
}

//
void BaconStatistics::stopStatistics() {
    if (hasStopped) return;

    //If we're logging vehicle positions, we'll save a log csv file with our position matrix
    if (collectingPositions) {
        FILE * pFile;
        pFile = fopen ( locationStatisticsFile, "w");
        fprintf(pFile, "X,Y,Count\n");
        for(auto iterator = locationMap.begin(); iterator != locationMap.end(); iterator++) {
            fprintf(pFile, "%s,%i\n",iterator->first.c_str(),iterator->second);
        }
        fclose(pFile);
    }

    //If we're logging vehicle positions, we'll save a log csv file with our position matrix
    if (collectingRequestNames) {
        FILE * pFile;
        pFile = fopen ( contentPopularityStatisticsFile, "w");
        fprintf(pFile, "#name,requests\n");
        for(auto iterator = contentRequestMap.begin(); iterator != contentRequestMap.end(); iterator++) {
            //if (iterator->second != 0) {
                fprintf(pFile, "%s,%i\n",iterator->first.c_str(),iterator->second);
            //}
        }
        fclose(pFile);
    }

    //Calculating Average Delays
    totalTransmissionDelay = totalTransmissionDelay/(double)totalTransmissionCount;
    completeTransmissionDelay += completeTransmissionDelay/(double)completeTransmissionCount;
    incompleteTranmissionDelay += incompleteTranmissionDelay/(double)incompleteTransmissionCount;

    //Saving Network Load Statistics
    FILE * pFile;
    pFile = fopen ( networkInstantLoadStatisticsFile, "w");
    fprintf(pFile, "#time,id,load\n");
    for(auto iterator = instantLoadList.begin(); iterator != instantLoadList.end(); iterator++) {
        fprintf(pFile,"%f,%i,%f\n",iterator->simTime.dbl(),iterator->vehicleId,iterator->loadValue);
    }
    fclose(pFile);
    pFile = fopen ( networkAverageLoadStatisticsFile, "w");
    fprintf(pFile, "#time,id,load\n");
    for(auto iterator = averageLoadList.begin(); iterator != averageLoadList.end(); iterator++) {
        fprintf(pFile,"%f,%i,%f\n",iterator->simTime.dbl(),iterator->vehicleId,iterator->loadValue);
    }
    fclose(pFile);

    //Saving Communication statistics previously saved as scalar and vector files
    pFile = fopen ( generalStatisticsFile, "w");
    fprintf(pFile,"averageLatency,averageCompletedLatency,averageFailedLatency");
    fprintf(pFile,"packetsSent,packetsForwarded,packetsUnserved,packetsLost,chunksLost,totalVehicles,activeVehicles,multimediaSentPackets,multimediaUnservedPackets,multimediaLostPackets,multimediaLostChunks,trafficSentPackets,trafficUnservedPackets,trafficLostPackets,trafficLostChunks,networkSentPackets,networkUnservedPackets,networkLostPackets,networkLostChunks,emergencySentPackets,emergencyUnservedPackets,emergencyLostPackets,emergencyLostChunks,localCacheHits,remoteCacheHits,localCacheMisses,remoteCacheMisses,cacheReplacements");

    fprintf(pFile,"%f,%f,%f",totalTransmissionDelay,completeTransmissionDelay,incompleteTranmissionDelay);

    fprintf(pFile,"%ld",packetsSent);
    fprintf(pFile,"%ld",packetsForwarded);
    fprintf(pFile,"%ld",packetsUnserved);
    fprintf(pFile,"%ld",packetsLost);
    fprintf(pFile,"%ld",chunksLost);
    fprintf(pFile,"%ld",multimediaSentPackets);
    fprintf(pFile,"%ld",multimediaUnservedPackets);
    fprintf(pFile,"%ld",multimediaLostPackets);
    fprintf(pFile,"%ld",multimediaLostChunks);
    fprintf(pFile,"%ld",trafficSentPackets);
    fprintf(pFile,"%ld",trafficUnservedPackets);
    fprintf(pFile,"%ld",trafficLostPackets);
    fprintf(pFile,"%ld",trafficLostChunks);
    fprintf(pFile,"%ld",networkSentPackets);
    fprintf(pFile,"%ld",networkUnservedPackets);
    fprintf(pFile,"%ld",networkLostPackets);
    fprintf(pFile,"%ld",networkLostChunks);
    fprintf(pFile,"%ld",emergencySentPackets);
    fprintf(pFile,"%ld",emergencyUnservedPackets);
    fprintf(pFile,"%ld",emergencyLostPackets);
    fprintf(pFile,"%ld",emergencyLostChunks);
    fprintf(pFile,"%ld",localCacheHits);
    fprintf(pFile,"%ld",remoteCacheHits);
    fprintf(pFile,"%ld",localCacheMisses);
    fprintf(pFile,"%ld",remoteCacheMisses);
    fprintf(pFile,"%ld",cacheReplacements);

    fclose(pFile);

    //I have since decided not to record statistics using the stuff provided by omnet, it takes too much space.
    /*
    //Recording Scalar Variables
    recordScalar("packetsSent",packetsSent);
    recordScalar("packetsForwarded",packetsForwarded);
    recordScalar("packetsUnserved",packetsUnserved);
    recordScalar("packetsLost",packetsLost);
    recordScalar("chunksLost",chunksLost);
    //
    recordScalar("totalVehicles",totalVehicles);
    recordScalar("activeVehicles",activeVehicles);
    //
    recordScalar("multimediaSentPackets",multimediaSentPackets);
    recordScalar("multimediaUnservedPackets",multimediaUnservedPackets);
    recordScalar("multimediaLostPackets",multimediaLostPackets);
    recordScalar("multimediaLostChunks",multimediaLostChunks);
    recordScalar("trafficSentPackets",trafficSentPackets);
    recordScalar("trafficUnservedPackets",trafficUnservedPackets);
    recordScalar("trafficLostPackets",trafficLostPackets);
    recordScalar("trafficLostChunks",trafficLostChunks);
    recordScalar("networkSentPackets",networkSentPackets);
    recordScalar("networkUnservedPackets",networkUnservedPackets);
    recordScalar("networkLostPackets",networkLostPackets);
    recordScalar("networkLostChunks",networkLostChunks);
    recordScalar("emergencySentPackets",emergencySentPackets);
    recordScalar("emergencyUnservedPackets",emergencyUnservedPackets);
    recordScalar("emergencyLostPackets",emergencyLostPackets);
    recordScalar("emergencyLostChunks",emergencyLostChunks);
    //
    recordScalar("localCacheHits",localCacheHits);
    recordScalar("remoteCacheHits",remoteCacheHits);
    recordScalar("localCacheMisses",localCacheMisses);
    recordScalar("remoteCacheMisses",remoteCacheMisses);
    recordScalar("cacheReplacements",cacheReplacements);
    //
    //
    //Recording Histograms
    packetsSentHist.recordAs("PacketsSent");
    packetsUnservedHist.recordAs("PacketsUnserved");
    packetsLostHist.recordAs("PacketsLost");
    chunksLostHist.recordAs("ChunksLost");
    serverBusyHist.recordAs("ServerBusy");
    contentUnavailableHist.recordAs("ContentUnavailable");
    hopCountHist.recordAs("HopCount");
    duplicateRequestHist.recordAs("DuplicateRequests");
    */

    hasStopped = true;

    //std::cout << "(St) STATISTICS COLLECTION IS DONE!\n";
    //std::cout.flush();

}

//
void BaconStatistics::logPosition(double x, double y) {
    if (!collectingPositions) return;
    //if (!shouldRecordData()) return;

    std::string currentLocation = "" + std::to_string(floor(x)) + "," + std::to_string(floor(y));

    //Trying to insert new location tuple
    if(locationMap.insert(std::make_pair(currentLocation, 1)).second == false) {
        //If object already exists lets increase its value
        locationMap[currentLocation] = locationMap[currentLocation] + 1;
    }
}

//
void BaconStatistics::logContentRequest(std::string contentName) {
    if (!collectingRequestNames) return;

    //Trying to insert new content request tuple
    //NOTE: Content requests are added as 0 because we include them ALL when initializing the content library
    if(contentRequestMap.insert(std::make_pair(contentName, 0)).second == false) {
        //If object already exists lets increase its value
        contentRequestMap[contentName] = contentRequestMap[contentName] + 1;
    }
}

void BaconStatistics::increasedBackloggedResponses(){
    BaconStatistics::backloggedResponses++;
    if (!shouldRecordData()) return;

    backloggedClientResponseVect.record(backloggedResponses);
}

//Increase number of Packets Sent by X
void BaconStatistics::increasePacketsSent(int x) {
    BaconStatistics::packetsSent+= x;
    if (!shouldRecordData()) return;
    //std::cout << "(St) +" << x << " good transfer.\n";
    //std::cout.flush();
    packetsSentVect.record(packetsSent);
}

//Increase number of Packets Sent by 1
void BaconStatistics::increasePacketsSent(){
    increasePacketsSent(1);
}

//Increase number of Packets Forwarded by X
void BaconStatistics::increasePacketsForwarded(int x) {
    BaconStatistics::packetsForwarded+= x;
    if (!shouldRecordData()) return;

    packetsForwardedVect.record(packetsForwarded);
}

//Increase number of Packets Forwarded by 1
void BaconStatistics::increasePacketsForwarded(){
    increasePacketsForwarded(1);
}

//Increase number of Packets were not served by X
void BaconStatistics::increasePacketsUnserved(int x) {
    BaconStatistics::packetsUnserved+= x;
    if (!shouldRecordData()) return;

    packetsUnservedVect.record(packetsUnserved);
}

//Increase number of Packets were not served by 1
void BaconStatistics::increasePacketsUnserved(){
    increasePacketsUnserved(1);
}

//Increase number of Packets Lost by X
void BaconStatistics::increasePacketsLost(int x) {
    if (!shouldRecordData()) return;

    BaconStatistics::packetsLost += x;
    packetsLostVect.record(packetsLost);
}

//Increase number of Packets Lost by 1
void BaconStatistics::increasePacketsLost() {
    increasePacketsLost(1);
}

//Increase number of Chunks Lost by X
void BaconStatistics::increaseChunksLost(int x) {
    BaconStatistics::chunksLost += x;
    if (!shouldRecordData()) return;

    chunksLostVect.record(chunksLost);
}

//Increase number of Chunks Lost by 1
void BaconStatistics::increaseChunksLost() {
    increaseChunksLost(1);
}

void BaconStatistics::setHopsCount(int hops) {
    //if (!shouldRecordData()) return;
    hopCountHist.collect(hops);

}

void BaconStatistics::setHopsCount(int hops, int count) {
    //if (!shouldRecordData()) return;
    for (int i = 0; i < count ; i++) {
        hopCountHist.collect(hops);
    }
}

void BaconStatistics::setPacketsSent(int x) {
    //if (!shouldRecordData()) return;
    packetsSentHist.collect(x);
}

void BaconStatistics::setPacketsUnserved(int x) {
    //if (!shouldRecordData()) return;
    packetsUnservedHist.collect(x);
}

void BaconStatistics::setPacketsLost(int x) {
    //if (!shouldRecordData()) return;
    packetsLostHist.collect(x);
}

void BaconStatistics::setChunksLost(int x) {
    //if (!shouldRecordData()) return;
    chunksLostHist.collect(x);
}

void BaconStatistics::setServerBusy(int x) {
    //if (!shouldRecordData()) return;
    serverBusyHist.collect(x);
}

void BaconStatistics::setContentUnavailable(int x) {
   // if (!shouldRecordData()) return;
    contentUnavailableHist.collect(x);
}

void BaconStatistics::logInterestForConnection(int connectionID) {
   // if (!shouldRecordData()) return;
    requestsPerConnectionHist.collect(connectionID);
}

void BaconStatistics::logDuplicateMessagesForConnection(int duplicates,int connection) {
    //if (!shouldRecordData()) return;
    duplicateRequestHist.collect(duplicates);
    //We're ignoring the connection ID for duplicates, but this has been setup with that in mind. Not adding a to-do as it's not something i'm thinking of doing soon.
}

//Increase the number of active Vehicles by 1 (new vehicle spawned)
void BaconStatistics::increaseActiveVehicles() {
    BaconStatistics::activeVehicles++;
    BaconStatistics::totalVehicles++;

    //NOTE: This does not care about general statistics collection
    //if (!hasStarted || hasStopped) return;
    activeVehiclesVect.record(activeVehicles);
}

//Decrease the number of active Vehicles by 1 (vehicle "deleted" / journey complete)
void BaconStatistics::decreaseActiveVehicles() {
    //Logging previous status so we have the ladder graph
    if (hasStarted && !hasStopped) activeVehiclesVect.record(activeVehicles);

    BaconStatistics::activeVehicles--;

    //if (!hasStarted || hasStopped) return;
    activeVehiclesVect.record(activeVehicles);
}

void BaconStatistics::increaseServerCacheHits() {
    BaconStatistics::serverCacheHits++;

    if (!shouldRecordData()) return;
    serverHitVect.record(serverCacheHits);
}

void BaconStatistics::increaseMessagesSent(ContentClass contentClass) {
    switch(contentClass) {
        case ContentClass::MULTIMEDIA:
            increaseMultimediaMessagesSent();
            break;
        case ContentClass::NETWORK:
            increaseNetworkMessagesSent();
            break;
        case ContentClass::TRAFFIC:
            increaseTrafficMessagesSent();
            break;
        case ContentClass::EMERGENCY_SERVICE:
            increaseEmergencyMessagesSent();
            break;
        default:
            std::cerr << "(St) Error: Invalid Class for increaseMessageSent()\n";
            std::cerr.flush();
            break;
    }
}


void BaconStatistics::increaseMessagesLost(ContentClass contentClass) {
    switch(contentClass) {
        case ContentClass::MULTIMEDIA:
            increaseMultimediaMessagesLost();
            break;
        case ContentClass::NETWORK:
            increaseNetworkMessagesLost();
            break;
        case ContentClass::TRAFFIC:
            increaseTrafficMessagesLost();
            break;
        case ContentClass::EMERGENCY_SERVICE:
            increaseEmergencyMessagesLost();
            break;
        default:
            std::cerr << "(St) Error: Invalid Class for increaseMessageLost()\n";
            std::cerr.flush();
            break;
    }
}


void BaconStatistics::increaseMessagesUnserved(ContentClass contentClass) {
    switch(contentClass) {
        case ContentClass::MULTIMEDIA:
            increaseMultimediaMessagesUnserved();
            break;
        case ContentClass::NETWORK:
            increaseNetworkMessagesUnserved();
            break;
        case ContentClass::TRAFFIC:
            increaseTrafficMessagesUnserved();
            break;
        case ContentClass::EMERGENCY_SERVICE:
            increaseEmergencyMessagesUnserved();
            break;
        default:
            std::cerr << "(St) Error: Invalid Class for increaseMessageUnserved()\n";
            std::cerr.flush();
            break;
    }
}


void BaconStatistics::increaseChunkLost(ContentClass contentClass) {
    switch(contentClass) {
        case ContentClass::MULTIMEDIA:
            increaseMultimediaChunksLost();
            break;
        case ContentClass::NETWORK:
            increaseNetworkChunksLost();
            break;
        case ContentClass::TRAFFIC:
            increaseTrafficChunksLost();
            break;
        case ContentClass::EMERGENCY_SERVICE:
            increaseEmergencyChunksLost();
            break;
        default:
            std::cerr << "(St) Error: Invalid Class for increaseChunkLost()\n";
            std::cerr.flush();
            break;
    }
}






void BaconStatistics::increaseMultimediaMessagesSent() {
    BaconStatistics::multimediaSentPackets++;

    if (!shouldRecordData()) return;
    multimediaSentPacketVect.record(multimediaSentPackets);
}

void BaconStatistics::increaseMultimediaMessagesUnserved() {
    BaconStatistics::multimediaUnservedPackets++;

    if (!shouldRecordData()) return;
    multimediaUnservedPacketVect.record(multimediaUnservedPackets);
}

void BaconStatistics::increaseMultimediaMessagesLost() {
    BaconStatistics::multimediaLostPackets++;

    if (!shouldRecordData()) return;
    multimediaLostPacketVect.record(multimediaLostPackets);
}

void BaconStatistics::increaseMultimediaChunksLost() {
    BaconStatistics::multimediaLostChunks++;

    if (!shouldRecordData()) return;
    multimediaLostChunkVect.record(multimediaLostChunks);
}

void BaconStatistics::increaseTrafficMessagesSent() {
    BaconStatistics::trafficSentPackets++;

    if (!shouldRecordData()) return;
    trafficSentPacketVect.record(trafficSentPackets);
}

void BaconStatistics::increaseTrafficMessagesUnserved() {
    if (!shouldRecordData()) return;
    BaconStatistics::trafficUnservedPackets++;
    trafficUnservedPacketVect.record(trafficUnservedPackets);
}

void BaconStatistics::increaseTrafficMessagesLost() {
    BaconStatistics::trafficLostPackets++;

    if (!shouldRecordData()) return;
    trafficLostPacketVect.record(trafficLostPackets);
}

void BaconStatistics::increaseTrafficChunksLost() {
    BaconStatistics::trafficLostChunks++;

    if (!shouldRecordData()) return;
    trafficLostChunkVect.record(trafficLostChunks);
}

void BaconStatistics::increaseNetworkMessagesSent() {
    BaconStatistics::networkSentPackets++;

    if (!shouldRecordData()) return;
    networkSentPacketVect.record(networkSentPackets);
}

void BaconStatistics::increaseNetworkMessagesUnserved() {
    BaconStatistics::networkUnservedPackets++;

    if (!shouldRecordData()) return;
    networkUnservedPacketVect.record(networkUnservedPackets);
}

void BaconStatistics::increaseNetworkMessagesLost() {
    BaconStatistics::networkLostPackets++;

    if (!shouldRecordData()) return;
    networkLostPacketVect.record(networkLostPackets);
}

void BaconStatistics::increaseNetworkChunksLost() {
    BaconStatistics::networkLostChunks++;

    if (!shouldRecordData()) return;
    networkLostChunkVect.record(networkLostChunks);
}

void BaconStatistics::increaseEmergencyMessagesSent() {
    BaconStatistics::emergencySentPackets++;

    if (!shouldRecordData()) return;
    emergencySentPacketVect.record(emergencySentPackets);
}

void BaconStatistics::increaseEmergencyMessagesUnserved() {
    BaconStatistics::emergencySentPackets++;

    if (!shouldRecordData()) return;
    emergencySentPacketVect.record(emergencySentPackets);
}

void BaconStatistics::increaseEmergencyMessagesLost() {
    BaconStatistics::emergencyUnservedPackets++;

    if (!shouldRecordData()) return;
    emergencyUnservedPacketVect.record(emergencyUnservedPackets);
}

void BaconStatistics::increaseEmergencyChunksLost() {
    BaconStatistics::emergencyLostChunks++;

    if (!shouldRecordData()) return;
    emergencyLostChunkVect.record(emergencyLostChunks);
}




void BaconStatistics::increaseLocalLateCacheHits() {
    BaconStatistics::localCacheLateHits++;

    if (!shouldRecordData()) return;
    localLateCacheHitVect.record(localCacheLateHits);
}

void BaconStatistics::increaseLocalCacheHits() {
    BaconStatistics::localCacheHits++;

    if (!shouldRecordData()) return;
    localCacheHitVect.record(localCacheHits);
}

void BaconStatistics::increaseRemoteCacheHits() {
    BaconStatistics::remoteCacheHits++;

    if (!shouldRecordData()) return;
    remoteCacheHitVect.record(remoteCacheHits);
}

void BaconStatistics::increaseLocalCacheMisses() {
    BaconStatistics::localCacheMisses++;

    if (!shouldRecordData()) return;
    localCacheMissVect.record(localCacheMisses);
}

void BaconStatistics::increaseRemoteCacheMisses() {
    BaconStatistics::remoteCacheMisses++;

    if (!shouldRecordData()) return;
    remoteCacheMissVect.record(remoteCacheMisses);
}

void BaconStatistics::increaseCacheReplacements() {
    BaconStatistics::cacheReplacements++;

    if (!shouldRecordData()) return;
    cacheReplacementVect.record(cacheReplacements);
}

void BaconStatistics::increaseCreatedInterests() {
    BaconStatistics::createdInterests++;

    if (!shouldRecordData()) return;
    createdInterestVect.record(createdInterests);
}

void BaconStatistics::increaseRegisteredInterests() {
    BaconStatistics::registeredInterests++;

    if (!shouldRecordData()) return;
    registeredInterestVect.record(registeredInterests);
}

void BaconStatistics::increaseFulfilledInterests() {
    BaconStatistics::fulfilledInterests++;

    if (!shouldRecordData()) return;
    fulfilledInterestVect.record(fulfilledInterests);
}

void BaconStatistics::logInstantLoad(int node, double load) {
    if (!shouldRecordData()) return;
    LoadAtTime_t newLoad;
    //if (load < 0) load = 0;
    //if (load > 1) load = 1;
    newLoad.loadValue = load;
    newLoad.vehicleId = node;
    newLoad.simTime = simTime();
    instantLoadList.push_front(newLoad);
}

void BaconStatistics::logAverageLoad(int node, double load) {
    if (!shouldRecordData()) return;
    LoadAtTime_t newLoad;

    newLoad.loadValue = load;
    newLoad.vehicleId = node;
    newLoad.simTime = simTime();
    averageLoadList.push_front(newLoad);
}

void BaconStatistics::addcompleteTransmissionDelay(double delay) {
    if (!shouldRecordData()) return;

    totalTransmissionCount++;
    completeTransmissionCount++;
    totalTransmissionDelay += delay;
    completeTransmissionDelay += delay;
    completeTranmissionDelayVect.record(delay);
    overallTranmissionDelayVect.record(delay);
}

void BaconStatistics::addincompleteTransmissionDelay(double delay) {
    if (!shouldRecordData()) return;

    totalTransmissionCount++;
    incompleteTransmissionCount++;
    totalTransmissionDelay += delay;
    incompleteTranmissionDelay += delay;
    incompleteTranmissionDelayVect.record(delay);
    overallTranmissionDelayVect.record(delay);
}

void BaconStatistics::increaseHopCountResult(int hopCount) {
    if (!shouldRecordData()) return;

    std::string vectorName = "HopCount-" + std::to_string(hopCount);
    omnetpp::cOutVector* newHopVector = new omnetpp::cOutVector();

    //Checking if the hopcount value map is already available in our statistics
    if(hopDistanceCountMap.insert(std::make_pair(hopCount, 1)).second == true) {
        hopDistanceCountMap[hopCount] = hopDistanceCountMap[hopCount] + 1;

        //Creating vector map
        newHopVector->setName(vectorName.c_str());

        //std::cout << "(St) Creating new Statistics Collection for " << vectorName << "\n";
        //std::cout.flush();

        //std::pair <int,omnetpp::cOutVector> newHopVectorPair;
        //newHopVectorPair = std::make_pair(hopCount, newHopVector);

        hopDistanceVectorMap.push_back(newHopVector);

        //Checking if the hopcount vector map list is already included in our statistics, if so things are fucked
        //if(hopDistanceVectorMap.insert(newHopVectorPair.pair()).second == false) {
        //    std::cerr << "(St) Error: Hop Count Vector Map existed without an accompanying Hop Distance Result Count Map.\n";
        //    std::cerr.flush();
        //}
    }
    else {
        //std::cout << "(St) Adding into Statistic Collection for " << vectorName << "\n";
        //std::cout.flush();
        hopDistanceCountMap[hopCount] = hopDistanceCountMap[hopCount] + 1;
    }

    for (auto it = hopDistanceVectorMap.begin(); it != hopDistanceVectorMap.end(); it++) {
        //std::cout << "(St) Comparing <" << (*it)->getName() << "> and <" << vectorName << ">\n";
        //std::cout.flush();
        if (strcmp((*it)->getName(),vectorName.c_str()) == 0) {
            //std::cout << "\t \\--> Found " << vectorName << "\n";
            //std::cout.flush();
            (*it)->record(hopDistanceCountMap[hopCount]);
            break;
        }
    }

    //std::cout << "\t\t \\--> Done.\n";
    //std::cout.flush();
}
