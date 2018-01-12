//Concent Centric Class - Felipe Modesto

#include <paradise/bacon/ContentStore.h>

using Veins::TraCIMobilityAccess;
using Veins::AnnotationManagerAccess;

Define_Module(ContentStore);

//Initialization Function
void ContentStore::initialize(int stage) {
    if (stage == 0) {
        //Getting Parent Module Index
        myId = getParentModule()->getIndex();

        cachePolicy = static_cast<CacheReplacementPolicy>(par("cacheReplacementPolicy").longValue());
        startingCache = par("startingCache").doubleValue();
        maxCachedContents = par("maxCachedContents").longValue();
        locationDependentCacheSize = par("geoCacheSize").longValue();
        maxCachedContents -= locationDependentCacheSize;                //Resizing total cache size

        gpsCacheWindowSize = par("gpsKnowledgeWindowSize").longValue();

        //Creating our GPS based list of lists
        gpsCacheFrequencyWindow.clear();
        OverheardMessageList_t* firstSecondList = new OverheardMessageList_t();
        firstSecondList->simTime = 0;
        gpsCacheFrequencyWindow.push_front(firstSecondList);

        librarySize = 0;
        WATCH(librarySize);
    }

    if (stage == 1) {
        //Getting modules we have to deal with
        traci = check_and_cast<Veins::TraCIMobility*>(getParentModule()->getSubmodule("veinsmobility"));
        manager = check_and_cast<ServiceManager *>(getParentModule()->getSubmodule("appl"));
    }

    if (stage == 2) {
        //Getting global modules we have to deal with
        cSimulation *sim = getSimulation();
        stats = check_and_cast<Statistics*>(sim->getModuleByPath("ParadiseScenario.statistics"));
        library = check_and_cast<GlobalLibrary*>(sim->getModuleByPath("ParadiseScenario.library"));

        nodeRole = library->requestStatus(myId);
        if (nodeRole == NodeRole::SERVER) {
            startingCache = -1;
            maxCachedContents = -1;
        }

        buildContentLibrary();

        gpsCacheTimerMessage = new cMessage("gpsCacheUpdateTimer");
        resetGPSTimer();
    }
}

//Finalization Function (not a destructor!)
void ContentStore::finish() {
    library->releaseStatus(nodeRole,myId);
    contentCache.empty();

    if (gpsCacheTimerMessage != NULL) {
        cancelAndDelete(gpsCacheTimerMessage);
        gpsCacheTimerMessage = NULL;
    }
}

//
void ContentStore::resetGPSTimer() {
    //std::cout << "[" << myId << "]\t(CS) Reseting GPS Timer\n";
    //std::cout.flush();

    if (gpsCacheTimerMessage == NULL) {
        gpsCacheTimerMessage = new cMessage("gpsCacheUpdateTimer");
    } else {
        cancelEvent(gpsCacheTimerMessage);
    }
    scheduleAt(simTime() + gpsUpdateTimerTime, gpsCacheTimerMessage);
}

