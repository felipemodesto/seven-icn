//Concent Centric Class - Felipe Modesto

#include <paradise/bacon/BaconClient.hh>

using Veins::TraCIMobilityAccess;
using Veins::AnnotationManagerAccess;

const simsignalwrap_t BaconClient::parkingStateChangedSignal = simsignalwrap_t(TRACI_SIGNAL_PARKING_CHANGE_NAME);

Define_Module(BaconClient);

//Initialization Function
void BaconClient::initialize(int stage) {
    //Initializing
    BaseWaveApplLayer::initialize(stage);

    switch(stage){
        case 0: {
            //Getting TraCI Manager (SUMO Connection & Annotations Ready)
            //traci = TraCIMobilityAccess().get(getParentModule());
            traci =  check_and_cast<Veins::TraCIMobility*>(getParentModule()->getSubmodule("veinsmobility"));

            annotations = AnnotationManagerAccess().getIfExists();
            ASSERT(annotations);

            //Setting Gates
            clientExchangeIn  = findGate("clientExchangeIn");
            clientExchangeOut = findGate("clientExchangeOut");

            //Getting Parameters
            minimumRequestDelay = par("minimumRequestDelay").doubleValue();
            maximumRequestDelay = par("maximumRequestDelay").doubleValue();
            locationTimerDelay = par("locationTimerDelay").doubleValue();
            requestTimeout = par("requestTimeout").doubleValue();

            multimediaInterest = par("multimediaInterest").doubleValue();
            trafficInterest = par("trafficInterest").doubleValue();
            networkInterest = par("networkInterest").doubleValue();
            emergencyInterest = par("emergencyInterest").doubleValue();

            maxOpenRequests = par("maxOpenRequests").doubleValue();

            GoodReplyRequests = 0;
            BadReplyRequests = 0;
            NoReplyRequests = 0;

            //3D Canvas UI Stuff
            //trailLength = par("trailLength");
            //modelURL = par("modelURL").stringValue();
            //showTxRange = par("showTxRange");
            //txRange = par("txRange");
            //labelColor = par("labelColor").stringValue();
            //rangeColor = par("rangeColor").stringValue();
            //trailColor = par("trailColor").stringValue();

            lastX = 0;
            lastY = 0;

            break;
        }

        case 2: {
//            auto scene = OsgEarthScene::getInstance()->getScene(); // scene is initialized in stage 0 so we have to do our init in stage 1
//            mapNode = osgEarth::MapNode::findMapNode(scene);
//
//            // build up the node representing this module
//            // an ObjectLocatorNode allows positioning a model using world coordinates
//            locatorNode = new osgEarth::Util::ObjectLocatorNode(mapNode->getMap());
//            auto modelNode = osgDB::readNodeFile(modelURL);
//            if (!modelNode)
//                throw cRuntimeError("Model file \"%s\" not found", modelURL.c_str());
//
//            // disable shader and lighting on the model so textures are correctly shown
//            modelNode->getOrCreateStateSet()->setAttributeAndModes(
//                                new osg::Program(),
//                                osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
//            modelNode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
//            //modelNode->getOrCreateStateSet()->setMode(GL_3D_COLOR_TEXTURE,)
//
//            const char *modelColor = par("modelColor");
//            if (*modelColor != '\0') {
//                auto color = osgEarth::Color(modelColor);
//                auto material = new osg::Material();
//                material->setAmbient(osg::Material::FRONT_AND_BACK, color);
//                material->setDiffuse(osg::Material::FRONT_AND_BACK, color);
//                material->setAlpha(osg::Material::FRONT_AND_BACK, 1.0);
//                modelNode->getOrCreateStateSet()->setAttribute(material, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
//            }
//
//            auto objectNode = new cObjectOsgNode(this);  // make the node selectable in Qtenv
//            objectNode->addChild(modelNode);
//            locatorNode->addChild(objectNode);
//
//            // create a node showing the transmission range
//            if (showTxRange) {
//                Style rangeStyle;
//                rangeStyle.getOrCreate<PolygonSymbol>()->fill()->color() = osgEarth::Color(rangeColor);
//                rangeStyle.getOrCreate<AltitudeSymbol>()->clamping() = AltitudeSymbol::CLAMP_TO_TERRAIN;
//                rangeStyle.getOrCreate<AltitudeSymbol>()->technique() = AltitudeSymbol::TECHNIQUE_DRAPE;
//                rangeNode = new CircleNode(mapNode.get(), GeoPoint::INVALID, Linear(txRange, Units::METERS), rangeStyle);
//                locatorNode->addChild(rangeNode);
//            }
//
//            // create a node containing a track showing the past trail of the model
//            if (trailLength > 0) {
//                trailStyle.getOrCreate<LineSymbol>()->stroke()->color() = osgEarth::Color(trailColor);
//                trailStyle.getOrCreate<LineSymbol>()->stroke()->width() = 50.0f;
//                trailStyle.getOrCreate<AltitudeSymbol>()->clamping() = AltitudeSymbol::CLAMP_RELATIVE_TO_TERRAIN;
//                trailStyle.getOrCreate<AltitudeSymbol>()->technique() = AltitudeSymbol::TECHNIQUE_DRAPE;
//                auto geoSRS = mapNode->getMapSRS()->getGeographicSRS();
//                trailNode = new FeatureNode(mapNode.get(), new Feature(new LineString(), geoSRS));
//                locatorNode->addChild(trailNode);
//            }
//
//            // add the locator node to the scene
//            mapNode->getModelLayerGroup()->addChild(locatorNode);

            cache = check_and_cast<BaconContentProvider *>(getParentModule()->getSubmodule("content"));
            cSimulation *sim = getSimulation();
            cModule *modp = sim->getModuleByPath("BaconScenario.statistics");
            stats = check_and_cast<BaconStatistics *>(modp);

            cModule *modlib = sim->getModuleByPath("BaconScenario.library");
            library = check_and_cast<BaconLibrary *>(modlib);

            //pendingRequest = NULL;
            contentTimerMessage = NULL;

            //Won't start timer for server nodes or if we're one of those nodes that doesn't make its own requests (MULE)
            if (minimumRequestDelay != -1 && maximumRequestDelay != -1) {
                //Starting First Request for after warmup time
                simtime_t firstTimerTime = uniform(minimumRequestDelay,maximumRequestDelay);
                startNewMessageTimer(firstTimerTime + stats->getStartTime() - minimumRequestDelay);
            } else {
                //We're a server. Should we do something?
            }

            runtimeTimer = NULL;
            //Checking if we're recording location
            if (stats->recordingPosition()) {
                runtimeTimer = new cMessage("runtimeTimer");
                resetLocationTimer();
            }
            break;
        }
        default:
            break;
    }
}

