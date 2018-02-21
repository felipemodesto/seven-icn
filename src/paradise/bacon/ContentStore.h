//
// Copyright (C) 2006-2011 Christoph Sommer <christoph.sommer@uibk.ac.at>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#ifndef BaconContentProvider_H
#define BaconContentProvider_H


#include <paradise/bacon/Definitions.h>
#include <paradise/bacon/GlobalLibrary.h>
#include <paradise/bacon/ServiceManager.h>

class ServiceManager;
class GlobalLibrary;
class Statistics;

using namespace omnetpp;
using namespace std;


/**
 * Application Layer Service Manager
 */
class ContentStore : public omnetpp::cSimpleModule {
    public:
        virtual void initialize(int stage);
        virtual void finish();

        virtual int numInitStages () const {
            return 3;
        }

    protected:
        int myId;

        TraCIMobility* traci;

        int contentExchangeIn;          // from Content Manager
        int contentExchangeOut;         // to Content Manager

        bool hasLibrary = false;
        NodeRole nodeRole = NodeRole::MULE;

        CacheLocationPolicy GPSCachePolicy = CacheLocationPolicy::IGNORE_LOCATION;

        std::list<CachedContent_t> contentCache;
        std::list<CachedContent_t> gpsCache;
        int librarySize;

        int gpsCacheWindowSize;
        std::list<OverheardGPSObjectList_t*> gpsCacheFrequencyWindow;
        OverheardGPSObjectList_t neighborGPSInformation;

        CacheReplacementPolicy cachePolicy;

        //Cache Size Properties (see other stuff below for more policies that affect cache size distribution)
        int maxCachedContents = 0;
        int startingCache = 0;
        int gpsCacheSize = 0;
        int priorityCacheSize = 0;
        int personalCacheSize = 0;
        int friendCacheSize = 0;
        int othersCacheSize = 0;

        double easingFactor = 0.75;

        ServiceManager* manager;
        Statistics* stats;
        GlobalLibrary* library;

        int gpsUpdateTimerTime = 1;                 //1 second per window slice
        cMessage *gpsCacheTimerMessage;              //Timer used for second to second updates

    protected:
        void runGPSCacheReplacement();                          //Cache replacement Policy implementation function
        void runCacheReplacement();                             //Cache replacement Policy implementation function
        void buildContentCache();                             //
        void addToLibrary(cMessage *msg);                       //
        void resetGPSTimer();

        void handleMessage(cMessage *msg)  override;                //CALLBACK FROM SELF MESSAGE, GENERALLY USED AS RANDOM TIMER CALLBACK

    public:
        bool gpsSubCacheEnabled();

        void increaseUseCount(std::string prefix);              //Increase the request (use) count for a specific content object
        void increaseUseCount(cMessage *msg);                   //Increase the request (use) count for a specific content object
        void increaseUseCount(Content_t* object);
        void increaseUseCount(int addedUses, Content_t* object);
        void increaseUseCount(int addedUses,std::string prefix);//Increase the request (use) count for a specific content object

        int getUseCount(std::string prefix);                    //Get the current use count for an object
        CacheReplacementPolicy getCachePolicy();                //Getter for the cache policy

        void shareGPSStatistics();
        void maintainGPSCache();
        void logGPSRequest(Content_t* object);
        void handleGPSPopularityMessage(WaveShortMessage* wsm);
        void handleGPSPopularityResponseMessage(WaveShortMessage* wsm);

        bool gpsPopularityCacheDecision(OverheardGPSObject_t* connection);      //Decision algorithm function whether item should be cached
        bool localPopularityCacheDecision(Connection_t* connection);            //Decision algorithm function whether item should be cached
        bool localMinimumPopularityCacheDecision(Connection_t* connection);     //Decision algorithm function whether item should be cached
        bool globalPopularityCacheDecision(Connection_t* connection);           //Decision algorithm function whether item should be cached
        bool globalMinimumPopularityCacheDecision(Connection_t* connection);    //Decision algorithm function whether item should be cached

        bool isObjectGPSRelevant(Content_t* object);                                //Return True if object is relevant for current Location Policy
        Content_t* availableFromCache(Content_t* contentObject);                    //Obtain item from cache (not other locally available heuristics)
        Content_t* availableFromLocation(Content_t* contentObject);                 //Obtain item from cache (not other locally available heuristics)
        Content_t* fetchViaLocation(Content_t* contentObject);
        ContentAvailabilityStatus availableForProvisioning(Content_t* contentObject, int requestID);       //Check if we have the item anywhere (cache, local providers, etc)
        void removeContentFromCache(Content_t* newContent);                     //Remove item from cache
        void removeFromGPSSideStatistics(Content_t* contentObject);
        void addContentToCache(Content_t* newContent);                          //Add content to Cache
        void addContentToGPSCache(OverheardGPSObject_t* gpsPopularItem);
        NodeRole getRole();                                             //Check if we're a SERVER or CLIENT or whatever (Have everything)
};


#endif
