/*
 * BaconLibrary.cc
 *
 *  Created on: Jan 25, 2016
 *      Author: felipe
 */

#include <paradise/bacon/GlobalLibrary.h>

Define_Module(GlobalLibrary);

bool compareLibraryItems(Content_t first, Content_t second) {
    return (first.contentIndex <= second.contentIndex);
}

//Initialization Function
void GlobalLibrary::initialize(int stage) {

    //Initializing
    if (stage == 0) {
        world = FindModule<BaseWorldUtility*>::findGlobalModule();

        zipfCaracterization = par("zipfCaracterization").doubleValue();
        sizeMultimedia = par("sizeMultimedia").longValue();
        sizeNetwork = par("sizeNetwork").longValue();
        sizeTransit = par("sizeTransit").longValue();

        libraryTransit = par("libraryTransit").longValue();
        libraryNetwork = par("libraryNetwork").longValue();
        libraryMultimedia = par("libraryMultimedia").longValue();

        priorityTransit = static_cast<ContentPriority>(par("priorityTransit").longValue());
        priorityNetwork = static_cast<ContentPriority>(par("priorityNetwork").longValue());
        priorityMultimedia = static_cast<ContentPriority>(par("priorityMultimedia").longValue());

        //If some of our Interest properties are 0 we null the library sizes
        //TODO: Clean this code by getting general interest statistics from users
        //if (priorityTransit == 0) libraryTransit = 0;
        //if (priorityNetwork == 0) libraryNetwork = 0;
        //if (priorityMultimedia == 0) libraryMultimedia = 0;

        locationModel = static_cast<LocationCorrelationModel>(par("locationCorrelationModel").longValue());

        maxVehicleServers = par("maxVehicleServers").longValue();
        maxVehicleClients = par("maxVehicleClients").longValue();
        requestSequenceFile = par("requestSequenceFile").stringValue();

        sectorWidth = par("sectorSize").longValue();
        sectorHeight = par("sectorSize").longValue();
        maximumViableDistance = par("maximumViableDistance").longValue();

        multimediaLibrary = NULL;
        networkLibrary = NULL;
        trafficLibrary = NULL;

        //Calculating world size in sections
        double scenarioWidth = world->getPgs()->x;
        double scenarioHeight = world->getPgs()->y;

        widthBlocks = (int)ceil(scenarioWidth/sectorWidth);
        heightBlocks = (int)ceil(scenarioHeight/sectorHeight);

        sectorCount = widthBlocks*heightBlocks;

        //std::cout << "(Lib) Request Location Correlation follows Distribution <" << locationModel << "> in <" << widthBlocks << ";" << heightBlocks << "> Grid.\n";
        //std::cout.flush();

        //Setting Sequential request index
        currentRequestIndex = 0;
    }

    //After we've been built and assume that the statistics object has also, we fill our content list
    if (stage == 1) {

        cSimulation *sim = getSimulation();
        cModule *modp = sim->getModuleByPath("ParadiseScenario.statistics");
        stats = check_and_cast<Statistics *>(modp);

        buildContentList();

        int simNumber = getSimulation()->getActiveEnvir()->getConfigEx()->getActiveRunNumber();
        std::cout << "[" << simNumber << "]\t(Lib) Writing Content List to Statistics for collection (warning: might be ignored).\n";
        std::cout.flush();

        //TODO: Fix so that our twitter shit still maps to content categories
        if (locationModel != LocationCorrelationModel::TWITTER) {
            //Adding all items from all libraries
            for (auto it = multimediaLibrary->begin(); it != multimediaLibrary->end(); it++) {
                stats->logContentRequest(it->contentPrefix, false, 0, 0);
            }
            for (auto it = networkLibrary->begin(); it != networkLibrary->end(); it++) {
                stats->logContentRequest(it->contentPrefix, false, 0, 0);
            }
            for (auto it = trafficLibrary->begin(); it != trafficLibrary->end(); it++) {
                stats->logContentRequest(it->contentPrefix, false, 0, 0);
            }
        }

        if (locationModel == LocationCorrelationModel::GPS) {
            //Creating the popularity distribution for the sectors randomly as well as associated sorted list
            sectorPopularityIndex = new int[sectorCount];
            sectorPopularityRanking = new int[sectorCount];
            for(int i = 0; i < sectorCount ; i++) sectorPopularityIndex[i] = 0;
            int allocatedSectors = 0;
            while (allocatedSectors < sectorCount) {
                int newSector = (random() % sectorCount) + 1;
                if (sectorPopularityRanking[newSector-1] == 0) {
                    allocatedSectors++;
                    sectorPopularityRanking[newSector-1] = allocatedSectors;
                    sectorPopularityIndex[allocatedSectors-1] = newSector;
                    //std::cout << allocatedSectors << " out of " << sectorCount << "\n";
                }
            }


            //std::cout << "(Lib) Created a sector map with <" << sectorCount << "> sectors.";
            //std::cout.flush();

            std::vector<double> probCurve = getProbabilityCurve(sectorCount);

            //Saving sector statistical distribution to file
            string filename = std::string(stats->getSimulationDirectory() + stats->getSimulationPrefix() + "_sector_map.csv");
            FILE * pFile = fopen ( filename.c_str(), "w");
            for(int i = 0; i < sectorCount ; i++) {
                //fprintf(pFile, "%i,%i\n",(i+1),sectorPopularityIndex[i]);
                fprintf(pFile, "%i,%i,%i,%f\n",getSectorRow(i+1),getSectorColumn(i+1),sectorPopularityRanking[i],probCurve[sectorPopularityRanking[i]-1]);
            }
            fclose(pFile);
            probCurve.clear();

            //If we're using GPS based library information, we override library sizes for non-zero library, and yes, they are shared
            if (libraryTransit != 0) libraryTransit = sectorCount;
            if (libraryNetwork != 0) libraryNetwork = sectorCount;
            if (libraryMultimedia != 0) libraryMultimedia = sectorCount;
            maxVehicleServers = 0;
        }

    }

}