//Function called prior to destructor where we close off statistics and whatnots
void BaconClient::finish() {
    if (contentTimerMessage != NULL) {
        cancelAndDelete(contentTimerMessage);
        contentTimerMessage = NULL;
    }

    if (runtimeTimer != NULL) {
        cancelAndDelete(runtimeTimer);
        runtimeTimer = NULL;
    }

    //Log Good, Bad and Unserved Requests
    std::cout << "(Cl) Stats for <" << myId << "> (" << GoodReplyRequests << "-" << BadReplyRequests << "-" << NoReplyRequests << ")\n";
    std::cout.flush();
}

/*/
//OMnet's Display update function, called every "frame"
void BaconClient::refreshDisplay() const {
    auto geoSRS = mapNode->getMapSRS()->getGeographicSRS();
    //double modelheading = fmod((360 + 90 + heading), 360) - 180;
    double longitude = getLongitude();
    double latitude = getLatitude();

    //Veins::TraCIMobility* mobility =  check_and_cast<Veins::TraCIMobility*>(getParentModule()->getSubmodule("veinsmobility"));
    Veins::TraCICommandInterface* traciInterface = traci->getCommandInterface();
    Coord currPos = traci->getCurrentPosition();
    std::pair<double, double> currLonLat = traciInterface->getLonLat(currPos);
    double angle = traci->getAngleRad();

    longitude = currLonLat.first;
    latitude = currLonLat.second;

    //double modelheading = fmod((360 + 90 + angle), 360) - 180;
    //EV << "LAT,LONG = < " << latitude << " , " << longitude << " >";

    locatorNode->getLocator()->setPosition(osg::Vec3d(longitude, latitude, 1.5));  // set altitude mode instead of fixed altitude
    locatorNode->getLocator()->setOrientation(osg::Vec3d(90 + angle * 180 / 3.14, 0, 0));
    //locatorNode->getLocator()->setOrientation(osg::Vec3d(modelheading, 0, 0));


    // if we are showing the model's track, update geometry in the trackNode
    if (trailNode) {
        // create and assign a new feature containing the updated geometry
        // representing the movement trail as continuous line segments
        auto trailFeature = new Feature(new LineString(&trail), geoSRS, trailStyle);
        trailFeature->geoInterp() = GEOINTERP_GREAT_CIRCLE;
        trailNode->setFeature(trailFeature);
    }


    // re-position the range indicator node
    if (showTxRange) rangeNode->setPosition(GeoPoint(geoSRS, longitude, latitude));

    // update the position on the 2D canvas, too
    //getDisplayString().setTagArg("p", 0, x);
    //getDisplayString().setTagArg("p", 1, y);

}
//*/