//
void ContentStore::runCacheReplacement(){
    //std::cout << "(CP) Running Cache Replacement!\n";
    //std::cout.flush();

    if (maxCachedContents == -1) {
        //std::cout << "(CP) <" << myId << "> We have infinite Cache. This is not relevant <3.\n";
        return;
    }

    int currentCacheSize = contentCache.size();
    int cacheOverflow = currentCacheSize - maxCachedContents + 1;

    //Checking if we actually have to get worried about cache replacement
    if (cacheOverflow <= 0) {
        EV << "(CP) Cache is fine... don't worry about it.\n";
        EV.flush();
        return;
    }
    for (int i = 0; i< cacheOverflow ; i++) {
        //std::cout << "(CP) Removing Item!\n";
        //std::cout.flush();
        switch(cachePolicy) {
            //RANDOM
            case CacheReplacementPolicy::RANDOM:
            {
                int removableIndex = uniform(0,maxCachedContents);
                int currentIndex = 0;
                for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
                    if (currentIndex == removableIndex) {
                        std::string itemName = it->referenceObject->contentPrefix;

                        //EV << "(CP) Removing Item <" << itemName << "> from Cache.\n";
                        //EV.flush();

                        //std::cout << "(CP) <" << myId << "> Removing Item <" << itemName << "> from Cache.\n";
                        //std::cout.flush();

                        it = contentCache.erase(it);
                        stats->increaseCacheReplacements();
                        librarySize--;
                        break;
                    }
                    currentIndex++;
                }
                break;
            }

            //LEAST RECENTLY USED
            case LRU:
            {
                //Looking for earliest time
                int sameTimeCount = 1;
                simtime_t earliestTime = SIMTIME_MAX;
                for (auto it = contentCache.begin(); it != contentCache.end() ; it++) {
                    if (it->lastAccessTime < earliestTime) {
                        earliestTime = it->lastAccessTime;
                        sameTimeCount = 1;
                    } else if (it->lastAccessTime == earliestTime) {
                        sameTimeCount++;
                    } else {
                        //DO NOTHING, ITEM is SAFE
                    }
                }
                double individualProbability = (double)(1/(double)sameTimeCount);
                if (individualProbability == 0) {
                    std::cout << "(CP) ERROR: PROBABILITY LOCK.\n";
                    std::cout.flush();
                }

                //if (sameTimeCount > 1) {
                //    std::cout << "(CP) Warning: Multiple items with same caching time detected! Earliest Cache Time is <" << earliestTime.str() << ">\n";
                //    std::cout.flush();
                //}

                bool foundRemoval = false;
                //Searching for last item with desired time value to be removed
                while (foundRemoval == false) {
                    for (auto it = contentCache.begin(); it != contentCache.end() ; it++) {
                        if (it->lastAccessTime == earliestTime) {
                            //Randomly selecting one of the items with the last access time with equal probability
                            if ( (foundRemoval == false) &&
                                 (sameTimeCount == 1 || uniform(0,1) < individualProbability)) {
                                foundRemoval = true;
                                std::string itemName = it->referenceObject->contentPrefix;

                                //std::cout << "(CP) <" << myId << "> Removing Item <" << it->contentPrefix << "> from Cache with Use Count <" << it->useCount << ">\t last access <" << it->lastAccessTime << ">.\n";
                                //std::cout.flush();

                                it = contentCache.erase(it);
                                stats->increaseCacheReplacements();
                                librarySize--;
                                break;
                            }
                        }
                    }
                }
                break;
            }

            //LEAST FREQUENTLY USED
            case LFU:
            {
                //std::cout << "(CP) Running LFU Cache Replacement.\n";
                //std::cout.flush();

                //Looking for least used messages (aware of multiple occurences of same metric value)
                int sameUseCount = 1;
                int timesUsed = library->getCurrentRequestIndex()+1;
                for (auto it = contentCache.begin(); it != contentCache.end() ; it++) {
                    if (it->useCount < timesUsed) {
                        timesUsed = it->useCount;
                        sameUseCount = 1;
                    } else if (it->useCount == timesUsed) {
                        sameUseCount++;
                    } else {
                        //DO NOTHING
                    }
                }
                double individualProbability = (double)(1/(double)sameUseCount);
                if (individualProbability == 0) {
                    std::cout << "(CP) ERROR: PROBABILITY LOCK.\n";
                    std::cout.flush();
                }

                //if (sameUseCount > 1) {
                //    std::cout << "(CP) Warning: Multiple items (total count: <" << sameUseCount << ">) with same cache use frequency detected! Lowest use count is <" << timesUsed << ">\n";
                //    std::cout.flush();
                //}

                bool foundRemoval = false;
                //Searching for last item with desired time value to be removed
                while (foundRemoval == false) {
                    for (auto it = contentCache.begin(); it != contentCache.end() ; it++) {
                        if (it->useCount == timesUsed) {
                            if ( (foundRemoval == false) &&
                                 (sameUseCount == 1 || uniform(0,1) < individualProbability)) {
                                foundRemoval = true;

                                //std::cout << "(CP) <" << myId << "> Removing Item <" << it->contentPrefix << "> from Cache with Uses <" << it->useCount << ">.\n";
                                //std::cout.flush();

                                it = contentCache.erase(it);
                                stats->increaseCacheReplacements();
                                librarySize--;
                                break;
                            }
                        }
                    }
                }
                break;
            }

            case FIFO: {
                contentCache.pop_front();
                break;
            }

            case GOD_POPULARITY: {
                int largestRanking = 0;
                //std::list<Content_t>::iterator contentPointer;
                for (auto it = contentCache.begin(); it != contentCache.end() ; it++) {
                    if (it->referenceObject->popularityRanking >= largestRanking) {
                        largestRanking = it->referenceObject->popularityRanking;
                    }
                }

                //TODO: (REVIEW) This is inneficient and I should rewrite for a single-loop
                for (auto it = contentCache.begin(); it != contentCache.end() ; it++) {
                if (it->referenceObject->popularityRanking == largestRanking) {
                        //std::cout <<  "(CP) Removing item <" << it->contentPrefix << "> with Popularity : <" << to_string(it->popularityRanking) << ">\n";
                        //std::cout.flush();

                        contentCache.erase(it);
                        stats->increaseCacheReplacements();
                        librarySize--;
                        break;
                    }
                }
                break;
            }
            //case BIG:
            //    break;

            //case MULT_FIRST:
            //    break;

            //case GPS_FIRST:
            //    break;

            default:
                EV << "(CP) DEFAULT CASE POLICY. OH NOES.\n";
                EV.flush();
                break;
        }
    }
}

