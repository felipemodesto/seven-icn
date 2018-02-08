//Concent Centric Class - Felipe Modesto

#include <paradise/bacon/ContentStore.h>

using Veins::TraCIMobilityAccess;
using Veins::AnnotationManagerAccess;

Define_Module(ContentStore);

//
bool compareGPSItems (OverheardGPSObject_t& first, OverheardGPSObject_t& second) {
    //std::cout << "(CS) Comparing items <" << first.contentPrefix << "> and <" << second.contentPrefix << ">!\n";
    return (first.referenceCount > second.referenceCount);
    //TODO: Decide if there should be an alterate scheme where the referenceCount/originCount should matter
}

//Initialization Function
void ContentStore::initialize(int stage) {
    if (stage == 0) {
        //Getting Parent Module Index
        myId = getParentModule()->getIndex();
        gpsCacheTimerMessage = NULL;

        cachePolicy = static_cast<CacheReplacementPolicy>(par("cacheReplacementPolicy").longValue());
        startingCache = par("startingCache").doubleValue();
        maxCachedContents = par("maxCachedContents").longValue();

        //We only do our GPS stuff if we have GPS Cache enabled
        GPSCachePolicy = static_cast<CacheLocationPolicy>(par("geoCachePolicy").longValue());
        if (GPSCachePolicy != CacheLocationPolicy::IGNORE_LOCATION) {
            //std::cout << "(CS) USING GPS\n";
            gpsCacheSize = par("geoCacheSize").longValue();
            maxCachedContents -= gpsCacheSize;                //Resizing total cache size
            gpsCacheWindowSize = par("gpsKnowledgeWindowSize").longValue();

            //Creating our GPS based list of lists
            OverheardGPSObjectList_t* firstSecondList = new OverheardGPSObjectList_t();
            firstSecondList->simTime = 0;
            gpsCacheFrequencyWindow.push_front(firstSecondList);
        }

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

        buildContentCache();

        if (GPSCachePolicy != CacheLocationPolicy::IGNORE_LOCATION) {
            gpsCacheTimerMessage = new cMessage("gpsCacheUpdateTimer");
            resetGPSTimer();
        }
    }
}

