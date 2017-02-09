/*
 * BaconLibrary.cc
 *
 *  Created on: Jan 25, 2016
 *      Author: felipe
 */

#include <paradise/bacon/BaconLibrary.hh>

Define_Module(BaconLibrary);

//Initialization Function
void BaconLibrary::initialize(int stage) {

    //Initializing
    if (stage == 0) {
        zipfCaracterization = par("zipfCaracterization").doubleValue();
        multimediaContentSize = par("multimediaLibrarySize").longValue();
        multimediaLibrary = NULL;
        buildContentList();
    }

    //After we've been built and assume that the statistics object has also, we fill our content list
    if (stage == 1) {
        cSimulation *sim = getSimulation();
        cModule *modp = sim->getModuleByPath("BaconScenario.statistics");
        stats = check_and_cast<BaconStatistics *>(modp);

        std::cout << "(Lib) Writing Content List to Statistics for collection (warning: might be ignored).\n";
        std::cout.flush();

        //Content Requests count from 0 so we can add ALL contents to the list in their "correct" order
        for (auto it = multimediaLibrary->begin(); it != multimediaLibrary->end(); it++) {
            stats->logContentRequest(it->contentPrefix);
        }
    }
}

//Finalization Function (not a destructor!)
void BaconLibrary::finish() {
    multimediaLibrary->clear();
}

//
void BaconLibrary::buildContentList() {
    //Checking for previously existing Library
    if (multimediaLibrary != NULL) return;
    multimediaLibrary = new std::list<Content_t>();

    std::cout << "(Lib) Building Content List with Zipf Value <" << zipfCaracterization << "> and size <" << multimediaContentSize << ">.\n";
    std::cout.flush();

    currentIndex = 0;
    //WATCH(currentIndex);

    //ContentCategoryDistribution_t* contentCategories[libraryCategoriesSize];

    //Setting Traffic Content Parameters
    trafficContent.category = ContentClass::TRAFFIC;
    trafficContent.averageByteSize = 10000;                                 //10kb
    trafficContent.averageCount = 1;                                        //Vehicles always have location but GPS coordinate needs to be within range on evaluation
    trafficContent.averagePopularity = ContentPopularity::POPULAR;          //popularity scale
    trafficContent.averagePriority = 10;                                    //1-100 priority scale

    //Setting Networking Content Parameters
    networkContent.category = ContentClass::NETWORK;
    networkContent.averageByteSize = 10240;                                 //10kb
    networkContent.averageCount = 1;                                        //Networking Information should always be available even if partially
    networkContent.averagePopularity = ContentPopularity::POPULAR;          //popularity scale
    networkContent.averagePriority = 20;                                    //1-100 priority scale

    //Setting Emergency Content Parameters
    emergencyContent.category = ContentClass::EMERGENCY_SERVICE;
    emergencyContent.averageByteSize = 512;                                 //10kb
    emergencyContent.averageCount = 1;                                      //Networking Information should always be available even if partially
    emergencyContent.averagePopularity = ContentPopularity::MANDATORY;      //popularity scale
    emergencyContent.averagePriority = 10;                                  //1-100 priority scale

    //Setting Multimedia Content Parameters
    multimediaContent.category = ContentClass::MULTIMEDIA;
    multimediaContent.averageByteSize = 100000;                             //100kb
    //multimediaContent.averageByteSize = 1000000;                          //1mb
    multimediaContent.averageCount = multimediaContentSize;                 //Number of unique multimedia objects we have
    multimediaContent.averagePopularity = ContentPopularity::UNPOPULAR;     //popularity scale
    multimediaContent.averagePriority = 1;                                  //1-100 priority scale

    //Adding elements to list
    //contentCategories[0] = &trafficContent;                                 //Adding Traffic to our category list
    //contentCategories[1] = &networkContent;                                 //Adding Networking to our category list
    //contentCategories[2] = &emergencyContent;                               //Adding Emergency to our category list
    //contentCategories[3] = &multimediaContent;                              //Adding Multimedia to our category list

    //Setting Up for Library Popularity Calculations
    float CDF = 0;
    float Hk = 0;
    float HN = 0;

    for (int i = 1 ; i <= multimediaContentSize ; i++) {
        HN += 1 / static_cast<double>(pow(i, zipfCaracterization));
    }
    multimediaCummulativeProbability = std::vector<double> (multimediaContentSize);


    //Loading Multimedia Objects
    for (int j = 0; j < multimediaContent.averageCount ; j++) {
        Content_t newContent;

        newContent.popularityRanking = j+1;
        newContent.contentClass = multimediaContent.category;
        newContent.contentSize = multimediaContent.averageByteSize;
        newContent.contentStatus = ContentStatus::AVAILABLE;
        newContent.contentPrefix = "m/" + std::to_string(j+1);
        newContent.useCount = 0;
        newContent.lastAccessTime = 0;

        multimediaLibrary->push_back(newContent);

        //If Zipf == 0 we don't use ZIPF, everything is uniformily randomly distributed
        if (zipfCaracterization != 0) {
            Hk += 1 / static_cast<double>(pow(j+1,zipfCaracterization));
            CDF = Hk/HN;

            //std::cout << std::to_string(j+1) << ", " << std::to_string(CDF) << "\n";
            //std::cout.flush();

            //multimediaCummulativeProbability[j] = CDF;
        } else {
            CDF = static_cast<double>(1/(double)multimediaContentSize) * (j+1);
            //std::cout << "Boo, " << std::to_string(j+1) << ", " << std::to_string(Hk) << ", " << std::to_string(CDF) << "\n";
            //std::cout.flush();
        }
        multimediaCummulativeProbability[j] = CDF;
    }

    std::cout << "(Lib) Content Library is ready.. Size: " << (multimediaContentSize) << "\n";
    std::cout.flush();
}

//
std::list<Content_t> BaconLibrary::getMultimediaContentList() {
    return *multimediaLibrary;
}

//
Content_t* BaconLibrary::getContent(std::string contentPrefix) {
    //std::cout << "(BaconLibrary) Searching for content item <" << contentPrefix.c_str() << "> \n";
    //std::cout.flush();

    //Removing possible quotes. OBS: WE WILL NEVER HAVE QUOTES!!!
    if (contentPrefix.c_str()[0] == '\"') {
        contentPrefix = contentPrefix.substr(1, contentPrefix.length() - 2); //No fucking idea why but strings added as parameters get extra quotes around them. WTF
    }

    for (auto it = multimediaLibrary->begin(); it != multimediaLibrary->end(); it++) {
        if (it->contentPrefix.compare(contentPrefix) == 0) {
            return &*it;
        }
    }

    return NULL;
}

//
int BaconLibrary::getContentClass(ContentClass cClass) {
    return static_cast<int>(cClass);
}

//
int BaconLibrary::getIndexForDensity(double value) {
    if (value < 0 || value > 1) {
        return -1;
    }

    int librarySize = multimediaCummulativeProbability.size();
    for (int j = 0 ; j < librarySize ; j++) {
        if (value <= multimediaCummulativeProbability[j]) {
            return j + 1;
        }
    }

    return -1;
}

//
long int BaconLibrary::getCurrentIndex() {
    return currentIndex;
}

//
long int BaconLibrary::getRequestIndex() {
    currentIndex++;
    return currentIndex;
}