//Finalization Function (not a destructor!)
void GlobalLibrary::finish() {
    if (multimediaLibrary) multimediaLibrary->clear();
    if (networkLibrary) networkLibrary->clear();
    if (trafficLibrary) trafficLibrary->clear();
}

//
void GlobalLibrary::registerClient(string clientPath){
    //std::cout << "(Lib) Enter registerClient\n";
    //std::cout.flush();

    Enter_Method_Silent();
    clientList.push_front(clientPath);
}

//
void GlobalLibrary::deregisterClient(string clientPath) {
    //std::cout << "(Lib) Enter deregisterClient\n";
    //std::cout.flush();

    Enter_Method_Silent();
    for(auto it = clientList.begin(); it != clientList.end(); it++) {
        if (clientPath.compare(*it) == 0) {
            clientList.erase(it);
           return;
        }
    }
}

//
void GlobalLibrary::handleMessage(cMessage *msg) {
    //std::cout << "(Lib) Enter handleMessage\n";
    //std::cout.flush();

    if ( msg == requestTimer ) {
        if (requestTimer == NULL) return;
        setupPendingRequests();
    }
}

//
void GlobalLibrary::setupPendingRequests() {
    //std::cout << "(Lib) Enter setupPendingRequests\n";
    //std::cout.flush();

    bool foundItem = true;
    simtime_t rangeLimit = 0.001;

    /*
    for (auto it = preemptiveRequests.begin() ; it != preemptiveRequests.end() ; it++) {
        std::cout << "(Lib) Item <" << (*it).x << ";" << (*it).y << "> <" << (*it).time << "> <" << (*it).object->contentPrefix << "> \n";
        std::cout.flush();
    }
    */

    //std::cout << "\t(Lib) Starting new requests.\n";
    //std::cout.flush();

    while (foundItem) {
        LocationRequest_t curRequest = preemptiveRequests.front();
        simtime_t timeDif = (curRequest.time - simTime());
        if (-rangeLimit < timeDif && timeDif < rangeLimit ) {
            //std::cout << "\t(Lib) Starting a new request for item <" << curRequest.object->contentPrefix << "> \t at position <" << curRequest.x << ";" << curRequest.y << "> \n";

            Coord requestLocation;
            requestLocation.x = curRequest.x;
            requestLocation.y = curRequest.y;

            double minDistance = DBL_MAX;
            Client* closestClient = NULL;

            //Checking neighbors for closest neighbor
            for(auto it = clientList.begin(); it != clientList.end();) {
                cModule* module = getModuleByPath((*it).c_str());
                if (module != NULL) {
                    Client* currentClient = check_and_cast<Client *>(module);
                    double distance = requestLocation.distance(currentClient->getPosition());
                    if (distance < minDistance) {
                        minDistance = distance;
                        closestClient = currentClient;
                    }
                    it++;
                } else {
                    it = clientList.erase(it);
                }
            }

            std::list<Client*> viableVehicles;
            //viableVehicles.push_front(closestClient);
            //Rechecking list for other neighbors that might be close
            for(auto it = clientList.begin(); it != clientList.end();it++) {
                cModule* module = getModuleByPath((*it).c_str());
                    Client* currentClient = check_and_cast<Client *>(module);
                    double distance = requestLocation.distance(currentClient->getPosition());
                    //Filtering vehicles up to 10% further or up to 100m from the closest car
                    if ( distance <= 250 && (distance < minDistance * 1.1 || distance < minDistance + 100)) {
                        viableVehicles.push_front(currentClient);
                    }
            }

            //Trying to select a viable vehicle
            bool foundVehicle = false;
            //int potentialVehicles = viableVehicles.size();
            while (!foundVehicle && viableVehicles.size() > 0) {
                int selectedVehicleIndex = floor(uniform(0,viableVehicles.size()));
                auto it = viableVehicles.begin();
                for (int i = 0; i < selectedVehicleIndex && it != viableVehicles.end() ;it++,i++); //Iterating to desired object

                //Attempting to get the client to send the packet
                bool response = (*it)->suggestContentRequest(curRequest.object);
                if (response) {
                    closestClient = (*it);
                    foundVehicle = true;
                    //std::cout << "\t(Lib) Request <" << curRequest.object->contentPrefix << "> was fulfilled by <" << (*it)->getID() << ">\n";
                    //std::cout.flush();
                } else {
                    viableVehicles.erase(it);
                }
            }

            if (!foundVehicle) {
                stats->increasedUnviableRequests();
                //std::cout << "\t(Lib) Warning : No Viable Vehicle found out of <" << potentialVehicles << "> potentials with minimum distance <" << minDistance << ">.\n";
            } else {
                stats->logDistanceFromTweet(requestLocation.distance(closestClient->getPosition()));
                //std::cout << "\t(Lib) Found a Viable Vehicle!!.\n";
            }
            //if (closestClient != NULL) closestClient->suggestContentRequest(curRequest.object);

            preemptiveRequests.front().object = NULL;
            preemptiveRequests.pop_front();
        } else {
            foundItem = false;
            //std::cout << "(Lib) Time Shift is off, not sure why: " << simTime() << " dif " << timeDif << "\n";
            //std::cout.flush();
        }
    }

    //Starting a new timer for the next item that is in front of our list
    //std::cout << "(Lib) Current Time <" << simTime() << "> Next Request is scheduled after time: "  << preemptiveRequests.front().time << std::endl;
    //std::cout.flush();

    if (preemptiveRequests.size() > 0) scheduleAt(preemptiveRequests.front().time, requestTimer);
    else std::cout << "(Lib) Warning: No more requests available for scheduling.\n";

    //std::cout << "(Lib) exit setupPendingRequests\n";
    //std::cout.flush();
}

