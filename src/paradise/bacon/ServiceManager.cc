//Concent Centric Class - Felipe Modesto

#include <paradise/bacon/ServiceManager.h>

using Veins::TraCIMobilityAccess;
using Veins::AnnotationManagerAccess;

using namespace omnetpp;

//using namespace osgEarth;
//using namespace osgEarth::Annotation;
//using namespace osgEarth::Features;

const simsignalwrap_t ServiceManager::parkingStateChangedSignal = simsignalwrap_t(TRACI_SIGNAL_PARKING_CHANGE_NAME);

Define_Module(ServiceManager);

//TODO: (IMPROVE) CHANGE ALL GETNAME/SETNAME to GETKIND/SETKIND to reduce the ridiculous overload with string evaluations
//USE INTEREST, DATA AND BEACON AS ONLY MESSAGE NAME TYPES AVAILABLE!!!

//=============================================================
// BASIC MODULE FUNCTIONALITY
//=============================================================

//Initialization Function
void ServiceManager::initialize(int stage) {
    //Initializing
    BaseWaveApplLayer::initialize(stage);

    switch (stage) {
        case 0: {
            //Getting Simulation Scenario Configured Parameters
            sendWhileParking = par("sendWhileParking").boolValue();
            transferTimeoutTime = par("transferTimeoutTime").doubleValue();
            maxSimultaneousConnections = par("maxSimultaneousConnections").doubleValue();

            cacheCopyProbability = par("cacheCopyProbability").doubleValue();
            cacheCoordinationPolicy = static_cast<CacheInNetworkCoordPolicy>(par("cacheCoordinationPolicy").longValue());

            slidingWindowSize = par("slidingWindowSize").longValue();
            minimumForwardDelay = par("minimumForwardDelay").doubleValue();
            maximumForwardDelay = par("maximumForwardDelay").doubleValue();
            maxAttempts = par("maxAttempts").doubleValue();
            interestBroadcastTimeout = par("interestBroadcastTimeout").doubleValue();
            windowTimeSlotDuration = par("windowSlotSize").doubleValue();
            bitrate = par("bitrate").longValue();
            priorityPolicy = static_cast<AccessRestrictionPolicy>(par("priorityPolicy").longValue());

            requestFallbackAllowed = par("requestFallbackAllowed").boolValue();

            //Getting TraCI Manager (SUMO Connection & Annotations Ready)
            traci = TraCIMobilityAccess().get(getParentModule());
            annotations = AnnotationManagerAccess().getIfExists();
            ASSERT(annotations);

            //Setting Gates
            clientExchangeIn = findGate("clientExchangeIn");
            clientExchangeOut = findGate("clientExchangeOut");

            //Setting Base Parameters
            sentMessage = false;
            isParking = false;
            lastDroveAt = simTime();

            totalPacketsForwarded = 0;
            totalChunksRetransmited = 0;
            totalChunksLost = 0;
            totalPacketsLost = 0;
            totalPacketsSent = 0;
            totalPacketsUnserved = 0;
            totalServerBusyResponses = 0;
            totalContentUnavailableResponses = 0;
            PITRetries = 0;
            currentlyActiveConnections = 0;

            currentBitLoad = 0;
            averageBitLoad = 0;
            instantBitLoad = 0;

            transitBitLoad = 0;
            networkBitLoad = 0;
            multimediaBitLoad = 0;

            enterSimulation = simTime();

            WATCH(currentBitLoad);
            WATCH(transitBitLoad);
            WATCH(networkBitLoad);
            WATCH(multimediaBitLoad);

            networkLoadStatus = NetworkLoadStatus::LOW_LOAD;

            updateNodeColor();

            //Set Parking state change listener on current car Object
            findHost()->subscribe(parkingStateChangedSignal, this);
            break;
        }

        case 1: {

            cSimulation *sim = getSimulation();
            stats = check_and_cast<Statistics *>(sim->getModuleByPath("ParadiseScenario.statistics"));
            library = check_and_cast<GlobalLibrary *>(sim->getModuleByPath("ParadiseScenario.library"));
            cache = check_and_cast<ContentStore *>(getParentModule()->getSubmodule("content"));

            //Adding vehicle to statistics
            stats->increaseActiveVehicles(myId);

            //Setting Up Beacon Event
            double maxOffset = par("maxOffset").doubleValue();
            sendBeaconEvt = new WaveShortMessage(MessageClass::SELF_BEACON_TIMER.c_str(), SEND_BEACON_EVT);

            //simulate asynchronous channel access
            double offSet = dblrand() * (par("beaconInterval").doubleValue()/2);
            offSet = offSet + floor(offSet/0.050)*0.050;
            individualOffset = dblrand() * maxOffset;
            scheduleAt(simTime() + offSet, sendBeaconEvt);


            //std::cout << "(SM) Starting Vehicle <" << myId << "> Time <" << simTime() << ">\n";
            //std::cout.flush();
        }
    }
}

//End of Execution Function (NOT A DESTRUCTOR, JUST CLEANUP AND STATISTICS RELATED)
void ServiceManager::finish() {
    //Logging HISTOGRAM statistics (separate from live-info collected by clients)
    stats->setPacketsSent(totalPacketsSent);
    stats->setPacketsLost(totalPacketsLost);
    stats->setPacketsUnserved(totalPacketsUnserved);
    stats->setContentUnavailable(totalContentUnavailableResponses);
    stats->setChunksLost(totalChunksLost);
    stats->setServerBusy(totalServerBusyResponses);

    simtime_t duration = simTime() - enterSimulation;
    stats->logParticipationDuration(duration.dbl());

    bool error = stats->decreaseActiveVehicles(myId);

    //For log purposes we'll inform our position data if we have vehicle exit issues
    if (error == false) {
        //std::cout << "(SM) <" << myId << "> Premature Exit, At: <" << traci->getCurrentPosition() << "> road ID: <" << traci->getRoadId() << ">, Time <" << simTime() << ">\n";
        //std::cout.flush();
    }


    //std::cout << "\t(SM) Killing Vehicle <" << myId << ">  Time <" << simTime() << "> at <" << traci->getRoadId() << ">\n";
    //std::cout.flush();

    //Cleaning up
    cleanConnections();
    cancelMessageTimerVector.clear();
}

//Maintenance function to reduce ConnectionList Sizes
void ServiceManager::cleanConnections() {
    //std::cout << "(SM) Enter cleanConnections\n";
    //std::cout.flush();

    //Iterating through open and existing connectionsa
    for (auto it = connectionList.begin(); it != connectionList.end();) {
        simtime_t difTime = simTime() - (*it)->requestTime;

        Connection_t* curCon = (*it);

        //Checking if the connection is marked as done
        if (ConnectionStatus::DONE_MIN < curCon->connectionStatus && curCon->connectionStatus < ConnectionStatus::DONE_MAX) {

            //Saving Statistics
            if (curCon->peerID == myId) {
                //Logging Hop Count for packages received correctly
                if ( curCon->connectionStatus == ConnectionStatus::DONE_AVAILABLE ||
                     curCon->connectionStatus == ConnectionStatus::DONE_RECEIVED  ||
                     curCon->connectionStatus == ConnectionStatus::DONE_FALLBACK ) {
                    stats->setHopsCount(curCon->downstreamHopCount);
                    stats->increaseHopCountResult(curCon->downstreamHopCount);
                }
                //TODO: (IMPLEMENT) Log all stats again!
            }

            //Deleting Connection Object
            if (curCon->connectionStatus == ConnectionStatus::DONE_PROVIDED) {
                //We wait an additional bit of time for provided content in case clients
                if (difTime.dbl() > (interestBroadcastTimeout*(maxAttempts + 1))) {
                    delete[] curCon->chunkStatusList;
                    //curCon->requestPrefix.clear();
                    it = connectionList.erase(it);
                    cancelTimer(curCon);
                    delete(curCon);
                } else {
                    it++;
                }
            } else {
                delete[] curCon->chunkStatusList;
                //curCon->requestPrefix.clear();
                it = connectionList.erase(it);
                cancelTimer(curCon);
                delete(curCon);
            }

        } else {
            //Checking messages that don't have a "DONE" status and that have been in our list for a long time for leftovers, forcing a status update on them
            if (difTime.dbl() >= (interestBroadcastTimeout*(maxAttempts + 1)) ) {
                switch(curCon->connectionStatus) {
                    case ConnectionStatus::WAITING_FOR_ACCEPT:
                        //TODO: (REVIEW) Review code so that this doesn't really happen anymore (Fix a broken cancel timer somewhere)
                        curCon->connectionStatus = ConnectionStatus::DONE_NO_CLIENT_REPLY;
                        it++;
                        break;

                    case ConnectionStatus::ERROR:
                        if (curCon->chunkStatusList != NULL) delete[] curCon->chunkStatusList;
                        //curCon->requestPrefix.clear();
                        it = connectionList.erase(it);
                        delete(curCon);
                        break;

                    default:
                        //std::cerr << "(SM) Error: <" << myId << "> Stale Request <" << con->requestID << ">\t Peer <" << con->peerID << ">\t has Untreated Status <" << con->connectionStatus << ">\t Attempts <" << con->attempts << ">\t difTime <" << floor(difTime) << ">\n";
                        //std::cerr.flush();
                        curCon->connectionStatus = ConnectionStatus::ERROR;
                        startTimer(curCon);
                        it++;
                        break;
                }
            } else {
                it++;
            }
        }
    }
    //std::cout << "(SM) Exit cleanConnections\n";
    //std::cout.flush();
}

//=============================================================
// FANCY DATA FUNCTIONS
//=============================================================

//Function called to update the Debug Circle UI of objects
void ServiceManager::updateNodeColor() {
    //std::cout << "(SM) Enter updateNodeColor\n";
    //std::cout.flush();

    bool providingContent = false;      //Red
    bool requestingContent = false;     //Green
    bool waitingForContent = false;     //Blue


    for (auto it = connectionList.begin(); it != connectionList.end(); it++) {
        //Comparing Request ID
        if (ConnectionStatus::WAITING_MIN < (*it)->connectionStatus && (*it)->connectionStatus < ConnectionStatus::WAITING_MAX) {
            waitingForContent = true;
        } else if (ConnectionStatus::TRANSFER_MIN < (*it)->connectionStatus && (*it)->connectionStatus < ConnectionStatus::TRANSFER_MAX) {
            providingContent = true;
        } else if (ConnectionStatus::RECEIVE_MIN < (*it)->connectionStatus && (*it)->connectionStatus < ConnectionStatus::RECEIVE_MAX) {
            requestingContent = true;
        }
    }

    int sequenceNumber = 0;
    if (providingContent)
        sequenceNumber += 4;
    if (requestingContent)
        sequenceNumber += 2;
    //if (waitingForContent)
    //    sequenceNumber += 1;

    std::string radius = "20";
    if (waitingForContent) radius = "40";

    std::string color = "r=5,black";

    //USING RGB Binary reference list:
    //Providing | Receiving | Waiting
    switch (sequenceNumber) {
    //000   -   IDLE
    case 0:
        color = ("gray");
        break;
        //001   -   WAITING
    case 1:
        color = ("blue");
        break;
        //010   -   RECEIVING
    case 2:
        color = ("green");
        break;
        //011   -   WAITING + RECEIVING
    case 3:
        color = ("cyan");
        break;
        //100   -   SENDING
    case 4:
        color = ("red");
        break;
        //101   -   SENDING + WAITING
    case 5:
        color = ("magenta");
        break;
        //110   -   SENDING + RECEIVING
    case 6:
        color = ("yellow");
        break;
        //111   -   ALL
    case 7:
        color = ("white");
        break;
    }
    //Updating String Text for DisplayObject
    color = "r=" + radius + "," + color;
    findHost()->getDisplayString().updateWith(color.c_str());

    //std::cout << "(SM) Exit updateNodeColor\n";
    //std::cout.flush();
}

//Starts a new timer for the given message ID (assumed to be in connection list) with 1 second default delay
void ServiceManager::startTimer(Connection_t* connection) {
    startTimer(connection, interestBroadcastTimeout); //Start a timer with a 1 second delay
}

//Starts a new timer for the given message ID (assumed to be in connection list) and specific delay
void ServiceManager::startTimer(Connection_t* connection, double time) {
    //std::cout << "(SM) Enter startTimer\n";
    //std::cout.flush();

    if (cancelTimer(connection) == true) {
        //EV_WARN << "(SM) Warning: Canceled a timer to start a timer.\n";
        //EV_WARN.flush();
    } else {
        //EV_WARN << "(SM) Don't worry about the cancelation message above. We good boys.\n";
        //EV_WARN.flush();
    }

    cMessage* messageTimer = new WaveShortMessage(MessageClass::SELF_TIMER.c_str());

    //ID for Connection
    cMsgPar* idParameter = new cMsgPar(MessageParameter::CONNECTION_ID.c_str());
    idParameter->setLongValue(connection->requestID);
    messageTimer->addPar(idParameter);

    //ID for Peer
    cMsgPar* peerParameter = new cMsgPar(MessageParameter::PEER_ID.c_str());
    peerParameter->setLongValue(connection->peerID);
    messageTimer->addPar(peerParameter);

    simtime_t timerTime = simTime() + time;
    cancelMessageTimerVector.push_back(messageTimer);
    scheduleAt(timerTime, messageTimer);

    //std::cout << "(SM) Exit startTimer\n";
    //std::cout.flush();
}

//Cancel/Update timer for connection timeouts
bool ServiceManager::cancelTimer(Connection_t* connection) {
    //std::cout << "(SM) Enter cancelTimer\n";
    //std::cout.flush();

    if (connection == NULL) {
        return false;
    }

    if (cancelMessageTimerVector.size() != 0) {
        for (auto it = cancelMessageTimerVector.begin() ; it != cancelMessageTimerVector.end();) {
            cMessage* timerMessage = (*it);
            cArray* parArray = timerMessage->getParListPtr();
            //Checking if our message exists
            if (timerMessage == NULL || timerMessage->isSelfMessage() == false  || timerMessage->isScheduled() == false || timerMessage->getArrivalModuleId() != getId()) {
                //If the timer object is listed inside the timer vector we delete it
                if (timerMessage != NULL) {
                    it = cancelMessageTimerVector.erase(it);
                    delete(timerMessage);
                } else {
                    it++;
                }
            } else {
                bool gotParameterException = false;
                try {

                    if (parArray != NULL) {
                        bool hasPeerID = timerMessage->hasPar(MessageParameter::PEER_ID.c_str());
                        bool hasConID = timerMessage->hasPar( MessageParameter::CONNECTION_ID.c_str());
                        if (!hasConID || !hasPeerID) {
                            gotParameterException = true;
                        }
                    } else {
                        gotParameterException = true;
                    }
                } catch (int e) {
                    gotParameterException = true;
                    std::cout << "(SM) <" << myId << "> Parameter Exception. Number <" << e << ">.\n";
                    std::cout.flush();
                }
                if (!gotParameterException) {
                    cMsgPar requestID = timerMessage->par(MessageParameter::CONNECTION_ID.c_str());    //static_cast<cMsgPar*>(parArray->get( MessageParameter::CONNECTION_ID.c_str()));
                    cMsgPar requestPeer = timerMessage->par(MessageParameter::PEER_ID.c_str());        //static_cast<cMsgPar*>(parArray->get(MessageParameter::PEER_ID.c_str()));
                    int ID = requestID.longValue();
                    int peerID = requestPeer.longValue();

                    if (ID == connection->requestID && peerID == connection->peerID) {
                        //Removing First just to make sure that we deleted everything
                        it = cancelMessageTimerVector.erase(it);
                        cancelAndDelete(timerMessage);


                        return true;
                    }  else {
                        it++;
                    }
                } else {
                    it++;
                }
            }
        }
    }

    if (stats->isTerminated() || !stats->allowedToRun()) {
        return true;
    }

    EV_WARN << "(SM) Warning: did not find the requested timer during live simulation section.\n";
    EV_WARN.flush();

    return false;
}

