/*
 * BaconLibrary.cc
 *
 *  Created on: Jan 25, 2016
 *      Author: felipe
 */

#include <paradise/bacon/BaconLibrary.hh>

Define_Module(BaconLibrary);

//Initialization Function
void BaconLibrary::initialize(int stage) {

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

        locationModel = static_cast<LocationCorrelationModel>(par("locationCorrelationModel").longValue());

        maxVehicleServers = par("maxVehicleServers").longValue();
        requestSequenceFile = par("requestSequenceFile").stringValue();

        multimediaLibrary = NULL;
        networkLibrary = NULL;
        trafficLibrary = NULL;

        //Calculating world size in sections
        double scenarioWidth = world->getPgs()->x;
        double scenarioHeight = world->getPgs()->y;

        widthBlocks = (int)ceil(scenarioWidth/sectorWidth);
        heightBlocks = (int)ceil(scenarioHeight/sectorHeight);

        sectorCount = widthBlocks*heightBlocks;

        std::cout << "(Lib) Request Location Correlation follows Distribution <" << locationModel << "> in <" << widthBlocks << ";" << heightBlocks << "> Grid.\n";
        std::cout.flush();

        //Setting Sequential request index
        currentIndex = 0;
    }

    //After we've been built and assume that the statistics object has also, we fill our content list
    if (stage == 1) {

        cSimulation *sim = getSimulation();
        cModule *modp = sim->getModuleByPath("BaconScenario.statistics");
        stats = check_and_cast<BaconStatistics *>(modp);

        buildContentList();

        std::cout << "(Lib) Writing Content List to Statistics for collection (warning: might be ignored).\n";
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
    }
}

//Finalization Function (not a destructor!)
void BaconLibrary::finish() {
    if (multimediaLibrary) multimediaLibrary->clear();
    if (networkLibrary) networkLibrary->clear();
    if (trafficLibrary) trafficLibrary->clear();
}

void BaconLibrary::registerClient(string clientPath){
    //std::cout << "(Lib) Enter registerClient\n";
    //std::cout.flush();

    Enter_Method_Silent();
    clientList.push_front(clientPath);
}

