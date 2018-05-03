#include <iostream>
#include <algorithm>    //Lower String
#include "tracker.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <boost/endian/conversion.hpp>
#include <cmath>




using namespace std;

/**
 *  Function that start the communication with the tracker
 *
 *  @param *param       : the struct containing the tracker parameter
 *  @param peer_id      : the client generated id
 *
 */
int start_tracker_request(TrackerParameter *param){

    int error_code;
    bool second_trying = false;


    //TODO Read this info from config files
    param->info_hash = "";
    param->port = 8999;
    param->uploaded = 0;
    param->downloaded = 0;
    shared_ptr<string> enc_url;
    string response = "";

    event_type event_status = STARTED;

    //TODO Process udp:// urls
    for(int i=0; i<param->tracker_urls.size(); i++){
        enc_url = url_builder(param->tracker_urls[i].c_str(), *param, event_status);

        cout << endl << "URL : " << *enc_url << endl;
        tracker_send_request(enc_url, &response);

        error_code = process_tracker_response(&response);
        response.clear();

        if(error_code < 0){

            if(!second_trying){
                //Seeing this from qbitorrent
                event_status = STOPPED;
                second_trying = true;
                i--;
                continue;
            }
        }

        //scrape_request(*enc_url, &response);


        if(second_trying){
            event_status = STARTED;
            second_trying = false;
        }
    }
    return 0;
}



/**
 * Questa funzione prende il puntatore alla struttura dei parametri e ne modifica quelli che devono
 * essere codificati mediante "urlencode"
 *
 * @param *param    puntatore a struttura dei parametri
 * @param *curl     (Opzionale) istanza della libreria curl
 *
 * @return 0 in caso di successo, < 0 se fallisce
 **/

int urlencode_paramenter(struct TrackerParameter *param, CURL *curl){

    //Valori di ritorno della codifica "urlencode"
    char* enc_info_hash_curl;
    char* enc_peer_id_curl;
    bool curl_passed=true;;

    if(curl == NULL){
        curl_passed=false;
        curl = curl_easy_init();
    }

    if(!curl)
        return CURL_NOT_INIT;

    //I campi "info_hash" e "peer_id" devono essere codificati tramite "urlencoding"
    enc_info_hash_curl = curl_easy_escape(curl, (char*)param->info_hash_raw, strnlen(param->info_hash_raw, 20*2 +1));   //Inserire SHA_DIGEST
    enc_peer_id_curl = curl_easy_escape(curl, param->peer_id.c_str(),param->peer_id.length());
    
    string enc_info_hash = string(enc_info_hash_curl);

    if(!enc_info_hash_curl || !enc_peer_id_curl){ //Se fallisco l'escape
        return CURL_ESCAPE_ERROR;
    }

    //Costruisco l'oggetto string
    param->info_hash.clear();
    param->peer_id.clear();
    param->info_hash.assign(enc_info_hash_curl);
    param->peer_id.assign(enc_peer_id_curl);


    //Libero la memoria
    curl_free(enc_info_hash_curl);
    curl_free(enc_peer_id_curl);



    if(!curl_passed)
        curl_easy_cleanup(curl);

    return 0;
}

/**
 *  Funzione che passato l'url del tracker ed i parametri costruisce l'url per effettuare
 *  la richiesta GET al tracker
 *
 *  @param tracker_url      : url del tracker
 *  @param param            : parametri da inviare al tracker
 *  @param curl             : (Opzionale) istanza della libreria curl
 *  @param tls              : (Opzionale) specifica se usare l'https, default = false
 *
 *  @return Un puntatore ad una stringa contenente l'url
*/

