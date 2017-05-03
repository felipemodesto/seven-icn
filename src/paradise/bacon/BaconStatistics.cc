/*
 * BaconStatistics.cc
 *
 *  https://omnetpp.org/pmwiki/index.php?n=Main.CollectingGlobalStatistics
 *
 *  Created on: Mar 11, 2016
 *      Author: felipe
 */

#include <paradise/bacon/BaconStatistics.h>

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
        collectingLoad = par("collectingLoad").boolValue();
        collectingNeighborhood = par("collectingNeighborhood").boolValue();
        statisticsStartTime = par("statisticsStartTime").doubleValue();
        statisticsStopTime = par("statisticsStopTime").doubleValue();

        simulationDirectoryFolder= par("simulationDirectoryFolder").stringValue();
        simulationPrefix= par("simulationPrefix").stringValue();

        generalStatisticsFile =             std::string(simulationDirectoryFolder + simulationPrefix + par("generalStatisticsFile").stringValue()).c_str();
        requestLocationStatsFile =          std::string(simulationDirectoryFolder + simulationPrefix + par("requestLocationStatsFile").stringValue()).c_str();
        hopcountFile =                      std::string(simulationDirectoryFolder + simulationPrefix + par("hopcountFile").stringValue()).c_str();
        locationStatisticsFile =            std::string(simulationDirectoryFolder + simulationPrefix + par("locationStatisticsFile").stringValue()).c_str();
        neighborhoodStatisticsFile =        std::string(simulationDirectoryFolder + simulationPrefix + par("neighborhoodStatisticsFile").stringValue()).c_str();
        contentPopularityStatisticsFile =   std::string(simulationDirectoryFolder + simulationPrefix + par("contentNameStatisticsFile").stringValue()).c_str();
        networkInstantLoadStatisticsFile =  std::string(simulationDirectoryFolder + simulationPrefix + par("networkInstantLoadStatisticsFile").stringValue()).c_str();
        networkAverageLoadStatisticsFile =  std::string(simulationDirectoryFolder + simulationPrefix + par("networkAverageLoadStatisticsFile").stringValue()).c_str();
        participationLengthStatsFile =      std::string(simulationDirectoryFolder + simulationPrefix + par("participationLengthStatsFile").stringValue()).c_str();

        startStatistics();

        clockTimerMessage = new cMessage("clockTimerMessage");
        scheduleAt(simTime() + 1, clockTimerMessage);
    }
}

void BaconStatistics::handleMessage(cMessage *msg) {
    if ( msg == clockTimerMessage ) {
        if (clockTimerMessage == NULL) return;
        scheduleAt(simTime() + 1, clockTimerMessage);
        keepTime();
    }
}

//Finalization Function (not a destructor!)
void BaconStatistics::finish() {
    stopStatistics();

    if (clockTimerMessage != NULL) {
        cancelAndDelete(clockTimerMessage);
        clockTimerMessage = NULL;
    }
}

//
bool BaconStatistics::allowedToRun() {
    //Enter_Method_Silent();
    if (statisticsStartTime < omnetpp::simTime().dbl() && omnetpp::simTime().dbl() < statisticsStopTime ) return true;
    return false;
}

//Getter for Simulation Start Time
double BaconStatistics::getStartTime() {
    //Enter_Method_Silent();
    return statisticsStartTime;
}

//Getter for Simulation Stop Time
double BaconStatistics::getStopTime() {
    //Enter_Method_Silent();
    return statisticsStopTime;
}

//Getter for Simulation Control parameter related to statistics collection
bool BaconStatistics::recordingStatistics() {
    //Enter_Method_Silent();
    return collectingStatistics;
}

//Getter for Simulation Control parameter related to position tracking
bool BaconStatistics::recordingPosition() {
    //Enter_Method_Silent();
    return collectingPositions;
}

