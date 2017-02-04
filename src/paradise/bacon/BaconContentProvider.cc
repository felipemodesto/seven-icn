//Concent Centric Class - Felipe Modesto

#include <paradise/bacon/BaconContentProvider.hh>

#include "veins/modules/application/ieee80211p/BaseWaveApplLayer.h"
#include "veins/modules/mobility/traci/TraCIMobility.h"

using Veins::TraCIMobilityAccess;
using Veins::AnnotationManagerAccess;

Define_Module(BaconContentProvider);

//Initialization Function
void BaconContentProvider::initialize(int stage) {
    if (stage == 0) {
        //Getting Parent Module Index
        myId = getParentModule()->getIndex();

        cachePolicy = static_cast<CacheReplacementPolicy>(par("cacheReplacementPolicy").longValue());
        availableMultimediaObjects = par("availableMultimediaObjects").doubleValue();
        maxCachedContents = par("maxCachedContents").longValue();

        librarySize = 0;
        WATCH(librarySize);

        contentLibrary = NULL;

        //EV << "(CP) Built a Library of size " << librarySize << ".\n";
        //EV.flush();
    }

    if (stage == 1) {
        //Getting modules we have to deal with
        traci = check_and_cast<Veins::TraCIMobility*>(getParentModule()->getSubmodule("veinsmobility"));
        manager = check_and_cast<BaconServiceManager *>(getParentModule()->getSubmodule("appl"));
    }

    if (stage == 2) {
        //Getting global modules we have to deal with
        cSimulation *sim = getSimulation();
        stats = check_and_cast<BaconStatistics*>(sim->getModuleByPath("BaconScenario.statistics"));
        library = check_and_cast<BaconLibrary*>(sim->getModuleByPath("BaconScenario.library"));
        buildContentLibrary();
    }
}

//Finalization Function (not a destructor!)
void BaconContentProvider::finish() {
}

//=============================================================
// CONTENT HANDLING FUNCTION (ON MESSAGES RECEIVED)
//=============================================================

