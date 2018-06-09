#include <ctime>        //For generating tracker key
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <iostream>

#include <glog/logging.h>   //Logging Library

#include "tracker.h"
#include "tracker_udp.hpp"
#include "peer.h"
#include "rang.hpp"

namespace tracker{

    using namespace std;

    
    boost::mutex peer_list_mutex;
    
    
    /**
     *  @brief Send request to each tracker in tracker_list
     *
     *  Read from config file (to implement) the port.
     *  Then analize the files and fill the tracker parameter structure.
     *  
     *  The appropriate tracker protocol is then choosed and executed; the parsed peers
     *  are added to the peer_list (passed by argument).
     * 
     *  \waring The config file and the "uploaded" and "downloaded" param are still unimplemented.
     *  \todo Read che config parameter from a config file
     * 
     *  @param *param       the struct containing the tracker parameter
     *  @param peer_id      the client generated id
     *  @param peer_list    vector where to store received peers
     *
     */
    int start_tracker_request(TParameter *param, const TList &tracker_list, pwp::PeerList peer_list){
        using namespace rang;

        cout << endl << rang::fg::cyan << rang::bg::gray;
        cout << "--------------Peer fetching from tracker------------------------------";
        cout << fg::reset << bg::reset << endl;

        //TODO Read this info from config files
        param->info_hash = "";
        param->port = 8999;
        param->uploaded = 0;
        param->downloaded = 0;
        param->numwant = 50;
        param->compact = true;
        //-------------------------------------

        if(urlencode_paramenter(param) < 0){
            LOG(FATAL) << "Cannot encode parameter correctly";
            return -1;
        }


        boost::thread_group t_group;
        TList::const_iterator it = tracker_list.begin();

        if(peer_list == nullptr)
            peer_list = make_shared<vector<pwp::peer>>(10);

        for(; it != tracker_list.end(); ++it){
            LOG(INFO) << endl << "Contacting tracker : " << *it;
            if(t_udp::is_udp_tracker(*it)){
                DLOG(INFO) << endl << "UDP Tracker ";
                t_group.add_thread(new boost::thread(&t_udp::udp_manager, *it, *param, peer_list));
            }
            else if(*it != ""){
                DLOG(INFO) << endl << "HTTP Tracker ";
                t_group.add_thread(new boost::thread(process_tracker_request, *it, param, peer_list));
            }
        }
    

        LOG(INFO) << "Waiting for joining";
        t_group.join_all(); //Sync all thread

        LOG(INFO) << "Joinned";

        _io_service.stop();
        _io_service.reset();

        remove_duplicate_peers(peer_list);

        cout << "Found " << style::bold << fg::green <<  peer_list->size() << fg::reset << style::reset << " available peers";

        return 0;
    }




    /**
     * @brief Remove duplicate peers from a peer_list
     * 
     * Take a PeerList and delete al duplicate elements.
     * Following this response (https://stackoverflow.com/questions/1041620/whats-the-most-efficient-way-to-erase-duplicates-and-sort-a-vector)
     * is better to convert the vector to a set and then convert back if the number element is low
     * \todo The test was with integers, re-test with pwp::peer
     * 
     * 
     * /warning Unimplemented function
     * 
     * @param peer_list 
     * @return uint         the peers number after the processing
     */

    uint remove_duplicate_peers(pwp::PeerList& peer_list){
        
        DLOG(WARNING) << "REMOVE DUPLICATE PEER : Unimplemented function";
        return 0;

        vector<pwp::peer>::iterator it = peer_list->begin();

        for(; it != peer_list->end(); ++it){

            //IMPEMENT
                
        }
    }



    /**
     *  @brief Main http tracker manager
     * 
     *  Manage the entire tracker request : it craft the "announce" request and parse the
     *  response to extract peers data, that subsequently is pushed inside peer_list
     * 
     *  @param tracker_url :    the url of the tracker to contact
     *  @param param :          the torrent file param
     *  @param peer_list :      a pointer to the vector where to store peers
     * 
     */