//
void BaconStatistics::keepTime() {
    //Enter_Method_Silent();
    int newTime = static_cast<int>(simTime().dbl());
    if (lastSecond < newTime) {
        lastSecond = newTime;

        clock_t end = clock();
        double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
        begin = end;

        int simNumber = getSimulation()->getActiveEnvir()->getConfigEx()->getActiveRunNumber();

        std::cout << "[" << simNumber << "]\t(St) Current Time is: " << lastSecond << " \t CPU Time: " << std::setprecision(2) << elapsed_secs << " sec(s)\n";
        std::cout.flush();

        if (hasStarted && !hasStopped) statisticsTimekeepingVect.record( (lastSecond - statisticsStartTime) / (double) (statisticsStopTime - statisticsStartTime) );
    }
}

//
bool BaconStatistics::shouldRecordData() {
    //Enter_Method_Silent();

    if (!allowedToRun()) return false;
    if (!collectingStatistics and !collectingPositions) return false;
    if (!hasStarted || hasStopped) return false;
    return true;
}

//
void BaconStatistics::startStatistics() {
    //Enter_Method_Silent();
    if (hasStarted || hasStopped || !collectingStatistics) return;

    //Getting a reference to the content Library
    cSimulation *sim = getSimulation();
    cModule *modlib = sim->getModuleByPath("BaconScenario.library");
    library = check_and_cast<BaconLibrary *>(modlib);

    //Checking if we already have simulation results for this file
    FILE *  pFile = fopen ( generalStatisticsFile.c_str(), "r");
    if (pFile != NULL) {
        std::cerr << "(St) Error: Simulation file already present in folder. Skipping simulation to accelerate simulation.\n";
        std::cerr.flush();
        exit(0);
    }

    std::cout << "(St) Statistics Collection has started.\n";
    std::cout.flush();

    hasStarted = false;
    hasStopped = false;

    requestsStarted = 0;
    packetsSent = 0;
    packetsForwarded = 0;
    packetsUnserved = 0;
    packetsLost = 0;
    packetsFallenback = 0;
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
    totalTransmissionDelay = 0;
    completeTransmissionDelay = 0;
    incompleteTranmissionDelay = 0;
    lastContactTime = 0;
    unviableRequests = 0;
    fallbackOutsourcedRequests = 0;

    packetsSentHist.setName("PacketsSent");
    packetsForwardedHist.setName("PacketsForwarded");
    packetsUnservedHist.setName("PacketsUnserved");
    packetsLostHist.setName("PacketsLost");
    chunksLostHist.setName("ChunksLost");
    serverBusyHist.setName("ServerBusy");
    contentUnavailableHist.setName("ContentUnavailable");

    distanceFromTweetVect.setName("DistanceFromTweet");
    unviableRequestsVect.setName("UnviableRequests");

    requestsStartedVect.setName("RequestsStarted");
    packetsSentVect.setName("SentPackets");
    packetsForwardedVect.setName("ForwardedPackets");
    packetsUnservedVect.setName("UnservedPackets");
    packetsFallenbackVect.setName("FallbackPackets");
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

    fallbackOutsourcedRequestsVect.setName("FallbackOutsourcedRequests");

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

    neighborCountVect.setName("ContactTime");
    participationLengthVect.setName("ParticipationTime");

    fallbackOutsourcedRequestsVect.record(0);
    unviableRequestsVect.record(0);
    participationLengthVect.record(0);
    neighborCountVect.record(0);
    requestsStartedVect.record(0);
    packetsSentVect.record(0);
    packetsUnservedVect.record(0);
    packetsFallenbackVect.record(0);
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
    //Enter_Method_Silent();
    if (hasStopped) return;
    int simNumber = getSimulation()->getActiveEnvir()->getConfigEx()->getActiveRunNumber();

    if (simTime() < statisticsStopTime) {
        std::cout << "\n[" << simNumber << "]\t (St) Warning: Simulation exited prematurely. Statistics will not be logged.\n\n";
        std::cerr.flush();
        return;
    }

    std::cout << "[" << simNumber << "]\t (St) File Folder <" << simulationDirectoryFolder << ">\n";

    std::cout << "[" << simNumber << "]\t (St) Saving File <" << requestLocationStatsFile << ">\n";
    std::cout << "[" << simNumber << "]\t (St) Saving File <" << locationStatisticsFile << ">\n";
    std::cout << "[" << simNumber << "]\t (St) Saving File <" << neighborhoodStatisticsFile << ">\n";
    std::cout << "[" << simNumber << "]\t (St) Saving File <" << contentPopularityStatisticsFile << ">\n";
    std::cout << "[" << simNumber << "]\t (St) Saving File <" << networkInstantLoadStatisticsFile << ">\n";
    std::cout << "[" << simNumber << "]\t (St) Saving File <" << networkAverageLoadStatisticsFile << ">\n";
    std::cout << "[" << simNumber << "]\t (St) Saving File <" << generalStatisticsFile << ">\n";
    std::cout << "[" << simNumber << "]\t (St) Saving File <" << participationLengthStatsFile << ">\n";

    std::cout << "[" << simNumber << "]\t (St) Statistics Collection has stopped. Saving Statistics.\n";
    std::cout.flush();

    //Checking for directory and creating if necessary
    FILE * pFile;
    /**/
    struct stat info;
    if( stat( simulationDirectoryFolder.c_str(), &info ) != 0 ) {
        std::cout << "[" << simNumber << "]\t (St) Warning: Statistics file path does not exist, creating...\n";
        std::cout.flush();
        mkdir(simulationDirectoryFolder.c_str(), 0755);     //TODO: (Rework) code to make it compilable with windows (_mkdir(folder))
        std::cout << "[" << simNumber << "]\t\t(St) \\--> Done.\n";
        std::cout.flush();

    } else if( info.st_mode & S_IFDIR ) {
        //printf( "%s is a directory\n", pathname );
    } else {
        std::cerr << "(St) Error: Path does not point to a valid directory. Statistics collection cannot proceed. Exiting simulation.\n";
        std::cerr.flush();
        exit(0);
        //printf( "%s is no directory\n", pathname );
    }
    //*/
    //Preemptively creating directories (should they not exist)
    /*
    if ( boost::filesystem::create_directories(simulationDirectoryFolder) == true) {
        std::cout << "[" << simNumber << "]\t (St) Warning: Statistics file path did not exist and had to be created\n";
        std::cout.flush();
    }*/

    //Saving Hop Statistics
    pFile = fopen ( hopcountFile.c_str(), "w");
    fprintf(pFile, "Hop,Count\n");
    for(auto iterator = hopDistanceCountMap.begin(); iterator != hopDistanceCountMap.end(); iterator++) {
        fprintf(pFile, "%i,%i\n",iterator->first,iterator->second);
    }
    fclose(pFile);

    //If we're logging vehicle positions, we'll save a log csv file with our position matrix
    if (collectingPositions) {
        pFile = fopen ( locationStatisticsFile.c_str(), "w");
        fprintf(pFile, "X,Y,Count\n");
        for(auto iterator = vehicleLocationMap.begin(); iterator != vehicleLocationMap.end(); iterator++) {
            fprintf(pFile, "%s,%i\n",iterator->first.c_str(),iterator->second);
        }
        fclose(pFile);
    }

    //If we're logging Contact Times
    if (collectingNeighborhood) {
        pFile = fopen ( neighborhoodStatisticsFile.c_str(), "w");
        fprintf(pFile, "Duration,Count\n");
        for(auto iterator = contactDurationMap.begin(); iterator != contactDurationMap.end(); iterator++) {
            fprintf(pFile, "%f,%i\n",iterator->first,iterator->second);
        }
        fclose(pFile);

        pFile = fopen ( participationLengthStatsFile.c_str(), "w");
        fprintf(pFile, "Length,Count\n");
        for(auto iterator = participationLengthMap.begin(); iterator != participationLengthMap.end(); iterator++) {
            fprintf(pFile, "%f,%i\n",iterator->first,iterator->second);
        }
        fclose(pFile);
    }

    //If we're logging vehicle positions, we'll save a log csv file with our position matrix
    if (collectingRequestNames) {
        pFile = fopen ( contentPopularityStatisticsFile.c_str(), "w");
        fprintf(pFile, "#name,requests\n");
        for(auto iterator = contentNameFrequencyMap.begin(); iterator != contentNameFrequencyMap.end(); iterator++) {
            //if (iterator->second != 0) {
                fprintf(pFile, "%s,%i\n",iterator->first.c_str(),iterator->second);
            //}
        }
        fclose(pFile);

        pFile = fopen ( requestLocationStatsFile.c_str(), "w");
        fprintf(pFile, "X,Y,Count\n");
        for(auto iterator = contentRequestLocationMap.begin(); iterator != contentRequestLocationMap.end(); iterator++) {
            fprintf(pFile, "%s,%i\n",iterator->first.c_str(),iterator->second);
        }
        fclose(pFile);
    }


    //Calculating Average Delays
    totalTransmissionDelay = totalTransmissionDelay/(double)totalTransmissionCount;
    completeTransmissionDelay = completeTransmissionDelay/(double)completeTransmissionCount;
    incompleteTranmissionDelay = incompleteTranmissionDelay/(double)incompleteTransmissionCount;

    //Saving Network Load Statistics
    if (collectingLoad) {
        pFile = fopen ( networkInstantLoadStatisticsFile.c_str(), "w");
        fprintf(pFile, "#time,id,load\n");
        for(auto iterator = instantLoadList.begin(); iterator != instantLoadList.end(); iterator++) {
            fprintf(pFile,"%f,%i,%f\n",iterator->simTime.dbl(),iterator->vehicleId,iterator->loadValue);
        }
        fclose(pFile);
        pFile = fopen ( networkAverageLoadStatisticsFile.c_str(), "w");
        fprintf(pFile, "#time,id,load\n");
        for(auto iterator = averageLoadList.begin(); iterator != averageLoadList.end(); iterator++) {
            fprintf(pFile,"%f,%i,%f\n",iterator->simTime.dbl(),iterator->vehicleId,iterator->loadValue);
        }
        fclose(pFile);
    }

    //Computing average hop-count from statistics
    long int countedMessages = 0;
    double countedDistances = 0;
    for(auto iterator = hopDistanceCountMap.begin(); iterator != hopDistanceCountMap.end(); iterator++) {
        //Filtering Infrastructure Fallback
        if (iterator->first >= 0) {
            countedMessages += iterator->first;
            countedDistances += iterator->first * iterator->second;
        }
    }
    countedDistances = countedMessages > 0 ? countedDistances / static_cast<double>(countedMessages) : 0;

    //Saving Communication statistics previously saved as scalar and vector files
    pFile = fopen ( generalStatisticsFile.c_str(), "w");
    fprintf(pFile,"%s","averageLatency,averageCompletedLatency,averageFailedLatency,packetsSent,packetsForwarded,packetsUnserved,packetsLost,packetsFallback,chunksLost,multimediaSentPackets,multimediaUnservedPackets,multimediaLostPackets,multimediaLostChunks,trafficSentPackets,trafficUnservedPackets,trafficLostPackets,trafficLostChunks,networkSentPackets,networkUnservedPackets,networkLostPackets,networkLostChunks,emergencySentPackets,emergencyUnservedPackets,emergencyLostPackets,emergencyLostChunks,localCacheHits,remoteCacheHits,localCacheMisses,remoteCacheMisses,cacheReplacements,fallbackRequests,averagedHopcount\n");

    fprintf(pFile,"%F",totalTransmissionDelay);
    fprintf(pFile,",%F",completeTransmissionDelay);
    fprintf(pFile,",%F",incompleteTranmissionDelay);

    fprintf(pFile,",%ld",packetsSent);
    fprintf(pFile,",%ld",packetsForwarded);
    fprintf(pFile,",%ld",packetsUnserved);
    fprintf(pFile,",%ld",packetsLost);
    fprintf(pFile,",%ld",packetsFallenback);
    fprintf(pFile,",%ld",chunksLost);
    fprintf(pFile,",%ld",multimediaSentPackets);
    fprintf(pFile,",%ld",multimediaUnservedPackets);
    fprintf(pFile,",%ld",multimediaLostPackets);
    fprintf(pFile,",%ld",multimediaLostChunks);
    fprintf(pFile,",%ld",trafficSentPackets);
    fprintf(pFile,",%ld",trafficUnservedPackets);
    fprintf(pFile,",%ld",trafficLostPackets);
    fprintf(pFile,",%ld",trafficLostChunks);
    fprintf(pFile,",%ld",networkSentPackets);
    fprintf(pFile,",%ld",networkUnservedPackets);
    fprintf(pFile,",%ld",networkLostPackets);
    fprintf(pFile,",%ld",networkLostChunks);
    fprintf(pFile,",%ld",emergencySentPackets);
    fprintf(pFile,",%ld",emergencyUnservedPackets);
    fprintf(pFile,",%ld",emergencyLostPackets);
    fprintf(pFile,",%ld",emergencyLostChunks);
    fprintf(pFile,",%ld",localCacheHits);
    fprintf(pFile,",%ld",remoteCacheHits);
    fprintf(pFile,",%ld",localCacheMisses);
    fprintf(pFile,",%ld",remoteCacheMisses);
    fprintf(pFile,",%ld",cacheReplacements);
    fprintf(pFile,",%ld",fallbackOutsourcedRequests);
    fprintf(pFile,",%F",countedDistances);


    fclose(pFile);

    hasStopped = true;

    /*
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer,sizeof(buffer),"%d-%m-%Y %I:%M:%S",timeinfo);
    std::string timestr(buffer);
    */

    std::cout << "[" << simNumber << "]\t (St) Statistics Collection is complete. Files have been saved\n";
    std::cout.flush();

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    std::cout << "[" << simNumber << "]\t\t\\--> Completion Time <" << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << ">\n";
    std::cout.flush();
}