//Called after timer timeouts, manages
void ServiceManager::handleSelfTimer(WaveShortMessage* timerMessage) {
    //std::cout << "(SM) Enter handleSelfTimer\n";
    //std::cout.flush();

    //Pulling parameters from selfmessage to figure out what's going on
    cArray parArray = timerMessage->getParList();
    cMsgPar* requestID = static_cast<cMsgPar*>(parArray.get(MessageParameter::CONNECTION_ID.c_str()));
    cMsgPar* requestPeer = static_cast<cMsgPar*>(parArray.get(MessageParameter::PEER_ID.c_str()));

    Connection_t* connection = getConnection(requestID->longValue(), requestPeer->longValue());

    //Checking if the connection actually exists
    if (connection == NULL) {
        //If the connection relating to it does not exist we will manually have to remove the Timer from our List
        for (auto it = cancelMessageTimerVector.begin() ; it != cancelMessageTimerVector.end();it++) {
            if ((*it)->getId() == timerMessage->getId()) {
                cancelMessageTimerVector.erase(it);
                break;
            }
        }

        if (timerMessage->isScheduled()) {
            cancelAndDelete(timerMessage);
        } else {
            delete(timerMessage);   //Can't cancel
        }
        return;
    }

    //Now that we're dealing with it... let's cancel that timer
    //cancelTimer(connection);

    if (connection->attempts >= maxAttempts) {
        //TODO: (DECIDE) Will we cancel our thing after this number of attempts? Will we just ignore this timer?

        //Checking if this is a server-sided or client-sided connection. If it's a client-sided connection then we gotta reset some stuff
        Interest_t* pendingInterest = getInterest(connection->requestPrefix);

        if (pendingInterest == NULL) {
            if (connection->peerID == myId) {

                //Notifying Client of Unavailable Stuff
                connection->connectionStatus = ConnectionStatus::DONE_UNAVAILABLE;

                //Checking if we got part of the content
                int requestSize = (int) ceil(connection->requestedContent->contentSize / (double) dataLengthBits);

                if (connection->chunkStatusList != NULL) {
                    for (int i = 0; i < requestSize ; i++) {
                       if (connection->chunkStatusList[i] == false) {
                           connection->connectionStatus = ConnectionStatus::DONE_PARTIAL;
                           break;
                       }
                    }
                }

                WaveShortMessage* wsm = getGenericMessage(connection);
                wsm->setKind(connection->connectionStatus);
                notifyOfContentAvailability(wsm,connection);

                cancelTimer(connection);
            } else {
                //std::cout << "(SM) <" << myId << "> Missing Interest Connection to <" << connection->peerID << "> went stale with status <" << connection->connectionStatus << ">.\n";
                //std::cout.flush();
                //Connection to client went stale, and since client has not replied, we're closing down this connection
                connection->connectionStatus = ConnectionStatus::DONE_NO_CLIENT_REPLY;
                cancelTimer(connection);
            }
        } else if (pendingInterest->providingConnection != NULL) {
            //std::cerr << "(SM) <" << myId << "> THE DESIGNATED SERVER FOR US (CLIENT) WENT STALE AND WE NEED TO DEAL WITH THIS!\n";
            //std::cerr.flush();

            //if (connection->peerID == myId) {
            //    std::cout << "(A providing connection for our local interest is gone.\n";
            //    std::cout.flush();
            //}

            //Cleaning potential server and trying again
            pendingInterest->providingConnection = NULL;
            connection->connectionStatus = ConnectionStatus::WAITING_FOR_NETWORK;
            connection->attempts = 1;

            //Attempting new content request forward
            WaveShortMessage* wsm = getGenericMessage(connection);
            forwardContentSearch(wsm,connection);
        } else {
            if (connection->peerID == myId) {
                connection->connectionStatus = ConnectionStatus::DONE_UNAVAILABLE;

                //Checking if we got part of the content
                int requestSize = (int) ceil(connection->requestedContent->contentSize / (double) dataLengthBits);
                if (connection->chunkStatusList != NULL) {
                    for (int i = 0; i < requestSize ; i++) {
                       if (connection->chunkStatusList[i] == false) {
                           connection->connectionStatus = ConnectionStatus::DONE_PARTIAL;
                           break;
                       }
                    }
                }

                WaveShortMessage* wsm = getGenericMessage(connection);
                wsm->setKind(connection->connectionStatus);
                notifyOfContentAvailability(wsm,connection);
                cancelTimer(connection);

                //std::cout << "(SM) Removing Connection <" << connection->requestID << "> with peer <" << connection->peerID << "> and status <" << connection->chunkStatusList << ">\n";

                //Checking if we're the only person looking for this interest (If we're the only ones, the interest becomes empty :D
                removeFromInterest(connection->requestPrefix,connection->peerID);

            } else {
                //Updating connection status and carrying on... not my problem anymore
                connection->connectionStatus = ConnectionStatus::DONE_UNAVAILABLE;
                removeFromInterest(connection->requestedContent->contentPrefix,connection->peerID);

                //Outsourced network communications
                cancelTimer(connection);
            }
        }

    } else {
        //std::cout << "(SM) <" << myId << "> something timed out. Time: <" << simTime() << "> Attempts: <" << connection->attempts << ">\n";
        //std::cout.flush();
        connection->attempts++;

        switch (connection->connectionStatus) {
            case ConnectionStatus::WAITING_FOR_NETWORK:
                {
                    //Sending Message once again in hopes of receiving a response
                    WaveShortMessage* wsm = getGenericMessage(connection);
                    forwardContentSearch(wsm,connection);
                }
                break;

            case ConnectionStatus::WAITING_FOR_ACCEPT:
                {
                    //This technically will never exist so we don't have to worry about local node stuff at this point
                    if (connection->peerID == myId) {
                        std::cerr << "(SM) Error: <" << myId << "> has local request with server status for remote node.\n";
                        std::cerr.flush();
                    }

                    //Sending Message once again in hopes of receiving a response
                    WaveShortMessage* wsm = getGenericMessage(connection);
                    //connection->connectionStatus = ConnectionStatus::DONE_RECEIVED;
                    //I already have an attempts++ as a general case
                    //connection->attempts++;
                    wsm->setKind(ConnectionStatus::DONE_AVAILABLE);
                    notifyOfContentAvailability(wsm,connection);
                    cancelTimer(connection);
                }
                break;

            case ConnectionStatus::RECEIVE_CLIENT: {
                    //Checking if all we're missing is the Completion Chunk (Why do we even have it again?
                    int requestSize = (int) ceil(connection->requestedContent->contentSize / (double) dataLengthBits);
                    if (connection->obtainedSize == requestSize) {
                        completeRemoteDataTransfer(connection);
                        cancelTimer(connection);
                    } else {
                        //Manually checking if chunks are missing
                        bool foundMissingChunk = false;
                        for (int i = 0; i < requestSize ; i++) {
                            if (connection->chunkStatusList[i] == false) {
                                //std::cout << "(SM) Missing Chunk is <" << i << ">\n";
                                //std::cout.flush();
                                foundMissingChunk = true;
                                requestChunkRetransmission(connection);
                                break;
                            }
                        }
                        if (!foundMissingChunk) {
                            //connection->connectionStatus == ConnectionStatus::DONE_RECEIVED;
                            completeRemoteDataTransfer(connection);
                            cancelTimer(connection);
                        }
                    }
                }
                break;

            case ConnectionStatus::WAITING_FOR_CONTENT:
                {

                    bool foundMissingChunk = false;
                    int requestSize = (int) ceil(connection->requestedContent->contentSize / (double) dataLengthBits);
                    for (int i = 0; i < requestSize ; i++) {
                        if (connection->chunkStatusList[i] == false) {
                            //std::cout << "(SM) Missing Chunk is <" << i << ">\n";
                            //std::cout.flush();
                            foundMissingChunk = true;
                            requestChunkRetransmission(connection);
                            break;
                        }
                    }

                    if (!foundMissingChunk) {
                        WaveShortMessage* wsm = getGenericMessage(connection);
                        wsm->setKind(ConnectionStatus::DONE_AVAILABLE);
                        notifyOfContentAvailability(wsm,connection);
                    }
                }
                break;

            //THIS CASE WILL NEVER HAPPEN, IT'S INSTANTANEOUS AND TRANSIENT
            //case ConnectionStatus::TRANSFER_TRANSFERING:
            //    //Sending Message once again in hopes of receiving a response
            //    //We might run into trouble at this stage, as this might be more complex than ALWAYS dealing with chunk retransmission... but we'll see
            //    //Yep, we fucked up. :D
            //    //requestChunkRetransmission(connection);
            //    std::cerr << "(SM) <" << myId << "> What do we do now? Our connection to <" << connection->peerID << "> has status <" << connection->connectionStatus << ">. Simtime: <" << simTime() << ">\n";
            //    std::cerr.flush();
            //    break;

            case ConnectionStatus::TRANSFER_WAITING_ACK:
            case ConnectionStatus::DONE_NO_CLIENT_REPLY:
                //TODO: (DECIDE) if no client reply means client is satisfied with content provided, currently we assume so and if both states can have the same action
                connection->connectionStatus = ConnectionStatus::DONE_PROVIDED;
                runCachePolicy(connection);
                cancelTimer(connection);
                break;

            case ConnectionStatus::DONE_PROVIDED:
                //std::cerr << "(SM) <" << myId << "> We should not really get to this stage but whatever.\n";
                cancelTimer(connection);
                break;

            case ConnectionStatus::DONE_REJECTED:
                cancelTimer(connection);
                break;


            case ConnectionStatus::IDLE:
                //If our connection idled and we're the local client let's retry networking it.
                //The next timer event should fix this.
                if (connection->peerID == myId) {
                    connection->connectionStatus = ConnectionStatus::WAITING_FOR_NETWORK;
                    WaveShortMessage* wsm = getGenericMessage(connection);
                    forwardContentSearch(wsm,connection);
                    //startTimer(connection);       //This has become redundant
                } else {
                    //If we're the server who has dealt with a connection that has looped back to idle due to errors or X, we'll just delete it.
                    connection->connectionStatus = ConnectionStatus::DONE_NO_CLIENT_REPLY;
                    removeFromInterest(connection->requestPrefix,connection->peerID);
                    cancelTimer(connection);
                }
                break;

            case ConnectionStatus::ERROR:
                //We literally don't know what is going on so let's become idle
                connection->connectionStatus = ConnectionStatus::IDLE;
                break;

            default:
                std::cerr << "(SM) <" << myId << "> Error: Self Timer has untreated state: <" << connection->connectionStatus << ">\n";
                std::cerr.flush();

                connection->connectionStatus = ConnectionStatus::ERROR;
                //cancelTimer(connection);
                break;
                //Note: I added this return so we don't clear stuff in the case of connection errors?
                //return;
        }
    }
    cleanConnections();
}

//Function used to generate a basic outgoing message from a connection
WaveShortMessage* ServiceManager::getGenericMessage(Connection_t* connection) {
    //std::cout << "(SM) Enter getGenericMessage\n";
    //std::cout.flush();

    //Preparing base message
    t_channel channel = dataOnSch ? type_SCH : type_CCH;
    WaveShortMessage * genericMessage = prepareWSM(MessageClass::DATA,dataLengthBits, channel, dataPriority, -1, 2);
    cMsgPar* contentNameParameter = new cMsgPar(MessageParameter::PREFIX.c_str());
    contentNameParameter->setStringValue(connection->requestPrefix.c_str());
    genericMessage->addPar(contentNameParameter);

    //Adding Connection ID
    cMsgPar* connectionParameter = new cMsgPar(MessageParameter::CONNECTION_ID.c_str());
    connectionParameter->setLongValue(connection->requestID);
    genericMessage->addPar(connectionParameter);

    //Adding Downstream Cache Distance Hop Count
    cMsgPar* hopDistanceParameter = new cMsgPar(MessageParameter::HOPS_LAST_CACHE.c_str());
    hopDistanceParameter->setLongValue(connection->downstreamCacheDistance);
    genericMessage->addPar(hopDistanceParameter);

    //Adding Downstream Hop Count
    cMsgPar* distanceParameter = new cMsgPar(MessageParameter::HOPS_DOWN.c_str());
    distanceParameter->setLongValue(connection->downstreamHopCount);
    genericMessage->addPar(distanceParameter);

    //Adding Upstream Hop Count
    cMsgPar* distanceBackParameter = new cMsgPar(MessageParameter::HOPS_UP.c_str());
    distanceBackParameter->setLongValue(connection->upstreamHopCount);
    genericMessage->addPar(distanceBackParameter);

    //Adding Local Use Count Value
    cMsgPar* useCountParameter = new cMsgPar(MessageParameter::REQUESTS_AT_SOURCE.c_str());
    //If the message regards a piece of content originating locally, we get the value from our cache, otherwise we inherit
    if (connection->downstreamHopCount == 0) {
        useCountParameter->setLongValue(cache->getUseCount(connection->requestPrefix));
    } else {
        useCountParameter->setLongValue(connection->remoteHopUseCount);
    }
    genericMessage->addPar(useCountParameter);

    //Adding other base parameters
    genericMessage->setSenderAddress(myId);
    genericMessage->setSenderPos(traci->getCurrentPosition());
    genericMessage->setRecipientAddress(connection->peerID);
    std::string wsmData = "<" + to_string(connection->requestID) +
            ";" + to_string(connection->peerID) +
            ";" + to_string(connection->downstreamHopCount) +
            ";" + to_string(connection->upstreamHopCount) +
            ";" + to_string(connection->downstreamCacheDistance) +
            ";" + to_string(connection->attempts) +
            ";" + to_string(connection->remoteHopUseCount) + ">";
    genericMessage->setWsmData(wsmData.c_str());

    return genericMessage;
}

//=============================================================
// INTERFACE CONTENT HANDLING FUNCTION (ON MESSAGES RECEIVED FOR EACH LINK TYPE)
//=============================================================

//Function called whenever we get a message coming from the network interfaces
void ServiceManager::onNetworkMessage(WaveShortMessage* wsm) {
    //std::cout << "(SM) Enter onNetworkMessage\n";
    //std::cout.flush();

    //Checking if we are the recipient
    if (wsm->getRecipientAddress() != -1 && wsm->getRecipientAddress() != myId) {
        //TODO: FINISH IMPLEMENTING PACKET OVERHEARING!!!
        /*
        //Checking if this is a data object
        if (strcmp(wsm->getName(), MessageClass::DATA.c_str()) == 0) {
            std::string prefixString = static_cast<cMsgPar*>(wsm->getParList().get(MessageParameter::PREFIX.c_str()))->str();
            if (prefixString.c_str()[0] == '\"') {
                prefixString = prefixString.substr(1, prefixString.length() - 2);
            }
            prefixString.erase(std::remove(prefixString.begin(), prefixString.end(), '\"'), prefixString.end());
            for(auto it = PIT.begin(); it != PIT.end();it++) {
                if ((*it)->interestPrefix.compare(prefixString) == 0) {
                    std::cout << "(SM) <" << myId << "> We are overhearing a data object for something we want!\n";
                    std::cout.flush();
                    break;
                }
                //else {
                //    std::cout << "(SM) <" << myId << "> Nope <" << prefixString << "> != <" << (*it)->interestPrefix << "> !\n";
                //    std::cout.flush();
                //}
            }
            //cMsgPar* requestID = static_cast<cMsgPar*>(parArray.get(MessageParameter::CONNECTION_ID.c_str()));
        }
        */
        delete(wsm);
        return;
    }

    //Getting Message Parameters
    cArray parArray = wsm->getParList();
    cMsgPar* requestID = static_cast<cMsgPar*>(parArray.get(MessageParameter::CONNECTION_ID.c_str()));

    //Checking if we have the GPS Cache subsystem enabled
    if (cache->hasGPSCache() == true) {
        //Checking if its a beacon or some other message type specific to direct communication
        if ( (strcmp(wsm->getName(), MessageClass::BEACON.c_str()) != 0) && (strcmp(wsm->getName(), MessageClass::GPS_BEACON.c_str()) != 0) ){
            //Checking if we are already tracking the connection
            if (checkForConnections(requestID->longValue()) == false) {
                cMsgPar* requestPrefix = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));
                std::string prefixValue = requestPrefix->stringValue();

                //Checking if this type of content object is location-dependent
                Content_t* contentObject = library->getContent(prefixValue);
                if (contentObject->contentClass == ContentClass::TRAFFIC) {
                    //std::cout << "[" << myId << "] (SM) Passing on information to Content Store on Connection ID <" << requestID->longValue() << "> from message of type <" << wsm->getName() << ">.\n";
                    //Notify Content Store of relevant request frequency statistics
                    cache->logOverheardGPSMessage(contentObject);
                } else {
                    //std::cout << "(SM) Content belongs to a class we don't care about.\n";
                }
            } else {
                //std::cout << "(SM) We already know about this communication.\n";
            }
        } else {
            //TODO: Treat BEACON & GPS BEACON Messages for sharing of GPS INFO
        }
    }

    //Checking Message Type
    if (strcmp(wsm->getName(), MessageClass::INTEREST.c_str()) == 0) {
        handleInterestMessage(wsm);
    } else if (strcmp(wsm->getName(), MessageClass::INTEREST_REPLY.c_str()) == 0) {
        handleInterestReplyMessage(wsm);
    } else if (strcmp(wsm->getName(), MessageClass::INTEREST_CANCEL.c_str()) == 0) {
        handleInterestRejectMessage(wsm);
    } else if (strcmp(wsm->getName(), MessageClass::INTEREST_ACCEPT.c_str()) == 0) {
        handleInterestAcceptMessage(wsm);
    } else if (strcmp(wsm->getName(), MessageClass::DATA_MISSING.c_str()) == 0) {
        handleInterestAcceptMessage(wsm);
    } else if (strcmp(wsm->getName(), MessageClass::DATA.c_str()) == 0) {
        handleContentMessage(wsm);
    } else if (strcmp(wsm->getName(), MessageClass::BEACON.c_str()) == 0) {
        onBeacon(wsm);
    } else if (strcmp(wsm->getName(), MessageClass::GPS_BEACON.c_str()) == 0) {
        cache->handleGPSPopularityMessage(wsm);
        delete(wsm);
    } else {
        std::cerr << "(SM) Unknown Message Type <" << wsm->getName() << ">\n";
        std::cerr.flush();
        delete(wsm);
    }
}