//
void GlobalLibrary::loadRequestSequence() {

    int simNumber = getSimulation()->getActiveEnvir()->getConfigEx()->getActiveRunNumber();
    std::cout << "[" << simNumber << "]\t(Lib) Loading Request List..." << std::endl;

    //clock_t buildClockTime = clock();

    std::string fileString = loadFile(requestSequenceFile);
    std::string token;
    std::string lineDelimiter = "\n";
    size_t pos = 0;

    //std::list<Content_t>* newLibrary = new std::list<Content_t>();
    multimediaLibrary = new std::list<Content_t>();
    int j = 1;
    //Reading Lines
    while ((pos = fileString.find(lineDelimiter)) != std::string::npos) {
        token = fileString.substr(0, pos);

        //std::cout << "(Lib) Parsing < " << token << " >\n";

        //Getting String Components
        std::string arr[4];
        int i = 0;

        for (int i=0; i< (int)token.length(); i++) {
            if (token[i] == ',')
                token[i] = ' ';
        }

        std::stringstream ssin(token);
        while (ssin.good() && i < 4){
            ssin >> arr[i];
            ++i;
        }

        //Searching the library to see if we already have this content object
        std::string contentPrefix = "m/" + arr[3];
        bool foundDuplicate = false;
        if (multimediaLibrary->size() > 0) {
            for (auto it = multimediaLibrary->begin() ; it != multimediaLibrary->end() ; it++) {
                if (contentPrefix.compare((*it).contentPrefix) == 0 ) {
                    foundDuplicate = true;
                    break;
                }
            }
        }

        if (!foundDuplicate) {
            char *pEnd;
            long int itemIndex = std::strtol(arr[3].c_str() , &pEnd, 10);

            Content_t newContent;
            //newContent.popularityRanking = j;         //Used as shorthand to the ranking of the object in its relative popularity queue
            newContent.popularityRanking = 1;
            newContent.contentIndex = itemIndex;
            newContent.priority = ContentPriority::PRIORITY_LOW;
            newContent.contentClass = ContentClass::MULTIMEDIA;
            newContent.contentSize = sizeMultimedia;
            newContent.contentPrefix = contentPrefix;
            //std::cout << "(Lib) Pushed Library Item: <" << newContent.contentPrefix << "> to library>\n";
            multimediaLibrary->push_back(newContent);

        }

        LocationRequest_t newRequest;
        newRequest.time = SIMTIME_ZERO + atof(arr[0].c_str()) + stats->getStartTime();  //Ta ligado gambiarra?
        newRequest.x = atof(arr[1].c_str());
        newRequest.y = atof(arr[2].c_str());
        newRequest.object = getContent(contentPrefix);
        preemptiveRequests.push_back(newRequest);

        //std::cout << "(Lib) Pushed Library Item: <" << newRequest.object->contentPrefix << "> and time <" << newRequest.time << ">\n";
        //std::cout << "(Lib) Current Last Item: <" << preemptiveRequests.back().object->contentPrefix << "> and time <" << preemptiveRequests.back().time << ">\n";

        //Skipping to next item
        fileString.erase(0, pos + lineDelimiter.length());
        j++;
    }

    //Setting Multimedia Content Parameters
    multimediaContent.category = ContentClass::MULTIMEDIA;
    multimediaContent.byteSize = sizeMultimedia;
    multimediaContent.count =  j - 1;
    multimediaContent.priority = priorityMultimedia;
    multimediaPrefix = "m";

    simtime_t firstTime = preemptiveRequests.front().time;
    std::cout << "(Lib) First Request is scheduled for: "  << firstTime << "\n";

    /*
    for (auto it = preemptiveRequests.begin() ; it != preemptiveRequests.end() ; it++) {
        std::cout << "(Lib) Item <" << (*it).x << ";" << (*it).y << "> <" << (*it).time << "> <" << (*it).object->contentPrefix << "> \n";
        std::cout.flush();
    }
    */

    requestTimer = new cMessage("requestTimer");
    scheduleAt(firstTime, requestTimer);

    //clock_t buildEndTime = clock();
    //double elapsed_secs = double(buildEndTime - buildClockTime) / CLOCKS_PER_SEC;

    //Sorting Content Library (So our items are sorted based on their index, fuck it if we have to do this a thousand times)
    multimediaLibrary->sort(compareLibraryItems);

    //Warning our simulation we are ready
    //std::cout << "\t Done. Build took : " << elapsed_secs << " second(s)" << std::endl;
}