//Finalization Function (not a destructor!)
void ContentStore::finish() {
    library->releaseStatus(nodeRole,myId);
    contentCache.empty();

    if (gpsCacheTimerMessage != NULL) {
        if (gpsCacheTimerMessage->isScheduled()) cancelAndDelete(gpsCacheTimerMessage);
        else delete(gpsCacheTimerMessage);
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
void ContentStore::maintainGPSCache() {
    OverheardGPSObjectList_t* frontFrequencySlice = gpsCacheFrequencyWindow.front();
    if (frontFrequencySlice->simTime != floor( simTime().dbl()) ) {

        //std::cout << "(CS) Moving our sliding window from <" << frontFrequencySlice->simTime << "> to <" << simTime().dbl() << ">\n";
        //std::cout.flush();

        //Checking if we need to delete a column
        if (static_cast<int>(gpsCacheFrequencyWindow.size()) == gpsCacheWindowSize) {
            //Deleting column and its content
            OverheardGPSObjectList_t* oldestFrequencySlice = gpsCacheFrequencyWindow.back();
            gpsCacheFrequencyWindow.pop_back();
            delete(oldestFrequencySlice);
        }

        //Since we moved to a separate second, we should merge the statistics from the previous front with our own
        for (auto itA = neighborGPSInformation.gpsList.begin(); itA != neighborGPSInformation.gpsList.end() ; itA++) {
            //std::cout << "(CS) Importing knowledge acquired from neighbors. We know this much knowledge: <" << neighborGPSInformation.gpsList.size() << ">\n";
            //std::cout.flush();

            bool foundRemoteItem = false;
            for (auto itB = frontFrequencySlice->gpsList.begin(); itB != frontFrequencySlice->gpsList.end() ; itB++) {
                if (library->equals(itA->referenceObject,itB->referenceObject)) {
                //if (itA->referenceObject->contentPrefix.compare(itB->referenceObject->contentPrefix) == 0) {
                    foundRemoteItem = true;
                    itB->referenceCount += itA->referenceCount * easingFactor;
                    //Popularity information incoming from other items has the easing added to it, even if they are part of the most recent sector, cause I said so, fuck it!
                }
            }
            //If we haven't found the remote item we create a new entry
            if (!foundRemoteItem) {
                OverheardGPSObject_t newGPSObject;
                newGPSObject.referenceObject = itA->referenceObject;
                newGPSObject.referenceCount = itA->referenceCount;
                //newGPSObject.contentClass = itA->contentClass;
                newGPSObject.referenceOriginCount = itA->referenceOriginCount;
                frontFrequencySlice->gpsList.push_front(newGPSObject);
            }
        }

        //Now we create a new column
        OverheardGPSObjectList_t* newerFrequencySlice = new OverheardGPSObjectList_t();
        newerFrequencySlice->simTime = floor(simTime().dbl());
        gpsCacheFrequencyWindow.push_front(newerFrequencySlice);
        frontFrequencySlice = newerFrequencySlice;
    }
}

//
void ContentStore::shareGPSStatistics() {
    //If we're in the startup time we dont even care
    if (simTime() < stats->getStartTime()) return;
    //std::cout << "(CS) <" << simTime() << "> Evaluating GPS Statistics for sharing purposes\n";

    //Creating a list of references
    OverheardGPSObjectList_t aggregatedGPSStatistics;
    aggregatedGPSStatistics.simTime = simTime().dbl();

    //Aggregating Values into it
    int temporalIndex = 0;
    for (auto slice = gpsCacheFrequencyWindow.begin(); slice != gpsCacheFrequencyWindow.end() ; slice++) {
        //std::cout << "(CS) Evaluating Cache Column Temporal Index: <" << temporalIndex << "> with size <" << gpsCacheFrequencyWindow.size() << ">\n";
        for (auto gpsObject = (*slice)->gpsList.begin(); gpsObject != (*slice)->gpsList.end() ; gpsObject++) {
            bool foundItem = false;
            for(auto aggregatedObject = aggregatedGPSStatistics.gpsList.begin() ; aggregatedObject != aggregatedGPSStatistics.gpsList.end(); aggregatedObject++) {
                if (library->equals(aggregatedObject->referenceObject,gpsObject->referenceObject)) {
                    aggregatedObject->referenceCount += gpsObject->referenceCount * pow(easingFactor,temporalIndex);
                    foundItem = true;
                    break;
                }
            }
            if (!foundItem) {
                OverheardGPSObject_t newObject;
                newObject.referenceCount = gpsObject->referenceCount * pow(easingFactor,temporalIndex);
                newObject.referenceObject = gpsObject->referenceObject;
                newObject.referenceOriginCount = gpsObject->referenceOriginCount;
                aggregatedGPSStatistics.gpsList.push_back(newObject);
            }
        }
        temporalIndex++;
    }

    //std::cout << "(CS) Sorting items for frequency evaluation.\n";

    //Sorting items based on frequency and deciding whether to share a subset if at all
    if (aggregatedGPSStatistics.gpsList.size() > 0) {
        aggregatedGPSStatistics.gpsList.sort(compareGPSItems);
        //std::cout << "\t[" << myId << "]\t(CS) Sorted list has size: <" << aggregatedGPSStatistics.gpsList.size() << ">\n";
        OverheardGPSObject_t mostPopularObject = aggregatedGPSStatistics.gpsList.front();

        //std::cout << "[" << myId << "]\t(CS) Advertising most popular non-cached item : <" << mostPopularObject.referenceObject->contentPrefix << "> with count <" << mostPopularObject.referenceCount << ">\n";

        //Advertising most popular object
        manager->advertiseGPSItem(mostPopularObject);
    }
}

//Function called to check whether object is relevant to us in terms of logging it for our Current Location Correlation Policy
bool ContentStore::isObjectGPSRelevant(Content_t* object) {
    //Reacting according to the Location Policy
    switch(GPSCachePolicy) {
        case CacheLocationPolicy::IGNORE_LOCATION:
            //Always return (Ignore subsystem)
            return false;

        case CacheLocationPolicy::CLIENT_CENTRIC:
            //Client-correlation: If cached == not relevant
            if (availableFromCache(object) != NULL) return false;
            break;

        case CacheLocationPolicy::PLC_NAIVE:
        case CacheLocationPolicy::PLC_SMART:
            //Server-Correlation: Relevant to log requests if we are where its valid
            //if ((availableFromCache(object) == NULL) && (availableFromLocation(object) == NULL)) return false;
            //std::cout << "\t\tMAYBE\n";
            if ((availableFromLocation(object) == NULL)) return false;
            break;
    }
    return true;
}

//Function called once per request
void ContentStore::logOverheardGPSMessage(Content_t* object) {

    //std::cout << "\tPlease?\n";

    //If the object is not relevant for us given the current Location policy, we dont log the message
    if (!isObjectGPSRelevant(object)) return;

    //Maintaining Cache for the sake of maintaining cache (moving if we change seconds, etc)
    maintainGPSCache();
    OverheardGPSObjectList_t* frontFrequencySlice = gpsCacheFrequencyWindow.front();

    //std::cout << "(CS) Attempting to Log GPS relevant information for item <" << object->contentPrefix << ">\n";
    //std::cout.flush();

    //Searching most recent slice
    bool foundItem = false;
    for (auto it = frontFrequencySlice->gpsList.begin(); it != frontFrequencySlice->gpsList.end() ; it++) {
        if (library->equals(object,it->referenceObject)) {
        //if (object->contentPrefix.compare(it->referenceObject->contentPrefix) == 0) {
            //If we found the element, let's update the status
            it->referenceCount++;
            foundItem = true;

            //std::cout << "[" << myId << "]\t(CS) Increasing Popularity to <" << it->referenceCount++ << ">\n";
            //std::cout.flush();
            break;
        }
    }

    //If we haven't found the object, we'll create a new entry
    if (!foundItem) {
        OverheardGPSObject_t newGPSObject;
        newGPSObject.referenceCount = 1;
        newGPSObject.referenceObject = object;
        //newGPSObject.contentPrefix = object->contentPrefix;
        //newGPSObject.contentClass = object->contentClass;
        newGPSObject.referenceOriginCount = 1;
        frontFrequencySlice->gpsList.push_front(newGPSObject);
    }
}

//Function called to potentially update our GPS statistics (and even our cache) with item
void ContentStore::handleGPSPopularityMessage(WaveShortMessage* wsm) {
    Enter_Method_Silent();

    cArray parArray = wsm->getParList();
    cMsgPar* prefixParameter = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));
    cMsgPar* frequencyParameter = static_cast<cMsgPar*>(parArray.get(MessageParameter::FREQUENCY.c_str()));
    cMsgPar* referenceFrequencyParameter = static_cast<cMsgPar*>(parArray.get(MessageParameter::NEIGHBORS.c_str()));

    OverheardGPSObject_t incomingPopularItem;
    incomingPopularItem.referenceCount = frequencyParameter->doubleValue();
    incomingPopularItem.referenceObject = library->getContent(prefixParameter->stringValue());
    incomingPopularItem.referenceOriginCount = referenceFrequencyParameter->doubleValue();

    //Note: If we're learning information from other nodes, it might be relevant to log that info, even if direct requests are not necessarily valid for us, right?
    //For now, we're only interested in locally popular content objects
    if (isObjectGPSRelevant(incomingPopularItem.referenceObject) == false) return;

    //Updating statistics
    OverheardGPSObject_t* incomingPopularItemReference = NULL;
    bool foundInNeighborList = false;
    for (auto iterator = neighborGPSInformation.gpsList.begin(); iterator != neighborGPSInformation.gpsList.end() ; iterator++) {
        if (library->equals(iterator->referenceObject,incomingPopularItem.referenceObject) == true) {
        //if (iterator->referenceObject->contentPrefix.compare(incomingPopularItem.referenceObject->contentPrefix) == 0) {
            iterator->referenceCount += incomingPopularItem.referenceCount;
            iterator->referenceOriginCount += incomingPopularItem.referenceOriginCount;
            foundInNeighborList = true;
            incomingPopularItemReference = &*iterator;
            break;
        }
    }

    if (!foundInNeighborList) {
        neighborGPSInformation.gpsList.push_front(incomingPopularItem);
        incomingPopularItemReference = &incomingPopularItem;
    }

    //Call the GPS popularity evaluation thingw (true = insert)
    if (gpsPopularityCacheDecision(incomingPopularItemReference) == true) {
        //We don't actually have the item here, but we are interested in it, so we should request it!
        //Don't add it to the cache :P
        //std::cout << "[" << myId << "]\t(CS) Auto obtaining item <" << incomingPopularItemReference->contentPrefix << "> cause fuck it, fuck you!\n";
        //TODO: (IMPLEMENT) Actually request the GPS item! (We already log our preemptive request)
        stats->increasePLCPreemptiveCacheRequests();
        addContentToGPSCache(incomingPopularItemReference);
        manager->notifyOfGPSInclusion(wsm->getSenderAddress(),incomingPopularItemReference);
    }
}