    void process_tracker_request(const string& tracker_url, const TParameter *param, pwp::PeerList peer_list){
        shared_ptr<string> enc_url;
        string response = "";
        int error_code;
        bool second_trying = false;

        string tracker_key = create_tracker_key();
        event_type event_status = STARTED;

        do{
            enc_url = url_builder(tracker_url.c_str(), *param, event_status, tracker_key);

            LOG(INFO) << endl << "URL : " << *enc_url << endl;

            tracker_send_request(enc_url, &response);

            error_code = process_tracker_response(&response, peer_list);
            assert(peer_list != nullptr);

            if(error_code >= 0){
                //If everything is OK
                LOG(INFO) << "Correct Response, intitiating PWP protocol with peers";

                assert(peer_list != nullptr);

                return;

            }else{  //Error Present

                //Manage error here
                switch(error_code){
                    case EMPTY_TRACKER:
                        LOG(INFO) << "Tracker [" << tracker_url << "] returned an empty list";
                        break;
                    default:
                        LOG(ERROR) << "There was an error while parsing tracker response. Code : " << error_code;
                }

                if(!second_trying){
                    //Seeing this from qbitorrent
                    event_status = STOPPED;
                    second_trying = true;
                    continue;
                }
            }

            //scrape_request(*enc_url, &response);

            if(second_trying){
                event_status = STARTED; //Really needed?
                second_trying = false;
            }

        }while(second_trying);
    }




    /**
     * This function take the tracker's param and urlencode the info_hash and the peer_id, the other parameters
     * are left untouched
     *
     * @param *param    pointer to a struct containing parameters
     * @param *curl     (Optional) instance of curl library
     *
     * @return 0 if success, < 0 otherwise
     **/

    int urlencode_paramenter(TParameter *param, CURL *curl){

        //Return values of "urlencode"
        char* enc_info_hash_curl;
        char* enc_peer_id_curl;
        bool curl_passed=true;;

        if(curl == NULL){
            curl_passed=false;
            curl = curl_easy_init();
        }

        if(!curl)
            return CURL_NOT_INIT;

        //The field "info_hash" and "peer_id" must be encoded by "urlencoding"
        enc_info_hash_curl = curl_easy_escape(curl, (char*)param->info_hash_raw, strnlen(param->info_hash_raw, 20*2 +1));   //Inserire SHA_DIGEST
        enc_peer_id_curl = curl_easy_escape(curl, param->peer_id.c_str(),param->peer_id.length());
        
        string enc_info_hash = string(enc_info_hash_curl);

        if(!enc_info_hash_curl || !enc_peer_id_curl){ //If the escape fails
            return CURL_ESCAPE_ERROR;
        }

        //String object build
        param->info_hash.clear();
        param->peer_id.clear();
        param->info_hash.assign(enc_info_hash_curl);
        param->peer_id.assign(enc_peer_id_curl);


        //Free the memory
        curl_free(enc_info_hash_curl);
        curl_free(enc_peer_id_curl);

        if(!curl_passed)
            curl_easy_cleanup(curl);

        return 0;
    }

    /**
     *  @brief Create the tracker GET request
     * 
     *  An HTTP GET request is crafted using tracker's url and request parameters
     * 
     *  In case of error the request is equal to ""
     *
     *  \todo Improve this function parmeters, since trakcer_url is already inside TParameter
     *  \todo Check if tracker_url is a proper url by validating it with regex
     *  \todo Manage errors
     * 
     *  @param tracker_url      tracker's url
     *  @param param            param to send to the tracker
     *  @param curl             : (Optional) curl library instance
     *  @param tls              : (Optional) specify if https should be used; default = false
     *
     *  @return a shared pointer to a string containing the url
    */

