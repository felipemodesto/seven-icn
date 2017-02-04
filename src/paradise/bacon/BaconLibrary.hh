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
class BaconStatistics;

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
    std::vector<double> multimediaCummulativeProbability;
    void buildContentList();

    double zipfCaracterization = 0;
    int multimediaContentSize = 50000;     //Category Wise

    BaconStatistics* stats;

public:

    int libraryCategoriesSize = 4;         //Category Wise

    long int getCurrentIndex();                      //For Statistics & Maximum Count Use
    long int getRequestIndex();                      //Returns next index (sequence ID) in global request list numbering scheme (to avoid doubles)
    virtual std::list<Content_t> getMultimediaContentList();
    virtual Content_t* getContent(std::string prefix);

    int getIndexForDensity(double value);
    int getContentClass(ContentClass cClass);
};

#endif /* BACONLIBRARY_H */
