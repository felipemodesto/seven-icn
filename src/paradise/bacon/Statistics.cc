/*
 * BaconStatistics.cc
 *
 *  https://omnetpp.org/pmwiki/index.php?n=Main.CollectingGlobalStatistics
 *
 *  Created on: Mar 11, 2016
 *      Author: felipe
 */

#include <paradise/bacon/Statistics.h>

Define_Module(Statistics);

Statistics::~Statistics() {
    if (clockTimerMessage != NULL) cancelAndDelete(clockTimerMessage);
    vehicleLocationMap.clear();
    contentRequestLocationMap.clear();
    contentNameFrequencyMap.clear();
    contactDurationMap.clear();
    participationLengthMap.clear();
    hopDistanceCountMap.clear();
    for (auto it = hopDistanceVectorMap.begin(); it != hopDistanceVectorMap.end(); it++) {
        (*it)->~cOutVector();
        it = hopDistanceVectorMap.erase(it);
    }
}

//Initialization Function
void Statistics::initialize(int stage) {
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

//
void Statistics::handleMessage(cMessage *msg) {
    if ( msg == clockTimerMessage ) {
        if (clockTimerMessage == NULL) return;
        scheduleAt(simTime() + 1, clockTimerMessage);
        keepTime();
    }
}

//Finalization Function (not a destructor!)
void Statistics::finish() {
    stopStatistics();

    if (clockTimerMessage != NULL) {
        cancelAndDelete(clockTimerMessage);
        clockTimerMessage = NULL;
    }
}

//
bool Statistics::allowedToRun() {
    Enter_Method_Silent();
    if (statisticsStartTime < omnetpp::simTime().dbl() && omnetpp::simTime().dbl() < statisticsStopTime ) return true;
    return false;
}

//Getter for Simulation Start Time
double Statistics::getStartTime() {
    Enter_Method_Silent();
    return statisticsStartTime;
}

//Getter for Simulation Stop Time
double Statistics::getStopTime() {
    Enter_Method_Silent();
    return statisticsStopTime;
}

//Getter for Simulation Control parameter related to statistics collection
bool Statistics::recordingStatistics() {
    Enter_Method_Silent();
    return collectingStatistics;
}

//Getter for Simulation Control parameter related to position tracking
bool Statistics::recordingPosition() {
    Enter_Method_Silent();
    return collectingPositions;
}

//
void Statistics::keepTime() {
    Enter_Method_Silent();
    int newTime = static_cast<int>(simTime().dbl());
    if (lastSecond < newTime) {
        lastSecond = newTime;

        clock_t end = clock();
        double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
        begin = end;

        std::time_t result = std::time(nullptr);
        struct tm * now = localtime( &result );

        int simNumber = getSimulation()->getActiveEnvir()->getConfigEx()->getActiveRunNumber();

        //char mbstr[100];
        std::cout << "[" << simNumber << "]\t SimTime: " << lastSecond << " \t CPU: " << std::setprecision(2) << round(elapsed_secs) << "s \tNow: " << std::asctime(now);//std::strftime(mbstr, sizeof(mbstr), "%b %e %H $M",now) << ">\n";
        //std::cout << "[" << simNumber << "]\t SimTime: " << lastSecond << " \t CPU: " << std::setprecision(2) << elapsed_secs << " s\tNow: " << std::asctime(now);//std::strftime(mbstr, sizeof(mbstr), "%b %e %H $M",now) << ">\n";
        std::cout.flush();

        if (hasStarted && !hasStopped) statisticsTimekeepingVect.record( (lastSecond - statisticsStartTime) / (double) (statisticsStopTime - statisticsStartTime) );
    }
}

//
bool Statistics::shouldRecordData() {
    Enter_Method_Silent();

    if (!allowedToRun()) return false;
    if (!collectingStatistics and !collectingPositions) return false;
    if (!hasStarted || hasStopped) return false;
    return true;
}

//
void Statistics::startStatistics() {
    Enter_Method_Silent();
    int simNumber = getSimulation()->getActiveEnvir()->getConfigEx()->getActiveRunNumber();

    if (hasStarted || hasStopped || !collectingStatistics) return;

    //Getting a reference to the content Library
    cSimulation *sim = getSimulation();
    cModule *modlib = sim->getModuleByPath("ParadiseScenario.library");
    library = check_and_cast<GlobalLibrary *>(modlib);

    //Checking if we already have simulation results for this file
    FILE *  pFile = fopen ( generalStatisticsFile.c_str(), "r");
    if (pFile != NULL) {
        std::cout << "[" << simNumber << "]\t (St) Warning: Simulation file already present in folder. Skipping simulation to accelerate simulation.\n";
        std::cout.flush();
        exit(0);
    }

    std::cout << "[" << simNumber << "]\t (St) Statistics Collection has started.\n";
    std::cout.flush();

    hasStarted = false;
    hasStopped = false;

    gpsAvailableFromLocation = 0;
    gpsCacheHits = 0;
    gpsProvisoningAttempts = 0;
    gpsPreemptiveRequests = 0;
    gpsCacheReplacements = 0;

    requestsStarted = 0;
    packetsSelfServed = 0;
    packetsSent = 0;
    packetsForwarded = 0;
    packetsUnserved = 0;
    packetsLost = 0;
    packetsFallenback = 0;
    chunksSent = 0;
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
    packetsSelfServVect.setName("SelfServidedPackets");
    packetsForwardedVect.setName("ForwardedPackets");
    packetsUnservedVect.setName("UnservedPackets");
    packetsFallenbackVect.setName("FallbackPackets");
    packetsLostVect.setName("LostPackets");
    chunksLostVect.setName("LostChunks");
    chunksSentVect.setName("SentChunks");
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
    gpsCacheReplacementVect.setName("GPSCacheReplacements");
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
    packetsSelfServVect.record(0);
    packetsUnservedVect.record(0);
    packetsFallenbackVect.record(0);
    packetsLostVect.record(0);
    chunksLostVect.record(0);
    chunksSentVect.record(0);
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
    gpsCacheReplacementVect.record(0);

    hasStarted = true;
}

//
void Statistics::stopStatistics() {
    Enter_Method_Silent();
    if (hasStopped) return;
    int simNumber = getSimulation()->getActiveEnvir()->getConfigEx()->getActiveRunNumber();

    if (simTime() < statisticsStopTime) {
        std::cout << "\n[" << simNumber << "]\t (St) Warning: Simulation exited prematurely. Statistics will not be logged.\n\n";
        std::cerr.flush();
        return;
    }

    //std::cout << "[" << simNumber << "]\t (St) File Folder <" << simulationDirectoryFolder << ">\n";

    //std::cout << "[" << simNumber << "]\t (St) Saving File <" << requestLocationStatsFile << ">\n";
    //std::cout << "[" << simNumber << "]\t (St) Saving File <" << locationStatisticsFile << ">\n";
    //std::cout << "[" << simNumber << "]\t (St) Saving File <" << neighborhoodStatisticsFile << ">\n";
    //std::cout << "[" << simNumber << "]\t (St) Saving File <" << contentPopularityStatisticsFile << ">\n";
    //std::cout << "[" << simNumber << "]\t (St) Saving File <" << networkInstantLoadStatisticsFile << ">\n";
    //std::cout << "[" << simNumber << "]\t (St) Saving File <" << networkAverageLoadStatisticsFile << ">\n";
    //std::cout << "[" << simNumber << "]\t (St) Saving File <" << generalStatisticsFile << ">\n";
    //std::cout << "[" << simNumber << "]\t (St) Saving File <" << participationLengthStatsFile << ">\n";

    std::cout << "[" << simNumber << "]\t (St) Statistics Collection has stopped. Saving Statistics.\n";
    std::cout.flush();

    //Checking for directory and creating if necessary
    FILE * pFile;
    /**/
    struct stat info;
    if( stat( simulationDirectoryFolder.c_str(), &info ) != 0 ) {
        std::cout << "[" << simNumber << "]\t (St) Warning: Statistics file path does not exist, creating...\n";
        std::cout.flush();
        mkdir(simulationDirectoryFolder.c_str(), 0755);     //TODO: (REFACTOR) code to make it compilable with windows (_mkdir(folder))
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
    totalTransmissionDelay = (double)totalTransmissionDelay/((double)totalTransmissionCount);
    completeTransmissionDelay = (double)completeTransmissionDelay/((double)completeTransmissionCount);
    incompleteTranmissionDelay = (double)incompleteTranmissionDelay/((double)incompleteTransmissionCount);

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
            countedMessages += iterator->second;
            countedDistances += iterator->second * iterator->first;
            //std::cout << "<" << iterator->second << "> Messages with distance <" << iterator->first << ">\n";
            //std::cout.flush();
        }
    }
    countedDistances = countedMessages > 0 ? countedDistances / static_cast<double>(countedMessages) : 0;

    //Saving Communication statistics previously saved as scalar and vector files
    pFile = fopen ( generalStatisticsFile.c_str(), "w");
    fprintf(pFile,"%s","averageLatency,averageCompletedLatency,averageFailedLatency");
    fprintf(pFile,"%s",",packetsSent,packetsSelfServed,packetsForwarded,packetsUnserved,packetsLost,packetsFallback,chunksLost,chunksSent");
    fprintf(pFile,"%s",",plcRequests,pclTransmissionAttempts,pclReplacements,plcCacheHits");
    //fprintf(pFile,"%s",",multimediaSentPackets,multimediaUnservedPackets,multimediaLostPackets,multimediaLostChunks,trafficSentPackets,trafficUnservedPackets,trafficLostPackets,trafficLostChunks,networkSentPackets,networkUnservedPackets,networkLostPackets,networkLostChunks,emergencySentPackets,emergencyUnservedPackets,emergencyLostPackets,emergencyLostChunks");
    fprintf(pFile,"%s",",localCacheHits,remoteCacheHits,serverCacheHits,localCacheMisses,remoteCacheMisses,cacheReplacements");
    fprintf(pFile,"%s",",averagedHopcount");
    fprintf(pFile,"%s","\n");

    fprintf(pFile,"%F",totalTransmissionDelay);
    fprintf(pFile,",%F",completeTransmissionDelay);
    fprintf(pFile,",%F",incompleteTranmissionDelay);

    fprintf(pFile,",%ld",packetsSent);
    fprintf(pFile,",%ld",packetsSelfServed);
    fprintf(pFile,",%ld",packetsForwarded);
    fprintf(pFile,",%ld",packetsUnserved);
    fprintf(pFile,",%ld",packetsLost);
    fprintf(pFile,",%ld",packetsFallenback);
    fprintf(pFile,",%ld",chunksLost);
    fprintf(pFile,",%ld",chunksSent);


    fprintf(pFile,",%ld",gpsPreemptiveRequests);
    fprintf(pFile,",%ld",gpsProvisoningAttempts);
    fprintf(pFile,",%ld",gpsCacheReplacements);
    fprintf(pFile,",%ld",gpsCacheHits);

    /*
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
    */

    fprintf(pFile,",%ld",localCacheHits);
    fprintf(pFile,",%ld",remoteCacheHits);
    fprintf(pFile,",%ld",serverCacheHits);
    fprintf(pFile,",%ld",localCacheMisses);
    fprintf(pFile,",%ld",remoteCacheMisses);
    fprintf(pFile,",%ld",cacheReplacements);
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

    time_t t = std::time(nullptr);
    struct tm * tm = std::localtime(&t);
    char buffer [80];
    std::strftime (buffer,80,"%d-%m-%Y %H-%M-%S",tm);

    //std::cout << "[" << simNumber << "]\t\t\\--> Completion Time <" << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << ">\n";
    std::cout << "[" << simNumber << "]\t\t\\--> Completion Time <" << buffer << ">\n";

    std::cout.flush();
}

