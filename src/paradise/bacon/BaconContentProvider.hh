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


#include <paradise/bacon/BaconStructures.hh>

#include <paradise/bacon/BaconLibrary.hh>
#include <paradise/bacon/BaconServiceManager.hh>
class BaconServiceManager;

using namespace omnetpp;
using namespace std;

/**
 * Application Layer Service Manager
 */
class BaconContentProvider : public omnetpp::cSimpleModule {
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

        std::list<CachedContent_t>* contentLibrary;
        int librarySize;

        CacheReplacementPolicy cachePolicy;
        int startingCache;
        int maxCachedContents;

        BaconServiceManager* manager;
        BaconStatistics* stats;
        BaconLibrary* library;

    protected:

        void runCacheReplacement();                             //Cache replacement Policy implementation function
        void buildContentLibrary();                             //
        void addToLibrary(cMessage *msg);                       //
        //void handleMessage(cMessage *msg) override;           //CALLBACK FROM SELF MESSAGE, GENERALLY USED AS RANDOM TIMER CALLBACK


    public:
        void increaseUseCount(std::string prefix);              //Increase the request (use) count for a specific content object
        void increaseUseCount(cMessage *msg);                   //Increase the request (use) count for a specific content object

        void increaseUseCount(int addedUses,std::string prefix);//Increase the request (use) count for a specific content object

        int getUseCount(std::string prefix);                    //Get the current use count for an object
        CacheReplacementPolicy getCachePolicy();                //Getter for the cache policy

        //bool brokenlocalPopularityCacheDecision(Connection_t* connection);    //Decision algorithm function whether item should be cached
        bool localPopularityCacheDecision(Connection_t* connection);    //Decision algorithm function whether item should be cached


        bool handleLookup(std::string prefix);                  //Deal with Content Lookup Requests
        void removeContentFromLibrary(Content_t* newContent);   //Remove item from cache
        void addContentToLibrary(Content_t* newContent);        //Add content to Cache
        bool isServer();                                        //Check if we're a SERVER (Have everything)
};


#endif