//
void ContentStore::maintainGPSCache() {
    OverheardMessageList_t* frontFrequencySlice = gpsCacheFrequencyWindow.front();
    if (frontFrequencySlice->simTime != floor( simTime().dbl()) ) {

        //std::cout << "(CS) Moving our sliding window from <" << frontFrequencySlice->simTime << "> to <" << simTime().dbl() << ">\n";
        //std::cout.flush();

        //Checking if we need to delete a column
        if (static_cast<int>(gpsCacheFrequencyWindow.size()) == gpsCacheWindowSize) {
            //Deleting column and its content
            OverheardMessageList_t* oldestFrequencySlice = gpsCacheFrequencyWindow.back();
            gpsCacheFrequencyWindow.pop_back();
            delete(oldestFrequencySlice);
        }

        //Since we moved to a separate second, we should merge the statistics from the previous front with our own
        for (auto itA = neighborGPSInformation.gpsList.begin(); itA != neighborGPSInformation.gpsList.end() ; itA++) {
            //std::cout << "(CS) Importing knowledge acquired from neighbors. We know this much knowledge: <" << neighborGPSInformation.gpsList.size() << ">\n";
            //std::cout.flush();

            bool foundRemoteItem = false;
            for (auto itB = frontFrequencySlice->gpsList.begin(); itB != frontFrequencySlice->gpsList.end() ; itB++) {
                if (itA->contentPrefix.compare(itB->contentPrefix) == 0) {
                    foundRemoteItem = true;
                    itB->referenceCount += itA->referenceCount;
                }
            }
            //If we haven't found the remote item we create a new entry
            if (!foundRemoteItem) {
                OverheardMessageObject_t newGPSObject;
                newGPSObject.contentPrefix = itA->contentPrefix;
                newGPSObject.referenceCount = itA->referenceCount;
                newGPSObject.contentClass = itA->contentClass;
                frontFrequencySlice->gpsList.push_front(newGPSObject);
            }
        }

        //Now we create a new column
        OverheardMessageList_t* newerFrequencySlice = new OverheardMessageList_t();
        newerFrequencySlice->simTime = floor(simTime().dbl());
        gpsCacheFrequencyWindow.push_front(newerFrequencySlice);
        frontFrequencySlice = newerFrequencySlice;
    }
}

//
void ContentStore::logLocationDependentRequest(Content_t* object) {
    //std::cout << "(CS) Attempting to Log GPS relevant information for item <" << object->contentPrefix << ">\n";
    //std::cout.flush();

    //Check if the object is already cached
    if (fetchFromCache(object->contentPrefix) != NULL) {
        //TODO: Validate if the logic holds... perhaps if we already have the item we don't care?
        //std::cout << "(CS) Object is cached! we don't care about GPS interests for this item.\n";
        //std::cout.flush();
        return;
    }

    //Maintaining Cache for the sake of maintaining cache
    maintainGPSCache();
    OverheardMessageList_t* frontFrequencySlice = gpsCacheFrequencyWindow.front();

    //Searching most recent slice
    bool foundItem = false;
    for (auto it = frontFrequencySlice->gpsList.begin(); it != frontFrequencySlice->gpsList.end() ; it++) {
        if (object->contentPrefix.compare(it->contentPrefix) == 0) {
            //If we found the element, let's update the status
            it->referenceCount++;
            foundItem = true;

            std::cout << "[" << myId << "]\t(CS) Increasing Popularity to <" << it->referenceCount++ << ">\n";
            std::cout.flush();
            break;
        }
    }

    //If we haven't found the object, we'll create a new entry
    if (!foundItem) {
        OverheardMessageObject_t newGPSObject;
        newGPSObject.contentPrefix = object->contentPrefix;
        newGPSObject.referenceCount = 1;
        newGPSObject.contentClass = object->contentClass;
        frontFrequencySlice->gpsList.push_front(newGPSObject);
    }
}