//
void ServiceManager::handleInterestRejectMessage(WaveShortMessage* wsm) {
    //std::cout << "(SM) Enter handleInterestRejectMessage\n";
    //std::cout.flush();

    //Getting Basic Parameters used for evaluation
    cArray parArray = wsm->getParList();
    cMsgPar* requestID = static_cast<cMsgPar*>(parArray.get(MessageParameter::CONNECTION_ID.c_str()));

    //Checking if we already have a downstream connection for this interest (we should)
    Connection_t* downstreamConnection = getConnection(requestID->longValue(),wsm->getSenderAddress());

    if (downstreamConnection == NULL) {
        cancelTimer(downstreamConnection);
        delete(wsm);
        return;
    }

    //Marking that the node does not want this content anymore
    downstreamConnection->connectionStatus = ConnectionStatus::DONE_REJECTED;

    //Canceling any related timers
    cancelTimer(downstreamConnection);
    delete(wsm);
}

//
void ServiceManager::handleInterestAcceptMessage(WaveShortMessage* wsm) {
    //std::cout << "(SM) Enter handleInterestAcceptMessage\n";
    //std::cout.flush();

    //Checking Message Type
    if (strcmp(wsm->getName(), MessageClass::INTEREST_ACCEPT.c_str()) != 0 &&
        strcmp(wsm->getName(), MessageClass::DATA_MISSING.c_str()) != 0) {
        std::cerr << "(SM) This is not an Accept!\n";
        std::cerr.flush();
        delete(wsm);
        return;
    }

    //Getting Basic Parameters used for evaluation
    cArray parArray = wsm->getParList();
    cMsgPar* requestID = static_cast<cMsgPar*>(parArray.get(MessageParameter::CONNECTION_ID.c_str()));
    cMsgPar* requestPrefix = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));
    cMsgPar* requestChunks = static_cast<cMsgPar*>(parArray.get(MessageParameter::SEQUENCE_NUMBER.c_str()));
    int idValue = requestID->longValue();
    std::string prefixValue = requestPrefix->stringValue();

    // :/
    prefixValue = library->cleanString(prefixValue);

    //Checking if we already have a downstream connection for this interest (we should)
    Connection_t* downstreamConnection = getConnection(idValue,wsm->getSenderAddress());
    if (downstreamConnection == NULL){

       //Checking if we have this piece of content. o.õ
       if (cache->checkIfAvailable(prefixValue,idValue) == true) {
           //Nodes should not be accepting connections that have not been created. This sounds bad.
           delete(wsm);
           return;
       } else {
           Interest_t* interest = getInterest(prefixValue);

           //Checking if there is already an impending interest regarding this content.. if so we just ignore
           if (interest != NULL) {
               //TODO: (DECIDE) It seems that at this point we've already tried connecting to this client but our connection exceeded attempts and we gave up.
               //However our client is still trying. Decide if we want to proceed or ignore
               delete(wsm);
               return;
           } else {
               //Let's treat this as a regular content request?
               wsm->setName(MessageClass::INTEREST.c_str());
               handleInterestMessage(wsm);
               return;
           }
       }

    }

    //Canceling potentially existing timer to continue communication process
    cancelTimer(downstreamConnection);

    //Checking if we're on a data missing scenario and "modifying" the message in that sense
    if (downstreamConnection->connectionStatus == ConnectionStatus::DONE_PROVIDED
        || downstreamConnection->connectionStatus == ConnectionStatus::TRANSFER_WAITING_ACK) {
        //We don't actually have to set the name to interest_accept, it's what it should actually be
        //wsm->setName(MessageClass::INTEREST_ACCEPT.c_str());
        downstreamConnection->connectionStatus = ConnectionStatus::WAITING_FOR_ACCEPT;
        downstreamConnection->attempts++;
        startTimer(downstreamConnection);
    } else if (downstreamConnection->connectionStatus == ConnectionStatus::WAITING_FOR_ACCEPT) {
        //If we're on a fresh connection let's reset our attempt counter
        downstreamConnection->attempts = 1;
        startTimer(downstreamConnection);
    }  else {
        //We are on some other sort of fresh connection but i'm not sure whats up
        downstreamConnection->connectionStatus = ConnectionStatus::WAITING_FOR_ACCEPT;
        downstreamConnection->attempts = 1;
        startTimer(downstreamConnection);
    }

    //We've been getting messages while in states other than WAITING_FOR_ACCEPT, weird. Seems like duplicates are incoming from time to time
    if (downstreamConnection->connectionStatus != ConnectionStatus::WAITING_FOR_ACCEPT) {
        if (downstreamConnection->connectionStatus == ConnectionStatus::TRANSFER_FORWARDING) {
            std::cout << "(SM) <" << myId << "> Yep, we're forwarding <" << downstreamConnection->requestID << "> to <" << downstreamConnection->peerID << ">. Forwarding gooood. Time: <" << simTime() << ">\n";
            std::cout.flush();
        } else {
            delete(wsm);
            return;
        }
    }
    std::vector<int>* chunkVector = NULL;

    //Checking if we have a chunk parameter (If we don't we assume the node wants ALL the chunks)
    if (requestChunks != NULL){
        //Figuring out the chunk list size and contents
        std::string chunkString = requestChunks->stringValue();

        //Checking if there is an explicit list of chunks that need retransmission
        if (chunkString.compare("-1") != 0) {
            size_t pos = 0;
            std::string token;
            std::string delimiter = ";";

            //Counting Dividers
            size_t n = std::count(chunkString.begin(), chunkString.end(), ';');

            int curIndex = 0;
            chunkVector = new std::vector<int>(n);

            if (chunkVector->size() == 0) {
                std::cerr << "(SM) Chunk Vector Generated has size 0 because of this weird ass string: <" << chunkString << ">\n";
                std::cerr.flush();
            }

            while ((pos = chunkString.find(delimiter)) != std::string::npos) {
                token = chunkString.substr(0, pos);
                if (token.length() != 0 && token.compare("") != 0) {
                   //std::cout << "" << token << "\t";

                   (*chunkVector)[curIndex] = atoi(token.c_str());
                   chunkString.erase(0, pos + delimiter.length());
                   curIndex++;
                }
            }

       }
    }
    //std::cout << "(SM) <" << myId << "> starting a new transfer to <" << downstreamConnection->peerID << "> with up/down: <" << downstreamConnection->upstreamHopCount << ";" << downstreamConnection->downstreamHopCount << ">\n";
    //std::cout.flush();

    //The Assumption that a client wants ALL the chunks means that they are just now accepting the network request
    //If our status was waiting for accept and we are in our first try, we count the increase use
    if (downstreamConnection->connectionStatus == ConnectionStatus::WAITING_FOR_ACCEPT && downstreamConnection->attempts <= 1) {
        //std::cout << "<" << myId << ">\t<" << downstreamConnection->attempts << ">\t<" << downstreamConnection->connectionStatus << ">\n";
        //std::cout.flush();

//        std::cout << "(SM) <" << myId << "> will provide <" << downstreamConnection->requestPrefix << "> to <" << downstreamConnection->peerID << "> Time <" << simTime() << ">.\n";
//        std::cout.flush();

        //This will only actually increase use count if object has been cached locally
        cache->increaseUseCount(downstreamConnection->requestPrefix);
    }

    transmitDataChunks(downstreamConnection,chunkVector);
    delete(wsm);  //We finally delete our base wsm object :P
}

//
void ServiceManager::handleInterestReplyMessage(WaveShortMessage* wsm) {
    //std::cout << "(SM) Enter handleInterestReplyMessage\n";
    //std::cout.flush();

    //Checking Message Type
    if (strcmp(wsm->getName(), MessageClass::INTEREST_REPLY.c_str()) != 0) {
        std::cerr << "(SM) <" << myId << ">  THIS <" << wsm->getName() << "> IS NOT AN INTEREST MESSAGE!\n";
        std::cerr.flush();

        delete(wsm);
        return;
    }

    //Getting Basic Parameters used for evaluation
    cArray parArray = wsm->getParList();
    cMsgPar* requestID = static_cast<cMsgPar*>(parArray.get(MessageParameter::CONNECTION_ID.c_str()));
    cMsgPar* requestPrefix = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));
    int idValue = requestID->longValue();
    std::string prefixValue = requestPrefix->stringValue();

    //std::cout << "(SM) <" << myId << "> is handling an Interest Reply from <" << wsm->getSenderAddress() << "> for connection <" << idValue << ">. Time: <" << simTime() << ">\n";
    //std::cout.flush();

    //if (myId == 10) {
    //    std::cout << "\t(SM) Look for a selftimer!\n";
    //    std::cout.flush();
    //}

    //Checking if we already have an upstream connection for this interest (Possibly because we heard a loop)
    Connection_t* upstreamConnection = getConnection(idValue,wsm->getSenderAddress());
    if (upstreamConnection != NULL){
       //Checking if this was a communication Cycle, if so fixing the broken stuff
       if (upstreamConnection->connectionStatus == ConnectionStatus::WAITING_FOR_NETWORK) {
           //This is redundant but whatever, we're cleaning the connection object
           upstreamConnection->connectionStatus = ConnectionStatus::WAITING_FOR_NETWORK;

           upstreamConnection->lostSize = 0;
           upstreamConnection->obtainedSize = 0;

           //If we're on a fresh connection let's reset our attempt counter
           upstreamConnection->attempts = 1;

           int requestSize = (int) ceil(upstreamConnection->requestedContent->contentSize / (double) dataLengthBits);
           if (upstreamConnection->chunkStatusList == NULL) {
               upstreamConnection->chunkStatusList = new bool[requestSize];
               for (int i = 0; i < requestSize; i++) {
                   upstreamConnection->chunkStatusList[i] = false;
               };
           }
           //Not sure why this had CANCEL TIMER and not START TIMER, fixed.
           //cancelTimer(upstreamConnection);
           startTimer(upstreamConnection);

       } else if (upstreamConnection->connectionStatus == ConnectionStatus::WAITING_FOR_CONTENT) {
           //In this case, our peer node did not receive our message!
           //std::cout << "A? <" << myId << ">\n";
           //std::cout.flush();
           acceptNetworkrequest(wsm,upstreamConnection);
           return;

       } else if (upstreamConnection->connectionStatus == ConnectionStatus::DONE_REJECTED) {
           //In this case, our peer node did not receive our message!
           rejectNetworkrequest(wsm,upstreamConnection);
           return;

       } else if (upstreamConnection->connectionStatus == ConnectionStatus::WAITING_FOR_ACCEPT) {
           //Relationship between client and server has inverted. Requires investigation
           //std::cerr << "(SM) <" << myId << "> Upstream Connection <" << upstreamConnection->requestID << "> to <" << upstreamConnection->peerID << "> regarding <" << upstreamConnection->requestedContent->contentPrefix << "> already exists. Current State is <" << upstreamConnection->connectionStatus << ">!\n";
           //std::cerr << "\t Interest? <" << getInterest(upstreamConnection->requestedContent->contentPrefix) << "> Cache: <" << cache->handleLookup(upstreamConnection->requestedContent->contentPrefix) << ">\n";
           //std::cerr.flush();
           rejectNetworkrequest(wsm,upstreamConnection);
           return;
       } else {
           //std::cerr << "(SM) <" << myId << "> Upstream Connection <" << upstreamConnection->requestID << "> to <" << upstreamConnection->peerID << "> regarding <" << upstreamConnection->requestedContent->contentPrefix << "> already exists. Current State is <" << upstreamConnection->connectionStatus << ">!\n";
           //std::cerr.flush();

           delete(wsm);
           return;
       }
    } else {
        //If a connection does not exist, we'll create it
        upstreamConnection = createClientSidedConnection(wsm);
        upstreamConnection->attempts = 1;
        upstreamConnection->connectionStatus = ConnectionStatus::WAITING_FOR_CONTENT;
    }

    //Checking if we already have an interest for the given prefix
    Interest_t* interest = getInterest(prefixValue);
    if (interest == NULL) {
        //std::cerr << "(SM) <" << myId << "> We don't have an interest for <" << prefixValue << "> but we technically have a connection on it?.\n";
        //std::cerr.flush();
        //If we don't have an interest it is our assumption that we obtained this content from somewhere else, resolved our interest and are now chilling.
        rejectNetworkrequest(wsm,upstreamConnection);
        return;
    }

    //Checking if the content provider was previously registered as being interested in this content
    for (auto it = interest->pendingConnections.begin() ; it != interest->pendingConnections.end(); it++) {
        Connection_t* pendingConnection = getConnection(idValue,*it);
        if (pendingConnection != NULL) {
            if (pendingConnection->peerID == upstreamConnection->peerID) {
                //std::cout << "(SM) <" << myId << "> It seems our peer <" << pendingConnection->peerID << "> got the content object before us and wants to provide us with it. Rubbing it in, eh?\n";
                //std::cout.flush();
                //it = interest->pendingConnections.erase(it);
                removeFromInterest(interest->interestPrefix,upstreamConnection->peerID);
                //TODO (REVIEW) See if this causes shit to go haywire
                break;
            }

            //Checking if this was the last pending node interested in this content. If so, this connection is pointless
            if (interest->pendingConnections.size() == 0) {
                std::cerr << "(SM) <" << myId << "> Obtaining content from <" << pendingConnection->peerID << "> would be pointless.\n";
                std::cerr.flush();

                deleteInterest(prefixValue);
                rejectNetworkrequest(wsm,upstreamConnection);
                return;
            }
        } else {
            //If there is no actual connection regarding this pending interest. then whatever.
        }
    }


    //Checking if some other connection is already attempting to solve this content
    if (interest->providingConnection != NULL) {
        switch(interest->providingConnection->connectionStatus) {
            //Cases where we should Accept the content request
            case ConnectionStatus::ERROR:
            case ConnectionStatus::WAITING_FOR_NETWORK:
            case ConnectionStatus::DONE_PARTIAL:
            case ConnectionStatus::DONE_NO_DATA:
            case ConnectionStatus::DONE_UNAVAILABLE:
                //std::cout << "B?\n";
                //std::cout.flush();
                acceptNetworkrequest(wsm,upstreamConnection);
                return;
                break;

            //"Else", where Reject the content request
            default:
                rejectNetworkrequest(wsm,upstreamConnection);
                return;
                break;
        }
    } else {
        //std::cout << "C?\n";
        //std::cout.flush();
        acceptNetworkrequest(wsm,upstreamConnection);
        return;
    }

    delete(wsm);
}

