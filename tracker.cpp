#include "tracker.h"
#include <iostream>

using namespace std;

//Funzione di testing
int test_tracker_url_builder(){

    struct TrackerParameter test;

    test.info_hash = "aaaaaa";
    test.peer_id = "3120";
    test.port = 444;
    test.uploaded = 200;
    test.downloaded = 500;

    string *url;

    url = url_builder("www.google.it", test);

    cout << endl << "URL : " << *url << endl;
    cout << "Is Valid? : " << check_url(url) << endl;

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

    if(curl)
        curl = curl_easy_init();

    if(!curl)
        return CURL_NOT_INIT;

    //I campi "info_hash" e "peer_id" devono essere codificati tramite "urlencoding"

    enc_info_hash_curl = curl_easy_escape(curl, param->info_hash.c_str(), param->info_hash.length());
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


    if(curl == NULL)
        curl = curl_easy_init();
    if(!curl)
        return NULL;


    string *url_req = new string();
    
    if(tls)
        *url_req = "https://";
    else
        *url_req = "http://";

    
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

    return url_req;
}


int tracker_send_request(struct TrackerParameter param, CURL *curl){


    if(curl == NULL)
        curl = curl_easy_init();


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

    if(curl == NULL)
        curl = curl_easy_init();

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url->c_str());
        /* don't write output to stdout */
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
        /* Perform the request */
        response = curl_easy_perform(curl);
        /* always cleanup */
        curl_easy_cleanup(curl);
    }

    return (response == CURLE_OK) ? true : false;
}