    shared_ptr<string> url_builder(const string& tracker_url, const TParameter& t_param, event_type event, const string& tracker_key, CURL *curl, bool tls){

        bool curl_passed=true;;

        if(curl == NULL){
            curl_passed=false;
            curl = curl_easy_init();
        }

        if(!curl)
            return NULL;


        TParameter param = t_param;

        shared_ptr<string> url_req(new string(""));

        //TODO Check if "tracker_url" is a proper url by validating it with regex
        *url_req = "";
        *url_req += tracker_url + "?";


        //Covert to lowercase
        //TODO test if it's not lowecase it continue to work
        //std::transform(param.info_hash.begin(), param.info_hash.end(), param.info_hash.begin(), ::tolower);

        *url_req += "info_hash=" + param.info_hash;
        *url_req += "&peer_id=" + param.peer_id;

        assert(param.port <= MAX_PORT_VALUE); //TODO Manage with error handler


        *url_req += "&port=" + to_string(param.port);
        *url_req += "&uploaded=" + to_string(param.uploaded);
        *url_req += "&downloaded=" + to_string(param.downloaded);
        *url_req += "&left=" + to_string(param.left);
        if(param.compact)
            *url_req += "&compact=1";   //Is a compact request?
        else
            *url_req += "&compact=0";   //Is a compact request?

        *url_req += "&no_peer_ids=1&supportcrypto=1&redundant=0";

        //Optional
        *url_req += "&numwant=" + to_string(param.numwant);
        *url_req += "&key=" + tracker_key;

        switch(event){
            case STARTED:
                *url_req += "&event=started";
                break;
            case STOPPED:
                *url_req += "&event=stopped";
                break;
        }

        if(!curl_passed){
            curl_easy_cleanup(curl);
        }

        LOG(INFO) << *url_req;

        return url_req;
    }


    /**
     *  @brief Manage the curl response
     * 
     *  Default function (take from curl example) that write the response into "data"
     */
    size_t writeFunction(void *ptr, size_t size, size_t nmemb, std::string* data) {
        if(data == NULL)
            return 0;
        data->append((char*) ptr, size * nmemb);
        return size * nmemb;
    }

    /**
     * @brief Execute the http(s) request and print the response
     *
     * @param *url : the url to contact (with the GET parameter)
     * @param curl : (Optional) the instance of the curl lib
     *
     * @return 0 if it's a success, <0 otherwise;
     */