//
NodeRole GlobalLibrary::requestStatus(int vehicleID) {
    //Checking if we have allocated a node as a server
    if (!serverVehicles.empty()) {
        //Checking if the vehicle was already inserted... just to make sure
        for (auto it = serverVehicles.begin() ; it != serverVehicles.end() ; it++) {
            if ((*it) == vehicleID) return NodeRole::SERVER;
        }
    }

    //Checking if we have already allocated the node as a client
    if (!clientVehicles.empty()) {
        for (auto it = clientVehicles.begin() ; it != clientVehicles.end() ; it++) {
            if ((*it) == vehicleID) return NodeRole::CLIENT;
        }
    }

    //Trying to allocate the node as a server
    if (allocatedVehicleServers < maxVehicleServers) {
        //std::cout << "(Lib) \t Vehicle <" << vehicleID << "> is a registered Server.\n";
        allocatedVehicleServers++;
        serverVehicles.push_front(vehicleID);
        return NodeRole::SERVER;
    }

    //Trying to allocate the vehicle as a client
    if (allocatedVehicleClients < maxVehicleClients || maxVehicleClients <= 0) {
        //std::cout << "(Lib) \t Vehicle <" << vehicleID << "> is a registered Client.\n";
        allocatedVehicleClients++;
        clientVehicles.push_front(vehicleID);
        return NodeRole::CLIENT;
    }

    //std::cout << "(Lib) Vehicle <" << vehicleID << "> is a registered mule (other positions are busy).\n";
    return NodeRole::MULE;
}

//Function called by clients when they exit the simulation to release their status as a server or client
void GlobalLibrary::releaseStatus(NodeRole status, int vehicleID){
    if (status == NodeRole::SERVER) {
        for (auto it = serverVehicles.begin() ; it != serverVehicles.end() ; it++) {
            if ((*it) == vehicleID) {
                allocatedVehicleServers--;
                serverVehicles.erase(it);
                return;
            }
        }
        std::cerr << "(Lib) Warning: Vehicle <" << vehicleID << "> is NOT a SERVER but thinks it is.\n";
    }

    if (status == NodeRole::CLIENT) {
        for (auto it = clientVehicles.begin() ; it != clientVehicles.end() ; it++) {
            if ((*it) == vehicleID) {
                allocatedVehicleClients--;
                clientVehicles.erase(it);
                return;
            }
        }
        std::cerr << "(Lib) Warning: Vehicle <" << vehicleID << "> is NOT a CLIENT but thinks it is.\n";
    }

    if (status != NodeRole::MULE) {
        std::cerr << "(Lib) Warning: Vehicle <" << vehicleID << "> is NOT a MULE but it should be.\n";
        std::cerr.flush();
    }
}

//
int GlobalLibrary::getActiveServers() {
    //std::cout << "(Lib) Enter getActiveServers\n";
    //std::cout.flush();
    return allocatedVehicleServers;
}

