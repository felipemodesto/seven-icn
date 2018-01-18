/*
 * BaconLibrary.hh
 *
 *  Created on: Jan 25, 2016
 *      Author: felipe
 */

#ifndef BACONLIBRARY_H
#define BACONLIBRARY_H

#include <paradise/bacon/Client.h>
#include <paradise/bacon/Definitions.h>
#include <paradise/bacon/Statistics.h>

class Statistics;
class Client;

using namespace omnetpp;
using namespace std;

//Content Library. Generates random content list and provides to whomever needs for simulation purposes. (Singleton)
class GlobalLibrary : public omnetpp::cSimpleModule {

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
    long int currentRequestIndex;

    std::list<Content_t>* multimediaLibrary;
    std::list<Content_t>* networkLibrary;
    std::list<Content_t>* trafficLibrary;

    std::vector<double> multimediaCummulativeProbabilityCurve;
    std::vector<double> networkCummulativeProbabilityCurve;
    std::vector<double> trafficCummulativeProbabilityCurve;

    std::vector<double> getProbabilityCurve(int count);
    std::vector<double> buildCategoryLibrary(int count, int byteSize, ContentPriority priority, ContentClass category, std::string classPrefix);
    void buildContentList();

    int* sectorPopularityIndex;        //Maps Sector Index to Popularity Index
    int* sectorPopularityRanking;      //Maps Popularity Index to Sector Index

    LocationCorrelationModel locationModel;
    int sectorCount = 1;
    int widthBlocks = 1;
    int heightBlocks = 1;
    double sectorWidth = 250;       //Unit = Meters
    double sectorHeight = 200;      //Unit = Meters
    int maximumViableDistance = 200;//in Meters

    int maxVehicleServers = 0;
    int allocatedVehicleServers = 0;
    int maxVehicleClients = -1;
    int allocatedVehicleClients = 0;

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
    std::list<int> clientVehicles;

    cMessage* requestTimer;             //Running Timer
    std::list<LocationRequest_t> preemptiveRequests;
    string requestSequenceFile;

    BaseWorldUtility *world;
    Statistics* stats;

public:
    std::string transitPrefix = "t";
    std::string networkPrefix = "n";
    std::string multimediaPrefix = "m";

    bool locationDependentContentMode();
    bool independentOperationMode();

    void setupPendingRequests();
    void loadRequestSequence();
    void registerClient(string clientPath);
    void deregisterClient(string clientPath);
    std::list<string> clientList;

    NodeRole requestStatus(int vehicleID);
    void releaseStatus(NodeRole status, int vehicleID);
    int getActiveServers();
    int getMaximumServers();

    int getSectorSize();
    int getSectorRow(int sectorCode);
    int getSectorColumn(int sectorCode);
    int getSector(double xPos, double yPos);                                        //Maps a XY point to a sector
    int getDistanceToSector(int sectorCode, double xPos, double yPos);              //Returns the distance between the XY point to the center of the sector with given sectorCode
    bool viablyCloseToContentLocation(int sectorCode, double xPos, double yPos);    //Returns true if node is close enough to sector for it to be considered as being able to produce that content object out of thin air

    long int getCurrentRequestIndex();                      //For Statistics & Maximum Count Use
    long int getRequestIndex();                      //Returns next index (sequence ID) in global request list numbering scheme (to avoid doubles)
    Content_t* getContent(std::string prefix);
    std::list<Content_t>* getMultimediaContentList();
    std::list<Content_t>* getTrafficContentList();
    std::list<Content_t>* getNetworkContentList();

    //float BaconLibrary::getDensityForPrefix(string itemPrefix);
    float getDensityForIndex(double index,ContentClass contentClass);
    int getIndexForDensity(double value, ContentClass contentClass);
    int getIndexForDensity(double value, ContentClass contentClass, int sector );
    int getIndexForDensity(double value, ContentClass contentClass, double xPos, double yPos );
    int getContentClass(ContentClass cClass);
    int getClassFreeIndex(string contentPrefix);    //Returns the last portion of the prefix, without any class property as an integer (cause that is what we use for simplicity)
    int getSectorFromPrefixIndex(int index);

    static std::string cleanString(std::string inputString);
    bool equals(Content_t* first, Content_t* second);
    bool equals(Content_t first, Content_t second);
    bool equals(Content_t first, std::string second);

};

#endif /* BACONLIBRARY_H */
