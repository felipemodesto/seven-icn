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
        sizeMultimedia = par("sizeMultimedia").longValue();
        sizeNetwork = par("sizeNetwork").longValue();
        sizeTransit = par("sizeTransit").longValue();

        libraryTransit = par("libraryTransit").longValue();
        libraryNetwork = par("libraryNetwork").longValue();
        libraryMultimedia = par("libraryMultimedia").longValue();

        priorityTransit = static_cast<ContentPriority>(par("priorityTransit").longValue());
        priorityNetwork = static_cast<ContentPriority>(par("priorityNetwork").longValue());
        priorityMultimedia = static_cast<ContentPriority>(par("priorityMultimedia").longValue());

        multimediaLibrary = NULL;
        networkLibrary = NULL;
        trafficLibrary = NULL;
        buildContentList();

        //Sequencial request index
        currentIndex = 0;
    }

    //After we've been built and assume that the statistics object has also, we fill our content list
    if (stage == 1) {
        cSimulation *sim = getSimulation();
        cModule *modp = sim->getModuleByPath("BaconScenario.statistics");
        stats = check_and_cast<BaconStatistics *>(modp);

        std::cout << "(Lib) Writing Content List to Statistics for collection (warning: might be ignored).\n";
        std::cout.flush();

        //Adding all items from all libraries
        for (auto it = multimediaLibrary->begin(); it != multimediaLibrary->end(); it++) {
            stats->logContentRequest(it->contentPrefix);
        }
        for (auto it = networkLibrary->begin(); it != networkLibrary->end(); it++) {
            stats->logContentRequest(it->contentPrefix);
        }
        for (auto it = trafficLibrary->begin(); it != trafficLibrary->end(); it++) {
            stats->logContentRequest(it->contentPrefix);
        }
    }
}

//Finalization Function (not a destructor!)
void BaconLibrary::finish() {
    multimediaLibrary->clear();
    networkLibrary->clear();
    trafficLibrary->clear();
}

//
void BaconLibrary::buildContentList() {
    //Checking for previously existing Library (Using multimedia as an example -> built one, built all)
    if (multimediaLibrary != NULL) return;

    std::cout << "(Lib) Building Library with size <" << (sizeMultimedia+sizeNetwork+sizeTransit) << ">.\n";
    std::cout.flush();

    //ContentCategoryDistribution_t* contentCategories[libraryCategoriesSize];

    //Setting Emergency Content Parameters
    emergencyContent.category = ContentClass::EMERGENCY_SERVICE;
    emergencyContent.byteSize = 512;
    emergencyContent.count = 1;                                         //Networking Information should always be available even if partially
    emergencyContent.priority = ContentPriority::PRIORITY_EMERGENCY;    //1-100 priority scale

    //Setting Traffic Content Parameters
    trafficContent.category = ContentClass::TRAFFIC;
    trafficContent.byteSize = sizeTransit;
    trafficContent.count = libraryTransit;
    trafficContent.priority = priorityTransit;
    transitPrefix = "t";

    //Setting Networking Content Parameters
    networkContent.category = ContentClass::NETWORK;
    networkContent.byteSize = sizeNetwork;
    networkContent.count = libraryNetwork;
    networkContent.priority = priorityNetwork;
    networkPrefix = "n";

    //Setting Multimedia Content Parameters
    multimediaContent.category = ContentClass::MULTIMEDIA;
    multimediaContent.byteSize = sizeMultimedia;
    multimediaContent.count = libraryMultimedia;
    multimediaContent.priority = priorityMultimedia;
    multimediaPrefix = "m";

    //Adding elements to list
    //contentCategories[0] = &trafficContent;                                 //Adding Traffic to our category list
    //contentCategories[1] = &networkContent;                                 //Adding Networking to our category list
    //contentCategories[2] = &emergencyContent;                               //Adding Emergency to our category list
    //contentCategories[3] = &multimediaContent;                              //Adding Multimedia to our category list

    //std::cout << "(Lib) Content Library is ready.. Size: " << (multimediaContentSize) << "\n";
    //std::cout.flush();

    //Building all three separate content lists
    multimediaCummulativeProbabilityCurve = buildCategoryLibrary(multimediaContent.count,multimediaContent.byteSize,multimediaContent.priority,multimediaContent.category,multimediaPrefix);
    networkCummulativeProbabilityCurve = buildCategoryLibrary(networkContent.count,networkContent.byteSize,networkContent.priority,networkContent.category,networkPrefix);
    trafficCummulativeProbabilityCurve = buildCategoryLibrary(trafficContent.count,trafficContent.byteSize,trafficContent.priority,trafficContent.category,transitPrefix);
}