//
void ContentStore::handleGPSPopularityResponseMessage(WaveShortMessage* wsm) {
    Enter_Method_Silent();

    std::cout << "(CS) Someone accepted our content. Cool\n";

    cArray parArray = wsm->getParList();
    cMsgPar* prefixParameter = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));

    string referencePrefix = prefixParameter->stringValue();

    //OverheardGPSObject_t* incomingPopularItemReference = NULL;
    //bool foundInNeighborList = false;
    for (auto iterator = neighborGPSInformation.gpsList.begin(); iterator != neighborGPSInformation.gpsList.end() ; iterator++) {
        if (library->equals(*(iterator->referenceObject),referencePrefix) == true) {
            iterator->referenceCount = round(iterator->referenceCount/2);
            return;
        }
    }

}

////////////////////////////////////////////
//GPS Cache Operations
////////////////////////////////////////////

//
void ContentStore::removeFromGPSSideStatistics(Content_t* contentObject) {
    //std::cout << "(CS) Removing knowledge of item <" << contentObject->contentPrefix << "> from separate GPS Statistics.\n";

    //Removing from Neighbor Statistics
    for (auto iterator = neighborGPSInformation.gpsList.begin(); iterator != neighborGPSInformation.gpsList.end() ; iterator++) {
        if (library->equals(iterator->referenceObject,contentObject) == true) {
        //if (contentObject->contentPrefix.compare(iterator->referenceObject->contentPrefix) == 0) {
           neighborGPSInformation.gpsList.erase(iterator);
           break;
       }
    }

    //Removing from all slices
    for (auto iterator = gpsCacheFrequencyWindow.begin(); iterator != gpsCacheFrequencyWindow.end(); iterator++) {
        for (auto sliceIterator = (*iterator)->gpsList.begin(); sliceIterator != (*iterator)->gpsList.end() ; sliceIterator++) {
            if (library->equals(contentObject,sliceIterator->referenceObject)) {
            //if (contentObject->contentPrefix.compare(sliceIterator->referenceObject->contentPrefix) == 0) {
                (*iterator)->gpsList.erase(sliceIterator);
                break;
            }
        }
    }
}

