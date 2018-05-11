#include "tracker_udp.hpp"


using namespace std;


int32_t get_transaction_id(){
    using namespace boost::random;
    boost::mt19937 randGen(time(NULL));
    boost::uniform_int<int32_t> uInt32Dist(0, std::numeric_limits<int32_t>::max());
    boost::variate_generator<boost::mt19937&, boost::uniform_int<int32_t> > getRand(randGen, uInt32Dist);

    return getRand();
}


void get_tracker_domain(string& tracker_url){
    tracker_url.erase(0, 7);    // udp://
    boost::erase_all(tracker_url, "udp://");
    boost::erase_all(tracker_url, '/');
}



int connect_request(){



};