//
void ContentStore::addContentToLibrary(Content_t* contentObject) {
    if (contentObject == NULL) {
        EV_ERROR << "(CP) Content to be added to library is NULL.\n";
        EV_ERROR.flush();

        std::cerr << "(CP) Error: Content to be added to library is NULL.\n";
        std::cerr.flush();
        return;
    }

    //Prior to Simulation Start during Warmup, we're just building our libraries so we ignore "searches"
    //Note: Please keep this in mind when cold starting simulations
    //if (simTime() >= stats->getStartTime()) {
    if (!nodeRole) {
        //Looking for item in content library
        for (auto it = contentCache.begin(); it != contentCache.end() ; it++) {
            if (it->referenceObject->contentPrefix.compare(contentObject->contentPrefix) == 0) {
                //std::cout << "(CP) <" << myId << "> Already has " << contentObject->contentPrefix << " in local Library.\n";
                //std::cout.flush();
                //increaseUseCount(contentObject->contentPrefix);
                return;
            }
        }
    }

    //Checking for Content replacement prior to adding content to library to ensure content will not be instantly removed based on policy behavior
    int currentCacheSize = contentCache.size();
    if ( currentCacheSize >= maxCachedContents && maxCachedContents != -1) {
        //std::cout << "(CP) Cache is full.\n";
        //std::cout.flush();

        stats->increaseCacheReplacements();
        runCacheReplacement();
    }

    //newContent.contentPopularity = contentObject->contentPopularity;
    //newContent.contentPriority =  contentObject->contentPriority;

    CachedContent_t newContent;
    newContent.referenceObject = contentObject;
    newContent.contentStatus = ContentStatus::AVAILABLE;
    newContent.lastAccessTime = simTime();
    newContent.useCount = 0;
    //newContent.expireTime = SIMTIME_S;
    contentCache.push_front(newContent);
    librarySize++;

    //EV << "(CP) Added content object " << contentObject->contentPrefix << " to local Library.\n";
    //EV.flush();

    //std::cerr << "\t(CP) <" << myId << "> Added content object " << contentObject->contentPrefix << " to local Library.\n";
    //std::cerr.flush();
}

//
void ContentStore::removeContentFromLibrary(Content_t* newContent) {
    //Searching for content
    for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
        if (newContent->contentPrefix.compare(it->referenceObject->contentPrefix) == 0) {
            std::cout << "(CP) <" << myId << "> Removing Item <" << it->referenceObject->contentPrefix << "> from Cache with Use Count <" << it->useCount << ">.\n";
            std::cout.flush();
            it = contentCache.erase(it);
            librarySize--;
            return;
        }
    }

    std::cerr << "(CP) <" << myId << "> Warning: Failed to remove object from Cache Library!\n";
    std::cerr.flush();
}