shared_ptr<string> url_builder(const string &tracker_url, const struct TrackerParameter &t_param, event_type event, CURL *curl, bool tls){

    /*
    *   TODO : Improve this function parmeters, since trakcer_url is already inside TrackerParameter
    */
    bool curl_passed=true;;

    if(curl == NULL){
        curl_passed=false;
        curl = curl_easy_init();
    }

    if(!curl)
        return NULL;


    struct TrackerParameter param = t_param;

    shared_ptr<string> url_req(new string(""));

    //TODO Check if "tracker_url" is a proper url by validating it with regex
    *url_req = "";
    *url_req += tracker_url + "?";

    //codifico i parmametri tramite urlencode

    if(urlencode_paramenter(&param, curl) < 0){
        return url_req; //Mando la stringa vuota così da generare errore
    }

    //Covert to lowercase
    //TODO test if it's not lowecase it continue to work
    //std::transform(param.info_hash.begin(), param.info_hash.end(), param.info_hash.begin(), ::tolower);

    *url_req += "info_hash=" + param.info_hash;
    *url_req += "&peer_id=" + param.peer_id;

    assert(param.port <= 65535); //TODO Sostituire con un gestore dell'errore


    *url_req += "&port=" + to_string(param.port);
    *url_req += "&uploaded=" + to_string(param.uploaded);
    *url_req += "&downloaded=" + to_string(param.downloaded);
    *url_req += "&left=" + to_string(param.left);
    *url_req += "&compact=1";   //Always prefer the compact resonse
    *url_req += "&no_peer_ids=1&supportcrypto=1&redundant=0";

    //Optional
    *url_req += "&numwant=50";
    *url_req += "&key=" + create_tracker_key();

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

    return url_req;
}


/**
 *  Funzione da passare alla richiesta curl per gestire la risposta
 */
size_t writeFunction(void *ptr, size_t size, size_t nmemb, std::string* data) {
    if(data == NULL)
        return 0;
    data->append((char*) ptr, size * nmemb);
    return size * nmemb;
}

/**
 * Execute the http(s) request and print the response
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
            cerr << "Error while setting the url" << endl;
            curl_easy_cleanup(curl);

            return -1;
        }
        /*
        *   Varie opzioni da aggiungere opzionalmente nel file di configurazione

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
        //Setto la funzione che andrà a scrivere e i rispettivi parametri
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
            cerr << endl << "Error while contacting the tracker" << endl;
            curl_easy_cleanup(curl);
            return -1;
        }

        cout << endl << "Response : " << *response << endl << endl << header_string << endl << endl;


        if(!curl_passed){
            curl_easy_cleanup(curl);
        }

    }

    return 0;
}


/**
 * Funzione che verifica che un'url è valido
 *
 *  @param *url     : URL da verificare
 *  @param *curl    : (Opzionale) istanza della libreria curl
 *
 *  @return (true) se è valido, altrimenti (false)
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

    //Se l'istanza non è stata passata allora pulisco
    if(!curl_passed){
        curl_easy_cleanup(curl);
    }

    return (response == CURLE_OK) ? true : false;
}



int process_tracker_response(string *response){

    int error_code;
    be_node *node;
    string key; 

    node = be_decoden(response->c_str(), response->length());

    if(node){
        for (int i=0; node->val.d[i].val; i++) {
            key = node->val.d[i].key;

            //According to the protocol, if "failure reason" is present,no other keys may be present, so simply return 
            if(key == "failure reason"){    
                cout << endl << "Error : " << node->val.d[i].val->val.s << endl;
                be_free(node);
                return -1;
            }else if(key == "complete"){
                cout << endl << "There are " << node->val.d[i].val->val.i << " seeders" << endl;
            }else if(key == "incomplete"){
                cout << endl << "There are " << node->val.d[i].val->val.i << " leechers" << endl;
            }else if(key == "interval"){
                cout << endl << "Interval is " << node->val.d[i].val->val.i << endl;
            }else if(key == "peers"){
                //Need to differentiate between dictionary rappresentation and binary rappresentation
                
                if(is_compact_response(response)){
                    cout << "Compact Response" << endl;
                    be_node *n = node->val.d[i].val;
                    assert(n->type == BE_STR);

                    cout << "String to parse : " << n->val.s;
   
                    parse_binary_peers(n->val.s);
                }else{
                    cout << "No compact response";
                    if(node->type == BE_DICT){
                        error_code = parse_dict_peer(node->val.d[i].val);
                        if(error_code < 0){
                            if(error_code == EMPTY_TRACKER){
                                cout << endl << "Empty Tracker List";
                                return EMPTY_TRACKER;
                            }

                        }

                    }

                }

            }   
        }
        be_free(node);
    }

    

}


int scrape_request(string &url, const TrackerParameter &param, string *response, CURL *curl){
    bool curl_passed=true;;

    if(curl == NULL){
        curl_passed=false;
        curl = curl_easy_init();
    }

    shared_ptr<string> scrap_url = get_scrape_url(url);

    if(scrap_url == nullptr)
        return -1;
    
    shared_ptr<string> final_url = url_builder(*scrap_url, param, STARTED);

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
 *  Function that get the tracker url and return the scraped version of it
 * 
 *  @param url : the url(annouunce version) of the tracker 
 * 
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

string create_tracker_key(){

    time_t time_key = time(0);
    return string("XX" + to_string(time_key)).substr(0, 8);

}

/**
 *  This function parse a non compact tracker's response according to the following protocol
 * 
 *  peers: (dictionary model) The value is a list of dictionaries, each with the following keys:
 *  peer id: peer's self-selected ID, as described above for the tracker request (string)
 *  ip: peer's IP address either IPv6 (hexed) or IPv4 (dotted quad) or DNS name (string)
 *  port: peer's port number (integer)
 * 
 *  @param node     : the bencoded node containing the list of dictionaries
 * 
 *  @return         : 0 on success, < 0 otherwise
 */