//Function called to decide whether the object should be cached or not
bool ContentStore::gpsPopularityCacheDecision(OverheardGPSObject_t* gpsPopularItem) {
    //If we have space available for this, sure, why not
    //TODO: Decide if this is reasonable
    if (static_cast<int>(gpsCache.size()) < gpsCacheSize) return true;

    //TODO: (IMPLEMENT) Design and Implement a greater than average mode evaluation for the GPS Pre-caching decision
    switch (GPSCachePolicy){
        case CacheLocationPolicy::PLC_NAIVE: {
                //We currently only have a bigger than the smallest item in side-cache mode

                //Iterating through our location-dependent cache to see if the suggested item is more popular than the stuff we already have
                CachedContent_t* minimumObject = &gpsCache.front();
                for (auto iterator = gpsCache.begin()++; iterator != gpsCache.end(); iterator++) {
                    if (minimumObject->useCount >= iterator->useCount) {
                        minimumObject = &*iterator;
                    }
                }

                //Deciding whether we remove the item
                //TODO: Consider the referenceOriginCount neighbor advertisements as part of the solution
                if (minimumObject->useCount <= gpsPopularItem->referenceCount) {
                    return true;
                }

            }
            break;

        case CacheLocationPolicy::PLC_SMART: {
                double sumValue = 0;

                //Iterating through our location-dependent cache to see if the suggested item is more popular than the stuff we already have
                CachedContent_t* minimumObject = &gpsCache.front();
                for (auto iterator = gpsCache.begin()++; iterator != gpsCache.end(); iterator++) {
                    //Checking if distance between this item and previous one is shorter than our limit and this one has greater ranking
                    if ( gpsPopularItem->referenceCount > iterator->useCount && library->distanceBetweenGPSContents(gpsPopularItem->referenceObject,iterator->referenceObject) <= (2 * library->getMinimumViableDistance())) {
                        //If our cache is full we'll remove this less relevant object (it has to be at this point)
                        //We do this preemptively to ensure correct item selection
                        iterator = gpsCache.erase(iterator);
                        stats->increasePLCCacheReplacements();
                        std::cout << "(CS) Preemptive GPS Preference!\n";
                        return true;
                    }

                    if (minimumObject->useCount >= iterator->useCount) {
                        minimumObject = &*iterator;
                    }
                    sumValue += iterator->useCount;
                }

                if (gpsCache.size() > 0) {
                    sumValue = sumValue / gpsCache.size();
                } else {
                    sumValue = 0;
                }
                //If item is below average, we don't care about it
                if (sumValue > gpsPopularItem->referenceCount) return false;

                //TODO: WAKAWAKA
            }
            break;

        default:
            std::cerr << "WARNING: UNKNOWN CACHE POLICY MALUCOOOO\n";
            break;
    }
    return false;
}

