#include "tracker.h"
#include <iostream>
#include <algorithm>    //Lower String

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using namespace std;

/**
 *  Function that start the communication with the tracker
 *
 *  @param *url         : the url of the tracker (without parameters)
 *  @param info_hash    : the SHA1 of the info key in the metainfo file
 *  @param peer_id      : the client generated id
 *
 */
int start_tracker_request(TrackerParameter *param){


    param->info_hash = "";
    param->port = 6969;
    param->uploaded = 0;
    param->downloaded = 0;

    string *enc_url;
    string response = "";

    enc_url = url_builder(param->tracker_url.c_str(), *param);

    cout << endl << "URL : " << *enc_url << endl;
    cout << "Is Valid? : " << check_url(enc_url) << endl;

    tracker_send_request(enc_url, &response);

    process_tracker_response(&response);


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

    *url_req += tracker_url + "?";

    //codifico i parmametri tramite urlencode

    if(urlencode_paramenter(&param, curl) < 0){
        return url_req; //Mando la stringa vuota così da generare errore
    }

    //Covert to lowercase
    std::transform(param.info_hash.begin(), param.info_hash.end(), param.info_hash.begin(), ::tolower);

    *url_req += "info_hash=" + param.info_hash;
    *url_req += "&peer_id=" + param.peer_id;

    assert(param.port <= 65535); //TODO Sostituire con un gestore dell'errore


    *url_req += "&port=" + to_string(param.port);
    *url_req += "&uploaded=" + to_string(param.uploaded);
    *url_req += "&downloaded=" + to_string(param.downloaded);
    *url_req += "&left=" + to_string(param.left);
    *url_req += "&event=started";




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

int tracker_send_request(string *url, string *response, CURL *curl){

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
            cerr << endl << "Errore nella richiesta al tracker" << endl;
            return -1;
        }

        cout << endl << "Risposta : " << *response << endl << endl << header_string << endl << endl;


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



int process_tracker_response(string *response){

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
            }




        }
    }
}