//
void BaconContentProvider::runCacheReplacement(){
    //std::cout << "(CP) Running Cache Replacement!\n";
    //std::cout.flush();

    if (maxCachedContents == -1) {
        std::cout << "(CP) <" << myId << "> We have infinite Cache. This is not relevant <3.\n";
        return;
    }

    int cacheSize = contentLibrary->size();
    int cacheOverflow = cacheSize - maxCachedContents + 1;

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
                for (auto it = contentLibrary->begin(); it != contentLibrary->end(); it++) {
                    if (currentIndex == removableIndex) {
                        std::string itemName = it->contentPrefix;

                        //EV << "(CP) Removing Item <" << itemName << "> from Cache.\n";
                        //EV.flush();

                        //std::cout << "(CP) <" << myId << "> Removing Item <" << itemName << "> from Cache.\n";
                        //std::cout.flush();

                        it = contentLibrary->erase(it);
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
                for (auto it = contentLibrary->begin(); it != contentLibrary->end() ; it++) {
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

                if (sameTimeCount > 1) {
                    std::cout << "(CP) Warning: Multiple items with same caching time detected! Earliest Cache Time is <" << earliestTime.str() << ">\n";
                    std::cout.flush();
                }

                bool foundRemoval = false;
                //Searching for last item with desired time value to be removed
                while (foundRemoval == false) {
                    for (auto it = contentLibrary->begin(); it != contentLibrary->end() ; it++) {
                        if (it->lastAccessTime == earliestTime) {
                            //Randomly selecting one of the items with the last access time with equal probability
                            if ( (foundRemoval == false) &&
                                 (sameTimeCount == 1 || uniform(0,1) < individualProbability)) {
                                foundRemoval = true;
                                std::string itemName = it->contentPrefix;

                                //std::cout << "(CP) <" << myId << "> Removing Item <" << it->contentPrefix << "> from Cache with Use Count <" << it->useCount << ">\t last access <" << it->lastAccessTime << ">.\n";
                                //std::cout.flush();

                                it = contentLibrary->erase(it);
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
            case FREQ_POPULARITY:
            case LFU:
            {
                //std::cout << "(CP) Running LFU Cache Replacement.\n";
                //std::cout.flush();

                //Looking for least used messages (aware of multiple occurences of same metric value)
                int sameUseCount = 1;
                int timesUsed = library->getCurrentIndex()+1;
                for (auto it = contentLibrary->begin(); it != contentLibrary->end() ; it++) {
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
                    for (auto it = contentLibrary->begin(); it != contentLibrary->end() ; it++) {
                        if (it->useCount == timesUsed) {
                            if ( (foundRemoval == false) &&
                                 (sameUseCount == 1 || uniform(0,1) < individualProbability)) {
                                foundRemoval = true;

                                //std::cout << "(CP) <" << myId << "> Removing Item <" << it->contentPrefix << "> from Cache with Uses <" << it->useCount << ">.\n";
                                //std::cout.flush();

                                it = contentLibrary->erase(it);
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
                contentLibrary->pop_front();
                break;
            }

            case GOD_POPULARITY: {
                int largestRanking = 0;
                //std::list<Content_t>::iterator contentPointer;
                for (auto it = contentLibrary->begin(); it != contentLibrary->end() ; it++) {
                    if (it->popularityRanking >= largestRanking) {
                        largestRanking = it->popularityRanking;
                    }
                }

                //TODO: This is inneficient and I should rewrite for a single-loop
                for (auto it = contentLibrary->begin(); it != contentLibrary->end() ; it++) {
                if (it->popularityRanking == largestRanking) {
                        //std::cout <<  "(CP) Removing item <" << it->contentPrefix << "> with Popularity : <" << to_string(it->popularityRanking) << ">\n";
                        //std::cout.flush();

                        contentLibrary->erase(it);
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
void BaconContentProvider::addContentToLibrary(Content_t* contentObject) {
    if (contentObject == NULL) {
        EV_ERROR << "(CP) Content to be added to library is NULL.\n";
        EV_ERROR.flush();

        std::cerr << "(CP) Error: Content to be added to library is NULL.\n";
        std::cerr.flush();
        return;
    }

    //Prior to Simulation Start during Warmup, we're just building our libraries so we ignore "searches"
    //Note: Please keep this in mind when cold starting simulations
    if (simTime() >= stats->getStartTime()) {
        //Looking for item in content library
        for (auto it = contentLibrary->begin(); it != contentLibrary->end() ; it++) {
            if (it->contentPrefix.compare(contentObject->contentPrefix) == 0) {
                //std::cout << "(CP) <" << myId << "> Already has " << contentObject->contentPrefix << " in local Library.\n";
                //std::cout.flush();
                //increaseUseCount(contentObject->contentPrefix);
                return;
            }
        }
    }

    //Checking for Content replacement prior to adding content to library to ensure content will not be instantly removed based on policy behavior
    int cacheSize = contentLibrary->size();
    if ( cacheSize >= maxCachedContents && maxCachedContents != -1) {
        //std::cout << "(CP) Cache is full.\n";
        //std::cout.flush();

        stats->increaseCacheReplacements();
        runCacheReplacement();
    }

    //newContent.contentPopularity = contentObject->contentPopularity;
    //newContent.contentPriority =  contentObject->contentPriority;

    Content_t newContent;
    newContent.contentClass = contentObject->contentClass;
    newContent.contentSize = contentObject->contentSize;
    newContent.contentStatus = ContentStatus::AVAILABLE;
    newContent.popularityRanking = contentObject->popularityRanking;
    newContent.contentPrefix = contentObject->contentPrefix;
    newContent.lastAccessTime = simTime();
    newContent.useCount = 0;
    //newContent.expireTime = SIMTIME_S;
    contentLibrary->push_front(newContent);
    librarySize++;

    //EV << "(CP) Added content object " << contentObject->contentPrefix << " to local Library.\n";
    //EV.flush();

    //std::cerr << "\t(CP) <" << myId << "> Added content object " << contentObject->contentPrefix << " to local Library.\n";
    //std::cerr.flush();
}

//
void BaconContentProvider::removeContentFromLibrary(Content_t* newContent) {
    /*
    //Unwrapping contents of request
    cArray parArray = msg->getParList();
    cMsgPar* requestName = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));
    std::string requestString = requestName->stringValue();

    if (requestString.c_str()[0] == '\"') {
        requestString = requestString.substr(1, requestString.length() - 2); //No fucking idea why but strings added as parameters get extra quotes around them. WTF
    }

    std::cout << "(CP) <" << myId << "> is Attempting to remove object from Cache.\n";
    std::cout.flush();
    */

    //Searching for content
    for (auto it = contentLibrary->begin(); it != contentLibrary->end(); it++) {
        if (newContent->contentPrefix.compare(it->contentPrefix) == 0) {
            std::cout << "(CP) <" << myId << "> Removing Item <" << it->contentPrefix << "> from Cache with Use Count <" << it->useCount << ">.\n";
            std::cout.flush();
            it = contentLibrary->erase(it);
            librarySize--;
            return;
        }
    }

    std::cerr << "(CP) <" << myId << "> Warning: Failed to remove object from Cache Library!\n";
    std::cerr.flush();
}

//
void BaconContentProvider::buildContentLibrary() {
    //Building Content Database
    if (contentLibrary != NULL) return;
    contentLibrary = new std::list<Content_t>();

    if (library == NULL) {
        std::cerr << "(CP) Error: Global Content Library module reference is null.\n";
        std::cerr.flush();
        return;
    }

    //Building our Content Library from the complete library
    std::list<Content_t> multimediaLibrary = library->getMultimediaContentList();
    //double multimediaContentAvailabilityRate = (double)availableMultimediaObjects/(double)completeLibrary.size();
    int librarySize = multimediaLibrary.size();

    EV << "(CP) Statistical Probability rate for any Multimedia Content Item is: " << availableMultimediaObjects/(double)librarySize << "\n";
    EV.flush();

    //std::cout << "(CP) <" << myId << "> Building Content Library.\n";
    //std::cout.flush();

    if (availableMultimediaObjects == -1) {
        //Adding Items from other categories

        std::cout << "(CP) <" << myId << "> is a Multimedia Server.\n";
        for (auto it = multimediaLibrary.begin(); it != multimediaLibrary.end(); it++) {
            //std::cout << "\t\\--> Adding <" << it->contentPrefix << ">to Library.\n";
            addContentToLibrary(&*it);
        }
        //std::cout.flush();

    } else {
        //Marking all items as unavailable in order to include exactly the desired number of items (plus the other category items)
        bool* inclusionList = new bool[librarySize];
        for (int i = 0 ; i < librarySize ; i++) {
            inclusionList[i] = false;
        }

        for (int i = 0 ; i < availableMultimediaObjects ; i++) {
            int curIndex = 0;
            bool foundItem = false;
            int desIndex = 0;
            while (!foundItem) {
                desIndex = uniform(0, multimediaLibrary.size());
                if (inclusionList[desIndex] == false) {
                    inclusionList[desIndex] = true;
                    foundItem = true;
                }
            }

            //Adding Multimedia Objects
            for (auto it = multimediaLibrary.begin(); it != multimediaLibrary.end(); it++) {
                if (curIndex == desIndex) {
                    std::cout << "(CP) <" << myId << "> is adding content object <" << (*it).contentPrefix << "> to its initial library.\n";
                    addContentToLibrary(&*it);
                    break;
                }
                curIndex++;
            }
        }
        delete[] inclusionList;

        //Adding Items from other categories
        for (auto it = multimediaLibrary.begin(); it != multimediaLibrary.end(); it++) {
            if (it->contentClass != ContentClass::MULTIMEDIA) {
                addContentToLibrary(&*it);
                break;
            }
        }
    }

    //std::cout << "(CP) <" << myId << "> Library is setup.\n";
    //std::cout.flush();
}

//
/*
void BaconContentProvider::handleMessage(cMessage *msg) {
    EV << "(CP) Received a Content Request Message: <" << msg->getName() << ">\n";
    EV.flush();

    //Checking for Lookup Requests
    if (MessageClass::INTEREST.compare(msg->getName()) == 0 ) {
        handleLookup(msg);
    } else if (MessageClass::DATA.compare(msg->getName()) == 0 ) {
        handleDataTransfer(msg);
    } else if (MessageClass::DATA_INCLUDE.compare(msg->getName()) == 0 ) {
        addToLibrary(msg);
        increaseUseCount(msg);
    } else if (MessageClass::DATA_EXLUDE.compare(msg->getName()) == 0 ) {
        removeContentFromLibrary(msg);
    } else {
        EV_WARN << "(CP) Warning: Unknown Message type incoming << " << msg->getName() << ".\n";
        EV_WARN.flush();
    }
}
*/

//Pseudosetter
void BaconContentProvider::increaseUseCount(cMessage *msg) {
    cArray parArray = msg->getParList();
    cMsgPar* requestName = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));
    increaseUseCount(requestName->str());
}

//PseudoSetter
void BaconContentProvider::increaseUseCount(std::string prefix) {
    increaseUseCount(1,prefix);
}

void BaconContentProvider::increaseUseCount(int addedUses, std::string prefix) {
    //Removing that god damn fucking quote
    prefix.erase(std::remove(prefix.begin(), prefix.end(), '\"'), prefix.end());
    if (prefix.c_str()[0] == '\"') {
        prefix = prefix.substr(1,prefix.length()-2);
    }

    //Looking for Item
    for (auto it = contentLibrary->begin(); it != contentLibrary->end(); it++) {
        if (prefix.compare(it->contentPrefix) == 0) {
            it->useCount += addedUses;
            it->lastAccessTime = simTime();

            //if (it->useCount > 1) {
                //std::cout << "(CP) <" << myId << ">\tIncreased Use count for <" << it->contentPrefix << "> to\t<" << it->useCount << ">\n";
                //std::cout.flush();
            //}

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
int BaconContentProvider::getUseCount(std::string prefix) {
    //Removing that god damn fucking quote
    prefix.erase(std::remove(prefix.begin(), prefix.end(), '\"'), prefix.end());
    if (prefix.c_str()[0] == '\"') {
        prefix = prefix.substr(1,prefix.length()-2);
    }

    //Looking for Item
    for (auto it = contentLibrary->begin(); it != contentLibrary->end(); it++) {
        if (prefix.compare(it->contentPrefix) == 0) {
            return it->useCount;
        }
    }
    return -1;
}

//Function meant to handle Content Lookup Requests
void BaconContentProvider::addToLibrary(cMessage *msg) {
    //Unwrapping contents of request
    cArray parArray = msg->getParList();
    cMsgPar* requestName = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));

    std::string nameValue = requestName->stringValue();
    if (nameValue.c_str()[0] == '\"') {
        nameValue = nameValue.substr(1,nameValue.length()-2);   //No fucking idea why but strings added as parameters get extra quotes around them. WTF
    }

    //std::cout << "(CP) Content \"" << nameValue << "\" was added to cache.\n";
    //std::cout.flush();

    addContentToLibrary(library->getContent(nameValue));
}

//Function meant to handle Content Lookup Requests
bool  BaconContentProvider::handleLookup(std::string nameValue) {
    /**/
    std::string lookupValue = nameValue;
    if (nameValue.c_str()[0] == '\"') {
        lookupValue = nameValue.substr(1,nameValue.length()-2);   //No fucking idea why but strings added as parameters get extra quotes around them. WTF
    }

    EV << "(CP) Handling Lookup for <" << lookupValue << ">.\n";
    EV.flush();

    buildContentLibrary();
    if (contentLibrary == NULL) {
        std::cerr << "(CP) Error: Content Library is NULL.\n";
        std::cerr.flush();
        return false;
    }

    //Looking for item in content library
    for (auto it = contentLibrary->begin(); it != contentLibrary->end() ; it++) {
        int comparison = it->contentPrefix.compare(lookupValue);

        if (comparison == 0) {
            //If the required information is TRAFFIC information then we'll look at our GPS position. If our position is identical to the request its local data and we treat it as unavailable because the purpose of this request is to obtain information about adjacent vehicles

            //std::cout << "(CP) Technically we found the content so we will return true.\n";
            //std::cout.flush();

            switch(it->contentClass) {
                case ContentClass::TRAFFIC: {
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
                    }
                case ContentClass::MULTIMEDIA:
                    //TODO: Decide if we're going to do anything else special
                    return true;

                default:
                    return false;
                    break;
            }
            break;
        }
    }
    return false;
}

//Getter to classify node as either server or client (servers don't have cache limitation applied during computation)
bool BaconContentProvider::isServer() {
    //TODO: Make this more maliable depending on content class (?)
    //Having all items implies being a server, which we use to log server based statistics
    if (availableMultimediaObjects == -1) {
        return true;
    }
    return false;
}

//Getter for the currently used Cache Policy
CacheReplacementPolicy BaconContentProvider::getCachePolicy() {
    return cachePolicy;
}

//(BROKEN - Sequential Downhops will cache same item, should consider distance since last cache) Decision algorithm function whether item should be cached
bool BaconContentProvider::brokenlocalPopularityCacheDecision(Connection_t* connection) {
    if (cachePolicy == CacheReplacementPolicy::FREQ_POPULARITY) {
        int sum = 0;
        int minUseCount = contentLibrary->size() > 0 ? INT_MAX : -1;
        //Looking for Item
        for (auto it = contentLibrary->begin(); it != contentLibrary->end(); it++) {
            sum += it->useCount;
            //Replacing minimum use count
            if (it->useCount < minUseCount) {
                minUseCount = it->useCount;
            }
            //minUseCount = it->useCount < minUseCount ? it->useCount : minUseCount;
        }
        double averagePopularity = sum > 0 ? floor(double(sum/(double)contentLibrary->size())) : 1;
        double estimatedResult = averagePopularity > 0 ? double(floor(log2(connection->remoteHopUseCount))/(double)averagePopularity) : 0;
        //std::cout << "< sum >< cacheSize >< minUseCount >< averagePopularity > < incomingHopCount >< remoteUseCount >\n";
        //std::cout << "  <" << sum << ">\t<" << maxCachedContents << ">\t\t<" << minUseCount << ">\t\t<" << averagePopularity << ">\t\t\t<" << connection->downstreamHopCount << ">\t\t\t<" << connection->remoteHopUseCount << ">\n";
        //std::cout << "  <" << estimatedResult << ">\n\n";
        if (minUseCount < connection->remoteHopUseCount || estimatedResult > 1) {
            return true;
        }
    }
    return false;
}



//Decision algorithm function whether item should be cached
bool BaconContentProvider::localPopularityCacheDecision(Connection_t* connection) {
    int sum = 0;
    int minUseCount = contentLibrary->size() > 0 ? INT_MAX : -1;
    //Looking for Item
    if (minUseCount == -1) {
        sum = 0;
        minUseCount = 0;
    } else {
        for (auto it = contentLibrary->begin(); it != contentLibrary->end(); it++) {
            sum += it->useCount;
            //Replacing minimum use count
            if (it->useCount < minUseCount) {
                minUseCount = it->useCount;
            }
            //minUseCount = it->useCount < minUseCount ? it->useCount : minUseCount;
        }
    }
    double averagePopularity = sum > 0 ? floor(double(sum/(double)contentLibrary->size())) : 0;
    double estimatedResult = connection->remoteHopUseCount > 0 ? double(floor(log2(connection->downstreamCacheDistance) + log2(connection->remoteHopUseCount))/(double)averagePopularity) : 0;

    if (estimatedResult >= 1) {
        //std::cout << "<  sum  >< cacheSize >< minUseCount >< averagePopularity > < incomingHopCount >< remoteUseCount >\n";
        //std::cout << "<" << sum << ">\t\t<" << maxCachedContents << ">\t\t<" << minUseCount << ">\t\t<" << averagePopularity << ">\t\t<" << connection->downstreamHopCount << ">\t\t<" << connection->remoteHopUseCount << ">\n";
        //std::cout << "<" << estimatedResult << ">";
        //std::cout << "\t< YES >\t\tTime:<" << simTime() << ">\n\n";
        return true;
    }

    if (minUseCount < connection->remoteHopUseCount) {
        //std::cout << "<  sum  >< cacheSize >< minUseCount >< averagePopularity > < incomingHopCount >< remoteUseCount >\n";
        //std::cout << "<" << sum << ">\t\t<" << maxCachedContents << ">\t\t<" << minUseCount << ">\t\t<" << averagePopularity << ">\t\t<" << connection->downstreamHopCount << ">\t\t<" << connection->remoteHopUseCount << ">\n";
        //std::cout << "<" << estimatedResult << ">";
        //std::cout << "\t< MAYBE >\t\tTime:<" << simTime() << ">\n\n";
        return true;
    }

    //std::cout << "\t< NO >\t\tTime:<" << simTime() << ">\n\n";
    return false;
}