//
void ContentStore::addContentToGPSCache(OverheardGPSObject_t* gpsPopularItem) {
    //std::cout << "[" << myId << "]\t(CS)\tAttempting to add a GPS item <" << gpsPopularItem->contentPrefix << "> to our Specialized GPS Cache <" << gpsCache.size() << " / " << gpsCacheSize << " >.\n";

    //If we're a server this is never relevant (or is it?)
    if (nodeRole == NodeRole::SERVER) {
        std::cout << "(CS) Warning: We are servers so we shouldnt be receiving requests to log location-specific items.\n";
        return;
    }

    //Check if item is already cached
    if (availableFromCache(gpsPopularItem->referenceObject) != NULL) return;

    //Checking if our GPS Cache is full (and suggesting a replacement if that is the case)
    int currentCacheSize = gpsCache.size();
    if ( currentCacheSize >= gpsCacheSize && gpsCacheSize != -1) {
        //stats->increaseGPSCacheReplacements();
        runGPSCacheReplacement();
    }

    //Adding new item to cache
    CachedContent_t newContent;
    newContent.referenceObject = gpsPopularItem->referenceObject;
    newContent.contentStatus = ContentStatus::AVAILABLE;
    newContent.lastAccessTime = simTime();
    newContent.cacheTime = newContent.lastAccessTime;
    newContent.useCount = gpsPopularItem->referenceCount;
    gpsCache.push_front(newContent);

    //removing it from our overheardGPS tables;
    removeFromGPSSideStatistics(newContent.referenceObject);
}

//LRU replacement policy for items cached via GPS side cache, may implement other alternate replacement models later, for now, whatever
void ContentStore::runGPSCacheReplacement() {
    //std::cout << "(CS) Running GPS Cache Replacement.\n";

    //Checking if we actually have to get worried about cache replacement
    if ((contentCache.size() - gpsCacheSize) <= 0) {
        //std::cout << "(CS) GPS Cache is fine... don't worry about it.\n";
        //std::cout.flush();
        return;
    }

    //Looking for least used messages (aware of multiple occurrences of same metric value)
    int sameUseCount = 1;
    int timesUsed = library->getCurrentRequestIndex()+1;
    for (auto it = gpsCache.begin(); it != gpsCache.end() ; it++) {
        if (it->useCount < timesUsed) {
            timesUsed = it->useCount;
            sameUseCount = 1;
        } else if (it->useCount == timesUsed) {
            sameUseCount++;
        } else {
            //DO NOTHING (if the size is greater, we dont care about this, right?)
        }
    }

    //Calculating the probability of any item with the same use count being selected
    double individualProbability = (double)(1/(double)sameUseCount);
    if (individualProbability == 0) {
        std::cout << "(CS) ERROR: PROBABILITY LOCK.\n";
        std::cout.flush();
    }

    bool foundRemoval = false;
    //Searching for last item with desired time value to be removed
    while (foundRemoval == false) {
        for (auto it = gpsCache.begin(); it != gpsCache.end() ; it++) {
            if (it->useCount <= timesUsed) {
                //We just remove the first one we find, dont care about having multiple items with the same probability anymore
                //if ( (foundRemoval == false) && (sameUseCount == 1 || (uniform(0,1) < individualProbability))) {
                    foundRemoval = true;

                    //std::cout << "(CS) <" << myId << "> Removing Item <" << it->contentPrefix << "> from Cache with Uses <" << it->useCount << ">.\n";
                    //std::cout.flush();

                    it = gpsCache.erase(it);
                    stats->increasePLCCacheReplacements();

                    break;
                //}
            }
        }
    }
}

////////////////////////////////////////////
//Regular Cache Operations
////////////////////////////////////////////

