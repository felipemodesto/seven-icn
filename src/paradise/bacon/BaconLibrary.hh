/*
 * BaconLibrary.hh
 *
 *  Created on: Jan 25, 2016
 *      Author: felipe
 */

#ifndef BACONLIBRARY_H
#define BACONLIBRARY_H

#include <paradise/bacon/BaconStructures.hh>

#include <paradise/bacon/BaconStatistics.hh>
#include <paradise/bacon/BaconClient.hh>
class BaconStatistics;
class BaconClient;

using namespace omnetpp;
using namespace std;

//Content Library. Generates random content list and provides to whomever needs for simulation purposes. (Singleton)
class BaconLibrary : public omnetpp::cSimpleModule {

private:
    //Keeping copies from appearing
    //BaconLibrary(){};
    //BaconLibrary(BaconLibrary const& copy){};     //This should not be implemented
    //void operator = (BaconLibrary const& copy){}; //This should not be implemented (either)

protected:

    virtual void initialize(int stage);
    virtual void finish();

    void handleMessage(cMessage *msg);

    virtual int numInitStages () const {
        return 2;
    }

    //Library Mathematical Information
    ContentCategoryDistribution_t multimediaContent;
    ContentCategoryDistribution_t trafficContent;
    ContentCategoryDistribution_t networkContent;
    ContentCategoryDistribution_t emergencyContent;
    long int currentIndex;

    std::list<Content_t>* multimediaLibrary;
    std::list<Content_t>* networkLibrary;
    std::list<Content_t>* trafficLibrary;

    std::vector<double> multimediaCummulativeProbabilityCurve;
    std::vector<double> networkCummulativeProbabilityCurve;
    std::vector<double> trafficCummulativeProbabilityCurve;

    std::vector<double> buildCategoryLibrary(int count, int byteSize, ContentPriority priority, ContentClass category, std::string classPrefix);
    void buildContentList();

    LocationCorrelationModel locationModel;
    int sectorCount = 1;
    int widthBlocks = 1;
    int heightBlocks = 1;
    double sectorWidth = 250;       //Unit = Meters
    double sectorHeight = 200;      //Unit = Meters

    int maxVehicleServers = 0;
    int allocatedVehicleServers = 0;

    double zipfCaracterization = 0;
    int sizeMultimedia = 1024;          //Packet size
    int sizeNetwork = 1024;             //Packet size
    int sizeTransit = 1024;             //Packet size

    int libraryTransit = 10000;         //Library size
    int libraryNetwork = 10000;         //Library size
    int libraryMultimedia = 10000;      //Library size

    ContentPriority priorityTransit;            //Priority
    ContentPriority priorityNetwork;            //Priority
    ContentPriority priorityMultimedia;         //Priority

    std::list<int> serverVehicles;

    cMessage* requestTimer;             //Running Timer
    std::list<LocationRequest_t> preemptiveRequests;
    string requestSequenceFile;

    BaseWorldUtility *world;
    BaconStatistics* stats;

public:
    std::string transitPrefix = "t";
    std::string networkPrefix = "n";
    std::string multimediaPrefix = "m";

    bool independentOperationMode();
    void setupPendingRequests();
    void loadRequestSequence();
    void registerClient(string clientPath);
    void deregisterClient(string clientPath);
    std::list<string> clientList;

    bool requestServerStatus(int vehicleID);
    void releaseServerStatus(int vehicleID);
    int getActiveServers();
    int getMaximumServers();

    long int getCurrentIndex();                      //For Statistics & Maximum Count Use
    long int getRequestIndex();                      //Returns next index (sequence ID) in global request list numbering scheme (to avoid doubles)
    Content_t* getContent(std::string prefix);
    std::list<Content_t>* getMultimediaContentList();
    std::list<Content_t>* getTrafficContentList();
    std::list<Content_t>* getNetworkContentList();

    int getIndexForDensity(double value, ContentClass contentClass);
    int getIndexForDensity(double value, ContentClass contentClass, int sector );
    int getIndexForDensity(double value, ContentClass contentClass, double xPos, double yPos );
    int getContentClass(ContentClass cClass);
};

#endif /* BACONLIBRARY_H */