//
void BaconStatistics::logPosition(double x, double y) {
    //Enter_Method_Silent();
    if (!collectingPositions) return;
    //if (!shouldRecordData()) return;

    std::string currentLocation = "" + std::to_string(floor(x)) + "," + std::to_string(floor(y));

    //Trying to insert new location tuple
    if(vehicleLocationMap.insert(std::make_pair(currentLocation, 1)).second == false) {
        //If object already exists lets increase its value
        vehicleLocationMap[currentLocation] = vehicleLocationMap[currentLocation] + 1;
    }
}

//
void BaconStatistics::logContactDuration(double contactDuration) {
    //Enter_Method_Silent();
    if (!collectingNeighborhood) return;

    neighborCountVect.record(lastContactTime);
    lastContactTime = contactDuration;

    //Trying to insert new content request tuple
    //NOTE: Content requests are added as 0 because we include them ALL when initializing the content library
    if(contactDurationMap.insert(std::make_pair(contactDuration, 1)).second == false) {
        //If object already exists lets increase its value
        contactDurationMap[contactDuration] = contactDurationMap[contactDuration] + 1;
    }
}

void BaconStatistics::logParticipationDuration(double participationLength) {
    //Enter_Method_Silent();
    if (!collectingNeighborhood) return;

    participationLengthVect.record(lastParticipationlength);
    lastParticipationlength = participationLength;

    //Trying to insert new content request tuple
    //NOTE: Content requests are added as 0 because we include them ALL when initializing the content library
    if(participationLengthMap.insert(std::make_pair(participationLength, 1)).second == false) {
        //If object already exists lets increase its value
        participationLengthMap[participationLength] = participationLengthMap[participationLength] + 1;
    }
}