//
void ContentStore::addContentToCache(Content_t* contentObject) {
    if (contentObject == NULL) {
        EV_ERROR << "(CS) Content to be added to library is NULL.\n";
        EV_ERROR.flush();

        std::cerr << "(CS) Error: Content to be added to library is NULL.\n";
        std::cerr.flush();
        return;
    }

    //Prior to Simulation Start during Warmup, we're just building our libraries so we ignore "searches"
    //Note: Please keep this in mind when cold starting simulations
    if (nodeRole == NodeRole::SERVER) {
        //Looking for item in content library
        for (auto it = contentCache.begin(); it != contentCache.end() ; it++) {
            if (library->equals(it->referenceObject,contentObject)) {
            //if (it->referenceObject->contentPrefix.compare(contentObject->contentPrefix) == 0) {
                //std::cout << "(CS) <" << myId << "> Already has " << contentObject->contentPrefix << " in local Library.\n";
                //std::cout.flush();
                //increaseUseCount(contentObject->contentPrefix);
                return;
            }
        }
    }

    //Checking for Content replacement prior to adding content to library to ensure content will not be instantly removed based on policy behavior
    int currentCacheSize = contentCache.size();
    if ( currentCacheSize >= maxCachedContents && maxCachedContents != -1) {
        //std::cout << "(CS) Cache is full.\n";
        //std::cout.flush();

        //Note: This stats call is not needed as the Cache Replacement Function does the statistics logging! Boo you
        //stats->increaseCacheReplacements();
        runCacheReplacement();
    }

    //newContent.contentPopularity = contentObject->contentPopularity;
    //newContent.contentPriority =  contentObject->contentPriority;

    CachedContent_t newContent;
    newContent.referenceObject = contentObject;
    newContent.contentStatus = ContentStatus::AVAILABLE;
    newContent.lastAccessTime = simTime();
    newContent.cacheTime = newContent.lastAccessTime;
    newContent.useCount = 0;
    //newContent.expireTime = SIMTIME_S;
    contentCache.push_front(newContent);
    librarySize++;

    //EV << "(CS) Added content object " << contentObject->contentPrefix << " to local Library.\n";
    //EV.flush();

    //std::cerr << "\t(CS) <" << myId << "> Added content object " << contentObject->contentPrefix << " to local Library.\n";
    //std::cerr.flush();
}

//
void ContentStore::removeContentFromCache(Content_t* newContent) {
    //Searching for content
    for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
        if (library->equals(newContent,it->referenceObject)) {
        //if (newContent->contentPrefix.compare(it->referenceObject->contentPrefix) == 0) {
            //std::cout << "(CS) <" << myId << "> Removing Item <" << it->referenceObject->contentPrefix << "> from Cache with Use Count <" << it->useCount << ">.\n";
            //std::cout.flush();
            it = contentCache.erase(it);
            librarySize--;
            return;
        }
    }

    std::cerr << "(CS) <" << myId << "> Warning: Failed to remove object from Cache Library!\n";
    std::cerr.flush();
}

//
void ContentStore::runCacheReplacement(){
    //std::cout << "(CS) Running Cache Replacement!\n";
    //std::cout.flush();

    //Checking if we're a server, and if so, we dont cate about cache replacement, we have infinite size
    if (maxCachedContents == -1) {
        //std::cout << "(CS) <" << myId << "> We have infinite Cache. This is not relevant <3.\n";
        return;
    }

    int currentCacheSize = contentCache.size();
    int cacheOverflow = currentCacheSize - maxCachedContents + 1;

    //Checking if we actually have to get worried about cache replacement
    if (cacheOverflow <= 0) {
        EV << "(CS) Cache is fine... don't worry about it.\n";
        EV.flush();
        return;
    }
    for (int i = 0; i< cacheOverflow ; i++) {
        //std::cout << "(CS) Removing Item!\n";
        //std::cout.flush();
        switch(cachePolicy) {
            //RANDOM
            case CacheReplacementPolicy::RANDOM:
            {
                int removableIndex = uniform(0,maxCachedContents);
                int currentIndex = 0;
                for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
                    if (currentIndex == removableIndex) {
                        //std::string itemName = it->referenceObject->contentPrefix;

                        //EV << "(CS) Removing Item <" << itemName << "> from Cache.\n";
                        //EV.flush();

                        //std::cout << "(CS) <" << myId << "> Removing Item <" << itemName << "> from Cache.\n";
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
                    std::cout << "(CS) ERROR: PROBABILITY LOCK.\n";
                    std::cout.flush();
                }

                //if (sameTimeCount > 1) {
                //    std::cout << "(CS) Warning: Multiple items with same caching time detected! Earliest Cache Time is <" << earliestTime.str() << ">\n";
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
                                //std::string itemName = it->referenceObject->contentPrefix;

                                //std::cout << "(CS) <" << myId << "> Removing Item <" << it->contentPrefix << "> from Cache with Use Count <" << it->useCount << ">\t last access <" << it->lastAccessTime << ">.\n";
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
                //std::cout << "(CS) Running LFU Cache Replacement.\n";
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
                    std::cout << "(CS) ERROR: PROBABILITY LOCK.\n";
                    std::cout.flush();
                }

                //if (sameUseCount > 1) {
                //    std::cout << "(CS) Warning: Multiple items (total count: <" << sameUseCount << ">) with same cache use frequency detected! Lowest use count is <" << timesUsed << ">\n";
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

                                //std::cout << "(CS) <" << myId << "> Removing Item <" << it->contentPrefix << "> from Cache with Uses <" << it->useCount << ">.\n";
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
                        //std::cout <<  "(CS) Removing item <" << it->contentPrefix << "> with Popularity : <" << to_string(it->popularityRanking) << ">\n";
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
                EV << "(CS) DEFAULT CASE POLICY. OH NOES.\n";
                EV.flush();
                break;
        }
    }
}