//
std::vector<double> BaconLibrary::buildCategoryLibrary(int count, int byteSize, ContentPriority priority, ContentClass category, std::string classPrefix) {

    std::cout << "(Lib) Building Category <" << classPrefix << ">: Zipf <" << zipfCaracterization << "> Item Count <" << (count) << ">.\n";
    std::cout.flush();

    //Setting Up for Library Popularity Calculations
    float CDF = 0;
    float Hk = 0;
    float HN = 0;

    std::list<Content_t>* newLibrary = new std::list<Content_t>();

    //Generating Harmonic Series
    for (int i = 1 ; i <= count ; i++) {
        HN += 1 / static_cast<double>(pow(i, zipfCaracterization));
    }
    std::vector<double> cummulativeProbability = std::vector<double> (count);

    //Generating Objects for Class
    for (int j = 0; j < count ; j++) {
        Content_t newContent;

        newContent.popularityRanking = j+1;         //Used as shorthand to the ranking of the object in its relative popularity queue
        newContent.contentClass = category;
        newContent.contentSize = byteSize;
        //newContent.contentStatus = ContentStatus::AVAILABLE;
        newContent.contentPrefix = classPrefix + "/" + std::to_string(j+1);
        //newContent.useCount = 0;
        //newContent.lastAccessTime = 0;

        newLibrary->push_back(newContent);

        //If Zipf == 0 we don't use ZIPF, everything is uniformily randomly distributed
        if (zipfCaracterization != 0) {
            Hk += 1 / static_cast<double>(pow(j+1,zipfCaracterization));
            CDF = Hk/HN;

            //std::cout << std::to_string(j+1) << ", " << std::to_string(CDF) << "\n";
            //std::cout.flush();

            //multimediaCummulativeProbability[j] = CDF;
        } else {
            CDF = static_cast<double>(1/(double)count) * (j+1);
            //std::cout << "Boo, " << std::to_string(j+1) << ", " << std::to_string(Hk) << ", " << std::to_string(CDF) << "\n";
            //std::cout.flush();
        }
        cummulativeProbability[j] = CDF;
    }

    //Setting the appropriate library pointer
    switch(category) {
        case ContentClass::MULTIMEDIA:
            multimediaLibrary = newLibrary;
            break;

        case ContentClass::NETWORK:
            networkLibrary = newLibrary;
            break;

        case ContentClass::TRAFFIC:
            trafficLibrary = newLibrary;
            break;

        default:
            std::cerr << "(SM) Error: Weird Class during constructor\n";
            std::cerr.flush();
            break;
    }

    //Returning the probability list
    return cummulativeProbability;
}

//
std::list<Content_t>* BaconLibrary::getMultimediaContentList() {
    return multimediaLibrary;
}

//
std::list<Content_t>* BaconLibrary::getTrafficContentList() {
    return trafficLibrary;
}

//
std::list<Content_t>* BaconLibrary::getNetworkContentList() {
    return networkLibrary;
}

//
Content_t* BaconLibrary::getContent(std::string contentPrefix) {
    //std::cout << "(BaconLibrary) Searching for content item <" << contentPrefix.c_str() << "> \n";
    //std::cout.flush();

    //Removing Stupid quotes
    if (contentPrefix.c_str()[0] == '\"') {
        contentPrefix = contentPrefix.substr(1, contentPrefix.length() - 2);
    }

    //Splitting into Substring to get classification
    std::string category = contentPrefix.substr(0, contentPrefix.find("/"));
    std::list<Content_t>* correctLibrary;

    if (category.compare(transitPrefix) == 0) {
        correctLibrary = trafficLibrary;
    } else if (category.compare(networkPrefix) == 0) {
        correctLibrary = networkLibrary;
    } else if (category.compare(multimediaPrefix) == 0) {
        correctLibrary = multimediaLibrary;
    } else {
        std::cerr << "(Lib) Error: Content Category is not known <" << category << ">\n";
        std::cerr.flush();
        return NULL;
    }

    for (auto it = correctLibrary->begin(); it != correctLibrary->end(); it++) {
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
int BaconLibrary::getIndexForDensity(double value, ContentClass contentClass ) {
    std::vector<double> probabilityCurve;

    //Figuring out the appropriate list
    switch(contentClass) {
        case ContentClass::MULTIMEDIA:
            probabilityCurve = multimediaCummulativeProbabilityCurve;
            break;

        case ContentClass::NETWORK:
            probabilityCurve = networkCummulativeProbabilityCurve;
            break;

        case ContentClass::TRAFFIC:
            probabilityCurve = trafficCummulativeProbabilityCurve;
            break;

        default:
            std::cerr << "(Lib) Error: Requesting density for undefined category <" << static_cast<int>(contentClass) << ">\n";
            return -1;
    }

    //Checking if the probability index is outside the possible range
    if (value < 0 || value > 1) {
        return -1;
    }

    //Checking which item meets the minimum probability
    int librarySize = probabilityCurve.size();
    for (int j = 0 ; j < librarySize ; j++) {
        if (value <= probabilityCurve[j]) {
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

