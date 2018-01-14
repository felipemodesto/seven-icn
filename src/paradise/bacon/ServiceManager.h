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

#ifndef BaconServiceManager_H
#define BaconServiceManager_H

#include <paradise/bacon/ContentStore.h>
#include <paradise/bacon/Definitions.h>
#include <paradise/bacon/Statistics.h>

class ContentStore;
class GlobalLibrary;
class Statistics;

using Veins::TraCIMobility;
using Veins::AnnotationManager;

/**
 * Application Layer Service Manager
 */
class ServiceManager : public BaseWaveApplLayer {

    public:
        virtual void receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj);

    protected:
        //Base variables & Class Related Stuff
        static const simsignalwrap_t parkingStateChangedSignal;
        AnnotationManager* annotations;
        simtime_t lastDroveAt;
        TraCIMobility* traci;
        Statistics* stats;
        GlobalLibrary* library;
        ContentStore* cache;

        WaveShortMessage* sendBeaconEvt;

        //Configurable Parameters (omnetpp.ini)
        uint64_t bitrate;
        double windowTimeSlotDuration;                  //
        int maxAttempts;
        double minimumForwardDelay;
        double maximumForwardDelay;

        int slidingWindowSize;
        double interestBroadcastTimeout;                    //Time before an interest broadcast is timed out
        double transferTimeoutTime;                         //Time before transfer timer is timed out (Timer Stuff)
        double maxSimultaneousConnections;                  //Number of simultaneous connected allowed for a node (-1 == Infinite)
        double cacheCopyProbability;                        //Cache copy probability for probabilistic cache in-network caching policy
        CacheInNetworkCoordPolicy cacheCoordinationPolicy;  //Configuration which enables/disables in-network caching
        AccessRestrictionPolicy priorityPolicy;

        //Networking Parameters
        int clientExchangeIn;                           //Input from Service Manager
        int clientExchangeOut;                          //Output to Service Manager
        bool sentMessage;
        bool isParking;
        bool sendWhileParking;
        bool requestFallbackAllowed;
        int currentlyActiveConnections;

        //Load Evaluation
        NetworkLoadStatus networkLoadStatus;
        std::list<double> networkLoadWindow;
        double currentBitLoad;
        double instantBitLoad;
        double averageBitLoad;
        //Class Based Load statistics
        double transitBitLoad;
        double networkBitLoad;
        double multimediaBitLoad;

        int lowMediumBandwidth = 50;
        int mediumHighBandwidth = 80;

        simtime_t enterSimulation;

        std::vector<cMessage*> cancelMessageTimerVector;
        std::list<Interest_t*> PIT;
        std::list<Connection_t*> connectionList;
        std::list<Neighbor_t> neighborList;
        std::list<NetworkPacket_t> packetList;

        //Statistics
        int totalChunksRetransmited;
        int totalChunksLost;
        int totalPacketsLost;
        int totalPacketsSent;
        int totalPacketsForwarded;
        int totalPacketsUnserved;
        int totalContentUnavailableResponses;
        int totalServerBusyResponses;
        int PITRetries;

    protected:

        virtual void initialize(int stage);
        virtual void finish();

        virtual int numInitStages () const {
            return 2;
        }
        void updateNodeColor();                                                         //UPDATES DEBUG CIRCLE COLOR BASED ON CLIENTSTATUS

        Connection_t* createGenericConnection(Content_t* content);                      //Create a Generic Connection Item
        Connection_t* createClientSidedConnection(WaveShortMessage* wsm);               //Creates a connection based on request messages both local and remote
        Connection_t* createServerSidedConnection(WaveShortMessage* wsm);               //Creates a connection based on request messages both local and remote
        Connection_t* getConnection(long requestID, int peerID);                        //Gets information for a given connection
        bool checkForConnections(long requestID);                                       //
        void cleanConnections();                                                        //STATISTICS COLLECTION AND MEMORY MANAGEMENT FUNCTION

        Interest_t* getInterest(std::string interest);
        bool createInterest(std::string interestPrefix, int senderAddress);
        bool addToInterest(std::string interestPrefix, int senderAddress);
        bool removeFromInterest(std::string interestPrefix, int senderAddress);
        bool deleteInterest(std::string interestPrefix);
        void fulfillPendingInterest(Connection_t* connection);

        //Handlers for outgoing shit
        void forwardContentSearch(WaveShortMessage* wsm, Connection_t* connection);         //send Broadcast INTEREST request to network interface
        void notifyOfContentAvailability(WaveShortMessage* wsm, Connection_t* connection);  //
        void acceptNetworkrequest(WaveShortMessage* wsm, Connection_t* connection);         //CALLED TO REPLY WITH "OK" INTEREST RESPONSE
        void rejectNetworkrequest(WaveShortMessage* wsm, Connection_t* connection);         //CALLED TO REPLY WITH "CANCEL" INTEREST RESPONSE
        void transmitDataChunks(Connection_t* connection, std::vector<int>* chunkVector);   //
        void requestChunkRetransmission(Connection_t* connection);                          //
        void completeRemoteDataTransfer(Connection_t* connection);                          //
        void replyAfterContentInclusion(Connection_t* connection);                          //"DELAYED" NOTIFICATION AFTER CONTENT IS OBTAINED (Sets up notifyOfContentAvailability)

        WaveShortMessage* getGenericMessage(Connection_t* connection);

        //
        void runCachePolicy(Connection_t* connection);                                      //
        void addContentToCache(Connection_t* connection);                                   //CALLED TO ADD DATA FROM TRANSFER TO CACHE
        void removeContentFromCache(Connection_t* connection);                              //CALLED TO REMOVE DATA FROM TRANSFER FROM CACHE
        void refreshNeighborhood();                                                          //
        bool addNeighbor(WaveShortMessage *msg);                                            //
        void logNetworkmessage(WaveShortMessage *msg);                                      //

        //Handlers for incoming shit
        void handleInterestRejectMessage(WaveShortMessage* wsm);
        void handleInterestAcceptMessage(WaveShortMessage* wsm);
        void handleInterestReplyMessage(WaveShortMessage* wsm);
        void handleInterestMessage(WaveShortMessage* wsm);
        void handleContentMessage(WaveShortMessage* wsm);

        //Timer
        void startTimer(Connection_t* connection, double delay);
        void startTimer(Connection_t* connection);
        bool cancelTimer(Connection_t* connection);
        void handleSelfTimer(WaveShortMessage *timerMessage);

        //Incoming Message Functions
        void handleMessage(cMessage *msg)  override;            //CALLBACK FROM SELF MESSAGE, GENERALLY USED AS RANDOM TIMER CALLBACK
        void handleLowerMsg(cMessage* msg) override;            //OVERRIDEN IMPLEMENTATION OF BASE WAVE MESSAGE HANDLING

        //Interface Message evaluation Function
        void onNetworkMessage(WaveShortMessage* wsm);           //ON WSM FROM NETWORK INTERFACE (LOWER)

        //Outgoing Message Functions
        void sendWSM(WaveShortMessage* wsm) override;           //FORWARD MESSAGE TO NETWORK INTERFACE VIA EXCHANGE INTERFACE
        void sendWSM(WaveShortMessage* wsm, double delay);      //FORWARD MESSAGE TO NETWORK INTERFACE VIA EXCHANGE INTERFACE
        void sendToClient(WaveShortMessage *wsm);               //FORWARD MESSAGE TO CLIENT VIA CLIENT EXCHANGE INTERFACE
        void sendBeacon();                                      //GENERIC BEACON BROADCAST MESSAGE GENERATOR

        //BaseWaveApplicationLayer Functions we don't really care about right now
        virtual void onBeacon(WaveShortMessage* wsm);
        virtual void onData(WaveShortMessage* wsm);
        void handlePositionUpdate(cObject* obj) override;
        void handleParkingUpdate(cObject* obj);

    public:
        static WaveShortMessage* convertCMessage(cMessage* msg);               //Convert cMessage to WaveShortMessage (Cast for now)
        ContentClass getClassFromPrefix(std::string prefix);
        void advertiseGPSItem(OverheardGPSObject_t mostPopularItem);
};

#endif