//
void ContentStore::buildContentCache() {
    //Building Content Database
    if (hasLibrary) return;
    hasLibrary = true;

    //std::cout << "<" << myId << "> Building Library <" << startingCache << ">.\n";
    //std::cout.flush();

    if (library == NULL) {
        std::cerr << "(CS) Error: Global Content Library module reference is null.\n";
        std::cerr.flush();
        return;
    }

    //Building our Content Library from the complete library
    if (startingCache == -1 and !library->locationDependentContentMode()) {
        //Adding Items from other categories
        //std::cout << "(CS) Server <" << myId << "> Standing By.\n";
        //std::cout.flush();

        std::list<Content_t>* multimediaLibrary = library->getMultimediaContentList();
        std::list<Content_t>* networkLibrary = library->getNetworkContentList();
        std::list<Content_t>* trafficLibrary = library->getTrafficContentList();

        librarySize = 0;
        if (multimediaLibrary != NULL) {
            for (auto it = multimediaLibrary->begin(); it != multimediaLibrary->end(); it++) {
                //std::cout << "\t\\--> Adding <" << it->contentPrefix << ">to Library.\n";
                addContentToCache(&*it);
            }
            librarySize += multimediaLibrary->size();
        }

        if (networkLibrary != NULL) {
            for (auto it = networkLibrary->begin(); it != networkLibrary->end(); it++) {
                //std::cout << "\t\\--> Adding <" << it->contentPrefix << ">to Library.\n";
                addContentToCache(&*it);
            }
            librarySize += networkLibrary->size();
        }

        if (trafficLibrary != NULL) {
            for (auto it = trafficLibrary->begin(); it != trafficLibrary->end(); it++) {
                //std::cout << "\t\\--> Adding <" << it->contentPrefix << ">to Library.\n";
                addContentToCache(&*it);
            }
            librarySize += trafficLibrary->size();
        }

    } else if (startingCache > 0) {
        //std::cout << "(CS) Assistant <" << myId << "> Standing By.\n";
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
                    std::cout << "(CS) <" << myId << "> is adding content object <" << (*it).contentPrefix << "> to its initial library.\n";
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

    //std::cout << "(CS) <" << myId << "> Library is setup.\n";
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
    Content_t* referenceObject = library->getContent(prefix);
    increaseUseCount(addedUses,referenceObject);
}

//
void ContentStore::increaseUseCount(Content_t* object) {
    increaseUseCount(1,object);
}

//
void ContentStore::increaseUseCount(int addedUses, Content_t* object) {

    //Looking for Item
    for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
        if (library->equals(object,it->referenceObject)) {
        //if (prefix.compare(it->referenceObject->contentPrefix) == 0) {
            it->useCount += addedUses;
            it->lastAccessTime = simTime();
            return;
        } else {
            //std::cerr << "\t "<< it->contentPrefix << "\n";
        }
    }

    //This edge case will happen whenever the item is not actually cached but we are just forwarding an interest request from the upstream
    //std::cerr << "(CS) <" << myId << "> Warning: DID NOT FIND ITEM NAME <" << prefix << ">\n";
    //std::cerr.flush();
}