void BaconLibrary::deregisterClient(string clientPath) {
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

void BaconLibrary::handleMessage(cMessage *msg) {
    //std::cout << "(Lib) Enter handleMessage\n";
    //std::cout.flush();

    if ( msg == requestTimer ) {
        if (requestTimer == NULL) return;
        setupPendingRequests();
    }
}

void BaconLibrary::setupPendingRequests() {
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
            BaconClient* closestClient = NULL;

            //Checking neighbors for closest neighbor
            for(auto it = clientList.begin(); it != clientList.end();) {
                cModule* module = getModuleByPath((*it).c_str());
                if (module != NULL) {
                    BaconClient* currentClient = check_and_cast<BaconClient *>(module);
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

            std::list<BaconClient*> viableVehicles;
            viableVehicles.push_front(closestClient);
            //Rechecking list for other neighbors that might be close
            for(auto it = clientList.begin(); it != clientList.end();it++) {
                cModule* module = getModuleByPath((*it).c_str());
                    BaconClient* currentClient = check_and_cast<BaconClient *>(module);
                    double distance = requestLocation.distance(currentClient->getPosition());
                    //Filtering vehicles up to 10% further or up to 100m from the closest car
                    if (distance < minDistance * 1.1 || distance < minDistance + 100) {
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
void BaconLibrary::loadRequestSequence() {
    std::cout << "(Lib) Loading Request List..." << std::endl;

    clock_t buildClockTime = clock();

    string fileString = loadFile(requestSequenceFile);
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
        string arr[4];
        int i = 0;

        for (int i=0; i< (int)token.length(); i++) {
            if (token[i] == ',')
                token[i] = ' ';
        }

        stringstream ssin(token);
        while (ssin.good() && i < 4){
            ssin >> arr[i];
            ++i;
        }

        //Searching the library to see if we already have this content object
        string contentPrefix = "m/" + arr[3];
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
            Content_t newContent;
            //newContent.popularityRanking = j;         //Used as shorthand to the ranking of the object in its relative popularity queue
            newContent.popularityRanking = 1;
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

    clock_t buildEndTime = clock();
    double elapsed_secs = double(buildEndTime - buildClockTime) / CLOCKS_PER_SEC;

    std::cout << "\t Done. Build took : " << elapsed_secs << " second(s)" << std::endl;
}

//
bool BaconLibrary::requestServerStatus(int vehicleID) {
    //std::cout << "(Lib) Enter requestServerStatus\n";
    //std::cout.flush();

    if (!serverVehicles.empty()) {
        for (auto it = serverVehicles.begin() ; it != serverVehicles.end() ; it++) {
            if ((*it) == vehicleID) return true;
        }
    }

    if (allocatedVehicleServers < maxVehicleServers) {
        allocatedVehicleServers++;
        serverVehicles.push_front(vehicleID);

        //std::cout << "\t(Lib) Add to <" << allocatedVehicleServers << "> servers by vehicle <" << vehicleID << "> \n";
        //std::cout.flush();
        return true;
    }
    return false;
}

//
void BaconLibrary::releaseServerStatus(int vehicleID){
    //std::cout << "(Lib) Enter releaseServerStatus\n";
    //std::cout.flush();
    for (auto it = serverVehicles.begin() ; it != serverVehicles.end() ; it++) {
        if ((*it) == vehicleID) {
            allocatedVehicleServers--;
            //std::cout << "\t(Lib) Sub to <" << allocatedVehicleServers << "> servers by vehicle <" << vehicleID << "> \n";
            //std::cout.flush();

            serverVehicles.erase(it);
            return;
        }
    }

    std::cerr << "(Lib) Warning: Vehicle <" << vehicleID << "> is NOT a server but thinks it is.\n";
    std::cerr.flush();
}

int BaconLibrary::getActiveServers() {
    //std::cout << "(Lib) Enter getActiveServers\n";
    //std::cout.flush();
    return allocatedVehicleServers;
}

int BaconLibrary::getMaximumServers() {
    //std::cout << "(Lib) Enter getMaximumServers\n";
    //std::cout.flush();
    return maxVehicleServers;
}

//
void BaconLibrary::buildContentList() {
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
std::vector<double> BaconLibrary::buildCategoryLibrary(int count, int byteSize, ContentPriority priority, ContentClass category, std::string classPrefix) {
    //std::cout << "(Lib) Enter buildCategoryLibrary\n";
    //std::cout.flush();

    std::cout << "(Lib) Building Category <" << classPrefix << ">: Zipf <" << zipfCaracterization << "> Item Count <" << (count) << ">.\n";
    std::cout.flush();

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
    for (int j = 0; j < count ; j++) {
        Content_t newContent;

        newContent.popularityRanking = j+1;         //Used as shorthand to the ranking of the object in its relative popularity queue
        newContent.contentClass = category;
        newContent.contentSize = byteSize;
        //newContent.contentStatus = ContentStatus::AVAILABLE;
        newContent.contentPrefix = classPrefix + "/" + std::to_string(j+1);
        //newContent.useCount = 0;
        //newContent.lastAccessTime = 0;

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

//
std::list<Content_t>* BaconLibrary::getMultimediaContentList() {
    return multimediaLibrary;
}

//
std::list<Content_t>* BaconLibrary::getTrafficContentList() {
    return trafficLibrary;
}

//
std::list<Content_t>* BaconLibrary::getNetworkContentList() {
    return networkLibrary;
}

//
Content_t* BaconLibrary::getContent(std::string contentPrefix) {
    //std::cout << "(Lib) Enter getContent\n";
    //std::cout.flush();

    //Removing Stupid quotes
    if (contentPrefix.c_str()[0] == '\"') {
        contentPrefix = contentPrefix.substr(1, contentPrefix.length() - 2);
    }

    //Splitting into Substring to get classification
    std::string category = contentPrefix.substr(0, contentPrefix.find("/"));
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

    for (auto it = correctLibrary->begin(); it != correctLibrary->end(); it++) {
        if (it->contentPrefix.compare(contentPrefix) == 0) {
            return &*it;
        }
    }

    return NULL;
}

bool BaconLibrary::independentOperationMode() {
    //std::cout << "(Lib) Enter independentOperationMode\n";
    //std::cout.flush();
    if (locationModel == LocationCorrelationModel::TWITTER) return false;
    return true;
}

//
int BaconLibrary::getContentClass(ContentClass cClass) {
    return static_cast<int>(cClass);
}

//
int BaconLibrary::getIndexForDensity(double value, ContentClass contentClass ) {
    return getIndexForDensity(value,contentClass,0);
}

int BaconLibrary::getIndexForDensity(double value, ContentClass contentClass, double xPos, double yPos ) {
    //std::cout << "(Lib) Enter getIndexForDensity\n";
    //std::cout.flush();
    int sector = 0;
    sector = floor(xPos/sectorWidth) + ( floor(yPos/sectorHeight) )*widthBlocks;

    switch (locationModel) {
        case LocationCorrelationModel::NONE:
            return getIndexForDensity(value,contentClass,0);
            break;

        case LocationCorrelationModel::GRID:
        case LocationCorrelationModel::SAW:
            //std::cout << "(Lib) <" << xPos << ";" << yPos << "> Maps to Sector <" << sector << "\\" << sectorCount << "> of <" << widthBlocks << "x" << heightBlocks << ">\n";
            return getIndexForDensity(value,contentClass,sector);
            break;

        default:
            std::cerr << "(Lib) ERROR: Not Implemented\n";
            return getIndexForDensity(value,contentClass);
    }
}

int BaconLibrary::getIndexForDensity(double value, ContentClass contentClass, int sector ) {
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
                case LocationCorrelationModel::GRID:
                    shiftedIndex = ( (sector * (int)ceil(librarySize/sectorCount)) + itemIndex) % librarySize;
                    break;

                case LocationCorrelationModel::SAW:
                    shiftedIndex = ( itemIndex + (librarySize % sector)) % librarySize;
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
long int BaconLibrary::getCurrentIndex() {
    return currentIndex;
}

//
long int BaconLibrary::getRequestIndex() {
    currentIndex++;
    return currentIndex;
}