//
void ServiceManager::handleInterestMessage(WaveShortMessage* wsm) {
    //std::cout << "(SM) Enter handleInterestMessage\n";
    //std::cout.flush();

    //Checking Message Type
    if (strcmp(wsm->getName(), MessageClass::INTEREST.c_str()) != 0) {
        std::cerr << "(SM) THIS IS NOT AN INTEREST MESSAGE!\n";
        std::cerr.flush();
        delete(wsm);
        return;
    }

    //Getting Basic Parameters used for evaluation
    cArray parArray = wsm->getParList();
    cMsgPar* requestID = static_cast<cMsgPar*>(parArray.get(MessageParameter::CONNECTION_ID.c_str()));
    cMsgPar* requestPrefix = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));
    int idValue = requestID->longValue();
    std::string prefixValue = requestPrefix->stringValue();

    //if (myId == 0) {
    //    std::cout << "(SM) <" << myId << "> We have the Desired Object <" << prefixValue << "> for connection <" << idValue << "> from <" << wsm->getSenderAddress() << "> time: <" << simTime() << ">\n";
    //    std::cout.flush();
    //}

    //If we have a priority/content restriction policy we check if it will be applied to this interest request
    if (priorityPolicy == AccessRestrictionPolicy::FORWARD_50 || priorityPolicy == AccessRestrictionPolicy::FORWARD_AND_DELAY ) {
        if (networkLoadStatus == NetworkLoadStatus::HIGH_LOAD) {
            switch(getClassFromPrefix(prefixValue)) {
                case ContentClass::MULTIMEDIA:
                    //DISCARD Low PRIORITY
                    //std::cout << "(SM) <" << myId << "> High Traffic Low Priority Absolute Discard.\n";
                    delete(wsm);
                    return;
                    break;

                case ContentClass::NETWORK:
                    //RESTRICT Medium PRIORITY (Probability)
                    if (uniform(0,1) > 0.5) {
                        //std::cout << "(SM) <" << myId << "> High Traffic Medium Priority Probabilistic Discard.\n";
                        delete(wsm);
                        return;
                    }
                    break;

                case ContentClass::TRAFFIC:
                    //KEEP High PRIORITY
                    break;

                default:
                    std::cerr << "(SM) Error: High Load Class Error.\n";
                    break;
            }
        } else if (networkLoadStatus == NetworkLoadStatus::MEDIUM_LOAD && getClassFromPrefix(prefixValue) == ContentClass::MULTIMEDIA) {
            if (uniform(0,1) > 0.5) {
                //std::cout << "(SM) <" << myId << "> Medium Traffic Low Priority Probabilistic Discard.\n";
                delete(wsm);
                return;
            }
        }
    }

    //Checking if we already have a downstream connection for this interest
    bool downstreamMessageAlreadyExisted = false;        //If a connection already existed we don't perform some actions like logging statistics
    Connection_t* downstreamConnection = getConnection(idValue,wsm->getSenderAddress());
    if (downstreamConnection != NULL){
        downstreamMessageAlreadyExisted = true;
        if (downstreamConnection->connectionStatus == ConnectionStatus::WAITING_FOR_NETWORK) {
           //Ignoring a re-broadcast?
           //if (cache->handleLookup(downstreamConnection->requestedContent->contentPrefix)) {
           //    std::cerr << "(SM) This request should LITERALLY have been fulfilled earlier. WTF.\n";
           //    std::cerr.flush();
           //}
            //I decided I should restart the timer as we've again become aware that this node is asking for stuff (timeout refresh)
            startTimer(downstreamConnection);
        } else if (downstreamConnection->connectionStatus == ConnectionStatus::WAITING_FOR_ACCEPT || downstreamConnection->connectionStatus == ConnectionStatus::TRANSFER_WAITING_ACK || downstreamConnection->connectionStatus == ConnectionStatus::DONE_PROVIDED ) {
            //If we've tried communicating or even providing content to this node and things went south to the point where he gave up and tried again we're probably dealing with significant collisions, but sure, let's give this another try
            //std::cout << "(SM) We're probably dealing with A LOT of noise for connection <" << idValue << "> to peer <" << wsm->getSenderAddress() << ">.\n";
            //std::cout.flush();
            downstreamConnection->connectionStatus = ConnectionStatus::IDLE;
            startTimer(downstreamConnection);
        } else if (downstreamConnection->connectionStatus == ConnectionStatus::WAITING_FOR_CONTENT || downstreamConnection->connectionStatus == ConnectionStatus::RECEIVE_CLIENT) {
           //If we're receiving a content but now get an interest from this node, we either assume that the node does not have the item or we just keep our state.
           //if we ignore that message we'll timeout and carry on normally.
           downstreamConnection->connectionStatus = ConnectionStatus::RECEIVE_CLIENT;
           delete(wsm);
           return;
        } else if (downstreamConnection->connectionStatus == ConnectionStatus::DONE_REJECTED) {
            //If the client rejected our connection but now wants it they might be dealing with noise on their end. Let's try to treat it
            //This is the last thing I changed in this part.. let's see what's up and maybe enable this piece of code
            //downstreamConnection->connectionStatus = ConnectionStatus::WAITING_FOR_NETWORK
        } else {
           //std::cerr << "\n\n(SM) <" << myId << "> Downstream Connection <" << downstreamConnection->requestID << "> with <" << downstreamConnection->peerID << "> already exists with wonky status <" << downstreamConnection->connectionStatus << ">!\n";
           //std::cerr.flush();
           downstreamConnection->connectionStatus = ConnectionStatus::ERROR;
           startTimer(downstreamConnection);
           delete(wsm);
           return;
        }
    } else {
        //Creating and setting parameters for a connection
        downstreamConnection = createServerSidedConnection(wsm);
        downstreamMessageAlreadyExisted = false;

        if (downstreamConnection == NULL) {
            std::cerr <<  "(SM) <" << myId << "> ERROR: Connection <" << idValue << "> with <" << wsm->getSenderAddress() << ">. Time: <" << simTime() << ">\n";
            std::cerr.flush();

            delete(wsm);
            return;
        }

        downstreamConnection->connectionStatus = ConnectionStatus::WAITING_FOR_NETWORK;
        startTimer(downstreamConnection);
    }

    //If we failed to create an interest a field was possibly missing so we'll ignore this connection
    if (downstreamConnection == NULL) {
        //std::cerr << "(SM) Could not create an interest.\n";
        //std::cerr.flush();

        delete(wsm);
        return;
    }

    //Logging Statistics for incoming duplicates for a given request
    if (downstreamMessageAlreadyExisted) {
        stats->logInterestForConnection(downstreamConnection->requestID);
    }

    //Checking if item is in our database
    if (cache->checkIfAvailable(prefixValue,idValue) == true) {

        //Checking to log statistics for a new connection
        if (!downstreamMessageAlreadyExisted) {
            if (downstreamConnection->peerID == myId) {
                stats->increaseLocalCacheHits();
            } else {
                //Having all items implies being a server, which we use to log server based statistics
                if (cache->getRole() == NodeRole::SERVER) {
                    stats->increaseServerCacheHits();
                } else {
                    stats->increaseRemoteCacheHits();
                }
            }
        }

        //Updating Connection Status to reflect that we have the content
        downstreamConnection->connectionStatus = ConnectionStatus::WAITING_FOR_ACCEPT;

        //If we're providing content locally we skip this and just set it as done
        if (downstreamConnection->peerID == myId) {
            //Marking Request as Available
            downstreamConnection->connectionStatus = ConnectionStatus::DONE_AVAILABLE;

            //As object is local we don't need to wait for network responses so we can cancel our timer
            cancelTimer(downstreamConnection);
        } else {
            startTimer(downstreamConnection);
        }

        //Notifying the client that we've got the content
        wsm->setKind(ConnectionStatus::DONE_AVAILABLE);
        notifyOfContentAvailability(wsm,downstreamConnection);
        cleanConnections();
        return;
    } else {
        //Checking if we already have an interest for the given prefix
        Interest_t* interest = getInterest(prefixValue);
        if (interest == NULL) {

            downstreamConnection->connectionStatus = ConnectionStatus::WAITING_FOR_NETWORK;
            //Creating new interest and forwarding it!
            bool result = createInterest(prefixValue, wsm->getSenderAddress());
            if (result == false) {
                std::cerr << "(SM) Error: Interest creation failed.\n";
                std::cerr.flush();
                startTimer(downstreamConnection);

                delete(wsm);
                return;
            }
            forwardContentSearch(wsm,downstreamConnection);

            //Loging statistics here (we have similar code below) so we can return and have the general case delete the WSM object
            if (downstreamConnection->peerID == myId) {
                stats->increaseLocalCacheMisses();
            } else {
                stats->increaseRemoteCacheMisses();
            }
            return;
        } else {
            //If we already have a pending interest there is not much we can do at this point but wait for a potential reply.
            //TODO: (DECIDE) Use the existing knowledge of previous interests (time since last request) to rebroadcast interests, possibly reset some timers?

            //We only start timers for interests we made changes to Old ones we have to let expire
            if (addToInterest(prefixValue, wsm->getSenderAddress()) == true) {
                //We start a timer because even if we have an interest the request might have come from a local interaface
                //This way, we need to have a way to notify our local client that the request has no response
                startTimer(downstreamConnection);
            }

            //Yeah, we're ignoring this as it was causing crazy weird interest related events for the same item messing everything up
            //if (result == false) {
            //    std::cerr << "(SM) Error: Interest Update failed.\n";
            //    std::cerr.flush();
            //    return;
            //}
            //std::cout << "(SM) <" << myId << "> It seems we already have an interest setup for <" << prefixValue << ">!\n";
            //std::cout.flush();
        }

        //Logging Cache Miss Statistics
        if (!downstreamMessageAlreadyExisted) {
            //Logging some Statistics
            if (downstreamConnection->peerID == myId) {
                stats->increaseLocalCacheMisses();
            } else {
                stats->increaseRemoteCacheMisses();
            }
        }
    }
    updateNodeColor();
    delete(wsm);
}

//Function called upon data delivery
void ServiceManager::handleContentMessage(WaveShortMessage* wsm) {
    //std::cout << "(SM) Enter handleContentMessage\n";
    //std::cout.flush();

    cArray parArray = wsm->getParList();
    cMsgPar* requestID = static_cast<cMsgPar*>(parArray.get(MessageParameter::CONNECTION_ID.c_str()));
    cMsgPar* requestSequenceNumber = static_cast<cMsgPar*>(parArray.get(MessageParameter::SEQUENCE_NUMBER.c_str()));
    long connectionID = requestID->longValue();
    long sequenceNumber = requestSequenceNumber->longValue();

    //Getting Connection
    Connection_t* connection = getConnection(connectionID,wsm->getSenderAddress());
    if (connection == NULL) {
        //std::cerr << "(SM) Error: <" << myId << "> Initiating remote data transfer for connection ID that does not exist <" << connectionID << ";" << wsm->getSenderAddress() << ">.\n";
        //std::cerr.flush();
        //This case means that we got the content, resolved all transmissions but something made it to our provider sent us content again...
        //This is kind of unnecessary but whatever
        delete (wsm);
        return;
    }

    //Canceling existing timer to proceed with communication
    cancelTimer(connection);

    //Checking if we're receiving outdated/duplicated contents for a connection we've already closed down
    if (connection->connectionStatus == ConnectionStatus::DONE_RECEIVED) {
        //std::cerr << "(SM) Warning: <" << myId << "> We're getting data on a complete connection. Maybe this is redundant? check timers!\n";
        //std::cerr.flush();

        delete (wsm);
        return;
    }

    //Checking for first chunk arrival
    //We include the IDLE state as pseudo-complete messages might have timer idled but should be fine, these are just duplicate chunks
    if (connection->connectionStatus != ConnectionStatus::WAITING_FOR_CONTENT &&
        connection->connectionStatus != ConnectionStatus::RECEIVE_CLIENT &&
        connection->connectionStatus != ConnectionStatus::IDLE) {

        //It seems roles have been reversed and we don't give a fuck
        //std::cerr << "(SM) Error: <" << myId << "> Receiving Client connection <" << connection->requestID << "> to <" << connection->peerID << "> is in a weird state <" << connection->connectionStatus << ">.\n";
        //std::cerr.flush();

        delete (wsm);
        return;
    }

    connection->connectionStatus = ConnectionStatus::RECEIVE_CLIENT;

    //Output Stuff
    //std::cout << "(SM) <" << myId << "> is receiving content from <" << connection->peerID << "> for Connection <" << connection->requestID << ">\n";
    //std::cout.flush();

    //Setting/Increasing Hop count on first fragment number
    if (sequenceNumber == 0) {
        cMsgPar* upstreamHopCount = static_cast<cMsgPar*>(parArray.get(MessageParameter::HOPS_UP.c_str()));
        connection->upstreamHopCount = upstreamHopCount->longValue();
        cMsgPar* downstreamHopCount = static_cast<cMsgPar*>(parArray.get(MessageParameter::HOPS_DOWN.c_str()));
        connection->downstreamHopCount = downstreamHopCount->longValue() + 1;
        cMsgPar* downstreamCachedHopCount = static_cast<cMsgPar*>(parArray.get(MessageParameter::HOPS_LAST_CACHE.c_str()));
        connection->downstreamCacheDistance = downstreamCachedHopCount->longValue() + 1;
        cMsgPar* remoteUseCount = static_cast<cMsgPar*>(parArray.get(MessageParameter::REQUESTS_AT_SOURCE.c_str()));
        connection->remoteHopUseCount = remoteUseCount->longValue();
    }

    //Calculating request size
    int requestSize = (int) ceil(connection->requestedContent->contentSize / (double) dataLengthBits);

    //Checking for Sequence End Chunk
    if (sequenceNumber == -1) {
        if (connection->obtainedSize != requestSize) {
            EV_WARN << "(SM) Warning: Missing Chunks Detected <" << connection->obtainedSize << ";" << requestSize << ">.\n";
            EV_WARN.flush();

            //std::cout << "(SM) Requesting Chunk Retransmission because <" << connection->obtainedSize << ";" << requestSize << ">!\n";
            //std::cout.flush();

            requestChunkRetransmission(connection);
            delete(wsm);
            return;
        }
    } else {
        //Adding chunk to list
        if (connection->chunkStatusList[sequenceNumber] == false) {
            //std::cout << "(SM) <" << myId << "> Accepted Chunk Item <" << sequenceNumber << ">.\n";
            //std::cout.flush();

            connection->chunkStatusList[sequenceNumber] = true;
            connection->obtainedSize++;
        } else {
            //std::cerr << "(SM) Warning: Obtained duplicate chunk <" << sequenceNumber << ">.\n";
            //std::cerr.flush();
        }
    }

    //Checking for incomplete transfer
    if (connection->obtainedSize == requestSize) {
        //if (sequenceNumber == -1) {
            //std::cout << "(SM) <" << myId << "> Completed <" << connection->requestPrefix << "> from <" << connection->peerID << "> time <" << simTime() << ">\n";
            //std::cout.flush();

            completeRemoteDataTransfer(connection);
        //}
        //else {
            //EV << "(SM) Obtained \"final\" Chunk Segment " << sequenceNumber
            //  << " from Connection " << connectionID << " totaling "
            //  << connection->obtainedSize << " out of " << requestSize
            //  << " total chunks. All we need now is the confirmation chunk.\n";
            //EV.flush();

            //std::cout << "(SM) <" << myId << "> awaits END chunk for Connection <" << connectionID << ">\n";
            //std::cout.flush();

            //Updating Timer
        //    startTimer(connection,transferTimeoutTime);
        //}
    } else {
        //EV << "(SM) Obtained Chunk Segment " << sequenceNumber
        //      << " from Connection " << connectionID << " totaling "
        //      << connection->obtainedSize << " out of " << requestSize
        //      << " total chunks.\n";
        //EV.flush();

        //cancelTimer(connection->connectionID);
        startTimer(connection,transferTimeoutTime);
    }

    updateNodeColor();
    delete (wsm);
}