//
void BaconStatistics::logContentRequest(std::string contentName, bool shouldLogContent, double x, double y) {
    //Enter_Method_Silent();
    if (shouldLogContent) {
        requestsStarted++;
        requestsStartedVect.record(requestsStarted);

        if (!collectingRequestNames) return;

        //Trying to insert new content request tuple
        //NOTE: Content requests are added as 0 because we include them ALL when initializing the content library
        if(contentNameFrequencyMap.insert(std::make_pair(contentName, 0)).second == false) {
            //If object already exists lets increase its value
            contentNameFrequencyMap[contentName] = contentNameFrequencyMap[contentName] + 1;
        }

        std::string currentLocation = "" + std::to_string(floor(x)) + "," + std::to_string(floor(y));
        //Trying to insert new location tuple
        if(contentRequestLocationMap.insert(std::make_pair(currentLocation, 1)).second == false) {
            //If object already exists lets increase its value
            contentRequestLocationMap[currentLocation] = contentRequestLocationMap[currentLocation] + 1;
        }
    }
}

void BaconStatistics::increaseFallbackRequests() {
    //Enter_Method_Silent();
    BaconStatistics::fallbackOutsourcedRequests++;
    if (!shouldRecordData()) return;

    fallbackOutsourcedRequestsVect.record(fallbackOutsourcedRequests);
}