//
int GlobalLibrary::getMaximumServers() {
    //std::cout << "(Lib) Enter getMaximumServers\n";
    //std::cout.flush();
    return maxVehicleServers;
}

//
void GlobalLibrary::buildContentList() {
    //std::cout << "(Lib) Enter buildContentList\n";
    //std::cout.flush();
    //Checking for previously existing Library (Using multimedia as an example -> built one, built all)
    if (multimediaLibrary != NULL) return;

    if (locationModel == LocationCorrelationModel::TWITTER) {
        loadRequestSequence();
        return;
    }

    //ContentCategoryDistribution_t* contentCategories[libraryCategoriesSize];

    std::cout << "(Lib) Building Library with size <" << (libraryMultimedia+libraryNetwork+libraryTransit) << ">.\n";
    std::cout.flush();

    //Setting Emergency Content Parameters
    emergencyContent.category = ContentClass::EMERGENCY_SERVICE;
    emergencyContent.byteSize = 512;
    emergencyContent.count = 1;                                         //Networking Information should always be available even if partially
    emergencyContent.priority = ContentPriority::PRIORITY_EMERGENCY;    //1-100 priority scale

    //Setting Traffic Content Parameters
    trafficContent.category = ContentClass::TRAFFIC;
    trafficContent.byteSize = sizeTransit;
    trafficContent.count = libraryTransit;
    trafficContent.priority = priorityTransit;
    transitPrefix = "t";

    //Setting Networking Content Parameters
    networkContent.category = ContentClass::NETWORK;
    networkContent.byteSize = sizeNetwork;
    networkContent.count = libraryNetwork;
    networkContent.priority = priorityNetwork;
    networkPrefix = "n";

    //Setting Multimedia Content Parameters
    multimediaContent.category = ContentClass::MULTIMEDIA;
    multimediaContent.byteSize = sizeMultimedia;
    multimediaContent.count = libraryMultimedia;
    multimediaContent.priority = priorityMultimedia;
    multimediaPrefix = "m";

    //Adding elements to list
    //contentCategories[0] = &trafficContent;                                 //Adding Traffic to our category list
    //contentCategories[1] = &networkContent;                                 //Adding Networking to our category list
    //contentCategories[2] = &emergencyContent;                               //Adding Emergency to our category list
    //contentCategories[3] = &multimediaContent;                              //Adding Multimedia to our category list

    //std::cout << "(Lib) Content Library is ready.. Size: " << (multimediaContentSize) << "\n";
    //std::cout.flush();

    //Building all three separate content lists
    multimediaCummulativeProbabilityCurve = buildCategoryLibrary(multimediaContent.count,multimediaContent.byteSize,multimediaContent.priority,multimediaContent.category,multimediaPrefix);
    networkCummulativeProbabilityCurve = buildCategoryLibrary(networkContent.count,networkContent.byteSize,networkContent.priority,networkContent.category,networkPrefix);
    trafficCummulativeProbabilityCurve = buildCategoryLibrary(trafficContent.count,trafficContent.byteSize,trafficContent.priority,trafficContent.category,transitPrefix);
}

//
std::vector<double> GlobalLibrary::buildCategoryLibrary(int count, int byteSize, ContentPriority priority, ContentClass category, std::string classPrefix) {
    //std::cout << "(Lib) Enter buildCategoryLibrary\n";
    //std::cout.flush();

    //std::cout << "(Lib) Building Category <" << classPrefix << ">: Zipf <" << zipfCaracterization << "> Item Count <" << (count) << ">.\n";
    //std::cout.flush();

    //Setting Up for Library Popularity Calculations
    float CDF = 0;
    float Hk = 0;
    float HN = 0;

    std::list<Content_t>* newLibrary = new std::list<Content_t>();

    //Generating Harmonic Series
    for (int i = 1 ; i <= count ; i++) {
        HN += 1 / static_cast<double>(pow(i, zipfCaracterization));
    }
    std::vector<double> cummulativeProbability = std::vector<double> (count);

    //Generating Objects for Class
    for (long j = 0; j < count ; j++) {
        Content_t newContent;

        newContent.popularityRanking = j+1;         //Used as shorthand to the ranking of the object in its relative popularity queue
        newContent.contentIndex = j+1;
        newContent.contentClass = category;
        newContent.contentSize = byteSize;
        newContent.contentPrefix = classPrefix + "/" + std::to_string(j+1);
        newLibrary->push_back(newContent);

        //If Zipf == 0 we don't use ZIPF, everything is uniformily randomly distributed
        if (zipfCaracterization != 0) {
            Hk += 1 / static_cast<double>(pow(j+1,zipfCaracterization));
            CDF = Hk/HN;

            //std::cout << std::to_string(j+1) << ", " << std::to_string(CDF) << "\n";
            //std::cout.flush();

            //multimediaCummulativeProbability[j] = CDF;
        } else {
            CDF = static_cast<double>(1/(double)count) * (j+1);
            //std::cout << "Boo, " << std::to_string(j+1) << ", " << std::to_string(Hk) << ", " << std::to_string(CDF) << "\n";
            //std::cout.flush();
        }
        cummulativeProbability[j] = CDF;
    }

    //Setting the appropriate library pointer
    switch(category) {
        case ContentClass::MULTIMEDIA:
            multimediaLibrary = newLibrary;
            break;

        case ContentClass::NETWORK:
            networkLibrary = newLibrary;
            break;

        case ContentClass::TRAFFIC:
            trafficLibrary = newLibrary;
            break;

        default:
            std::cerr << "(Lib) Error: Weird Class during constructor\n";
            std::cerr.flush();
            break;
    }

    //Returning the probability list
    return cummulativeProbability;
}