//
void ContentStore::buildContentLibrary() {
    //Building Content Database
    if (hasLibrary) return;
    hasLibrary = true;

    //std::cout << "<" << myId << "> Building Library <" << startingCache << ">.\n";
    //std::cout.flush();

    if (library == NULL) {
        std::cerr << "(CP) Error: Global Content Library module reference is null.\n";
        std::cerr.flush();
        return;
    }

    //Building our Content Library from the complete library
    if (startingCache == -1 and !library->locationDependentContentMode()) {
        //Adding Items from other categories
        std::cout << "(CP) Server <" << myId << "> Standing By.\n";
        std::cout.flush();

        std::list<Content_t>* multimediaLibrary = library->getMultimediaContentList();
        std::list<Content_t>* networkLibrary = library->getNetworkContentList();
        std::list<Content_t>* trafficLibrary = library->getTrafficContentList();

        librarySize = 0;
        if (multimediaLibrary != NULL) {
            for (auto it = multimediaLibrary->begin(); it != multimediaLibrary->end(); it++) {
                //std::cout << "\t\\--> Adding <" << it->contentPrefix << ">to Library.\n";
                addContentToLibrary(&*it);
            }
            librarySize += multimediaLibrary->size();
        }

        if (networkLibrary != NULL) {
            for (auto it = networkLibrary->begin(); it != networkLibrary->end(); it++) {
                //std::cout << "\t\\--> Adding <" << it->contentPrefix << ">to Library.\n";
                addContentToLibrary(&*it);
            }
            librarySize += networkLibrary->size();
        }

        if (trafficLibrary != NULL) {
            for (auto it = trafficLibrary->begin(); it != trafficLibrary->end(); it++) {
                //std::cout << "\t\\--> Adding <" << it->contentPrefix << ">to Library.\n";
                addContentToLibrary(&*it);
            }
            librarySize += trafficLibrary->size();
        }

    } else if (startingCache > 0) {
        //std::cout << "(CP) Assistant <" << myId << "> Standing By.\n";
        //std::cout.flush();

        /*
        //Marking all items as unavailable in order to include exactly the desired number of items (plus the other category items)
        bool* inclusionList = new bool[fullLibrary.size()];
        for (int i = 0 ; i < fullLibrary.size() ; i++) {
            inclusionList[i] = false;
        }

        //Iterating for the size of the pre-cache
        for (int i = 0 ; i < startingCache ; i++) {
            int curIndex = 0;
            bool foundItem = false;
            int desIndex = 0;

            //Randomly attempting to add item to our cache
            while (!foundItem) {
                desIndex = uniform(0, fullLibrary.size());
                if (inclusionList[desIndex] == false) {
                    inclusionList[desIndex] = true;
                    foundItem = true;
                }
            }

            //Adding Objects to the Local Content Library (based on the item selected - marked as true)
            for (auto it = fullLibrary.begin(); it != fullLibrary.end(); it++) {
                if (curIndex == desIndex) {
                    std::cout << "(CP) <" << myId << "> is adding content object <" << (*it).contentPrefix << "> to its initial library.\n";
                    addContentToLibrary(&*it);
                    break;
                }
                curIndex++;
            }
        }
        delete[] inclusionList;
        */
    } else {
        //std::cout << "(SM) Vehicle <" << myId << "> Standing By.\n";
        //std::cout.flush();
    }

    //std::cout << "(CP) <" << myId << "> Library is setup.\n";
    //std::cout.flush();
}

//Pseudosetter
void ContentStore::increaseUseCount(cMessage *msg) {
    cArray parArray = msg->getParList();
    cMsgPar* requestName = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));
    increaseUseCount(requestName->str());
}

//PseudoSetter
void ContentStore::increaseUseCount(std::string prefix) {
    increaseUseCount(1,prefix);
}

//
void ContentStore::increaseUseCount(int addedUses, std::string prefix) {
    //Removing that god damn fucking quote
    prefix = library->cleanString(prefix);

    //Looking for Item
    for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
        if (prefix.compare(it->referenceObject->contentPrefix) == 0) {
            it->useCount += addedUses;
            it->lastAccessTime = simTime();
            return;
        } else {
            //std::cerr << "\t "<< it->contentPrefix << "\n";
        }
    }

    //This edge case will happen whenever the item is not actually cached but we are just forwarding an interest request from the upstream
    //std::cerr << "(CP) <" << myId << "> Warning: DID NOT FIND ITEM NAME <" << prefix << ">\n";
    //std::cerr.flush();
}

//Getter
int ContentStore::getUseCount(std::string prefix) {
    //Removing that god damn fucking quote
    prefix = library->cleanString(prefix);

    //Looking for Item
    for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
        if (prefix.compare(it->referenceObject->contentPrefix) == 0) {
            return it->useCount;
        }
    }
    return -1;
}

//Function meant to handle Content Lookup Requests
void ContentStore::addToLibrary(cMessage *msg) {
    //Unwrapping contents of request
    cArray parArray = msg->getParList();
    cMsgPar* requestName = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));

    std::string nameValue = requestName->stringValue();
    if (nameValue.c_str()[0] == '\"') {
        nameValue = nameValue.substr(1,nameValue.length()-2);   //No fucking idea why but strings added as parameters get extra quotes around them. WTF
    }

    addContentToLibrary(library->getContent(nameValue));
}