//Function called to reply an interest message of content availability
void ServiceManager::notifyOfContentAvailability(WaveShortMessage* wsm, Connection_t* connection) {
    //std::cout << "(SM) Enter notifyOfContentAvailability\n";
    //std::cout.flush();

    //std::cout << "V";

    if (connection == NULL) {
        std::cerr << "(SM) <" << myId << "> Error: Connection on NotifyContentAvailability is Null.\n";
        std::cerr.flush();

        delete(wsm);
        return;
    }

    //If this availability was not the result of content being obtained from upstream, let's set the downstream properties
    if (connection->downstreamHopCount == -1) {
        connection->downstreamHopCount = 0;
        connection->downstreamCacheDistance = 0;
    }

    //
    if (dataOnSch) {
        wsm->setChannelNumber(Channels::SCH1);
    } else {
        wsm->setChannelNumber(Channels::CCH);
    }

    //Removing old Hops Up Value
    cObject* oldHops = wsm->removeObject(MessageParameter::HOPS_UP.c_str());   //Removing previous hopcount object
    if (oldHops != NULL) delete(oldHops);

    //Filling some parameters
    cMsgPar* upwardsDistance = new cMsgPar(MessageParameter::HOPS_UP.c_str());
    upwardsDistance->setLongValue(connection->upstreamHopCount);
    wsm->addPar(upwardsDistance);

    cMsgPar* downwardDistance = new cMsgPar(MessageParameter::HOPS_DOWN.c_str());
    downwardDistance->setLongValue(connection->downstreamHopCount);
    wsm->addPar(downwardDistance);

    //Setting parameters
    wsm->setSenderPos(traci->getPositionAt(simTime()));
    wsm->setSenderAddress(myId);
    wsm->setRecipientAddress(connection->peerID);
    wsm->setName(MessageClass::INTEREST_REPLY.c_str());

    //Checking if we should notify the network interface or our local client
    if (connection->peerID == myId) {

        //For client connections we just skip and say we have the data so we're good
        wsm->setName(MessageClass::DATA.c_str());
        //Technically the status of the content availability has already been set up above

        //std::cout << "(SM) <" << myId << "> Notifying Client of Result: <" << connection->connectionStatus << "> Hops: <" << connection->upstreamHopCount << ";" << connection->downstreamHopCount << "> Time: <" << simTime() << ">\n";
        //std::cout.flush();

        if (requestFallbackAllowed == true && connection->connectionStatus != ConnectionStatus::DONE_AVAILABLE) {
            //Updating status of content (assuming we can freely get it from the internet)
            wsm->setKind(ConnectionStatus::DONE_FALLBACK);
            connection->downstreamCacheDistance = 1;
            connection->upstreamHopCount = 1;
            //Running cache policy to see if we should keep the content
            runCachePolicy(connection);

            //Refactoring Connection Distances so that they are kept separately in terms of hop-count statistics
            connection->downstreamCacheDistance = -1;
            connection->upstreamHopCount = -1;
        }

        //Notifying client of content availability
        sendToClient(wsm);
        cancelTimer(connection);
    } else {
        //Given that we have the content, let's set the parameters in our connection object
        connection->connectionStatus = ConnectionStatus::WAITING_FOR_ACCEPT;
        wsm->setKind(connection->connectionStatus);

        //Setting Delay prior to network forward request
        sendWSM(wsm);

        connection->attempts++;
        startTimer(connection,interestBroadcastTimeout * (maxAttempts + 1 )); //More time than timeouts can ever take
    }

    updateNodeColor();
}

//Function called to outsource content discovery, broadcasting an interest
void ServiceManager::forwardContentSearch(WaveShortMessage* wsm, Connection_t* connection) {
    //std::cout << "(SM) Enter forwardContentSearch\n";
    //std::cout.flush();

    if (connection == NULL) {
        std::cerr << "(SM) <" << myId << "> Error: Connection on ForwardContentSearch is NULL.\n";
        std::cerr.flush();

        delete(wsm);
        return;
    }

    //std::cout << "(SM) <" << myId << "> is Forwarding connection <" << connection->requestID << "> search for <" << connection->requestPrefix << "> with uphops: <" << connection->upstreamHopCount << ">.\n";
    //std::cout.flush();

    //Checking maximum hop count prior to transmission
    if (connection->upstreamHopCount > 15) {
        //connection->connectionStatus = ConnectionStatus::DONE_UNAVAILABLE;
        //std::cout << "(SM) <" << myId << "> Reached Maximum hopcount (15) for connection <" << connection->requestID << ">.\n";
        //std::cout.flush();
        cancelTimer(connection);
        connection->connectionStatus = ConnectionStatus::DONE_UNAVAILABLE;
        delete(wsm);
        return;
    }

    //Setting Parameters
    wsm->setName(MessageClass::INTEREST.c_str());
    wsm->setBitLength(headerLength);

    if (dataOnSch) {
        wsm->setChannelNumber(Channels::SCH1);
    } else {
        wsm->setChannelNumber(Channels::CCH);
    }

    //Setting parameters
    wsm->setSenderPos(traci->getPositionAt(simTime()));
    wsm->setSenderAddress(myId);

    wsm->setRecipientAddress(-1);

    //Replacing/Adding upstream hop count
    cObject* badObj = wsm->removeObject(MessageParameter::HOPS_UP.c_str());   //Removing previous hopcount object
    if (badObj != NULL ) delete(badObj);
    cMsgPar* upwardsDistance = new cMsgPar(MessageParameter::HOPS_UP.c_str());
    upwardsDistance->setLongValue(connection->upstreamHopCount);
    wsm->addPar(upwardsDistance);

    //Replacing/Adding upstream hop count
    badObj = wsm->removeObject(MessageParameter::HOPS_LAST_CACHE.c_str());   //Removing previous hopcount object
    if (badObj != NULL ) delete(badObj);
    cMsgPar* cacheDistance = new cMsgPar(MessageParameter::HOPS_LAST_CACHE.c_str());
    cacheDistance->setLongValue(connection->downstreamCacheDistance);
    wsm->addPar(cacheDistance);

    //Setting Forwarded Message status as "Waiting"
    connection->connectionStatus = ConnectionStatus::WAITING_FOR_NETWORK;

    //If we have a priority/content restriction policy we check if it will be applied to this interest request
    double forwardDelay = uniform(minimumForwardDelay,maximumForwardDelay);
    if (priorityPolicy == AccessRestrictionPolicy::ADD_DELAY || priorityPolicy == AccessRestrictionPolicy::FORWARD_AND_DELAY) {
        if (networkLoadStatus == NetworkLoadStatus::HIGH_LOAD) {
            if (uniform(0,1) > 0.5) {
                if (getClassFromPrefix(connection->requestPrefix) == ContentClass::NETWORK) {
                    forwardDelay = uniform(maximumForwardDelay,maximumForwardDelay*2);
                    //std::cout << "(SM) <" << myId << "> High Traffic Medium Priority 2x Max_Delay\n";
                } else if (getClassFromPrefix(connection->requestPrefix) == ContentClass::NETWORK) {
                    forwardDelay = uniform(maximumForwardDelay,maximumForwardDelay*4);
                    //std::cout << "(SM) <" << myId << "> High Traffic Low Priority 4x Max_Delay\n";
                }
            }
        } else if (networkLoadStatus == NetworkLoadStatus::MEDIUM_LOAD && getClassFromPrefix(connection->requestPrefix) == ContentClass::MULTIMEDIA) {
            if (uniform(0,1) > 0.5) {
                forwardDelay = uniform(maximumForwardDelay,maximumForwardDelay*2);
                //std::cout << "(SM) <" << myId << "> Medium Traffic Low Priority 2x Max_Delay\n";
            }
        }
    }

    //Forwarding content to network with either a standard delay or policy delay
    sendWSM(wsm,forwardDelay);
    updateNodeColor();

    //std::cout << "(SM) <" << myId << "> forwarding interest with up/down: <" << connection->upstreamHopCount << ";" << connection->downstreamHopCount << "> at <" << simTime() << ">\n";
    //std::cout.flush();

    //connection->attempts++;

    startTimer(connection,interestBroadcastTimeout);
}

//Function called to accept a proposal for content offered by a remote server
void ServiceManager::acceptNetworkrequest(WaveShortMessage* wsm, Connection_t* connection) {
    //std::cout << "(SM) Enter acceptNetworkrequest\n";
    //std::cout.flush();

    //Checking our connection, which we assume is already configured as an upstream connection (connecting us to the server which will provide us with data)
    if (connection == NULL) {
        std::cerr << "(SM) Error: Cannot Accept remote data transfer for connection that does not exist.\n";
        std::cerr.flush();

        delete(wsm);
        return;
    }

    //std::cout << "(SM) <" << wsm->getSenderAddress() << "> --> data --> <" << myId << "> Up/Down <" << connection->upstreamHopCount << ";" << connection->downstreamHopCount << ">. simTime: <" << simTime() << ">\n";
    //std::cout.flush();


    //Checking our connection, which we assume is already configured as an upstream connection (connecting us to the server which will provide us with data)
    if (connection->chunkStatusList == NULL) {
        std::cerr << "(SM) Error: Chunk status list for connection does not exist.\n";
        std::cerr.flush();
        delete(wsm);
        return;
    }

    //Setting relevant parameters in connection
    connection->connectionStatus = ConnectionStatus::RECEIVE_CLIENT;
    Interest_t* relatedInterest = getInterest(connection->requestedContent->contentPrefix);

    //Making sure we are still interested in this content
    if (relatedInterest == NULL) {
        rejectNetworkrequest(wsm,connection);
        return;
    }
    relatedInterest->providingConnection = connection;

    //Setting our local sender parameters
    wsm->setRecipientAddress(connection->peerID);
    wsm->setSenderAddress(myId);
    wsm->setSenderPos(traci->getCurrentPosition());

    //Generating Lost Chunk Request String
    int requestSize = (int) ceil(connection->requestedContent->contentSize / (double) dataLengthBits);
    std::string missingChunkList = "";
    bool specifiedChunks = false;
    for (int i = 0; i < requestSize ; i++) {
        if (connection->chunkStatusList[i] == false) {
            connection->lostSize++;
            missingChunkList += std::to_string(i) + ";";
            specifiedChunks = true;
        }
    }
    if (!specifiedChunks) missingChunkList = "-1";
    if (missingChunkList.compare("") == 0) missingChunkList = "-1";

    //Setting the Sequence Number Parameter
    cMsgPar* sequenceParameter = new cMsgPar(MessageParameter::SEQUENCE_NUMBER.c_str());
    sequenceParameter->setStringValue(missingChunkList.c_str());
    wsm->addPar(sequenceParameter);

    //Making sure our message has our uphop count
    cMsgPar* uphopsParameter = new cMsgPar(MessageParameter::HOPS_UP.c_str());
    uphopsParameter->setLongValue(connection->upstreamHopCount);
    wsm->addPar(uphopsParameter);

    //Reseting the response request because it seems it didnt work before? not sure...
    cObject* badObj = wsm->removeObject(MessageParameter::TYPE.c_str());   //Removing previous hopcount object
    if (badObj != NULL ) delete(badObj);
    //Adding Interest Accept thing...
    cMsgPar* classParameter = new cMsgPar(MessageParameter::TYPE.c_str());
    classParameter->setStringValue(MessageClass::INTEREST_ACCEPT.c_str());
    wsm->addPar(classParameter);
    wsm->setName(MessageClass::INTEREST_ACCEPT.c_str());

    //if (connection->peerID == 0 && connection->requestID == 323) {
    //    std::cout << "(SM) <" << myId << "> is trying to accept a request provided by 0 at Time: <" << simTime() << ">\n";
    //    std::cout.flush();
    //}

    //Setting Delay prior to network forward request
    sendWSM(wsm);
    updateNodeColor();

    //connection->attempts++;
    startTimer(connection);
    //delete(wsm);
}

//Function called to reject a proposal for content offered by a remote server
void ServiceManager::rejectNetworkrequest(WaveShortMessage* wsm, Connection_t* connection) {
    //std::cout << "(SM) Enter rejectNetworkrequest\n";
    //std::cout.flush();

    if (connection == NULL) {
        EV_ERROR << "(SM) Cannot Reject request for a Connection that does not exist.\n";
        EV_ERROR.flush();
        delete(wsm);
        return;
    }

    connection->connectionStatus = ConnectionStatus::DONE_REJECTED;

    Coord currentPosition = traci->getCurrentPosition();
    wsm->setRecipientAddress(connection->peerID);
    wsm->setSenderAddress(myId);
    wsm->setSenderPos(currentPosition);

    //Reseting the response request because it seems it didnt work before? not sure...
    cObject* badObj = wsm->removeObject(MessageParameter::TYPE.c_str());   //Removing previous hopcount object
    if (badObj != NULL ) delete(badObj);
    cMsgPar* classParameter = new cMsgPar(MessageParameter::TYPE.c_str());
    classParameter->setStringValue(MessageClass::INTEREST_CANCEL.c_str());
    wsm->addPar(classParameter);
    wsm->setName(MessageClass::INTEREST_CANCEL.c_str());

    sendWSM(wsm);
    updateNodeColor();

    //TODO: (DECIDE) if we should set a timer to inform that we're ignoring an interest cancel message
    //connection->attempts++;
    //startTimer(connection);

}

//Function called by transmitting node whenever it gets a data_missing message (notification of missing chunks)
void ServiceManager::transmitDataChunks(Connection_t* connection, std::vector<int>* chunkVector) {
    //std::cout << "(SM) Enter transmitDataChunks\n";
    //std::cout.flush();

    //Checking if the connection is null (not previously listed and therefore cannot be used for retransmission)
    if (connection == NULL) {
        EV_ERROR << "(SM) ERROR: Null connection on retransmission request.\n";
        EV_ERROR.flush();
        return;
    }

    //Making sure we're transmitting data for a connection that makes sense
    if (connection->connectionStatus != ConnectionStatus::WAITING_FOR_ACCEPT) {
        std::cerr << "(SM) Error: <" << myId << "> Connection Status is NOT WAITING_FOR_ACCEPT.\n";
        std::cerr.flush();
    }

    //Updating Connection Status
    connection->connectionStatus = ConnectionStatus::TRANSFER_TRANSFERING;

    //Calculating number of Chunks and warning receiving Node
    int requestSize = connection->requestedContent->contentSize;
    //Calculating number of items to be sent
    int queueSize = ceil(requestSize / (double) dataLengthBits);

    //std::cout << "(SM) Chunks per Message: <" << queueSize << ">\n";

    //Null chunk vector implies full transmission
    if (chunkVector == NULL) {
        //std::cout << "(SM) <" << myId << "> is Starting FRESH a Data Transfer for Connection <" << connection->connectionID << "> with upHops <" << connection->upstreamHopCount << "> and downHops <" << connection->downstreamHopCount << ">\n";
        //std::cout.flush();

        //Reserving space for the size we want & listing all packets to be sent
        //chunkVector->reserve(queueSize);
        chunkVector = new std::vector<int>(queueSize);
        for (int i = 0; i < queueSize; i++) {
            (*chunkVector)[i] = i;
        }
    } else {
        queueSize = chunkVector->size();
    }

    //Generating and Preparing a Basic Message
    WaveShortMessage * baseChunkMessage = getGenericMessage(connection);

    //std::cout << "(SM) <" << myId << "> Providing <" << queueSize << "> Data Packets woth of data.\n";
    //std::cout.flush();

    //Figuring out 1 single time value after which we send all data chunks
    double transmissionDelay = uniform(minimumForwardDelay,maximumForwardDelay);

    //Sending all data chunks to lower layers
    for (int i = 0 ; i < queueSize; i++) {
        std::string contentString = "content_chunk_" + std::to_string((*chunkVector)[i]);
        WaveShortMessage* chunkMessage = baseChunkMessage->dup();

        //Adding Sequence Number
        cMsgPar* clientIDParameter = new cMsgPar(MessageParameter::SEQUENCE_NUMBER.c_str());
        clientIDParameter->setLongValue((*chunkVector)[i]);
        chunkMessage->addPar(clientIDParameter);

        sendWSM(chunkMessage,transmissionDelay + minimumForwardDelay * i);
    }

    //Sending final content chunk
    std::string contentString = "content_chunk_-1";
    WaveShortMessage* chunkMessage = baseChunkMessage->dup();
    chunkMessage->setBitLength(headerLength);

    //Adding Sequence Number
    cMsgPar* sequenceIDParameter = new cMsgPar(MessageParameter::SEQUENCE_NUMBER.c_str());
    sequenceIDParameter->setLongValue(-1);
    chunkMessage->addPar(sequenceIDParameter);

    sendWSM(chunkMessage,transmissionDelay +  + minimumForwardDelay * queueSize);

    connection->connectionStatus = ConnectionStatus::TRANSFER_WAITING_ACK;

    updateNodeColor();

    //Deleting original base chunk message (as its not necessary anymore)
    cObject* badObj = baseChunkMessage->removeObject(MessageParameter::PREFIX.c_str());
    if (badObj != NULL ) delete(badObj);
    badObj = baseChunkMessage->removeObject(MessageParameter::CONNECTION_ID.c_str());
    if (badObj != NULL ) delete(badObj);
    badObj = baseChunkMessage->removeObject(MessageParameter::HOPS_LAST_CACHE.c_str());
    if (badObj != NULL ) delete(badObj);
    badObj = baseChunkMessage->removeObject(MessageParameter::HOPS_DOWN.c_str());
    if (badObj != NULL ) delete(badObj);
    badObj = baseChunkMessage->removeObject(MessageParameter::HOPS_UP.c_str());
    if (badObj != NULL ) delete(badObj);
    badObj = baseChunkMessage->removeObject(MessageParameter::REQUESTS_AT_SOURCE.c_str());
    if (badObj != NULL ) delete(badObj);
    badObj = baseChunkMessage->removeObject(MessageParameter::PREFIX.c_str());
    if (badObj != NULL ) delete(badObj);
    delete(baseChunkMessage);

    //Here we start a longer timer to make sure that our client's timer times out before our own
    //This makes it so servers never timeout before clients
    startTimer(connection,interestBroadcastTimeout * (maxAttempts + 2)); //More time than timeouts can ever take (Including an extension based on the node receiving part of our data and postponing THEIR timer)
}