//Returns vector with individual probability of each item
std::vector<double> GlobalLibrary::getProbabilityCurve(int count) {
    //Setting Up for Library Popularity Calculations
    float CDF = 0;
    float Hk = 0;
    float HN = 0;

    //Generating Harmonic Series
    for (int i = 1 ; i <= count ; i++) {
        HN += 1 / static_cast<double>(pow(i, zipfCaracterization));
    }
    std::vector<double> cummulativeProbability = std::vector<double> (count);

    //Generating Objects for Class
    for (long j = 0; j < count ; j++) {

        //If Zipf == 0 we don't use ZIPF, everything is uniformily randomly distributed
        if (zipfCaracterization != 0) {
            Hk += 1 / static_cast<double>(pow(j+1,zipfCaracterization));
            CDF = Hk/HN;

            //std::cout << std::to_string(j+1) << ", " << std::to_string(CDF) << "\n";
            //std::cout.flush();

            //multimediaCummulativeProbability[j] = CDF;
        } else {
            CDF = static_cast<double>(1/(double)count) * (j+1);
            //std::cout << "Boo, " << std::to_string(j+1) << ", " << std::to_string(Hk) << ", " << std::to_string(CDF) << "\n";
            //std::cout.flush();
        }
        cummulativeProbability[j] = CDF;
    }

    std::vector<double> individualProbability = std::vector<double> (count);
    individualProbability[0] = cummulativeProbability[0];
    for (long j = 1; j < count ; j++) {
        individualProbability[j] = cummulativeProbability[j] - cummulativeProbability[j-1];
    }

    //Returning the probability list
    return individualProbability;
}

//
std::list<Content_t>* GlobalLibrary::getMultimediaContentList() {
    return multimediaLibrary;
}

//
std::list<Content_t>* GlobalLibrary::getTrafficContentList() {
    return trafficLibrary;
}

//
std::list<Content_t>* GlobalLibrary::getNetworkContentList() {
    return networkLibrary;
}

//
Content_t* GlobalLibrary::getContent(std::string contentPrefix) {
    //std::cout << "(Lib) Enter getContent\n";
    //std::cout.flush();

    //Removing Stupid quotes
    if (contentPrefix.c_str()[0] == '\"') {
        contentPrefix = contentPrefix.substr(1, contentPrefix.length() - 2);
    }

    //Splitting into Substring to get classification
    std::string category = contentPrefix.substr(0, contentPrefix.find("/"));
    char *pEnd;
    long int itemIndex = std::strtol(contentPrefix.substr(contentPrefix.find("/") + 1).c_str() , &pEnd, 10);
    std::list<Content_t>* correctLibrary;

    if (category.compare(transitPrefix) == 0) {
        correctLibrary = trafficLibrary;
    } else if (category.compare(networkPrefix) == 0) {
        correctLibrary = networkLibrary;
    } else if (category.compare(multimediaPrefix) == 0) {
        correctLibrary = multimediaLibrary;
    } else {
        std::cerr << "(Lib) Error: Content Category is not known <" << category << ">\n";
        std::cerr.flush();
        return NULL;
    }

    //We don't apply the good method on twitter requests because its not efficient, shit is not ordered properly
    if (locationModel != LocationCorrelationModel::TWITTER) {
        //Attempting to directly fetch the object from the library to speed lookups as items are stored sequentially
        std::list<Content_t>::iterator attemptedContent = correctLibrary->begin();
        std::advance(attemptedContent,itemIndex-1);
        if (attemptedContent->contentIndex == itemIndex) return &*attemptedContent;

        //We warn of errors only if we are not in setup time
        if (simTime() > 1) {
            std::cout << "(Lib) Warning: Library Indexes are de-synced. We asked for <" << itemIndex << "> and got <" << attemptedContent->popularityRanking << ">\n";
        }
    }

    //If the new method fails, we use the old method where we actualy iterate over the Library
    for (auto it = correctLibrary->begin(); it != correctLibrary->end(); it++) {
        if (it->contentPrefix.compare(contentPrefix) == 0) {
            return &*it;
        }
    }

    return NULL;
}

