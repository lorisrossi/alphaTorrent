#include "tracker.h"
#include <iostream>

using namespace std;

/**
 *  Function that start the communication with the tracker
 * 
 *  @param *url         : the url of the tracker (without parameters)
 *  @param info_hash    : the SHA1 of the info key in the metainfo file
 *  @param peer_id      : the client generated id
 * 
 */
int start_tracker_request(string *url, unsigned char *info_hash_param, const char* peer_id){

    struct TrackerParameter test;

    test.info_hash = "";
    test.peer_id = string(peer_id);
    test.port = 6969;
    test.uploaded = 0;
    test.downloaded = 0;
    test.info_hash_raw = info_hash_param;

    string *enc_url;

    enc_url = url_builder(url->c_str(), test);

    cout << endl << "URL : " << *enc_url << endl;
    cout << "Is Valid? : " << check_url(enc_url) << endl;

    tracker_send_request(url);

    delete enc_url;





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

    enc_info_hash_curl = curl_easy_escape(curl, (char*)param->info_hash_raw, 20);
    enc_peer_id_curl = curl_easy_escape(curl, param->peer_id.c_str(),param->peer_id.length());

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

string *url_builder(string tracker_url, struct TrackerParameter param, CURL *curl, bool tls){

    bool curl_passed=true;;
 
    if(curl == NULL){
        curl_passed=false;
        curl = curl_easy_init();
    }

    if(!curl)
        return NULL;


    string *url_req = new string("");
    /*
    if(tls)
        *url_req = "https://";
    else
        *url_req = "http://";
    */
    
    //TODO Check if "tracker_url" is a proper url by validating it with regex

    *url_req += tracker_url;

    //codifico i parmametri tramite urlencode

    if(urlencode_paramenter(&param, curl) < 0){
        return url_req; //Mando la stringa vuota così da generare errore
    }

    *url_req += "?info_hash=" + param.info_hash;
    *url_req += "?peer_id=" + param.peer_id;

    assert(param.port <= 65535); //TODO Sostituire con un gestore dell'errore


    *url_req += "?port=" + to_string(param.port);
    *url_req += "?uploaded=" + to_string(param.uploaded);
    *url_req += "?downloaded=" + to_string(param.downloaded);



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

int tracker_send_request(string *url, CURL *curl){

    CURLcode code;
    bool curl_passed=true;;
 
    if(curl == NULL){
        curl_passed=false;
        curl = curl_easy_init();
    }

    if (curl) {
        code = curl_easy_setopt(curl, CURLOPT_URL, url->c_str());
        if(code != CURLE_OK){
            cerr << "Errore nel settaggio dell'url" << endl;
            return -1;
        }
        /*
        *   Varie opzioni da aggiungere opzionalmente nel file di configurazione
        
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_USERPWD, "user:pass");
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.42.0");
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        */

        string response_string;
        string header_string;
        //Setto la funzione che andrà a scrivere e i rispettivi parametri 
        code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);
        
        char* url_eff;
        long response_code;
        double elapsed;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &elapsed);
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url_eff);
        
        code = curl_easy_perform(curl);
        if(code != CURLE_OK){
            cerr << endl << "Errore nella richiesta al tracker" << endl;
            return -1;
        }

        cout << endl << "Risposta : " << response_string << endl << endl << header_string << endl << endl;
    
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
bool check_url(string *url, CURL *curl)
{
    CURLcode response;
    bool curl_passed=true;;

    if(curl == NULL){
        curl_passed=false;
        curl = curl_easy_init();
    }

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url->c_str());
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