//
void BaconClient::resetLocationTimer() {
    if (runtimeTimer == NULL) return;
    scheduleAt(simTime() + 1, runtimeTimer);
}

//
void BaconClient::notifyLocation() {
    //Getting Vehicle's current position
    Coord currPos = traci->getCurrentPosition();

    //Recording statistics to statistics class

    //Only recording location statistics if vehicle changes position
    double curX = round(currPos.x);
    double curY = round(currPos.y);
    if (curX != lastX || curY != lastY ) {
        lastX = curX;
        lastY = curY;
        stats->logPosition(curX,curY);
    }
    resetLocationTimer();
}

//
void BaconClient::startNewMessageTimer() {
    startNewMessageTimer(requestTimer);
    stats->keepTime();
}

//
void BaconClient::startNewMessageTimer(simtime_t timerTime) {
    requestTimer = uniform(minimumRequestDelay,maximumRequestDelay);

    //Old method used to check if we had an ongoing request to stop us from having multiple
    //For now we will ALWAYS start a new request timer
    //if (pendingRequest != NULL) {
    //    if ( pendingRequest->contentStatus == ContentStatus::SERVED ) {
    //        std::cout << "(SM) This should have been Cleared. o.õ\n";
    //        std::cout.flush();
    //        completedRequests.push_front(pendingRequest);
    //        //delete(pendingRequest);
    //        pendingRequest = NULL;
    //    }
    //    //else {
    //    //    EV << "Request has not been fulfilled and will be kept in queue.\n";
    //    //    EV.flush();
    //    //}
    //}

    if (contentTimerMessage == NULL) {
        contentTimerMessage = new cMessage("contentTimerMessage");
    } else {
        cancelEvent(contentTimerMessage);
    }

    //Scheduling the self-timer for the service request message
    scheduleAt(simTime() + timerTime, contentTimerMessage);
}

//=============================================================
// CONTENT REQUEST MANIPULATION FUNCTIONS
//=============================================================

void BaconClient::cleanRequestList() {
    //Checking Ongoing Connections
    for(auto it = ongoingRequests.begin(); it != ongoingRequests.end();) {
        //Checking if the request has gone super stale
        if ((*it)->requestTime + requestTimeout < simTime()) {
            (*it)->contentStatus = ContentStatus::UNSERVED;
            (*it)->fullfillTime = simTime();
            backloggedRequests.push_front(*it);

            NoReplyRequests++;
            it = ongoingRequests.erase(it);
        } else {
            it++;
        }
    }

    //Checking Stale/Backlogged Connections
    for(auto it = backloggedRequests.begin(); it != backloggedRequests.end();) {
        //Checking if the request has gone super stale
        if ((*it)->requestTime + (10*requestTimeout) < simTime()) {
            //Removing super stale request
            it = backloggedRequests.erase(it);
        } else {
            it++;
        }
    }

    //Checking complete Connections
    for(auto it = completedRequests.begin(); it != completedRequests.end();) {
        //Checking if the request has gone super stale
        if ((*it)->requestTime + (10*requestTimeout) < simTime()) {
            //Removing super stale request
            it = completedRequests.erase(it);
        } else {
            it++;
        }
    }
}