//Getter
int ContentStore::getUseCount(std::string prefix) {
    //Removing that god damn fucking quote
    prefix = library->cleanString(prefix);

    //Looking for Item
    for (auto it = contentCache.begin(); it != contentCache.end(); it++) {
        if (library->equals(*(it->referenceObject),prefix)) {
        //if (prefix.compare(it->referenceObject->contentPrefix) == 0) {
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

    addContentToCache(library->getContent(nameValue));
}

//Function meant to handle Content Lookup Requests
ContentAvailabilityStatus  ContentStore::availableForProvisioning(Content_t* contentObject, int requestID) {
    //This function only runs once, but we call it just in case the vehicle receives a request during its setup
    buildContentCache();

    //Initially, we check if we are using provider-based location for content availability, and if we are close, yes, we can provide the content (skips cache searches)
    if (library->locationDependentContentMode()) {
        if (availableFromLocation(contentObject) != NULL) {
            stats->logProvisionAttempt(myId, requestID);
            return ContentAvailabilityStatus::AVAILABLE_LOCATION;
        }
    }

    Content_t* object = availableFromCache(contentObject);
    if (object != NULL) {
        if (object->contentClass == ContentClass::GPS_DATA) {
             //TODO: (IMPLEMENT) If working with TRAFFIC information, we'll require coordinate values to be part of the name... so deal with this in the future :D
             /*
             cMsgPar* requestLocation = static_cast<cMsgPar*>(parArray.get(MessageParameter::COORDINATES.c_str()));

             //Checking for valid location
             if (requestLocation == NULL) {
                 //opp_error("Traffic request has no coordinates associated with it!");
                 EV << "(CS) Warning: Traffic request has no coordinates associated with it!";
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
             std::cerr << "(CS) Error: GPS Location dependent solution is not implemented\n";
             std::cerr.flush();
             return ContentAvailabilityStatus::AVAILABLE_GPS_CACHE;
         }
         //std::cout << "<" << myId << ">\t \\--> Found Requested object, my role is <" << nodeRole << ">.\n";
         //std::cout.flush();

         //In general, if we found our item, we have it. (See implementation for GPS location data
         stats->logProvisionAttempt(myId, requestID);
         return ContentAvailabilityStatus::AVAILABLE_CACHE;
    }

       //If the item was not found
    return ContentAvailabilityStatus::NOT_AVAILABLE;
}

//
Content_t* ContentStore::availableFromLocation(Content_t* contentObject) {
    //Get Local Position
     Coord currentLocation = traci->getCurrentPosition();

     //Extrapolating theoretical sector from content prefix
     //int contextFreeIndex = library->getClassFreeIndex(contentPrefix);
     int sectorCode = library->getSectorFromPrefixIndex(contentObject->contentIndex);

     if (library->viablyCloseToContentLocation(sectorCode, currentLocation.x, currentLocation.y)) {
         //TODO: (CONFIRM) Apply cache insertion rules to this new object we hypothetically have access to (?)
         return contentObject;
     }
     return NULL;
}

//
Content_t* ContentStore::availableFromCache(Content_t* object){
    //Looking for item in content library
     for (auto it = contentCache.begin(); it != contentCache.end() ; it++) {
         if (library->equals(it->referenceObject,object) == true) return it->referenceObject;
     }

     //Looking for item in GPS Content Library
     for (auto it = gpsCache.begin(); it != gpsCache.end() ; it++) {
         if (library->equals(it->referenceObject,object) == true) return it->referenceObject;
     }

     return NULL;
}

//Getter to classify node as either server or client (servers don't have cache limitation applied during computation)
NodeRole ContentStore::getRole() {
    //TODO: (REVIEW) Make this more malleable depending on content class (?)
    return nodeRole;
}

//Getter for the Separate GPS Cache subsystem
bool ContentStore::gpsSubCacheEnabled() {
    Enter_Method_Silent();
    return GPSCachePolicy != CacheLocationPolicy::IGNORE_LOCATION;
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
        maintainGPSCache();

        shareGPSStatistics();

        resetGPSTimer();
    }else {
        std::cout << "\t(CS): Error: Unknown message: Name <" << msg->getName() << ">\tand kind\t<" << msg->getKind() << ">\n";
        std::cout.flush();
    }
}