//
bool GlobalLibrary::independentOperationMode() {
    if (locationModel == LocationCorrelationModel::TWITTER) return false;
    return true;
}

//
bool GlobalLibrary::locationDependentContentMode() {
    if (locationModel == LocationCorrelationModel::GPS) return true;
    return false;
}

//
int GlobalLibrary::getContentClass(ContentClass cClass) {
    return static_cast<int>(cClass);
}


int GlobalLibrary::getSectorSize() {
    if (sectorWidth == sectorHeight) return sectorWidth;
    return -1;  //If the sizes are different then we're fucked and who cares, I didn't have time to implement logic with non-regular sectors
}

//
int GlobalLibrary::getSectorRow(int sectorCode) {
    Enter_Method_Silent();
    return static_cast<int> (floor((sectorCode-1) / widthBlocks)) + 1;
}

//
int GlobalLibrary::getSectorColumn(int sectorCode) {
    Enter_Method_Silent();
    return static_cast<int> ((sectorCode-1) % widthBlocks) + 1;
}

//
int GlobalLibrary::getSector(double xPos, double yPos) {
    Enter_Method_Silent();
    int sector = floor(xPos/sectorWidth) + ( floor(yPos/sectorHeight) )* widthBlocks;
    return sector;
}

//
int GlobalLibrary::getDistanceToSector(int sectorCode, double xPos, double yPos) {
    Enter_Method_Silent();
    int sectorCenterX = static_cast <int> (floor((floor((sectorCode-1) % widthBlocks) * sectorWidth) + sectorWidth/2));
    int sectorCenterY = static_cast <int> (floor((floor((sectorCode-1) / widthBlocks) * sectorHeight) + sectorHeight/2));
    double eucledianDistance = round(sqrt( pow(sectorCenterX - xPos, 2) + pow(sectorCenterY - yPos, 2) ));
    //std::cout << " \n Sector Analysis <" << sectorCode << "> with <" << floor(xPos) << " ; " << floor(yPos)  << "> Against <" << sectorCenterX << " ; " << sectorCenterY << ">\t Distance: " << eucledianDistance << "\n";
    return static_cast <int> (eucledianDistance);
}

//
bool GlobalLibrary::viablyCloseToContentLocation(int sectorCode, double xPos, double yPos) {
    Enter_Method_Silent();
    int calculatedDistance = getDistanceToSector(sectorCode, xPos, yPos);
    //std::cout << "(Lib) Node is <" << calculatedDistance << "> linear meters to sector in question";
    //std::cout.flush();
    if (calculatedDistance <= maximumViableDistance) return true;
    return false;
}

//
int GlobalLibrary::getIndexForDensity(double value, ContentClass contentClass ) {
    return getIndexForDensity(value,contentClass,0);
}

//
int GlobalLibrary::getIndexForDensity(double value, ContentClass contentClass, double xPos, double yPos ) {
    Enter_Method_Silent();
    int sector = getSector(xPos,yPos);
    switch (locationModel) {
        case LocationCorrelationModel::NONE:
            return getIndexForDensity(value,contentClass,0);
            break;

        case LocationCorrelationModel::GRID:
        case LocationCorrelationModel::SAW:
            //std::cout << "(Lib) <" << xPos << ";" << yPos << "> Maps to Sector <" << sector << "\\" << sectorCount << "> of <" << widthBlocks << "x" << heightBlocks << ">\n";
            return getIndexForDensity(value,contentClass,sector);
            break;

        case LocationCorrelationModel::GPS:
            //std::cout << "(Lib) Generating Sector value from coordinates <" << xPos << ";" << yPos << "> Maps to Sector <" << sector << ">";
            //return sector;
            return getIndexForDensity(value,contentClass,sector);
        default:
            std::cerr << "(Lib) ERROR: Not Implemented\n";
            return getIndexForDensity(value,contentClass);
    }
}

//
float GlobalLibrary::getDensityForIndex(double index,ContentClass contentClass ) {
    switch(contentClass) {
        case ContentClass::MULTIMEDIA:
            return multimediaCummulativeProbabilityCurve[index+1] - multimediaCummulativeProbabilityCurve[index];
            break;

        case ContentClass::NETWORK:
            return networkCummulativeProbabilityCurve[index+1] - networkCummulativeProbabilityCurve[index];
            break;

        case ContentClass::TRAFFIC:
            return trafficCummulativeProbabilityCurve[index+1] - trafficCummulativeProbabilityCurve[index];
            break;

        default:
            std::cerr << "(Lib) Error: Requesting density for undefined category <" << static_cast<int>(contentClass) << ">\n";
            return -1;
    }
    return 1;
}

