#include <string>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <curl/curl.h>
#include <curl/easy.h>
#include "bencode.h"    //For processing response

using namespace std;

//Error Codes
#define CURL_NOT_INIT -10
#define CURL_ESCAPE_ERROR -11


//Struttura di riferimento che contiene tutti i parametri del tracker

struct TrackerParameter{
    string tracker_url;
    string info_hash;
    string peer_id;
    uint port;
    uint uploaded;
    uint downloaded;
    uint left;
    char* info_hash_raw;
};



/*****************************************************************************************
 * 
 *  Tracker Request
 * 
 *  GET Parameter List
 * 
 *  info_hash : urlencoded 20-byte SHA1 hash of the value of the info key from the Metainfo file 
 *  peer_id: urlencoded 20-byte string used as a unique ID for the client, generated by the client at startup.
 *  port: The port number that the client is listening on.
 *  uploaded: The total amount uploaded in base ten ASCII.
 *  downloaded: The total amount downloaded in base ten ASCII
 *  left: The number of bytes this client still has to download in base ten ASCII.
 *  compact: Setting this to 1 indicates that the client accepts a compact response. 
 *  no_peer_id: Indicates that the tracker can omit peer id field in peers dictionary.
 *  event: If specified, must be one of started, completed, stopped
 *      started: The first request to the tracker must include the event key with this value.
 *      stopped: Must be sent to the tracker if the client is shutting down gracefully.
 *      completed: Must be sent to the tracker when the download completes.
 * 
 * 
 * **************************************************************************************/


int tracker_send_request(string *url, string  *response, CURL *curl = NULL);
bool check_url(string *url, CURL *curl=NULL);
string *url_builder(string tracker_url, struct TrackerParameter param,CURL *curl=NULL,  bool tls = false);
int urlencode_paramenter(struct TrackerParameter *param, CURL *curl = NULL);
int start_tracker_request(TrackerParameter *param);
int process_tracker_response(string *response);