//
void BaconClient::startContentRequest() {
    //Checking if we're in a ready communication state
    if (!stats->allowedToRun()) {
        return;
    }

    //Checking if we are a server
    if (minimumRequestDelay == -1 && maximumRequestDelay == -1) {
        //Being a Server we don't request content
        return;
    }

    cleanRequestList();

    //TODO: (DECIDE) Decide if we should limit concurrent requests at the client level
    if ((int)ongoingRequests.size() >= (int)maxOpenRequests) {
        startNewMessageTimer();
        return;
    }

    t_channel channel = dataOnSch ? type_SCH : type_CCH;
    WaveShortMessage * requestMessage = prepareWSM(MessageClass::INTEREST, headerLength, channel, dataPriority, -1, 2);

    //Choosing content request type
    double contentInterest = multimediaInterest + networkInterest + trafficInterest + emergencyInterest;
    double contentRequestInterstType = uniform(0, contentInterest);
    int contentClass = 0;
    Content_t* curRequest;
    int requestedContentIndex = 0;

    //WE'll use the following sequence / ranges:
    //      TRAFFIC                 NETWORK                 MULTIMEDIA
    //range 0-->traffic / traffic-->traffic+network / traffic+network->sum
    std::list<Content_t>* appropriateLibrary;

    //TRAFFIC
    if (contentRequestInterstType < trafficInterest) {
        appropriateLibrary = library->getTrafficContentList();
        contentClass = library->getContentClass(ContentClass::TRAFFIC);
        double randomIndex = uniform(0,1);
        requestedContentIndex = library->getIndexForDensity(randomIndex,ContentClass::TRAFFIC);

    //NETWORK
    } else if (contentRequestInterstType < trafficInterest + networkInterest ) {
        appropriateLibrary = library->getNetworkContentList();
        contentClass = library->getContentClass(ContentClass::NETWORK);
        double randomIndex = uniform(0,1);
        requestedContentIndex = library->getIndexForDensity(randomIndex,ContentClass::NETWORK);

    //MULTIMEDIA
    } else if (contentRequestInterstType < trafficInterest + networkInterest + multimediaInterest ) {
        appropriateLibrary = library->getMultimediaContentList();
        contentClass = library->getContentClass(ContentClass::MULTIMEDIA);
        double randomIndex = uniform(0,1);
        requestedContentIndex = library->getIndexForDensity(randomIndex,ContentClass::MULTIMEDIA);

    } else {
        //TODO (IMPLEMENT) Emergency and GPS related request Events
        /*
        //Setting request parameters
        requestedContent = 1;

        //Attaching location info to message
        //Veins::TraCIMobility* mobility =  check_and_cast<Veins::TraCIMobility*>(getParentModule()->getSubmodule("veinsmobility"));
        Coord currPos = traci->getCurrentPosition();
        std::string coordString = std::to_string(floor(currPos.x)) + ";" + std::to_string(floor(currPos.y)) + ";" + std::to_string(floor(currPos.z));

        EV << "(Cl) Current Vehicle Position < " << coordString << " >\n";
        EV.flush();

        //Adding Coordinate Parameter to Request Message
        cMsgPar* locationParameter = new cMsgPar(MessageParameter::COORDINATES.c_str());
        locationParameter->setStringValue(coordString.c_str());
        requestMessage->addPar(locationParameter);
        */
    }

    //Looking for item in content library
    int itemCounter = 0;
    for (auto it = appropriateLibrary->begin(); itemCounter < requestedContentIndex; it++, itemCounter++) {
        curRequest = &*it;
    }

    //Copying data from currentRequest to our PendingRequest (I don't know an easier way to memcopy the object pointed by an iterator, please bear with me on this one
    PendingContent_t* pendingRequest = new PendingContent_t();
    pendingRequest->contentClass = curRequest->contentClass;
    pendingRequest->contentStatus = curRequest->contentStatus;
    pendingRequest->contentPrefix = curRequest->contentPrefix;
    pendingRequest->contentSize = -1;   //We don't necessarily know what the size of the content item is
    pendingRequest->requestTime = simTime();
    pendingRequest->fullfillTime = SimTime::ZERO;   //We set it to zero so we know this can't have happened
    int messageID = library->getRequestIndex();
    pendingRequest->pendingID = messageID;

    //If by some weird reason we got a null object then we'll discard and try again later
    if (pendingRequest == NULL) {
        std::cerr << "(Cl) Error: <" << myId << "> Generated broken Request for Object <" + pendingRequest->contentPrefix + ">";
        std::cerr .flush();
        return;
    }

    //std::cout << "(Cl) <" << myId << "> NR for <" << pendingRequest->contentPrefix << "> of class <" << static_cast<int>(pendingRequest->contentClass) << "> Time<" << simTime() << ">\n";
    //std::cout.flush();

    //std::cout << "(Cl) <" << contentRequestInterstType << ">\t->\t<" << pendingRequest->contentPrefix << ">\n";
    //std::cout.flush();

    //Adding new request to our request list
    ongoingRequests.push_back(pendingRequest);

    //Content Type
    cMsgPar* contentTypeParameter = new cMsgPar(MessageParameter::CLASS.c_str());
    contentTypeParameter->setLongValue(contentClass);
    requestMessage->addPar(contentTypeParameter);

    //Named Prefix for specific content
    cMsgPar* contentNameParameter = new cMsgPar(MessageParameter::PREFIX.c_str());
    contentNameParameter->setStringValue(pendingRequest->contentPrefix.c_str());
    requestMessage->addPar(contentNameParameter);

    //Adding Priority for Content
    cMsgPar* priorityParameter = new cMsgPar(MessageParameter::PRIORITY.c_str());
    priorityParameter->setLongValue(static_cast<int>(pendingRequest->priority));
    requestMessage->addPar(priorityParameter);

    //Adding Size for Content
    cMsgPar* sizeParameter = new cMsgPar(MessageParameter::SIZE.c_str());
    sizeParameter->setLongValue(pendingRequest->contentSize);
    requestMessage->addPar(sizeParameter);

    //Adding User ID
    cMsgPar* clientIDParameter = new cMsgPar(MessageParameter::PEER_ID.c_str());
    clientIDParameter->setLongValue(myId);
    requestMessage->addPar(clientIDParameter);

    //ID for Content
    cMsgPar* idParameter = new cMsgPar(MessageParameter::CONNECTION_ID.c_str());
    idParameter->setLongValue(messageID);
    requestMessage->addPar(idParameter);

    //Adding Message Type (Interest)
    cMsgPar* classParameter = new cMsgPar(MessageParameter::TYPE.c_str());
    classParameter->setStringValue(MessageClass::INTEREST.c_str());
    requestMessage->addPar(classParameter);


    //Adding Uphop Count (Interest)
    cMsgPar* hopsParameter = new cMsgPar(MessageParameter::HOPS_UP.c_str());
    hopsParameter->setLongValue(-1);    //Negative Uphop count at client level (set to 0 locally at Service Manager)
    requestMessage->addPar(hopsParameter);

    //Adding our local position at time of content request
    requestMessage->setSenderPos(traci->getPositionAt(simTime()));
    requestMessage->setSenderAddress(myId);

    //Logging the statistics about the content request about to be made
    stats->logContentRequest(pendingRequest->contentPrefix);

    //Sending request
    sendWSM(requestMessage);
}