//
int GlobalLibrary::getIndexForDensity(double value, ContentClass contentClass, int sector ) {
    //std::cout << "(Lib) Enter getIndexForDensity\n";
    //std::cout.flush();
    std::vector<double> probabilityCurve;
    //Figuring out the appropriate list
    switch(contentClass) {
        case ContentClass::MULTIMEDIA:
            probabilityCurve = multimediaCummulativeProbabilityCurve;
            break;

        case ContentClass::NETWORK:
            probabilityCurve = networkCummulativeProbabilityCurve;
            break;

        case ContentClass::TRAFFIC:
            probabilityCurve = trafficCummulativeProbabilityCurve;
            break;

        default:
            std::cerr << "(Lib) Error: Requesting density for undefined category <" << static_cast<int>(contentClass) << ">\n";
            return -1;
    }

    //Checking if the probability index is outside the possible range
    if (value < 0 || value > 1) {
        return -1;
    }

    //Checking which item meets the minimum probability
    int librarySize = probabilityCurve.size();
    int itemIndex = -1;
    for (int j = 0 ; j < librarySize ; j++) {
        if (value <= probabilityCurve[j]) {
            //return j + 1;
            itemIndex = j+1;

            int shiftedIndex = 0;

            switch (locationModel) {
                case LocationCorrelationModel::NONE:
                    shiftedIndex = itemIndex;
                    break;

                case LocationCorrelationModel::GRID:
                    shiftedIndex = ( (sector * (int)ceil(librarySize/sectorCount)) + itemIndex) % librarySize;
                    break;

                case LocationCorrelationModel::SAW:
                    shiftedIndex = ( itemIndex + (librarySize % sector)) % librarySize;
                    break;

                case LocationCorrelationModel::GPS: //For GPS, the sectors themselves have probability associated with them, we can treat them like the regular one
                    shiftedIndex = itemIndex;
                    break;

                default:
                    std::cout << "(Lib) Error!!\n";
                    break;
            }

            //std::cout << "(Lib) Shift to <" << shiftedIndex << "<   <--<  <" << itemIndex << "> for <" << sector << "\\" << sectorCount << ">\n";
            return shiftedIndex;
            break;
        }
    }

    return -1;
}

//
int GlobalLibrary::getClassFreeIndex(string contentPrefix) {

    std::stringstream ss(contentPrefix);
    std::string item;
    while (std::getline(ss, item, '/')) {
        //std::cout << "item: <" << item << ">\n";
    }

    //std::cout << "In theory, our element is <" << item << ">\n";
    //std::cout.flush();

    return std::stoi(item);
}

//
int GlobalLibrary::getSectorFromPrefixIndex(int index) {
    return sectorPopularityIndex[index-1];
}

//
long int GlobalLibrary::getCurrentRequestIndex() {
    return currentRequestIndex;
}

//
long int GlobalLibrary::getRequestIndex() {
    currentRequestIndex++;
    return currentRequestIndex;
}

//
std::string GlobalLibrary::cleanString(std::string inputString) {
    //Removing that god damn fucking quote
    if (inputString.c_str()[0] == '\"') {
        inputString = inputString.substr(1,inputString.length()-2);
    }
    return inputString;
    //return inputString.erase(std::remove(inputString.begin(), inputString.end(), '\"'), inputString.end());
}

//
bool GlobalLibrary::equals(Content_t* first, Content_t* second) {
    return first == second;
    //return equals(*first,*second);
}
//
bool GlobalLibrary::equals(Content_t first, Content_t second) {
    if (first.contentClass != second.contentClass) return false;
    if (first.contentIndex != second.contentIndex) return false;
    return true;
}

//
bool GlobalLibrary::equals(Content_t first, std::string second) {
    //Splitting into Substring to get classification
    std::string secondCategory = second.substr(0, second.find("/"));
    char *pEnd;
    long int itemIndex = std::strtol(second.substr(second.find("/") + 1).c_str() , &pEnd, 10);

    //Comparing Index
    if (first.contentIndex != itemIndex) return false;

    //Fetching and Comparing Category
    if (secondCategory.compare(transitPrefix) == 0) {
        if (first.contentClass == ContentClass::TRAFFIC) return true;
    } else if (secondCategory.compare(networkPrefix) == 0) {
        if (first.contentClass == ContentClass::NETWORK) return true;
    } else if (secondCategory.compare(multimediaPrefix) == 0) {
        if (first.contentClass == ContentClass::MULTIMEDIA) return true;
    }

    //Edge Case
    std::cerr << "(Lib) Error: Content Category substring was not found <" << secondCategory << ">\n";
    std::cerr << "\t" << secondCategory.compare(transitPrefix) << "\t" << secondCategory.compare(networkPrefix) << "\t" << secondCategory.compare(multimediaPrefix) << "\n";
    return false;
}