    int tracker_send_request(shared_ptr<string> url, string *response, CURL *curl){

        CURLcode code;
        bool curl_passed=true;;

        if(curl == NULL){
            curl_passed=false;
            curl = curl_easy_init();
        }

        if (curl) {
            code = curl_easy_setopt(curl, CURLOPT_URL, url->c_str());
            if(code != CURLE_OK){
                LOG(ERROR) << "Error while setting the url" << endl;
                curl_easy_cleanup(curl);

                return -1;
            }
            /*
            *   Various options to fill from configuraton files

            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
            curl_easy_setopt(curl, CURLOPT_USERPWD, "user:pass");
            */
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Transmission/2.92\r\n");
            /*
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
            curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
            */

            //string response_string;
            string header_string;
            //Set the function that handle the received data (writeFunction)
            code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);

            char* url_eff;
            long response_code;
            double elapsed;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &elapsed);
            curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url_eff);

            code = curl_easy_perform(curl);
            if(code != CURLE_OK){
                LOG(ERROR) << endl << "Error while contacting the tracker" << endl;
                curl_easy_cleanup(curl);
                return -1;
            }

            DLOG(INFO) << endl << "Response : " << *response << "\t\n" << header_string;


            if(!curl_passed){
                curl_easy_cleanup(curl);
            }

        }

        return 0;
    }


    /**
     *  @brief Checks if the url is valid
     * 
     *  \warning Untested
     *
     *  @param *url     : URL to check 
     *  @param *curl    : (Optional) instance of curl library
     *
     *  @return (true) if it's valid, (false) otherwise
     */
    bool check_url(const string &url, CURL *curl)
    {
        CURLcode response;
        bool curl_passed=true;;

        if(curl == NULL){
            curl_passed=false;
            curl = curl_easy_init();
        }

        if(curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            /* don't write output to stdout */
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
            /* Perform the request */
            response = curl_easy_perform(curl);

        }

        if(!curl_passed){
            curl_easy_cleanup(curl);
        }

        return (response == CURLE_OK) ? true : false;
    }


    /**
     * @brief Parse tracker response and extract peers
     * 
     * Decode the bencoded tracker response and detect if the response is "compact" of "dictionary"
     * After calling the appropriate function the PeerList is populated with extracted peers.
     * 
     * 
     * @param response 
     * @param peer_list 
     * @return int 0 on success, <0 otherwise
     */


    int process_tracker_response(string *response, pwp::PeerList peer_list){

        int error_code;
        be_node *node;
        string key; 

        if(response->empty())
            return -1;

        node = be_decoden(response->c_str(), response->length());

        if(node){
            for (int i=0; node->val.d[i].val; i++) {
                key = node->val.d[i].key;

                //According to the protocol, if "failure reason" is present,no other keys may be present, so simply return 
                if(key == "failure reason"){    
                    LOG(ERROR) << "Error : " << node->val.d[i].val->val.s;
                    be_free(node);
                    return -1;
                }else if(key == "complete"){
                    LOG(INFO) << endl << "There are " << node->val.d[i].val->val.i << " seeders";
                }else if(key == "incomplete"){
                    LOG(INFO) << endl << "There are " << node->val.d[i].val->val.i << " leechers";
                }else if(key == "interval"){
                    LOG(INFO) << endl << "Interval is " << node->val.d[i].val->val.i;
                }else if(key == "peers"){
                    //Need to differentiate between dictionary rappresentation and binary rappresentation
                    
                    if(is_compact_response(response)){
                        LOG(INFO) << "Compact Response";
                        be_node *n = node->val.d[i].val;
                        assert(n->type == BE_STR);

                        string resp = string(n->val.s);

                        LOG(INFO)  << "String to parse : " << resp;
    
                        parse_binary_peers(resp, peer_list);
                    }else{
                        LOG(INFO) << "No compact response";
                        if(node->type == BE_DICT){

                            error_code = parse_dict_peer(node->val.d[i].val, peer_list);
                            if(error_code < 0){
                                if(error_code == EMPTY_TRACKER){
                                    LOG(WARNING) << "Empty Tracker List";
                                    return EMPTY_TRACKER;
                                }

                            }

                        }

                    }
                }   
            }
            be_free(node);
        }

        return 0;

    }


    /**
     * @brief Generate a scrape request from a tracker url
     * 
     * @param url       tracker url
     * @param param 
     * @param response  reference to a buffer that will store the response
     * @param curl      (Optional) instance of the curl library
     * @return int 
     */


    int scrape_request(string &url, const TParameter &param, string *response, CURL *curl){
        bool curl_passed=true;;

        if(curl == NULL){
            curl_passed=false;
            curl = curl_easy_init();
        }

        shared_ptr<string> scrap_url = get_scrape_url(url);

        if(scrap_url == nullptr)
            return -1;
        
        //FIX ME - Send less parameters
        shared_ptr<string> final_url = url_builder(*scrap_url, param, STARTED, "");

        if(tracker_send_request(final_url, response) < 0){
            cout << endl << "Error in Scraping Request" << endl;
            return -1;
        }

        
        //Se l'istanza non è stata passata allora pulisco
        if(!curl_passed){
            curl_easy_cleanup(curl);
        }        
        
        return 0;
    }

    /**
     *  @brief Get tracker url and return the scraped version of it
     * 
     *  @param url : the url(annouunce version) of the tracker 
     *  @return a pointer to the new url or nullptr if the tracker doesn't support scraping
     */


    shared_ptr<string> get_scrape_url(const string &url){
        shared_ptr<string> scrape_url (new string(url));

        size_t old_found;
        size_t found = 0 ;
        do{
            old_found = found;
            found = url.find('/', found+1);
        }while(found != string::npos);

        if(old_found == string::npos)
            return nullptr;
        
        if(url.substr(old_found+1, 9) != "announce")
            return nullptr;
        
        scrape_url->replace(old_found+1, 9, "scrape");

        return scrape_url;
    }



    /**
     * @brief Create the key needed for tracker protocol
     * 
     * @return string cointaining the key
     * 
     */
    string create_tracker_key(){
        time_t time_key = time(0);
        return string("XX" + to_string(time_key)).substr(0, 8);
    }

    /**
     *  @brief Parse a non compact tracker response
     * 
     *  This function parse a non compact tracker's response according to the following protocol\n
     * 
     *  peers: (dictionary model) The value is a list of dictionaries, each with the following keys:\n
     *  peer id: peer's self-selected ID, as described above for the tracker request (string)\n
     *  ip: peer's IP address either IPv6 (hexed) or IPv4 (dotted quad) or DNS name (string)\n
     *  port: peer's port number (integer)\n
     * 
     *  \bug The address of the peer could be of three types : IPv6, DNS and IPv6. Currently just IPv4 is supported.
     *  \todo Add support for IPv6 and DNS addresses.
     * 
     * 
     *  @param node  the bencoded node containing the list of dictionaries
     * 
     *  @return      0 on success, < 0 otherwise
     */

    int parse_dict_peer(be_node *node, pwp::PeerList peer_list){

        string key;
        struct pwp::peer tpeer;

        const boost::asio::ip::address inv_address = boost::asio::ip::address::from_string("0.0.0.0");  //from_string deprecated function


        if(node->type != BE_LIST){  //Need to be a  list of dict
            LOG(ERROR) << "Peer dictionary corrupted";
            return -1;
        }

        for(int j=0; node->val.l[j]; ++j){
            be_node *dict_node = node->val.l[j];
            
            if(dict_node->type != BE_DICT){
                LOG(ERROR) << "Peer dictionary doesn't exists";
                return -2;
            }

            tpeer.port = 0; //To check later if it's found
            tpeer.addr = inv_address; //To check later if it's found

            for (int i=0; dict_node->val.d[i].val; ++i) {
                key = dict_node->val.d[i].key;
                    if (key == "peer id"){
                        assert(dict_node->val.d[i].val->type == BE_STR);
                        LOG(INFO) << endl << "Peer ID : " << dict_node->val.d[i].val->val.s << endl;

                        tpeer.peer_id = string(dict_node->val.d[i].val->val.s);

                    }else if (key == "ip"){
                        //TODO The other two types : DNS, and IPv6
                        assert(dict_node->val.d[i].val->type == BE_STR);
                        LOG(INFO) << endl << "\tIP : " << dict_node->val.d[i].val->val.s << endl;

                        tpeer.addr = boost::asio::ip::address::from_string(dict_node->val.d[i].val->val.s);
                    
                    }else if (key == "port"){
                        assert(dict_node->val.d[i].val->type == BE_INT);
                        assert(dict_node->val.d[i].val->val.i >= 0 && dict_node->val.d[i].val->val.i <= MAX_PORT_VALUE);

                        LOG(INFO) << endl << "\tport : " << dict_node->val.d[i].val->val.i << endl;

                        tpeer.port = (uint)dict_node->val.d[i].val->val.i;
                    }
            }

            if(tpeer.addr != inv_address && tpeer.port != 0){    //Check if all(necessary) param is present
                //FIXME Not sure if mutex is needed....
                peer_list_mutex.lock();
                peer_list->push_back(tpeer);
                peer_list_mutex.unlock();
            }else{
                LOG(WARNING) << "Malformed peer detected";
            }
        }
        return 0;
    }

    /**
     * @brief Parse a binary tracker response
     * 
     * This function parse a compact tracker's response according to the following protocol
     * 
     * peers: (binary model) Instead of using the dictionary model described above, the peers value may be a string consisting of multiples of 6 bytes. 
     * First 4 bytes are the IP address and last 2 bytes are the port number. All in network (big endian) notation.
     * 
     * @param resp         the response to parse
     * @param peer_list    the structure where to store extracted peers
     * 
     */

    void parse_binary_peers(const string& resp, pwp::PeerList peer_list){
        //No IPv6 support??

        for(int i=0; i+5<=resp.length(); i=i+6){

            string ip = to_string((uint8_t)(resp[i])) + "." + to_string((uint8_t)(resp[i+1])) + "." + to_string((uint8_t)resp[i+2]) + "." + to_string((uint8_t)resp[i+3]);

            uint16_t port = 0;
            port = ((uint16_t)resp[i+5] << 8) | resp[i+4];    //Bitwise
            port = ntohs(port); //Convert to host order

            try{
                pwp::peer tpeer;
                tpeer.addr = boost::asio::ip::address::from_string(ip);
                tpeer.port = port;
                tpeer.peer_id = ""; //Unspecified peer_id in compact response

                peer_list_mutex.lock();
                peer_list->push_back(tpeer);    //Insert Peer into the list
                peer_list_mutex.unlock();
            }catch(exception& e){
                LOG(ERROR) << e.what() << '\n';
            }
            
        }
    }

    /**
     *  @brief Check if a response is in a compact format
     * 
     *  \todo Improve and re-test this function
     * 
     *  @param *response    : the response to check
     *  @return             : true if it's compact, otherwise false
     */

    bool is_compact_response(const string *response){
        //TODO to improve
        std::size_t found = response->find("peers");
        if (found!=std::string::npos){
            if(response->at(found+5) == '0' || response->at(found+5) == 'l')
                return false;
            else
                return true;
        }
        return false;

    }


}   //End tracker namespace