//
void ServiceManager::requestChunkRetransmission(Connection_t* connection) {
    //std::cout << "(SM) Enter requestChunkRetransmission\n";
    //std::cout.flush();

    int requestSize = (int) ceil(connection->requestedContent->contentSize / (double) dataLengthBits);

    std::string missingChunkList = "";
    for (int i = 0; i < requestSize ; i++) {
        if (connection->chunkStatusList[i] == false) {
            connection->lostSize++;
            missingChunkList += std::to_string(i) + ";";
            totalChunksLost++;
        }
    }

    //At some point the obtained chunk counter fucks up... :/ so let's just ignore this and notify that content is ready, wont we?
    //It's possibly some out of order delivery bullshit (though it shouldn't because it's all 1 to 1 single hop communication... but whatever
    if (missingChunkList.compare("") == 0) {
        //std::cerr << "(SM) Are you sure any chunks are actually missing? Status: <" << connection->connectionStatus << ">\n";
        //std::cerr.flush();
        connection->obtainedSize = requestSize;
        completeRemoteDataTransfer(connection);
        return;
    }

//    std::cout << "(SM) <" << myId << "> Missing Chunks for <" << connection->requestPrefix << "> from <" << connection->peerID << "> time <" << simTime() << ">\n";
//    std::cout.flush();

    t_channel channel = dataOnSch ? type_SCH : type_CCH;
    WaveShortMessage * retransmissionMessage = prepareWSM(MessageClass::INTEREST,headerLength, channel, dataPriority, -1, 2);

    //Adding Message Type
    retransmissionMessage->setName(MessageClass::DATA_MISSING.c_str());
    cMsgPar* classParameter = new cMsgPar(MessageParameter::TYPE.c_str());
    classParameter->setStringValue(MessageClass::DATA_MISSING.c_str());
    retransmissionMessage->addPar(classParameter);

    //Adding Uphops just in case our connection got shutdown on the other end
    cMsgPar* uphopsParameter = new cMsgPar(MessageParameter::HOPS_UP.c_str());
    uphopsParameter->setLongValue(connection->upstreamHopCount);
    retransmissionMessage->addPar(uphopsParameter);

    //Adding Content Status
    cMsgPar* responseParameter = new cMsgPar(MessageParameter::STATUS.c_str());
    responseParameter->setBoolValue(false);
    retransmissionMessage->addPar(responseParameter);

    //Adding Content Type
    cMsgPar* contentTypeParameter = new cMsgPar(MessageParameter::CLASS.c_str());
    contentTypeParameter->setLongValue(library->getContentClass(connection->contentClass));
    retransmissionMessage->addPar(contentTypeParameter);

    //Adding Named Prefix for specific content
    cMsgPar* contentNameParameter = new cMsgPar(MessageParameter::PREFIX.c_str());
    contentNameParameter->setStringValue(connection->requestPrefix.c_str());
    retransmissionMessage->addPar(contentNameParameter);

    //Adding ID for Connection
    cMsgPar* idParameter = new cMsgPar(MessageParameter::CONNECTION_ID.c_str());
    idParameter->setLongValue(connection->requestID);
    retransmissionMessage->addPar(idParameter);

    //Adding Missing chunks into sequence number parameter
    cMsgPar* sequenceParameter = new cMsgPar(MessageParameter::SEQUENCE_NUMBER.c_str());
    sequenceParameter->setStringValue(missingChunkList.c_str());
    retransmissionMessage->addPar(sequenceParameter);

    retransmissionMessage->setRecipientAddress(connection->peerID);
    retransmissionMessage->setSenderAddress(myId);
    retransmissionMessage->setSenderPos(traci->getCurrentPosition());

    //Updating Status
    connection->connectionStatus = ConnectionStatus::WAITING_FOR_CONTENT;

    //Sending retransmit packet
    sendWSM(retransmissionMessage);

    //connection->attempts++;
    startTimer(connection, transferTimeoutTime);
}

//
void ServiceManager::completeRemoteDataTransfer(Connection_t* connection) {
    //std::cout << "(SM) Enter completeRemoteDataTransfer\n";
    //std::cout.flush();

    if (connection->connectionStatus == ConnectionStatus::DONE_RECEIVED) {
        std::cerr << "(SM) Warning: <" << myId << "> We already have this connection marked as done.\n";
        std::cerr.flush();
        return;
    }

    //std::cout << "V";

    switch (connection->connectionStatus) {
        //Client Side State
        case ConnectionStatus::RECEIVE_CLIENT:
            //Updating Connection Status, marking as completed
            connection->connectionStatus = ConnectionStatus::DONE_RECEIVED;

            //Checking if we have to do something cache related
            runCachePolicy(connection);

            //Continue Downstreams & local client dependencies
            fulfillPendingInterest(connection);

            //TODO: (DECIDE) if we should notify our server that we got the content or if we should just let it go, assuming that no communication is good communication
            break;

        //Server Side State
        //case ConnectionStatus::TRANSFER_WAITING_ACK:
        //    //Marking Connection as Completed
        //    connection->connectionStatus = ConnectionStatus::DONE_PROVIDED;
        //
        //    //Checking if we have to do something cache related
        //    runCachePolicy(connection);
        //    break;

        default:
            std::cout << "(SM) Warning: Unknown Connection status during Remote Data Transfer Completion.\n";
            std::cout.flush();
            break;
    }

    //NOTE: This was disabled as connections seemingly were deleted before they should
    //Statistics will save the information from this connection and delete it from our list reducing size and improving lookups
    //saveStatistics();
    updateNodeColor();
}

//
void ServiceManager::fulfillPendingInterest(Connection_t* connection) {
    //std::cout << "(SM) Enter fulfillPendingInterest\n";
    //std::cout.flush();

    Interest_t* pendingInterest = getInterest(connection->requestedContent->contentPrefix);

    //This should be not null because even local requests have
    if (pendingInterest == NULL) {
        //std::cerr << "(SM) <" << myId << "> No interest found for a connection that theoretically was downloaded for someone. o.õ \n";
        //std::cerr.flush();
        return;
    }

    //Iterating over all possible connections
    for (auto it = pendingInterest->pendingConnections.begin() ; it != pendingInterest->pendingConnections.end() ; it++) {

        //if ((*it) == myId) {
        //    std::cout << "(SM) LOCAL CONTENT! YAAAAY.\n";
        //    std::cout.flush();
        //} else {
        //    std::cout << "(SM) Forwarding Content to neighbor <" << pendingConnection->peerID << ">. Current status of our connection is <" << pendingConnection->connectionStatus << ">\n";
        //    std::cout.flush();
        //}

        Connection_t* pendingConnection = getConnection(connection->requestID,*it);

        //if (myId == 17 && connection->requestID == 178) {
        //    std::cout << "(SM) Fulfilling Request to peer <" << (*it) << ">. The connection object for this client is <" << pendingConnection << "> with status <" << pendingConnection->connectionStatus << "> and attempts <" << pendingConnection->attempts << "> at time <" << simTime() << ">\n";
        //    std::cout.flush();
        //}

        if (pendingConnection != NULL) {
            pendingConnection->upstreamHopCount = connection->upstreamHopCount;
            pendingConnection->downstreamHopCount = connection->downstreamHopCount;
            pendingConnection->downstreamCacheDistance = connection->downstreamCacheDistance;

            //TODO: (GLITCH) Find out why the hell sometimes this comparison does not work properly
            //Updating local connection status
            if (pendingConnection->peerID == myId) {
                //std::cout <<
                pendingConnection->connectionStatus = ConnectionStatus::DONE_RECEIVED;
                runCachePolicy(pendingConnection);
            } else {
                pendingConnection->connectionStatus = ConnectionStatus::TRANSFER_FORWARDING;
            }

            //std::cout << "\t(SM) <" << myId << "> Fulfilling connection <" << pendingConnection->requestID << "> to <" << pendingConnection->peerID << "> with status <" << pendingConnection->connectionStatus << ">\n";
            //std::cout.flush();

            //Canceling a potentially existing Timer
            replyAfterContentInclusion(pendingConnection);

            if (connection->peerID != myId) {
                startTimer(pendingConnection);
            } else {
                cancelTimer(pendingConnection);
            }
        } else {
           //std::cout << "(SM) Pending Connection in Interest is NULL.\n";
           //std::cout.flush();
        }
    }

    //Deleting Pending Interest
    deleteInterest(connection->requestPrefix);
    //pendingInterest->pendingConnections.clear();
    //PIT.remove(pendingInterest);

    cleanConnections();
}

//Reply to node that we have content after some delay, possibly after getting it from another request and checking our PIT
void ServiceManager::replyAfterContentInclusion(Connection_t* connection) {
    //std::cout << "(SM) Enter replyAfterContentInclusion\n";
    //std::cout.flush();

    t_channel channel = dataOnSch ? type_SCH : type_CCH;
    WaveShortMessage * responseMessage = prepareWSM(MessageClass::INTEREST_REPLY, headerLength, channel, dataPriority, -1, 2);

    Content_t* content = connection->requestedContent;

    //Content Type
    cMsgPar* contentTypeParameter = new cMsgPar(MessageParameter::CLASS.c_str());
    contentTypeParameter->setLongValue(static_cast<int>(content->contentClass));
    responseMessage->addPar(contentTypeParameter);

    //Named Prefix for specific content
    cMsgPar* contentNameParameter = new cMsgPar(MessageParameter::PREFIX.c_str());
    contentNameParameter->setStringValue(content->contentPrefix.c_str());
    responseMessage->addPar(contentNameParameter);

    //Adding Priority for Content
    //cMsgPar* priorityParameter = new cMsgPar(MessageParameter::PRIORITY.c_str());
    //priorityParameter->setLongValue(content->contentPriority);
    //responseMessage->addPar(priorityParameter);

    //Adding Size for Content
    cMsgPar* sizeParameter = new cMsgPar(MessageParameter::SIZE.c_str());
    sizeParameter->setLongValue(content->contentSize);
    responseMessage->addPar(sizeParameter);

    //Adding Popularity for Content
    cMsgPar* popularityParameter = new cMsgPar(MessageParameter::POPULARITY.c_str());
    popularityParameter->setLongValue(content->contentSize);
    responseMessage->addPar(popularityParameter);

    //Adding Message Type (Interest)
    cMsgPar* classParameter = new cMsgPar(MessageParameter::TYPE.c_str());
    classParameter->setStringValue(MessageClass::INTEREST_REPLY.c_str());
    responseMessage->addPar(classParameter);

    //ID for Content
    cMsgPar* idParameter = new cMsgPar(MessageParameter::CONNECTION_ID.c_str());
    idParameter->setLongValue(connection->requestID);
    responseMessage->addPar(idParameter);

    //Uphop Parameter
    cMsgPar* uphopparameter = new cMsgPar(MessageParameter::HOPS_UP.c_str());
    uphopparameter->setLongValue(connection->upstreamHopCount);
    responseMessage->addPar(uphopparameter);

    //Downhop Parameter
    cMsgPar* downhopparameter = new cMsgPar(MessageParameter::HOPS_DOWN.c_str());
    downhopparameter->setLongValue(connection->downstreamHopCount);
    responseMessage->addPar(downhopparameter);

    //Downstream Hop Distance Parameter
    cMsgPar* cacheDistanceParameter = new cMsgPar(MessageParameter::HOPS_LAST_CACHE.c_str());
    cacheDistanceParameter->setLongValue(connection->downstreamCacheDistance);
    responseMessage->addPar(cacheDistanceParameter);

    //Provider Address
    responseMessage->setSenderAddress(myId);
    responseMessage->setSenderPos(traci->getCurrentPosition());

    //This is a duplicate and we shouldn't really have to deal with this but whatever
    if (connection->peerID == myId) {
        connection->connectionStatus = ConnectionStatus::DONE_AVAILABLE;
        cancelTimer(connection);
    }
    //else {
    //    //Waiting for client to accept
    //    connection->connectionStatus = ConnectionStatus::WAITING_FOR_ACCEPT;
    //}
    responseMessage->setKind(connection->connectionStatus);

    notifyOfContentAvailability(responseMessage,connection);
    //cleanConnections();
}

//=============================================================
// CONNECTION LIST FUNCTIONS
//=============================================================


//Returns true if any connection has been overheard for said requestID
bool ServiceManager::checkForConnections(long requestID) {
    //std::cout << "(SM) Looking for any connections for specific requestID\n";
    //std::cout.flush();

    for (auto it = connectionList.begin(); it != connectionList.end(); it++) {
        if ((*it)->requestID == requestID) {
            return true;
        }
    }
    return false;
}

//Returns connection from list of connections. Will return NULL if ID is not listed
Connection_t* ServiceManager::getConnection(long requestID, int peerID) {
    //std::cout << "(SM) Enter getConnection\n";
    //std::cout.flush();

    for (auto it = connectionList.begin(); it != connectionList.end(); it++) {
        //Checking Downstream
        //if (myId == 0 && requestID == 2) {
        //    std::cout << "(SM) <" << myId << "> Checking Request <" << (*it)->requestID << "> from peer <" << (*it)->peerID << ">\n";
        //    std::cout.flush();
        //}
        if ((*it)->requestID == requestID && (*it)->peerID == peerID) {
            //if (myId == 0 && requestID == 2) {
            //    std::cout << "(SM) <" << myId << "> Found it! <" << *it << ">\n";
            //    std::cout.flush();
            //}
            return *it;
        }
    }
    return NULL;
}

//creates a generic connection (but does not add it to the connection list as the connection object has generic parameters set)
Connection_t* ServiceManager::createGenericConnection(Content_t* content) {
    //std::cout << "(SM) Enter createGenericConnection\n";
    //std::cout.flush();

    if (content == NULL) {
        EV_ERROR << "(SM) CreateConnection got NULL WSM object.\n";
        EV_ERROR.flush();
        return NULL;
    }

    //Creating a Generic Connection Based on a content
    Connection_t* newConnection = new Connection_t();
    newConnection->requestID = -1;
    newConnection->peerID = -1;
    newConnection->downstreamCacheDistance = -1;
    newConnection->downstreamHopCount = -1;
    newConnection->upstreamHopCount = -1;
    newConnection->requestPrefix = content->contentPrefix;
    newConnection->contentClass = content->contentClass;
    newConnection->requestTime = simTime();
    //newConnection->replyFromPIT = false;
    newConnection->connectionStatus = ConnectionStatus::IDLE;
    newConnection->obtainedSize = -1;
    newConnection->lostSize = -1;
    newConnection->attempts = 1;
    newConnection->peerPosition = Coord::ZERO;
    newConnection->pendingMessage = NULL;
    newConnection->requestedContent = content;
    newConnection->remoteHopUseCount = 0;


    //WATCH(newConnection->connectionStatus);
    //WATCH_RW(newConnection);

    return newConnection;
}