//Function meant to handle Content Lookup Requests
bool  ContentStore::checkIfAvailable(std::string nameValue, int requestID) {
    //Removing '"'s from text
    std::string lookupValue = nameValue;
    if (nameValue.c_str()[0] == '\"') {
        lookupValue = nameValue.substr(1,nameValue.length()-2);   //No fucking idea why but strings added as parameters get extra quotes around them. WTF
    }

    //This function only runs once, but we call it just in case the vehicle receives a request during its setup
    buildContentLibrary();

    //Initially, we check if we are using provider-based location for content availability, and if we are close, yes, we can provide the content (skips cache searches)
    if (library->locationDependentContentMode()) {
        //Get Local Position
        Coord currentLocation = traci->getCurrentPosition();

        //Extrapolating theoretical sector from content prefix
        int contextFreeIndex = library->getClassFreeIndex(lookupValue);
        int sectorCode = library->getSectorFromPrefixIndex(contextFreeIndex);

        if (library->viablyCloseToContentLocation(sectorCode, currentLocation.x, currentLocation.y)) {
            //int calculatedDistance = library->getDistanceToSector(sectorCode, currentLocation.x, currentLocation.y);
            //std::cout << "(Lib) \tNode is close enough to fulfill request for <" << lookupValue << ">!\n";
            //std::cout << "\t\\-->We are <" << calculatedDistance << "> linear meters from sector center <" << sectorCode << "> in question.\n";
            //std::cout << "\t\\--> Our position is: < " << currentLocation.x << " ; " << currentLocation.y << " >\n";
            //std::cout << "\t\\--> Sector Center-point is located at <" << (library->getSectorColumn(sectorCode) * library->getSectorSize()) << " ; " << std::to_string(library->getSectorRow(sectorCode) * library->getSectorSize()) << " >\n";
            //std::cout.flush();

            //todo? Apply cache insertion rules to this new object we hypothetically have access to (?)
            stats->logProvisionAttempt(myId, requestID);
            return true;
        }
    }

    Content_t* object = fetchFromCache(lookupValue);
    if (object != NULL) {
        if (object->contentClass == ContentClass::GPS_DATA) {
             //TODO: (IMPLEMENT) If working with TRAFFIC information, we'll require coordinate values to be part of the name... so deal with this in the future :D
             /*
             cMsgPar* requestLocation = static_cast<cMsgPar*>(parArray.get(MessageParameter::COORDINATES.c_str()));

             //Checking for valid location
             if (requestLocation == NULL) {
                 //opp_error("Traffic request has no coordinates associated with it!");
                 EV << "(CP) Warning: Traffic request has no coordinates associated with it!";
                 return false;
             }

             //Splitting String
             std::string coordinates = requestLocation->str();

             //Getting current location
             Coord currPos = traci->getCurrentPosition();
             std::string providerCoordinates = "\"" + std::to_string(floor(currPos.x)) + ";" + std::to_string(floor(currPos.y)) + ";" + std::to_string(floor(currPos.z)) + "\"";

             //Comparing Locations to check if we are the same vehicle
             if (providerCoordinates.compare(requestLocation->str()) == 0) {
                 return false;
             } else {
                 //opp_warning("OMG, SOMEONE ELSE JUST TOTES ASKED US FOR OUR TRAFFIC INFO OMG.");
                 //TODO (IMPLEMENT) location based evaluation for GPS location requests
                 return true;
             }
             */
             std::cerr << "(CP) Error: GPS Location dependent solution is not implemented\n";
             std::cerr.flush();
             return false;
         }
         //std::cout << "<" << myId << ">\t \\--> Found Requested object, my role is <" << nodeRole << ">.\n";
         //std::cout.flush();

         //In general, if we found our item, we have it. (See implementation for GPS location data
         stats->logProvisionAttempt(myId, requestID);
         return true;
    }

       //If the item was not found
    return false;
}

//
Content_t* ContentStore::fetchFromCache(std::string prefix){
    std::string lookupValue = prefix;
    if (prefix.c_str()[0] == '\"') {
        lookupValue = prefix.substr(1,prefix.length()-2);   //No fucking idea why but strings added as parameters get extra quotes around them. WTF
    }

    //Looking for item in content library
     for (auto it = contentCache.begin(); it != contentCache.end() ; it++) {
         int comparison = it->referenceObject->contentPrefix.compare(lookupValue);

         //Checking if the strings match (slow! :/)
         if (comparison == 0) {
             return it->referenceObject;
         }
     }
     return NULL;
}

//Getter to classify node as either server or client (servers don't have cache limitation applied during computation)
NodeRole ContentStore::getRole() {
    //TODO: (REVIEW) Make this more malleable depending on content class (?)
    return nodeRole;
}