//
void BaconClient::handleMessage(cMessage *msg) {
    if ( msg == contentTimerMessage ) {
        //Starting a new request
        startContentRequest();

        //Creating timer for next request
        startNewMessageTimer();
    } else if (msg == runtimeTimer) {
        notifyLocation();
    } else {
       handleLowerMsg(msg);
    }
}

//=============================================================
// CONTENT TRANSMISSION FUNCTIONS
//=============================================================


//Function that Sends Message directly to the Content Provider
void BaconClient::sendToServiceManager(cMessage *msg) {
    send(msg, "clientExchangeOut");
}

//=============================================================
// MOBILITY & OTHER UNUSED FUNCTIONS
//=============================================================

/**/
//Generic Broadcast Message Transmission Function
void BaconClient::sendMessage(std::string messageContent) {
    std::cerr << "(Cl) Error: sendMessage method should not be called in the Client Class!\n";
    std::cerr.flush();
}

//Generic funciton called upon receiving a beacon message - AKA : IGNORE Clients do not implement this feature
void BaconClient::onBeacon(WaveShortMessage* wsm) {
    std::cerr << "(Cl) Error: onBeacon method should not be called in the Client Class!\n";
    std::cerr.flush();
}

//
void BaconClient::onData(WaveShortMessage* wsm) {
    //Checking the message type
    if (strcmp(wsm->getName(),MessageClass::INTEREST.c_str()) == 0 ) {
        std::cerr << "(Cl) Error: Lookup Responses should be of type DATA (or similar replies to client...\n";
        std::cerr.flush();
        return;
    }

    //Getting parameters from message
    cArray parArray = wsm->getParList();
    cMsgPar* prefixPar = static_cast<cMsgPar*>(parArray.get(MessageParameter::PREFIX.c_str()));
    cMsgPar* idPar = static_cast<cMsgPar*>(parArray.get(MessageParameter::CONNECTION_ID.c_str()));
    std::string prefixString = prefixPar->str();
    int requestID = idPar->longValue();

    //Fixing the god damn string
    if (prefixString.c_str()[0] == '\"') {
        prefixString = prefixString.substr(1, prefixString.length() - 2);
    }
    prefixString.erase(std::remove(prefixString.begin(), prefixString.end(), '\"'), prefixString.end());

    //bool foundBacklog = false;
    bool foundRequest = false;
    PendingContent_t* desiredRequest = NULL;

    //Searching for the request in our list of ongoing requests
    for (auto it = ongoingRequests.begin(); it != ongoingRequests.end();) {
        if ( prefixString.compare((*it)->contentPrefix) == 0 && requestID == (*it)->pendingID ) {
            foundRequest = true;
            desiredRequest = *it;

            it = ongoingRequests.erase(it);
            break;
        } else {
            it++;
        }
    }

    //If the current respond does not match to the current pending request (given it exists)
    if (!foundRequest) {
        for (auto it = backloggedRequests.begin(); it != backloggedRequests.end();) {
            //std::cerr << "\t<" << it->contentPrefix << ">.\n";
            if (prefixString.compare((*it)->contentPrefix) == 0 && requestID == (*it)->pendingID) {
                foundRequest = true;
                desiredRequest = *it;
                stats->increasedBackloggedResponses();

                if ((*it)->contentStatus != ContentStatus::UNSERVED) {
                    std::cerr << "(Cl) Error: Found Request for <" << prefixString << "> on backlog list.\n";
                    std::cerr.flush();
                }

                it = backloggedRequests.erase(it);
                break;
            } else {
                it++;
            }
        }
    }

    //If we haven't found the request in the backlog, the only locally available option is in the complete list
    if (!foundRequest) {
        for (auto it = completedRequests.begin(); it != completedRequests.end(); it++) {
            if (prefixString.compare((*it)->contentPrefix) == 0 && requestID == (*it)->pendingID) {
                //We should be fine here, as we have confirmed having received the content we don't care about its future
                foundRequest = true;
                desiredRequest = *it;
                //We don't do anything extra fancy here cause we're done, our packet is complete and we don't have to re-mark it as complete
                return;
            }
        }
    }

    //Checking if we simply don't have this request anywhere, which is pretty wtf to be honest and would imply a logic error in the service manager
    if (desiredRequest == NULL) {
        std::cerr << "(Cl) Error: We got data but had no pending requests! Check ServiceManager for potential Network Data being routed to local client.\n";
        std::cerr.flush();

        if (!foundRequest) {
            std::cerr << "\t(Cl) Error: <" << myId << "> We got the following response <" << wsm->getName() << ";" << wsm->getKind() << "> but had no pending requests for <" << prefixString << ">!\n\tPlease Check ServiceManager for potential Network Data being routed to local client.\n";
            std::cerr.flush();
            return;
        }
    }


    if (strcmp(wsm->getName(),MessageClass::DATA.c_str()) == 0 ) {

        desiredRequest->fullfillTime = simTime();
        SimTime difTime = desiredRequest->fullfillTime - desiredRequest->requestTime;

        switch( wsm->getKind() ) {
            case ConnectionStatus::DONE_AVAILABLE:
            case ConnectionStatus::DONE_RECEIVED:
                {
                    GoodReplyRequests++;
                    stats->increasePacketsSent();

                    //std::cout << "+";
                    double difDouble = difTime.dbl();
                    if (difTime <= requestTimeout) {
                        stats->addcompleteTransmissionDelay(difDouble);
                        stats->increaseMessagesSent(desiredRequest->contentClass);
                    }
                    else {
                        std::cout << "(Cl) Warning: Stale Response with delay: <" << difDouble << ">\n";
                        std::cout.flush();
                        //stats->addincompleteTransmissionDelay(difDouble);
                    }

                    //Logging Time it took for communication
                    completedRequests.push_front(desiredRequest);
                }
                break;

            case ConnectionStatus::DONE_UNAVAILABLE:
            case ConnectionStatus::DONE_NO_DATA:
                {
                    NoReplyRequests++;
                    stats->increasePacketsUnserved();

                    //std::cout << "?";
                    double difDouble = difTime.dbl();
                    stats->addincompleteTransmissionDelay(difDouble);
                    stats->increaseMessagesUnserved(desiredRequest->contentClass);

                    //Adding an incomplete transfer to our backlogged list for future tests
                    backloggedRequests.push_front(desiredRequest);
                }
                break;

            case ConnectionStatus::DONE_PARTIAL:
                {
                    BadReplyRequests++;
                    stats->increasePacketsLost();

                    //std::cout << "-";
                    //Adding an incomplete transfer to our backlogged list for future tests
                    backloggedRequests.push_front(desiredRequest);
                    stats->increaseMessagesLost(desiredRequest->contentClass);

                    double difDouble = difTime.dbl();
                    stats->addincompleteTransmissionDelay(difDouble);
                }
                break;

            default:
                std::cerr << "(Cl) Error: <" << myId << "> Client Response state invalid. State received <" << wsm->getKind() << "> Note: THIS MAKES NO SENSE o.o.\n";
                std::cerr.flush();
                break;
        }

        //Marking the request as served, as we are done with it
        desiredRequest->contentStatus = ContentStatus::SERVED;

    } else {
        std::cout << "(Cl) THIS SHOULD NOT HAPPEN <" << wsm->getName() << ">\n";
        std::cout.flush();

        EV << "(Cl) THIS ELSE CASE SHOULD NEVER OCCUR.\n";
        EV.flush();
    }
}

//IGNORE
void BaconClient::receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj) {
}

//IGNORE
void BaconClient::handleParkingUpdate(cObject* obj) {
}

//IGNORE
void BaconClient::handlePositionUpdate(cObject* obj) {
}

//IGNORE
void BaconClient::sendWSM(WaveShortMessage* wsm) {
    recordPacket(PassedMessage::OUTGOING, PassedMessage::LOWER_DATA, wsm);
    sendDelayed(wsm, 0, clientExchangeOut);
}
//*/
