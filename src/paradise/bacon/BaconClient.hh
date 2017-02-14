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

#ifndef BaconClient_H
#define BaconClient_H

#include <paradise/bacon/BaconStructures.hh>

#include <paradise/bacon/BaconLibrary.hh>
#include <paradise/bacon/BaconContentProvider.hh>

using namespace omnetpp;
using namespace osgEarth;
using namespace osgEarth::Annotation;
using namespace osgEarth::Features;

/**
 * Application Layer Service Manager
 */
//class BaconClient : public BaseWaveApplLayer , public IMobileNode {
class BaconClient : public BaseWaveApplLayer {

    public:
        virtual void initialize(int stage) override;
        virtual void finish();
        virtual void receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj) override;

        //double getX() const override { return x; }
        //double getY() const override { return y; }
        //double getLatitude() const override { return OsgEarthScene::getInstance()->toLatitude(y); }
        //double getLongitude() const override { return OsgEarthScene::getInstance()->toLongitude(x); }
        //double getTxRange() const override { return txRange; }

    protected:
        //==========================================//
        //BASE STUFF
        static const simsignalwrap_t parkingStateChangedSignal;
        TraCIMobility* traci;
        AnnotationManager* annotations;
        simtime_t lastDroveAt;

        //==========================================//
//        //OSG EARTH STUFF
//        // the node containing the osgEarth data
//        osg::observer_ptr<osgEarth::MapNode> mapNode = nullptr;
//        // osgEarth node for 3D visualization
//        osg::ref_ptr<osgEarth::Util::ObjectLocatorNode> locatorNode = nullptr;
//        // range indicator node
//        osg::ref_ptr<osgEarth::Annotation::CircleNode> rangeNode = nullptr;
//        // trail annotation
//        osg::ref_ptr<osgEarth::Annotation::FeatureNode> trailNode = nullptr;
//        osgEarth::Style trailStyle;
//        osgEarth::Vec3dVector trail;  // recently visited points
//        std::string modelURL;
//        std::string labelColor;
//        std::string rangeColor;
//        std::string trailColor;
//        unsigned int trailLength;
//        bool showTxRange;
//        double txRange;
//        // node position and heading (speed is constant in this model)
//        double heading;  // in degrees
//        double x, y;  // in meters, relative to playground origin


        //==========================================//
        //VEINS MOBILITY STUFF

        cMessage *runtimeTimer;             //Timer used for second to second updates
        cMessage *contentTimerMessage;      //Self message sent in timer
        simtime_t requestTimer;             //Time until user demands content

        //==========================================//
        //MY STUFF!

        BaconStatistics* stats;
        BaconLibrary* library;
        BaconContentProvider* cache;

        int clientExchangeIn;           //Input from Service Manager
        int clientExchangeOut;          //Output to Service Manager

        int maxOpenRequests;
        double locationTimerDelay;      //Time between calls for location
        double minimumRequestDelay;     //Minimum time until Content Request
        double maximumRequestDelay;     //Maximum time until Content Request
        double requestTimeout;          //

        double multimediaInterest;      //Interest in Multimedia Content
        double trafficInterest;         //Interest in Road Traffic Information Content
        double networkInterest;         //Interest in Network Information Content
        double emergencyInterest;       //Emergency Information "Interest"

        int GoodReplyRequests;
        int BadReplyRequests;
        int NoReplyRequests;

        //Last Known coordinates
        double lastX;
        double lastY;

        //PendingContent_t* pendingRequest;//Current Pending Request
        std::list<PendingContent_t*> ongoingRequests;       //List of ongoing Requests
        std::list<PendingContent_t*> backloggedRequests;    //List of content to which a negative reply was given, hopefully so that we can track late responses somehow
        std::list<PendingContent_t*> completedRequests;     //List of content to which a positive reply was given, so we can track duplicate responses

    protected:

        virtual int numInitStages () const {
            return 3;
        }

        //virtual void refreshDisplay() const override;
        //==========================================//
        virtual void resetLocationTimer();                          //RESETS THE TIMER DELAY FOR LOCATION TIMER MESSAGE
        virtual void notifyLocation();                              //NOTIFIES STATISTICS CLASS OF POSITION (SHOULD BE CALLED EVERY SECOND)
        void cleanRequestList();                                    //Deletes old entries from our request lists
        //==========================================//
        void onBeacon(WaveShortMessage* wsm)  override;             //ON WSM BEACON MESSAGE
        void onData(WaveShortMessage* wsm)  override;               //ON WSM DATA MESSAGE
        void handlePositionUpdate(cObject* obj) override;
        void sendWSM(WaveShortMessage* wsm)  override;
        void sendMessage(std::string messageContent);               //USED TO SEND WSM MESSAGES
        void handleParkingUpdate(cObject* obj);
        //==========================================//
        void startNewMessageTimer(simtime_t timerTime);
        void startNewMessageTimer();
        void startContentRequest();
        void handleMessage(cMessage *msg)  override;                //CALLBACK FROM SELF MESSAGE, GENERALLY USED AS RANDOM TIMER CALLBACK
        void sendToServiceManager(cMessage *msg);
};


#endif