//
string Statistics::getSimulationDirectory() {
    Enter_Method_Silent();
    return simulationDirectoryFolder;
}

//
string Statistics::getSimulationPrefix() {
    Enter_Method_Silent();
    return simulationPrefix;
}

//Function called by content servers to denote that they have received a request for an item and are trying to serve the content/
//This is used to monitor if contents have not had any replies or if the return path failed at some point.
void Statistics::logProvisionAttempt(int providerID, int requestID) {
    Enter_Method_Silent();

    RequestReplyAttempt_t* newAttempt = new RequestReplyAttempt_t();
    newAttempt->clientID = -1;
    newAttempt->providerID = providerID;
    newAttempt->requestID = requestID;
    openRequestList.push_back(newAttempt);
}

//
void Statistics::logPosition(double x, double y) {
    Enter_Method_Silent();
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
void Statistics::logContactDuration(double contactDuration) {
    Enter_Method_Silent();
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

//
void Statistics::logParticipationDuration(double participationLength) {
    Enter_Method_Silent();
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
void Statistics::logContentRequest(std::string contentName, bool shouldLogContent, double x, double y) {
    Enter_Method_Silent();
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

/*
//
void BaconStatistics::increaseFallbackRequests() {
    Enter_Method_Silent();
    BaconStatistics::fallbackOutsourcedRequests++;
    if (!shouldRecordData()) return;

    fallbackOutsourcedRequestsVect.record(fallbackOutsourcedRequests);
}
*/

//
void Statistics::logDistanceFromTweet(double distance) {
    Enter_Method_Silent();
    if (!shouldRecordData()) return;
    distanceFromTweetVect.record(distance);
}

//
void Statistics::increasedUnviableRequests(){
    Enter_Method_Silent();
    Statistics::unviableRequests++;
    if (!shouldRecordData()) return;

    unviableRequestsVect.record(unviableRequests);
}

//
void Statistics::increasedBackloggedResponses(){
    Enter_Method_Silent();
    Statistics::backloggedResponses++;
    if (!shouldRecordData()) return;

    backloggedClientResponseVect.record(backloggedResponses);
}


//Increase number of Packets Sent to self by 1
void Statistics::increasePacketsSelfServed(int myId, int requestID) {
    Enter_Method_Silent();
    increasePacketsSelfServed(1,myId,requestID);
}

//Increase number of Packets Sent to self by X
void Statistics::increasePacketsSelfServed(int x, int myId, int requestID) {
    Enter_Method_Silent();
    //Logging self-message
    Statistics::packetsSelfServed+= x;
    if (shouldRecordData()) packetsSelfServVect.record(packetsSelfServed);
}

//Increase number of Packets Sent by X
void Statistics::increasePacketsSent(int x, int myId, int requestID) {
    Enter_Method_Silent();

    //Checking if we are the node that sent the request originally, logging separately in that case
    for (auto iterator = openRequestList.begin(); iterator != openRequestList.end(); iterator++) {
        //Checking if the entry relates to the current
        if ( (*iterator)->requestID == requestID) {
            //Checking if it' s a self message
            if ( (*iterator)->requestID == myId ) {
                increasePacketsSelfServed(x,myId,requestID);
            } else {
                Statistics::packetsSent+= x;
                if (shouldRecordData()) packetsSentVect.record(packetsSent);
            }

            //Removing all logged attempts to fulfill content from database
            while (iterator != openRequestList.end()) {
                if ( (*iterator)->requestID == requestID)
                    delete((*iterator));
                    iterator = openRequestList.erase(iterator);
            }
            //Since we found our stuff, we'll break the loop
            return;
        }
    }

    //If we don't find any attempts, something might be wrong? but yeah, whatever, lets log a a successful transfer
    Statistics::packetsSent+= x;
    if (shouldRecordData()) packetsSentVect.record(packetsSent);
}

//Increase number of Packets Sent by 1
void Statistics::increasePacketsSent(int myId, int requestID){
    Enter_Method_Silent();
    increasePacketsSent(1,myId,requestID);
}

//Increase number of Packets Forwarded by X
void Statistics::increasePacketsForwarded(int x, int myId, int requestID) {
    Enter_Method_Silent();
    Statistics::packetsForwarded+= x;
    if (!shouldRecordData()) return;

    packetsForwardedVect.record(packetsForwarded);
}

//Increase number of Packets Forwarded by 1
void Statistics::increasePacketsForwarded(int myId, int requestID){
    Enter_Method_Silent();
    increasePacketsForwarded(1,myId,requestID);
}

//Increase number of Packets were not served by X
void Statistics::increasePacketsUnserved(int x, int myId, int requestID) {
    Enter_Method_Silent();

    //Checking if anyone at any point attempted
    for (auto iterator = openRequestList.begin(); iterator != openRequestList.end(); iterator++) {
        //Checking if the entry relates to the current
        if ( (*iterator)->requestID == requestID) {
            //Calling the other function where we log this statistics appropriately
            increasePacketsLost(x,myId,requestID);

            //Removing all logged attempts to fulfill content from database
            while (iterator != openRequestList.end()) {
                if ( (*iterator)->requestID == requestID)
                    delete((*iterator));
                    iterator = openRequestList.erase(iterator);
            }
            //Since we found our stuff, we'll break the loop
            return;
        }
    }

    //If we didn't find ANY instances of nodes trying to provide for this connection, we'll list it as unserved
    Statistics::packetsUnserved+= x;
    if (shouldRecordData()) packetsUnservedVect.record(packetsUnserved);
}

//Increase number of Packets were not served by 1
void Statistics::increasePacketsUnserved(int myId, int requestID){
    Enter_Method_Silent();
    increasePacketsUnserved(1,myId,requestID);
}

//Increase number of Packets were not served by X
void Statistics::increasePacketsFallenBack(long int x, int myId, int requestID) {
    Enter_Method_Silent();
    Statistics::packetsFallenback+= x;
    if (shouldRecordData()) packetsFallenbackVect.record(packetsFallenback);
}

//Increase number of Packets obtained externally from outside-simulation infrastructure
void Statistics::increasePacketsFallenBack(int myId, int requestID) {
    Enter_Method_Silent();
    increasePacketsFallenBack(1,myId,requestID);
}

//Increase number of Packets Lost by X
void Statistics::increasePacketsLost(int x, int myId, int requestID) {
    Enter_Method_Silent();

    Statistics::packetsLost += x;
    if (shouldRecordData()) packetsLostVect.record(packetsLost);
}

//Increase number of Packets Lost by 1
void Statistics::increasePacketsLost(int myId, int requestID) {
    Enter_Method_Silent();
    increasePacketsLost(1,myId,requestID);
}

//Increase number of Chunks Lost by X
void Statistics::increaseChunksLost(int x, int myId, int requestID) {
    Enter_Method_Silent();
    Statistics::chunksLost += x;
    if (shouldRecordData()) chunksLostVect.record(chunksLost);
}

//Increase number of Chunks Lost by 1
void Statistics::increaseChunksLost(int myId, int requestID) {
    Enter_Method_Silent();
    increaseChunksLost(1,myId,requestID);
}


//Increase number of Chunks Lost by X
void Statistics::increaseChunksSent(int x, int myId, int requestID) {
    Enter_Method_Silent();
    Statistics::chunksSent += x;
    if (shouldRecordData()) chunksSentVect.record(chunksSent);
}

//Increase number of Chunks Lost by 1
void Statistics::increaseChunksSent(int myId, int requestID) {
    Enter_Method_Silent();
    increaseChunksSent(1,myId,requestID);
}

void Statistics::setHopsCount(int hops) {
    Enter_Method_Silent();
    hopCountHist.collect(hops);

}

void Statistics::setHopsCount(int hops, int count) {
    Enter_Method_Silent();
    for (int i = 0; i < count ; i++) {
        hopCountHist.collect(hops);
    }
}

void Statistics::setPacketsSent(int x) {
    Enter_Method_Silent();
    packetsSentHist.collect(x);
}

void Statistics::setPacketsUnserved(int x) {
    Enter_Method_Silent();
    packetsUnservedHist.collect(x);
}

void Statistics::setPacketsLost(int x) {
    Enter_Method_Silent();
    packetsLostHist.collect(x);
}

void Statistics::setChunksLost(int x) {
    Enter_Method_Silent();
    chunksLostHist.collect(x);
}

void Statistics::setServerBusy(int x) {
    Enter_Method_Silent();
    serverBusyHist.collect(x);
}

void Statistics::setContentUnavailable(int x) {
    Enter_Method_Silent();
    contentUnavailableHist.collect(x);
}

void Statistics::logInterestForConnection(int connectionID) {
    Enter_Method_Silent();
    requestsPerConnectionHist.collect(connectionID);
}

void Statistics::logDuplicateMessagesForConnection(int duplicates,int connection) {
    Enter_Method_Silent();
    duplicateRequestHist.collect(duplicates);
    //We're ignoring the connection ID for duplicates, but this has been setup with that in mind. Not adding a to-do as it's not something i'm thinking of doing soon.
}

//Increase the number of active Vehicles by 1 (new vehicle spawned)
bool Statistics::increaseActiveVehicles(int vehicleId) {
    Enter_Method_Silent();
    Statistics::activeVehicles++;
    Statistics::totalVehicles++;

    //NOTE: This does not care about general statistics collection
    activeVehiclesVect.record(activeVehicles);

    //std::cout << "(St) Add <" << vehicleId << "> Total <" << activeVehicles << "> Servers <" << library->getActiveServers() << "/" << library->getMaximumServers() << "> Time <" << simTime() << ">\n";
    //std::cout.flush();

    return true;
}

//Decrease the number of active Vehicles by 1 (vehicle "deleted" / journey complete)
bool Statistics::decreaseActiveVehicles(int vehicleId) {
    Enter_Method_Silent();
    //Logging previous status so we have the ladder graph
    if (hasStarted && !hasStopped) activeVehiclesVect.record(activeVehicles);

    Statistics::activeVehicles--;

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

//
void Statistics::increaseServerCacheHits() {
    Enter_Method_Silent();
    Statistics::serverCacheHits++;

    if (!shouldRecordData()) return;
    serverHitVect.record(serverCacheHits);
}

//
void Statistics::increaseMessagesSent(ContentClass contentClass) {
    Enter_Method_Silent();

    //Checking which method to fulfill
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

//
void Statistics::increaseMessagesLost(ContentClass contentClass) {
    Enter_Method_Silent();
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

//
void Statistics::increaseMessagesUnserved(ContentClass contentClass) {
    Enter_Method_Silent();
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

//
void Statistics::increaseChunkLost(ContentClass contentClass) {
    Enter_Method_Silent();
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

//
void Statistics::increaseMultimediaMessagesSent() {
    Enter_Method_Silent();
    Statistics::multimediaSentPackets++;

    if (!shouldRecordData()) return;
    multimediaSentPacketVect.record(multimediaSentPackets);
}

//
void Statistics::increaseMultimediaMessagesUnserved() {
    Enter_Method_Silent();
    Statistics::multimediaUnservedPackets++;

    if (!shouldRecordData()) return;
    multimediaUnservedPacketVect.record(multimediaUnservedPackets);
}

//
void Statistics::increaseMultimediaMessagesLost() {
    Enter_Method_Silent();
    Statistics::multimediaLostPackets++;

    if (!shouldRecordData()) return;
    multimediaLostPacketVect.record(multimediaLostPackets);
}

void Statistics::increaseMultimediaChunksLost() {
    Enter_Method_Silent();
    Statistics::multimediaLostChunks++;

    if (!shouldRecordData()) return;
    multimediaLostChunkVect.record(multimediaLostChunks);
}

void Statistics::increaseTrafficMessagesSent() {
    Enter_Method_Silent();
    Statistics::trafficSentPackets++;

    if (!shouldRecordData()) return;
    trafficSentPacketVect.record(trafficSentPackets);
}

void Statistics::increaseTrafficMessagesUnserved() {
    Enter_Method_Silent();
    if (!shouldRecordData()) return;
    Statistics::trafficUnservedPackets++;
    trafficUnservedPacketVect.record(trafficUnservedPackets);
}

void Statistics::increaseTrafficMessagesLost() {
    Enter_Method_Silent();
    Statistics::trafficLostPackets++;

    if (!shouldRecordData()) return;
    trafficLostPacketVect.record(trafficLostPackets);
}

void Statistics::increaseTrafficChunksLost() {
    Enter_Method_Silent();
    Statistics::trafficLostChunks++;

    if (!shouldRecordData()) return;
    trafficLostChunkVect.record(trafficLostChunks);
}

void Statistics::increaseNetworkMessagesSent() {
    Enter_Method_Silent();
    Statistics::networkSentPackets++;

    if (!shouldRecordData()) return;
    networkSentPacketVect.record(networkSentPackets);
}

void Statistics::increaseNetworkMessagesUnserved() {
    Enter_Method_Silent();
    Statistics::networkUnservedPackets++;

    if (!shouldRecordData()) return;
    networkUnservedPacketVect.record(networkUnservedPackets);
}

void Statistics::increaseNetworkMessagesLost() {
    Enter_Method_Silent();
    Statistics::networkLostPackets++;

    if (!shouldRecordData()) return;
    networkLostPacketVect.record(networkLostPackets);
}

void Statistics::increaseNetworkChunksLost() {
    Enter_Method_Silent();
    Statistics::networkLostChunks++;

    if (!shouldRecordData()) return;
    networkLostChunkVect.record(networkLostChunks);
}

void Statistics::increaseEmergencyMessagesSent() {
    Enter_Method_Silent();
    Statistics::emergencySentPackets++;

    if (!shouldRecordData()) return;
    emergencySentPacketVect.record(emergencySentPackets);
}

void Statistics::increaseEmergencyMessagesUnserved() {
    Enter_Method_Silent();
    Statistics::emergencySentPackets++;

    if (!shouldRecordData()) return;
    emergencySentPacketVect.record(emergencySentPackets);
}

void Statistics::increaseEmergencyMessagesLost() {
    Enter_Method_Silent();
    Statistics::emergencyUnservedPackets++;

    if (!shouldRecordData()) return;
    emergencyUnservedPacketVect.record(emergencyUnservedPackets);
}

void Statistics::increaseEmergencyChunksLost() {
    Enter_Method_Silent();
    Statistics::emergencyLostChunks++;

    if (!shouldRecordData()) return;
    emergencyLostChunkVect.record(emergencyLostChunks);
}


void Statistics::increaseNodeAtGPSLocationCacheHits() {
    Enter_Method_Silent();
    Statistics::gpsAvailableFromLocation++;
}

void Statistics::increasePLCPreemptiveCacheRequests() {
    Enter_Method_Silent();
    Statistics::gpsPreemptiveRequests++;
}

void Statistics::increasePLCCacheHits() {
    Enter_Method_Silent();
    Statistics::gpsCacheHits++;
}

void Statistics::increasePLCCacheReplacements() {
    Enter_Method_Silent();
    Statistics::gpsCacheReplacements++;
}

void Statistics::increasePLCProvisioningAttempts() {
    Enter_Method_Silent();
    Statistics::gpsProvisoningAttempts++;
}


void Statistics::increaseLocalLateCacheHits() {
    Enter_Method_Silent();
    Statistics::localCacheLateHits++;

    if (!shouldRecordData()) return;
    localLateCacheHitVect.record(localCacheLateHits);
}

void Statistics::increaseLocalCacheHits() {
    Enter_Method_Silent();
    Statistics::localCacheHits++;

    if (!shouldRecordData()) return;
    localCacheHitVect.record(localCacheHits);
}

void Statistics::increaseRemoteCacheHits() {
    Enter_Method_Silent();
    Statistics::remoteCacheHits++;

    if (!shouldRecordData()) return;
    remoteCacheHitVect.record(remoteCacheHits);
}

void Statistics::increaseLocalCacheMisses() {
    Enter_Method_Silent();
    Statistics::localCacheMisses++;

    if (!shouldRecordData()) return;
    localCacheMissVect.record(localCacheMisses);
}

void Statistics::increaseRemoteCacheMisses() {
    Enter_Method_Silent();
    Statistics::remoteCacheMisses++;

    if (!shouldRecordData()) return;
    remoteCacheMissVect.record(remoteCacheMisses);
}

void Statistics::increaseCacheReplacements() {
    Enter_Method_Silent();
    Statistics::cacheReplacements++;

    if (!shouldRecordData()) return;
    cacheReplacementVect.record(cacheReplacements);
}

void Statistics::increaseCreatedInterests() {
    Enter_Method_Silent();
    Statistics::createdInterests++;

    if (!shouldRecordData()) return;
    createdInterestVect.record(createdInterests);
}

void Statistics::increaseRegisteredInterests() {
    Enter_Method_Silent();
    Statistics::registeredInterests++;

    if (!shouldRecordData()) return;
    registeredInterestVect.record(registeredInterests);
}

void Statistics::increaseFulfilledInterests() {
    Enter_Method_Silent();
    Statistics::fulfilledInterests++;

    if (!shouldRecordData()) return;
    fulfilledInterestVect.record(fulfilledInterests);
}

void Statistics::logInstantLoad(int node, double load) {
    Enter_Method_Silent();
    if (!shouldRecordData()) return;
    LoadAtTime_t newLoad;
    newLoad.loadValue = load;
    newLoad.vehicleId = node;
    newLoad.simTime = simTime();
    instantLoadList.push_front(newLoad);
}

void Statistics::logAverageLoad(int node, double load) {
    Enter_Method_Silent();
    if (!shouldRecordData()) return;
    LoadAtTime_t newLoad;

    newLoad.loadValue = load;
    newLoad.vehicleId = node;
    newLoad.simTime = simTime();
    averageLoadList.push_front(newLoad);
}

void Statistics::addcompleteTransmissionDelay(double delay) {
    Enter_Method_Silent();
    if (!shouldRecordData()) return;

    totalTransmissionCount++;
    completeTransmissionCount++;
    totalTransmissionDelay += delay;
    completeTransmissionDelay += delay;
    overallTranmissionDelayVect.record(delay);
    completeTranmissionDelayVect.record(delay);
}

void Statistics::addincompleteTransmissionDelay(double delay) {
    Enter_Method_Silent();
    if (!shouldRecordData()) return;

    totalTransmissionCount++;
    incompleteTransmissionCount++;
    totalTransmissionDelay += delay;
    incompleteTranmissionDelay += delay;
    overallTranmissionDelayVect.record(delay);
    incompleteTranmissionDelayVect.record(delay);
}

void Statistics::increaseHopCountResult(int hopCount) {
    Enter_Method_Silent();
    if (!shouldRecordData()) return;

    std::string vectorName = "HopCount-" + std::to_string(hopCount);
    omnetpp::cOutVector * newHopVector = new omnetpp::cOutVector();

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