int parse_dict_peer(be_node *node){

    string key;

    if(node->type != BE_LIST)  //Need to be a  list of dict
        return -1;

    for(int j=0; node->val.l[j]; ++j){
        be_node *dict_node = node->val.l[j];
        assert(dict_node->type == BE_DICT);     //TODO Better managment of the condition

        for (int i=0; dict_node->val.d[i].val; ++i) {
            key = dict_node->val.d[i].key;
                if (key == "peer id"){
                    assert(dict_node->val.d[i].val->type == BE_STR);
                    cout << endl << "Peer ID : " << dict_node->val.d[i].val->val.s << endl;
                }else if (key == "ip"){
                    assert(dict_node->val.d[i].val->type == BE_STR);
                    cout << endl << "IP : " << dict_node->val.d[i].val->val.s << endl;
                }else if (key == "port"){
                    assert(dict_node->val.d[i].val->type == BE_INT);
                    cout << endl << "port : " << dict_node->val.d[i].val->val.i << endl;
                }
        }
    }
    return 0;
}

/**
 * WORK IN PROGRESS!!!
 * 
 * This function parse a compact tracker's response according to the following protocol
 * 
 * peers: (binary model) Instead of using the dictionary model described above, the peers value may be a string consisting of multiples of 6 bytes. 
 * First 4 bytes are the IP address and last 2 bytes are the port number. All in network (big endian) notation.
 * 
 * @param *str  : the string to parse
 */

int parse_binary_peers(char *str){
    //No IPv6 support??

    char *initial_str = str;

    int len = strlen(str);

    string t = string(str);

    for(int i=0; i<len; i=i+6){

        //Experimenting the correct parsing

        cout << endl << (int)(t.at(i)) << "." << (int)t.at(i+1) << "." << (int)t[i+2] << "." << (int)t[i+3] << endl;

        cout << endl << "IP[" << i << "] : " << boost::endian::endian_reverse((int)str[i]) << "." << boost::endian::endian_reverse((int)str[i+1]) << "." << boost::endian::endian_reverse((int)str[i+2]) << "." << boost::endian::endian_reverse((int)str[i+3])<< endl;

    }

}

/**
 *  This functionc check if a response is in a compact format
 * 
 *  @param *response    : the response to check
 *  @return             : true if it's compact, otherwise false
 */

bool is_compact_response(const string *response){
    //TODO da migliorare
    std::size_t found = response->find("peers");
    if (found!=std::string::npos){
        if(response->at(found+5) == '0' || response->at(found+5) == 'l')
            return false;
        else
            return true;
    }
    return false;

}