//
Connection_t* ServiceManager::createServerSidedConnection(WaveShortMessage *wsm) {
    //std::cout << "(SM) Enter createServerSidedConnection\n";
    //std::cout.flush();

    //Unwrapping contents of request
    cArray parArray = wsm->getParList();
    cMsgPar* requestID = static_cast<cMsgPar*>(parArray.get(MessageParameter::CONNECTION_ID.c_str()));
    cMsgPar* requestPrefixField = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));
    cMsgPar* requestUpHops = static_cast<cMsgPar*>(parArray.get(MessageParameter::HOPS_UP.c_str()));

    if (requestUpHops == NULL) {
        std::cerr << "(SM) Error: uphop count is null in incoming (interest?) message!\n";
        std::cerr.flush();
        return NULL;
    }

    //Getting Values from Unwrapped Parameters
    int idValue = requestID->longValue();
    std::string prefixValue = requestPrefixField->str();
    int upHops = requestUpHops->longValue();

    //Removing quotes from string
    if (prefixValue.c_str()[0] == '\"') {
        prefixValue = prefixValue.substr(1, prefixValue.length() - 2);
    }

    //Checking if we have the requested content object
    Content_t* requestedContent = library->getContent(prefixValue);
    if (requestedContent == NULL) {
        std::cerr << "(SM) Error: Did not find the requested content!\n";
        std::cerr.flush();
        return NULL;
    }

    //Checking if we have an existing connection with content requester
    Connection_t* newConnection = getConnection(idValue,wsm->getSenderAddress());
    if (newConnection != NULL) {
        std::cerr << "(SM) Error: Cannot create a server-sided connection as it already exists with state <" << newConnection->connectionStatus << ">!\n";
        std::cerr.flush();
        return newConnection;
    }

    //Creating our connection
    newConnection = createGenericConnection(requestedContent);

    //Filling relevant fields in the connection object
    newConnection->requestID = idValue;
    newConnection->peerID = wsm->getSenderAddress();
    newConnection->peerPosition = wsm->getSenderPos();
    newConnection->upstreamHopCount = upHops + 1;
    //We do not set the downstream hop count here, we're doing it on our reply messages o.õ
    newConnection->connectionStatus = ConnectionStatus::IDLE;

    //Adding Connection to Connection List
    connectionList.push_front(newConnection);

    //Returning functional connection
    return newConnection;
}

//
Connection_t* ServiceManager::createClientSidedConnection(WaveShortMessage *wsm) {
    //std::cout << "(SM) Enter createClientSidedConnection\n";
    //std::cout.flush();

    //Unwrapping contents of request
    cArray parArray = wsm->getParList();
    cMsgPar* requestID = static_cast<cMsgPar*>(parArray.get(MessageParameter::CONNECTION_ID.c_str()));
    cMsgPar* requestPrefixField = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));
    cMsgPar* requestUpHops = static_cast<cMsgPar*>(parArray.get(MessageParameter::HOPS_UP.c_str()));
    cMsgPar* requestDownHops = static_cast<cMsgPar*>(parArray.get(MessageParameter::HOPS_DOWN.c_str()));
    cMsgPar* requestCacheDistance = static_cast<cMsgPar*>(parArray.get(MessageParameter::HOPS_LAST_CACHE.c_str()));

    //
    if (requestUpHops == NULL || requestDownHops == NULL) {
        std::cerr << "(SM) Error: either hop count is null in response message!\n";
        std::cerr.flush();
        return NULL;
    }

    //Getting Values from Unwrapped Parameters
    int idValue = requestID->longValue();
    std::string prefixValue = requestPrefixField->str();
    int upHops = requestUpHops->longValue();
    int downHops = requestDownHops->longValue();
    int cacheDistance = requestCacheDistance->longValue();

    //Removing quotes from string
    if (prefixValue.c_str()[0] == '\"') {
        prefixValue = prefixValue.substr(1, prefixValue.length() - 2);
    }

    Content_t* requestedContent = library->getContent(prefixValue);
    if (requestedContent == NULL) {
        std::cerr << "(SM) Error: Did not found the requested content!\n";
        std::cerr.flush();
        return NULL;
    }

    //Creating our connection
    Connection_t* newConnection = createGenericConnection(requestedContent);

    //Filling relevant fields in the connection object
    newConnection->requestID = idValue;
    newConnection->peerID = wsm->getSenderAddress();
    newConnection->peerPosition = wsm->getSenderPos();
    newConnection->upstreamHopCount = upHops;
    newConnection->downstreamHopCount = downHops + 1;
    newConnection->downstreamCacheDistance = cacheDistance + 1;
    newConnection->connectionStatus = ConnectionStatus::IDLE;

    newConnection->lostSize = 0;
    newConnection->obtainedSize = 0;
    int requestSize = (int) ceil(newConnection->requestedContent->contentSize / (double) dataLengthBits);
    if (newConnection->chunkStatusList == NULL) {
        newConnection->chunkStatusList = new bool[requestSize];
        for (int i = 0; i < requestSize; i++) {
            newConnection->chunkStatusList[i] = false;
        };
    }

    //Adding Connection to Connection List
    connectionList.push_front(newConnection);

    //Returning functional connection
    return newConnection;
}

//=============================================================
// CACHE RELATED FUNCTIONS
//=============================================================

//In-network Cache Policy
void ServiceManager::runCachePolicy(Connection_t* connection) {
    //std::cout << "(SM) Enter runCachePolicy\n";
    //std::cout.flush();


    //We always add local requests to cache
    if (connection->peerID == myId) {
        //std::cout <<  "(SM) <" << myId << "> Adding local request connection <" << connection->requestPrefix << "> aka <" << connection->requestID << "> to Library!\n";
        //std::cout.flush();
        addContentToCache(connection);
        return;
    }

    //Checking if we're a client or server
    switch (connection->connectionStatus) {
        //Client Side State
        case ConnectionStatus::DONE_RECEIVED:
        case ConnectionStatus::DONE_FALLBACK:
            {
                /*
                //If our caching policy is the distributed popularity estimation method, we need remote information which we pass here
                if (cache->getCachePolicy() == CacheReplacementPolicy::FREQ_POPULARITY) {
                    if (connection->remoteHopUseCount > 1 && cache->brokenlocalPopularityCacheDecision(connection)) {
                        //std::cout << "(SM) <" <<myId  << "> Attempting Caching Policy on Object <" << connection->requestPrefix << "> HopCount <" << connection->downstreamHopCount << "> Remote UseCount <" << connection->remoteHopUseCount << "> Current UseCount is <" << (cache->getUseCount(connection->requestPrefix)) << ">\n";
                        //std::cout.flush();
                        addContentToCache(connection);
                        break;
                    }
                }
                */
                switch (cacheCoordinationPolicy) {
                    case NEVER:     //Never leave a copy
                        break;

                    case LCE:       //Always leave a copy
                        addContentToCache(connection);
                        break;

                    case LCD:       //Leave a copy on first hop down
                        if (connection->downstreamHopCount == 1) {
                            //std::cout << "(SM) <" <<myId  << "> is Leaving Copy Down on first hop of downstream.\n";
                            //std::cout.flush();
                            addContentToCache(connection);
                        }
                        break;

                    case MCD:       //Move Copy Down
                        //We always get a copy (in this section of code), node above deletes his (delete local copy in senderComplete)
                        //std::cout << "(SM) <" <<myId  << ">  added content to cache and will delete it after downstream is complete.\n";
                        //std::cout.flush();
                        addContentToCache(connection);
                        break;

                    case PROB:{     //Copy to cache with probability (See probability variable)
                        double randomUniformValue = uniform(0,1);
                        if (cacheCopyProbability >= randomUniformValue) {
                            addContentToCache(connection);
                            //std::cout << "(SM) <" <<myId  << ">  Copying to Cache on success <" << cacheCopyProbability << "> over <" << randomUniformValue << ">\n";
                            //std::cout.flush();
                        } else {
                            //std::cout << "(SM) <" <<myId  << ">  No copy, probability failure of <" << cacheCopyProbability << "> over <" << randomUniformValue << ">\n";
                            //std::cout.flush();
                        }
                        break;
                    }

                    case PROB30p:{     //Copy to cache with probability (See probability variable)
                        double randomUniformValue = uniform(0,1);
                        if (randomUniformValue >= 0.3) {
                            addContentToCache(connection);
                            //std::cout << "(SM) <" <<myId  << ">  Copying to Cache on success <" << cacheCopyProbability << "> over <" << randomUniformValue << ">\n";
                            //std::cout.flush();
                        } else {
                            //std::cout << "(SM) <" <<myId  << ">  No copy, probability failure of <" << cacheCopyProbability << "> over <" << randomUniformValue << ">\n";
                            //std::cout.flush();
                        }
                        break;
                    }

                    case PROB70p:{     //Copy to cache with probability (See probability variable)
                        double randomUniformValue = uniform(0,1);
                        if (randomUniformValue >= 0.7) {
                            addContentToCache(connection);
                            //std::cout << "(SM) <" <<myId  << ">  Copying to Cache on success <" << cacheCopyProbability << "> over <" << randomUniformValue << ">\n";
                            //std::cout.flush();
                        } else {
                            //std::cout << "(SM) <" <<myId  << ">  No copy, probability failure of <" << cacheCopyProbability << "> over <" << randomUniformValue << ">\n";
                            //std::cout.flush();
                        }
                        break;
                    }

                    case RC_ONE:{    //Random Copy One
                        //TODO: (IMPLEMENT) Random Copy one -> Required additional message fields for control
                        std::cerr << "(SM) <" <<myId  << ">  Random Copy One not implemented!\n";
                        std::cerr.flush();
                        break;
                    }

                    case DISTANCE:{     //Distance based probabilistic
                            double denseCacheProbability = connection->downstreamHopCount/(double)connection->upstreamHopCount;
                            double randomUniformValue = uniform(0,1);
                            if (denseCacheProbability >= randomUniformValue) {
                                addContentToCache(connection);
                                //std::cout << "(SM) <" <<myId  << ">  Copying to Cache on success <" << denseCacheProbability << "> over <" << randomUniformValue << ">\n";
                                //std::cout.flush();
                            } else {
                                //std::cout << "(SM) <" <<myId  << ">  No copy, probability failure of <" << denseCacheProbability << "> over <" << randomUniformValue << ">\n";
                                //std::cout.flush();
                            }
                            break;
                        }

                    //My Work (Local Average)
                    case LOC_PROB:{     //Local Probability Estimation Method
                        if (cache->localPopularityCacheDecision(connection)) {
                            //std::cout << "(SM) <" <<myId  << "> Attempting Caching Policy on Object <" << connection->requestPrefix << "> HopCount <" << connection->downstreamHopCount << "> Remote UseCount <" << connection->remoteHopUseCount << "> Current UseCount is <" << (cache->getUseCount(connection->requestPrefix)) << ">\n";
                            //std::cout.flush();
                            addContentToCache(connection);

                            //Logging the remote popularity value of the content object
                            cache->increaseUseCount(connection->remoteHopUseCount,connection->requestPrefix);
                            break;
                        }
                        break;
                    }

                    //My Work (Local Minimum)
                    case LOC_PROB_MIN:{     //Local Minimum Popularity Estimation
                        if (cache->localMinimumPopularityCacheDecision(connection)) {
                            addContentToCache(connection);

                            //Logging the remote popularity value of the content object
                            cache->increaseUseCount(connection->remoteHopUseCount,connection->requestPrefix);
                            break;
                        }
                        break;
                    }

                    //My Work (Global Average Comparison)
                    case GLOB_PROB:{     //Global Popularity Definition
                        if (cache->globalPopularityCacheDecision(connection)) {
                            addContentToCache(connection);

                            //Logging the remote popularity value of the content object
                            cache->increaseUseCount(connection->remoteHopUseCount,connection->requestPrefix);
                            break;
                        }
                        break;
                    }

                    //My Work (Global Minimum Comparison)
                    case GLOB_PROB_MIN:{     //Global Popularity Definition
                        if (cache->globalMinimumPopularityCacheDecision(connection)) {
                            addContentToCache(connection);

                            //Logging the remote popularity value of the content object
                            cache->increaseUseCount(connection->remoteHopUseCount,connection->requestPrefix);
                            break;
                        }
                        break;
                    }

                    default:
                        std::cerr << "(SM) Cache Coordination Policy <" << cacheCoordinationPolicy << "> Not Known!\n";
                        std::cerr.flush();
                        break;

                }
            }
            break;

        case ConnectionStatus::TRANSFER_WAITING_ACK:
            //We don't operate on CACHE until we know that things are ok
            break;

        //Checking for potential cache operation requirements from a server standpoint
        case ConnectionStatus::DONE_PROVIDED:
            //Checking if we need to delete our local copy after forwarding content (Cache coordination policy dependent)
            if (cacheCoordinationPolicy == CacheInNetworkCoordPolicy::MCD && connection->downstreamHopCount != 0) {
                removeContentFromCache(connection);
            }
            break;

        default:
            std::cerr << "(SM) <" << myId << "> cache policy cannot be run on state <" << connection->connectionStatus << ">\n";
            std::cerr.flush();
            break;
    }
}

//Function called to request content be added to cache
void ServiceManager::addContentToCache(Connection_t* connection) {
    //std::cout << "(SM) Enter addContentToCache\n";
    //std::cout.flush();

    cache->addContentToCache(connection->requestedContent);
    connection->downstreamCacheDistance = 0;
}

//Function called to request content be removed from cache (only called by move copy down)
void ServiceManager::removeContentFromCache(Connection_t* connection) {
    //std::cout << "(SM) Enter removeContentFromCache\n";
    //std::cout.flush();

    cache->removeContentFromCache(connection->requestedContent);
}

//=============================================================
// PIT FUNCTIONS
//=============================================================

//Function called to obtain connection from list of connections. Will return NULL if ID is not listed
Interest_t* ServiceManager::getInterest(std::string interest) {
    //std::cout << "(SM) Enter getInterest\n";
    //std::cout.flush();

    for (auto it = PIT.begin(); it != PIT.end(); it++) {
        //Comparing Request ID
        if ((*it)->interestPrefix.compare(interest) == 0) {
            return *it;
        }
    }
    return NULL;
}

//
bool ServiceManager::createInterest(std::string interestPrefix, int senderAddress) {
    //std::cout << "(SM) Enter createInterest\n";
    //std::cout.flush();

    //Checking if connection is already listed
    if (getInterest(interestPrefix) != NULL) {
        EV_WARN << "Interest already exists. Ignoring request.\n";
        EV_WARN.flush();
        return false;
    }

    Interest_t* newInterest = new Interest_t();
    newInterest->interestPrefix = interestPrefix;
    newInterest->lastTimeRequested = simTime();
    newInterest->totalTimesRequested = 1;
    newInterest->pendingConnections = std::vector<int>();
    newInterest->pendingConnections.push_back(senderAddress);
    newInterest->providingConnection = NULL;

    PIT.push_front(newInterest);

    stats->increaseCreatedInterests();
    stats->increaseRegisteredInterests();

    return true;
}

//Function used add another node to the interest list
bool ServiceManager::addToInterest(std::string interestPrefix, int senderAddress) {
    //std::cout << "(SM) Enter addToInterest\n";
    //std::cout.flush();

    //Unwrapping contents of request
    Interest_t* interest = getInterest(interestPrefix);
    if (interest == NULL) {
        EV_ERROR << "(SM) Error: Interest <" << interestPrefix << "> does not exist, unable to update.\n";
        EV_ERROR.flush();
        return false;
    }

    //std::cout << "(SM) <" << myId << "> Updating an interest for prefix <" << interestPrefix << "> for peer <" << senderAddress << ">\n";
    //std::cout.flush();

    //Updating Fields Accordingly
    for (auto it = interest->pendingConnections.begin(); it != interest->pendingConnections.end(); it++) {
        if ((*it) == senderAddress) {
            //std::cout << "(SM) Duplicate Interst.\n";
            //std::cout.flush();
            return false;
        }
    }

    interest->pendingConnections.push_back(senderAddress);
    interest->lastTimeRequested = simTime();
    interest->totalTimesRequested++;

    stats->increaseRegisteredInterests();

    return true;
}