void BaconStatistics::increasedUnviableRequests(){
    BaconStatistics::unviableRequests++;
    if (!shouldRecordData()) return;

    unviableRequestsVect.record(unviableRequests);
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

void BaconStatistics::logDistanceFromTweet(double distance) {
    if (!shouldRecordData()) return;
    distanceFromTweetVect.record(distance);
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

//Increase number of Packets were not served by X
void BaconStatistics::increasePacketsFallenBack(long int x) {
    BaconStatistics::packetsFallenback+= x;
    if (!shouldRecordData()) return;

    packetsFallenbackVect.record(packetsFallenback);
}
void BaconStatistics::increasePacketsFallenBack() {
    increasePacketsFallenBack(1);
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
    hopCountHist.collect(hops);

}

void BaconStatistics::setHopsCount(int hops, int count) {
    for (int i = 0; i < count ; i++) {
        hopCountHist.collect(hops);
    }
}

void BaconStatistics::setPacketsSent(int x) {
    packetsSentHist.collect(x);
}

void BaconStatistics::setPacketsUnserved(int x) {
    packetsUnservedHist.collect(x);
}

void BaconStatistics::setPacketsLost(int x) {
    packetsLostHist.collect(x);
}

void BaconStatistics::setChunksLost(int x) {
    chunksLostHist.collect(x);
}

void BaconStatistics::setServerBusy(int x) {
    serverBusyHist.collect(x);
}

void BaconStatistics::setContentUnavailable(int x) {
    contentUnavailableHist.collect(x);
}

void BaconStatistics::logInterestForConnection(int connectionID) {
    requestsPerConnectionHist.collect(connectionID);
}

void BaconStatistics::logDuplicateMessagesForConnection(int duplicates,int connection) {
    duplicateRequestHist.collect(duplicates);
    //We're ignoring the connection ID for duplicates, but this has been setup with that in mind. Not adding a to-do as it's not something i'm thinking of doing soon.
}

//Increase the number of active Vehicles by 1 (new vehicle spawned)
bool BaconStatistics::increaseActiveVehicles(int vehicleId) {
    BaconStatistics::activeVehicles++;
    BaconStatistics::totalVehicles++;

    //NOTE: This does not care about general statistics collection
    activeVehiclesVect.record(activeVehicles);

    //std::cout << "(St) Add <" << vehicleId << "> Total <" << activeVehicles << "> Servers <" << library->getActiveServers() << "/" << library->getMaximumServers() << "> Time <" << simTime() << ">\n";
    //std::cout.flush();

    return true;
}

//Decrease the number of active Vehicles by 1 (vehicle "deleted" / journey complete)
bool BaconStatistics::decreaseActiveVehicles(int vehicleId) {
    //Logging previous status so we have the ladder graph
    if (hasStarted && !hasStopped) activeVehiclesVect.record(activeVehicles);

    BaconStatistics::activeVehicles--;

    activeVehiclesVect.record(activeVehicles);

    //std::cout << "(St) Rem <" << vehicleId << "> Total <" << activeVehicles << "> Servers <" << library->getActiveServers() << "/" << library->getMaximumServers() << "> Time <" << simTime() << ">\n";
    //std::cout.flush();

    if (simTime() < statisticsStopTime) {
        //std::cout << "(St) Warning: Vehicle <" << vehicleId << "> exited simulation prematurely at time <" << simTime() << ">\n";
        //std::cout.flush();
        return false;
    }
    return true;
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
    overallTranmissionDelayVect.record(delay);
    completeTranmissionDelayVect.record(delay);
}

void BaconStatistics::addincompleteTransmissionDelay(double delay) {
    if (!shouldRecordData()) return;

    totalTransmissionCount++;
    incompleteTransmissionCount++;
    totalTransmissionDelay += delay;
    incompleteTranmissionDelay += delay;
    overallTranmissionDelayVect.record(delay);
    incompleteTranmissionDelayVect.record(delay);
}

void BaconStatistics::increaseHopCountResult(int hopCount) {
    //Enter_Method_Silent();
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