//Getter for the currently used Cache Policy
CacheReplacementPolicy ContentStore::getCachePolicy() {
    return cachePolicy;
}

//Decision algorithm function whether item should be cached
bool ContentStore::localPopularityCacheDecision(Connection_t* connection) {
    //Always cache if space is available
    if ( (int)contentCache.size() < maxCachedContents) {
        return true;
    }

    int sum = 0;
    for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
        sum += it->useCount;

    }

    if (sum == 0) {
        return true;
    }

    double averagePopularity = sum > 0 ? floor(double(sum/(double)contentCache.size())) : 0;
    double estimatedResult = connection->remoteHopUseCount > 0 ? double(floor(log2(connection->downstreamCacheDistance+1) + log2(connection->remoteHopUseCount))/(double)averagePopularity) : 0;

    if (estimatedResult >= 1) {
        return true;
    }

    //std::cout << "\t< NO >\t\tTime:<" << simTime() << ">\n\n";
    return false;
}

//
bool ContentStore::localMinimumPopularityCacheDecision(Connection_t* connection) {
    //Always cache if space is available
    if ( (int)contentCache.size() < maxCachedContents) {
        return true;
    }

    int minUseCount = contentCache.size() > 0 ? INT_MAX : -1;
    //Looking for Item
    if (minUseCount == -1) {
        minUseCount = 0;
    } else {
        for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
            //Replacing minimum use count
            if (it->useCount < minUseCount) {
                minUseCount = it->useCount;
            }
        }
    }

    if (minUseCount == 0) {
        return true;
    }

    double estimatedResult = connection->remoteHopUseCount > 0 ? double(floor(log2(connection->downstreamCacheDistance+1) + log2(connection->remoteHopUseCount))/(double)minUseCount) : 0;

    //I think these two do the same thing?
    if (minUseCount < connection->remoteHopUseCount) {
        return true;
    }

    if (estimatedResult >= 1) {
        return true;
    }

    return false;
}

//Decision algorithm function whether item should be cached
bool ContentStore::globalPopularityCacheDecision(Connection_t* connection) {
    //Always cache if space is available
    if ( (int)contentCache.size() < maxCachedContents) {
        return true;
    }

    float sum = 0;
    float remoteFrequency = library->getDensityForIndex(connection->requestedContent->popularityRanking,connection->requestedContent->contentClass);

    for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
        float itemDensity = library->getDensityForIndex(it->referenceObject->popularityRanking,it->referenceObject->contentClass );
        sum += itemDensity;
    }

    if (sum == 0) {
        return true;
    }

    double averageFrequency = sum > 0 ? double(sum/(double)contentCache.size()) : 0;
    double estimatedResult = remoteFrequency > 0 ? double((log2(connection->downstreamCacheDistance + 1)*remoteFrequency)/(double)averageFrequency) : 0;

    if (estimatedResult >= 1) {
        return true;
    }

    return false;
}

//Decision algorithm function whether item should be cached
bool ContentStore::globalMinimumPopularityCacheDecision(Connection_t* connection) {
    //Always cache if space is available
    if ( (int)contentCache.size() < maxCachedContents) {
        return true;
    }

    float remoteFrequency = library->getDensityForIndex(connection->requestedContent->popularityRanking,connection->requestedContent->contentClass);
    float minFrequency = contentCache.size() > 0 ? INT_MAX : 0;
    for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
        float itemDensity = library->getDensityForIndex(it->referenceObject->popularityRanking,it->referenceObject->contentClass );
        if (itemDensity < minFrequency) {
            minFrequency = itemDensity;
        }
    }

    double estimatedResult = remoteFrequency > 0 ? double((log2(connection->downstreamCacheDistance + 1)*remoteFrequency)/(double)minFrequency) : 0;

    if (estimatedResult >= 1) {
        return true;
    }

    return false;
}

//
void ContentStore::handleMessage(cMessage *msg) {
    //std::cout << "ME FUDEU MANO, A LOKA ESSA PORRA AQUI TEM Q TAR RODANDO\n";
    if ( msg == gpsCacheTimerMessage || strcmp(msg->getName(),"gpsCacheUpdateTimer") == 0  ) {
        //TODO: perform maintenance
        resetGPSTimer();
    }else {
        std::cout << "\t(CS): Error: Unknown message: Name <" << msg->getName() << ">\tand kind\t<" << msg->getKind() << ">\n";
        std::cout.flush();
    }
}