//Function used remove a node to the interest list
bool ServiceManager::removeFromInterest(std::string interestPrefix, int senderAddress) {
    //std::cout << "(SM) Enter removeFromInterest\n";
    //std::cout.flush();

    //Unwrapping contents of request
    Interest_t* interest = getInterest(interestPrefix);
    if (interest == NULL) {
        EV_ERROR << "(SM) Error: Interest <" << interestPrefix << "> does not exist, unable to update.\n";
        EV_ERROR.flush();
        return false;
    }

    //
    for (auto it = interest->pendingConnections.begin(); it != interest->pendingConnections.end(); it++) {
        if ((*it) == senderAddress) {
            it = interest->pendingConnections.erase(it);
            if (interest->pendingConnections.size() == 0) {
                deleteInterest(interestPrefix);
            }
            break;
        }
    }

    EV_WARN << "(SM) Warning: Did not find an item part of interest to delete.\n";
    EV_WARN.flush();
    return false;
}

//
bool ServiceManager::deleteInterest(std::string interest) {
    //std::cout << "(SM) Enter deleteInterest\n";
    //std::cout.flush();

    for (auto it = PIT.begin(); it != PIT.end(); it++) {
        //Comparing Request ID
        if ((*it)->interestPrefix.compare(interest) == 0) {
            (*it)->pendingConnections.clear();
            (*it)->contentReference = NULL;
            (*it)->providingConnection = NULL;
            delete((*it));
            it = PIT.erase(it);
            return true;
        }
    }

    EV_WARN << "(SM) Warning: Did not find an interest to erase.\n";
    EV_WARN.flush();
    return false;
}

//
void ServiceManager::refreshNeighborhood() {
    //std::cout << "(SM) Enter refreshNeighborhood\n";
    //std::cout.flush();

    //Avoiding logging bad communication overhead prior to evaluation
    if (!stats->allowedToRun()) return;

    //Checking if any neighbors are stale
    double beaconInterval = par("beaconInterval").doubleValue();
    for (auto it = neighborList.begin(); it != neighborList.end();) {
        if ((*it).lastContact < simTime() - (beaconInterval * 5)) {
            simtime_t timeDif = (*it).lastContact - (*it).firstContact;
            stats->logContactDuration(roundf((timeDif.dbl() * 100) / 100));
            it = neighborList.erase(it);
        } else {
            it++;
        }
    }

    //Popping back of load window, adding new
    if ((int)networkLoadWindow.size() == slidingWindowSize) networkLoadWindow.pop_back();
    networkLoadWindow.push_front(instantBitLoad);

    //Calculating New Window in Sliding Window
    //TODO: (REVIEW) Sliding Window
    double averageNetworkLoad = 0;
    for(auto it = networkLoadWindow.begin(); it != networkLoadWindow.end();it++) {
        averageNetworkLoad += (*it);
    }
    averageNetworkLoad = averageNetworkLoad/(double)networkLoadWindow.size(); //(Number of slots in window)

    //TODO: Implement Exponential weight for Moving Average

    //Logging Statistics
    stats->logAverageLoad(myId,averageNetworkLoad);
    stats->logInstantLoad(myId,instantBitLoad);

    /*/
    if (stats->allowedToRun() && myId == 12) {
        std::cout << "\t(SM) <" << myId << ">\t Status:<" << networkLoadStatus << ">  I:<" << to_string(instantBitLoad) << "%>\tA:<" << averageNetworkLoad << "%>\t<" << to_string(currentBitLoad/(double)(1000000)) << "Mbps>\t <" << packetList.size() << "Msgs>\n";
        std::cout.flush();
    }
    //*/
}

//
bool ServiceManager::addNeighbor(WaveShortMessage *msg) {
    //std::cout << "(SM) Enter addNeighbor\n";
    //std::cout.flush();

    for (auto it = neighborList.begin(); it != neighborList.end();it++) {
        //If we already have this neighbor listed we just update its position and last contact  time
        if ((*it).neighborID == msg->getSenderAddress()) {
            (*it).lastContact = simTime();
            (*it).neighborPosition = msg->getSenderPos();
            (*it).load = static_cast<NetworkLoadStatus>(msg->par(MessageParameter::LOAD.c_str()).longValue());
            (*it).neighborCentrality = msg->par(MessageParameter::CENTRALITY.c_str()).longValue();
            return false;
        }
    }

    //Adding new neighbor to list
    Neighbor_t newNeigh;
    newNeigh.firstContact = simTime();
    newNeigh.lastContact = simTime();
    newNeigh.neighborID = msg->getSenderAddress();
    newNeigh.neighborPosition = msg->getSenderPos();
    newNeigh.load = static_cast<NetworkLoadStatus>(msg->par(MessageParameter::LOAD.c_str()).longValue());
    newNeigh.neighborCentrality = msg->par(MessageParameter::CENTRALITY.c_str()).longValue();
    neighborList.push_back(newNeigh);
    return true;
}

//
void ServiceManager::logNetworkmessage(WaveShortMessage *msg) {
    //std::cout << "(SM) Enter logNetworkmessage\n";
    //std::cout.flush();

    //Logging sent chunks
    if (strcmp(msg->getName(), MessageClass::DATA.c_str()) == 0) stats->increaseChunksSent(myId, 0);    //Request ID doesnt matter for chunk logging

    //Creating new Packet
    NetworkPacket_t freshPacket;
    freshPacket.arrival = simTime();
    freshPacket.bitSize = msg->getBitLength();

    //Checking if its a beacon
    if (strcmp(msg->getName(), MessageClass::BEACON.c_str()) == 0) {
        freshPacket.type = ContentClass::BEACON;
    } else {
        std::string prefixString = static_cast<cMsgPar*>(msg->getParList().get(MessageParameter::PREFIX.c_str()))->str();
        //If the message is not a beacon we figure out the type to save the message type
        switch(getClassFromPrefix(prefixString)) {
            case ContentClass::MULTIMEDIA:
                multimediaBitLoad += freshPacket.bitSize;
                break;
            case ContentClass::NETWORK:
                networkBitLoad += freshPacket.bitSize;
                break;
            case ContentClass::TRAFFIC:
                transitBitLoad += freshPacket.bitSize;
                break;
            default:
                std::cerr << "(SM) Error: Prefix type does not match any known category!\n";
                break;
        }
    }

    //Updating Loads
    currentBitLoad += freshPacket.bitSize;

    //Adding packet to list
    packetList.push_back(freshPacket);

    //Removing old packets from our count
    for (auto it = packetList.begin();it != packetList.end();){
        if ((*it).arrival + windowTimeSlotDuration < simTime()) {
            //Reducing Load and Removing Old packet from List
            currentBitLoad -= (*it).bitSize;

            //Checking which category we should reduce load from (if any)
            switch((*it).type) {
                case ContentClass::TRAFFIC:
                    transitBitLoad -= (*it).bitSize;
                    break;
                case ContentClass::NETWORK:
                    networkBitLoad -= (*it).bitSize;
                    break;
                case ContentClass::MULTIMEDIA:
                    multimediaBitLoad -= (*it).bitSize;
                    break;
                default:
                    //Nothing is done
                    break;
            }

            //Removing from List
            it = packetList.erase(it);
        } else {
            //Since network obtained packets are stored from older to newer, we can break
            break;
        }
    }

    //Calculating network load for the current moment
    instantBitLoad = 100 * (currentBitLoad/(double)bitrate);

    //Establishing in which network state we are currently
    if (instantBitLoad < lowMediumBandwidth) {
        networkLoadStatus = NetworkLoadStatus::LOW_LOAD;
    } else if (instantBitLoad > mediumHighBandwidth ) {
        networkLoadStatus = NetworkLoadStatus::HIGH_LOAD;
    } else {
        networkLoadStatus = NetworkLoadStatus::MEDIUM_LOAD;
    }
    //if (myId == 12 && stats->allowedToRun()) {
    //    std::cout << "instant Load: " << instantBitLoad << "\n";
    //}

    //std::cout << "(SM) Exit logNetworkmessage\n";
    //std::cout.flush();
}

//=============================================================
// MESSAGE TRANSMISSION AND ARRIVAL FUNCTIONS
//=============================================================

//
//Note: This is always called from the Content Store (technically once per Second or however long we set the refresh timer for it)
void ServiceManager::advertiseGPSItem(OverheardGPSObject_t mostPopularItem) {
    Enter_Method_Silent();
    WaveShortMessage * gpsBeaconMessage = prepareWSM(MessageClass::GPS_BEACON, beaconLengthBits, type_CCH, dataPriority, -1, -2);

    //Adding Centrality of node
    cMsgPar* popularPrefixParameter = new cMsgPar(MessageParameter::PREFIX.c_str());
    popularPrefixParameter->setStringValue(mostPopularItem.contentPrefix.c_str());
    gpsBeaconMessage->addPar(popularPrefixParameter);

    //Adding Local Load Perception
    cMsgPar* popularityFrequencyParameter = new cMsgPar(MessageParameter::FREQUENCY.c_str());
    popularityFrequencyParameter->setDoubleValue(mostPopularItem.referenceCount);
    gpsBeaconMessage->addPar(popularityFrequencyParameter);

    //Adding -1 as a representation of no request ID
    cMsgPar* requestIDParameter = new cMsgPar(MessageParameter::CONNECTION_ID.c_str());
    requestIDParameter->setLongValue(-1);
    gpsBeaconMessage->addPar(requestIDParameter);

    sendWSM(gpsBeaconMessage);
}

//Function that Sends Message directly to the Client
void ServiceManager::sendToClient(WaveShortMessage *msg) {
    //std::cout << "(SM) Enter sendToClient\n";
    //std::cout.flush();

    send(msg, "clientExchangeOut");
}

//Function responsible for sending messages to lower layers
void ServiceManager::sendWSM(WaveShortMessage* wsm) {
    double transmissionDelay = uniform(minimumForwardDelay,maximumForwardDelay);
    sendWSM(wsm,transmissionDelay);
}

//Function responsible for sending messages to lower layers
void ServiceManager::sendWSM(WaveShortMessage* wsm, double forwardDelay) {
    //std::cout << "(SM) Enter sendWSM\n";
    //std::cout.flush();

    if (isParking && !sendWhileParking) return;

    //Logging Message Sent Statistics
    logNetworkmessage(wsm);


    //This function call logs all outgoing network interface messages sent by this node (including beacon messages)
    sendDelayedDown(wsm,forwardDelay);
}

void ServiceManager::sendBeacon() {
    //std::cout << "(SM) Enter sendBeacon\n";
    //std::cout.flush();

    //Updating neighbor prior to beacon broadcast
    refreshNeighborhood();

    //std::cout << "(SM) <" << myId << "> Sending Beacon!\n";


    WaveShortMessage * beaconMessage = prepareWSM(MessageClass::BEACON, beaconLengthBits, type_CCH, beaconPriority, -1, -1);

    //Adding Centrality of node
    cMsgPar* neighborhoodSizeParameter = new cMsgPar(MessageParameter::CENTRALITY.c_str());
    neighborhoodSizeParameter->setLongValue(neighborList.size());
    beaconMessage->addPar(neighborhoodSizeParameter);

    //Adding Local Load Perception
    cMsgPar* loadParameter = new cMsgPar(MessageParameter::LOAD.c_str());
    loadParameter->setLongValue(instantBitLoad);
    beaconMessage->addPar(loadParameter);

    //Adding -1 as a representation of no request ID
    cMsgPar* requestIDParameter = new cMsgPar(MessageParameter::CONNECTION_ID.c_str());
    requestIDParameter->setLongValue(-1);
    beaconMessage->addPar(requestIDParameter);

    sendWSM(beaconMessage);
}

//Function called on SelfMessages amongst others. Forwards to HandleLowerMsg() so message management can be centralized.
//We override this function because VEINS has fucked up data typing policies and forces "Data" and "Beacon" types. Fuck that.
void ServiceManager::handleMessage(cMessage *msg) {
    //std::cout << "(SM) Enter handleMessage\n";
    //std::cout.flush();

    //Handling Self-Timers before everything as self-timer messages are simpler and did not come from any interface but from ourselves
    if (strcmp(msg->getName(), MessageClass::SELF_TIMER.c_str()) == 0) {
        WaveShortMessage* wsm = convertCMessage(msg);
        handleSelfTimer(wsm);
        return;
    } else if (strcmp(msg->getName(), MessageClass::SELF_BEACON_TIMER.c_str()) == 0) {
        sendBeacon();

        //Setting up a new message
        cancelEvent(sendBeaconEvt);
        scheduleAt(simTime() + par("beaconInterval").doubleValue(), sendBeaconEvt);
        return;
    }

    handleLowerMsg(msg);
}

//Function called to handle ALL messages from lower layers
void ServiceManager::handleLowerMsg(cMessage* msg) {
    //std::cout << "(SM) Enter handleLowerMsg\n";
    //std::cout.flush();

    WaveShortMessage* wsm = convertCMessage(msg);
    cGate* inputGate = wsm->getArrivalGate();
    //int recepient = wsm->getRecipientAddress();

    //Checking Message Input Gate
    //CLIENT LINK
    if (inputGate == NULL || inputGate->getBaseId() == clientExchangeIn) {
        wsm->setSenderAddress(myId);    //Something was causing this to happen so we'll deal with it
        onNetworkMessage(wsm);          //We're treating client messages as network messages cause they technically behave the same and the only difference is how we reply (we skip steps with local clients)

    //NETWORK LINK
    } else if (inputGate->getBaseId() == lowerLayerIn) {
        //This function call, placed right here, logs all messages incoming from the network interface
        logNetworkmessage(wsm);
        onNetworkMessage(wsm);
    //OTHER PORT
    } else {
        std::cerr << "(SM) Error: Unknown Network Interface <" << inputGate->getBaseId() << ">.\n";
        std::cerr.flush();
    }

    updateNodeColor();
}

//=============================================================
// MOBILITY & OTHER UNUSED FUNCTIONS
//=============================================================

//IGNORE : Function from Base Class
void ServiceManager::onBeacon(WaveShortMessage* wsm) {
    //std::cout << "(SM) <" << myId << "> Got a Beacon!\n";
    addNeighbor(wsm);
    delete (wsm);
}

//IGNORE : Function from Base Class
void ServiceManager::onData(WaveShortMessage* wsm) {
    handleContentMessage(wsm);
}

//IGNORE : Function from Base Class
void ServiceManager::receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj) {
    Enter_Method_Silent();
    if (signalID == mobilityStateChangedSignal) {
        handlePositionUpdate(obj);
    } else if (signalID == parkingStateChangedSignal) {
        handleParkingUpdate(obj);
    }
}

//IGNORE : Function from Base Class
void ServiceManager::handleParkingUpdate(cObject* obj) {
}

//IGNORE : Function from Base Class
void ServiceManager::handlePositionUpdate(cObject* obj) {
    BaseWaveApplLayer::handlePositionUpdate(obj);
}

//=============================================================
// EXTRA FUNCTIONALITIES
//=============================================================

WaveShortMessage* ServiceManager::convertCMessage(cMessage* msg) {
    //std::cout << "(SM) Enter convertCMessage\n";
    //std::cout.flush();

    WaveShortMessage* wsm = dynamic_cast<WaveShortMessage*>(msg);
    ASSERT(wsm);
    return wsm;
}

ContentClass ServiceManager::getClassFromPrefix(string prefix) {
    //std::cout << "(SM) Enter getClassFromPrefix\n";
    //std::cout.flush();

    //Getting the message request prefix
    if (prefix.c_str()[0] == '\"') {
        prefix = prefix.substr(1, prefix.length() - 2);
    }

    //Comparing against the multimedia prefix
    if (library->transitPrefix.compare(prefix) < 0) {
        return ContentClass::TRAFFIC;
    } else if (library->networkPrefix.compare(prefix) < 0) {
        return ContentClass::NETWORK;
    } else if (library->multimediaPrefix.compare(prefix) < 0) {
        return ContentClass::MULTIMEDIA;
    } else {
        std::cerr << "(SM) Error: Prefix type does not match any known category!\n";
    }
    return ContentClass::EMERGENCY_SERVICE;
}
