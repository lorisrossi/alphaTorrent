#include "tracker.h"
#include "tracker_udp.hpp"
#include "peer.h" //from int to bint
#include "pwp.hpp" //for is_inv_address

namespace t_udp{
    using namespace std;

    int32_t get_transaction_id(){
        using namespace boost::random;
        boost::mt19937 randGen(time(NULL));
        boost::uniform_int<int32_t> uInt32Dist(0, std::numeric_limits<int32_t>::max());
        boost::variate_generator<boost::mt19937&, boost::uniform_int<int32_t> > getRand(randGen, uInt32Dist);

        return getRand();
    }



    bool is_udp_tracker(const std::string& tracker_url){
        if(tracker_url.length() < 6)
            return false;
        if(tracker_url.substr(0,6) == "udp://")
            return true;
        return false;
    }

    void get_tracker_domain(std::string tracker_url, std::string& udp_tracker, uint& port){
        tracker_url.erase(0, 6);    // udp://

        std::string s_port;
        std::size_t found = tracker_url.find(":");
        if (found!=std::string::npos)
            s_port = tracker_url.substr(found+1);

        found = s_port.find("/");
        if (found!=std::string::npos)
            s_port = s_port.substr(0, found);       

        found = tracker_url.find(":");
        if (found!=std::string::npos)
            tracker_url = tracker_url.substr(0, found);    

        port = std::stoul(s_port);
        udp_tracker = tracker_url;

    }




    void get_connect_request(connect_request c, std::vector<uint8_t>& req){
       
        std::vector<uint8_t> pid_action = {0,0,4,23,39,16,25,128,0,0, 0,0,0,0};
       
        //std::vector<uint8_t> pid = from_int_to_bint(c.protocol_id);
        //std::vector<uint8_t> action = {0,0,0,0};
        std::vector<uint8_t> tid = from_int_to_bint(c.transaction_id);
        
        req.clear();
        req = pid_action;
        req.insert( req.end(), tid.begin(), tid.end() );
    }


    void udp_manager(const std::string tracker_url, tracker::TParameter param, pwp::PeerList peer_list){
        using namespace std;
        using namespace boost::asio::ip;


        DLOG(INFO) << "Called UDP procedure " << endl;


        uint port;
        string tracker_domain;

        get_tracker_domain(tracker_url, tracker_domain, port);

        uint32_t trans_id = get_transaction_id();
        
        //Start the request
        try{ 


            udp::resolver resolver(_io_service); //Domain resolve                 
            udp::resolver::query query(tracker_domain, std::to_string(port), boost::asio::ip::resolver_query_base::numeric_service);   //Specify IP and Port
            
            udp::endpoint recv_endpoint = *resolver.resolve(query);

            udp::socket socket(_io_service);    //Create the socket

            boost::system::error_code error;

            DLOG(INFO) << "Testing : " << recv_endpoint.address().to_string() << "\t " << std::to_string(port);
            

            socket.connect(recv_endpoint);

            //Try connection

            std::vector<uint8_t> req(16);

            connect_request c;
            c.transaction_id = trans_id;


            get_connect_request(c, req);

            //assert(req.size() == 16);

            if(!socket.is_open()){
                LOG(ERROR) << "Socket closed";
                return;
            }


            DLOG(INFO) << "Connect request : ";
            DLOG(INFO) << string_to_hex(req);
            DLOG(INFO) << "Sending request...";
            socket.send_to(boost::asio::buffer(req), recv_endpoint);

            // if(_io_service.stopped()){
            //     _io_service.reset();
            //     _io_service.run();
            // }

            LOG(INFO) << "Sended " << "bytes \nWaiting for response...";
            std::vector<uint8_t> recv_buf(16);
            udp::endpoint sender_endpoint;
            size_t len = socket.receive_from(boost::asio::buffer(recv_buf), sender_endpoint);


            if(_io_service.stopped()){
                _io_service.reset();
                _io_service.run();
            }



            DLOG(INFO) << "UDP - Reqponse : ";

            uint32_t trans_id_peer;
            uint64_t conn_id;
            std::vector<uint8_t> conn_id_v;

            verify_connect_resp(recv_buf, trans_id_peer, conn_id, conn_id_v);


            DLOG(INFO) << "Transaction ID : " << std::to_string(trans_id_peer) << "\nConnection ID : " << std::to_string(conn_id);

            
            LOG(INFO) << "Sending announce...";

            req.resize(98);
            get_announce_req(req, param, conn_id_v);
            DLOG(INFO) << string_to_hex(req);

            socket.send_to(boost::asio::buffer(req), recv_endpoint);

            cout << "Sended announce \nWaiting for response...";
            recv_buf.resize(512);
            len = socket.receive_from(boost::asio::buffer(recv_buf), sender_endpoint);


            cout << "Response : \n" << string_to_hex(recv_buf);

            parse_announce_resp(recv_buf, peer_list);
            
            cout << endl << "LOLOL";
            
            
            
            
            //cout << "Parsed " << num << " peers from " << recv_endpoint.address().to_string() << ":" << std::to_string(port);


            if(_io_service.stopped()){
                _io_service.reset();
                _io_service.run();
            }            

            if (error == boost::asio::error::eof){
                LOG(ERROR) << "Connection Closed";
                return; // Connection closed cleanly by peer.
            }else if (error){
                throw boost::system::system_error(error); // Some other error.
            }
        }catch (std::exception& e){
            LOG(ERROR) << "Error UDP : " << e.what();
            return;
        }

        cout << "Exiting from ";

    }




    void verify_connect_resp(std::vector<uint8_t>& resp, uint32_t& trans_id, uint64_t& conn_id, std::vector<uint8_t>& conn_id_v){

        if(resp[0] == 0 && resp[1] == 0 && resp[2] == 0 && resp[3] == 0)
            ;
        else
            return;


        vector<uint8_t>::const_iterator first = resp.begin() + 4;
        vector<uint8_t>::const_iterator last = resp.begin() + 8;
        vector<uint8_t> trans_id_v(first, last);

        trans_id = make_int(trans_id_v);

        vector<uint8_t>::const_iterator first1 = resp.begin() + 8;
        vector<uint8_t>::const_iterator last1 = resp.begin() + 12;
        vector<uint8_t>::const_iterator first2 = resp.begin() + 12; //Not needed
        vector<uint8_t>::const_iterator last2 = resp.begin() + 16;
        vector<uint8_t> conn_id_v1(first1, last1);
        vector<uint8_t> conn_id_v2(first2, last2);

        uint32_t conn_id_1 = make_int(conn_id_v1);
        uint32_t conn_id_2 = make_int(conn_id_v2);
        
        conn_id = (uint64_t) conn_id_1 << 32 | conn_id_2;


        conn_id_v.clear();
        conn_id_v = vector<uint8_t>(first1, last2);

    }


    void get_announce_req(std::vector<uint8_t>& req, const tracker::TParameter& param, std::vector<uint8_t>& conn_id_v){
        using namespace std;

        vector<uint8_t>::iterator it;
        uint32_t trans_id = get_transaction_id();
        req.clear();

        //Connection ID
        // vector<uint8_t> c_id_v(8);
        // c_id_v = from_int64_to_bint(conn_id);
        assert(conn_id_v.size() == 8);

        const vector<uint8_t> action = {0,0,0,1};

        //Transaction ID
        vector<uint8_t> t_id_v(4);
        t_id_v = from_int_to_bint(trans_id);
        assert(t_id_v.size() == 4);
       
        //Copy info_hash
        vector<uint8_t> info_hash_v(20);

        for(int i=0; i<20; ++i){
            info_hash_v[i] = (uint8_t)param.info_hash_raw[i];
        }

        //Peer ID
        vector<uint8_t> peer_id_v(20);
        for(int i=0; i<20; ++i){
            peer_id_v[i] = (uint8_t)param.peer_id.at(i);
        }
        assert(peer_id_v.size() == 20);

        vector<uint8_t> downloaded_v = from_int64_to_bint(param.downloaded);
        vector<uint8_t> left_v = from_int64_to_bint(param.left);
        vector<uint8_t> uploaded_v = from_int64_to_bint(param.uploaded);

        vector<uint8_t> event_v = {0,0,0,0};  //Started
        vector<uint8_t> ip_v = {0,0,0,0};  //Default

        vector<uint8_t> key_v = {0,1,1,1};  //Test Key

        vector<uint8_t> numwant_v = from_int_to_bint(param.numwant); 

        vector<uint8_t> port = {2, 255};   //Test Port


        req.insert( req.end(), conn_id_v.begin(), conn_id_v.end() );
        req.insert( req.end(), action.begin(), action.end() );
        req.insert( req.end(), t_id_v.begin(), t_id_v.end() );
        req.insert( req.end(), info_hash_v.begin(), info_hash_v.end() );
        req.insert( req.end(), peer_id_v.begin(), peer_id_v.end() );
        req.insert( req.end(), downloaded_v.begin(), downloaded_v.end() );
        req.insert( req.end(), left_v.begin(), left_v.end() );
        req.insert( req.end(), uploaded_v.begin(), uploaded_v.end() );
        req.insert( req.end(), event_v.begin(), event_v.end() );
        req.insert( req.end(), ip_v.begin(), ip_v.end() );
        req.insert( req.end(), key_v.begin(), key_v.end() );
        req.insert( req.end(), numwant_v.begin(), numwant_v.end() );
        req.insert( req.end(), port.begin(), port.end() );

        assert(req.size() == 98);

    }



    void parse_announce_resp(std::vector<uint8_t>& resp, pwp::PeerList peer_list){
        using namespace std;

        action_type resp_type = static_cast<action_type>(resp[3]);
        
        //To check if it's at least 20 bytes len response
        int n =0;

        switch(resp_type){

            case none:
                cout << endl << "NONE request received, exiting...";
                return;
            
            case error:
                cout << endl << "Tracker respond with error" << endl;
                process_error(resp);
                return;

            case announce:
                cout << endl << "Valid Response" << endl;
                parse_announce_resp_peers(resp, peer_list);
                cout << endl << "Exiting here" << endl;
                break;



        }
        cout << endl << "Reacher here" << endl;
        return;
    }



/**
 * In case of a tracker error,
 *
 *   server replies packet:
 *   size 	name 	description
 *   int32_t 	action 	The action, in this case 3, for error. See actions.
 *   int32_t 	transaction_id 	Must match the transaction_id sent from the client.
 *   int8_t[] 	error_string 	The rest of the packet is a string describing the error.
 * 
 */

    void process_error(std::vector<uint8_t>& resp){
        using namespace std;

        string s;

        std::vector<uint8_t>::iterator it = resp.begin() + 8;

        for (; it != resp.end(); ++it)
        {
            s += static_cast<char>(*it);
        }

        cout << endl << "Error string : " << s << endl;
    }



    void parse_announce_resp_info(std::vector<uint8_t>& resp){



    }


    void parse_announce_resp_peers(std::vector<uint8_t>& resp, pwp::PeerList peer_list){
        using namespace std;

        vector<uint8_t>::iterator it = resp.begin() + 20;   //Peers list
        uint i=0;
        uint f=0;

        for(; it != resp.end() && (it+6) < resp.end(); it = it +6){
            i++;
            if(i<(resp.size()/6 -1)){
                cout << endl << "Limit reached" << endl;
                return;
            }

            bInt raw_ip = {
                *(it),
                *(it+1),
                *(it+2),
                *(it+3)
            };

            uint32_t ip_addr_int = make_int(raw_ip);
            if((it - resp.begin() >= resp.size()))
                return;
            boost::asio::ip::address_v4 ip_addr(ip_addr_int);

            if(is_inv_address(ip_addr)){
                cout << endl << "Invalid address";
                continue;
            }

            uint16_t port = (uint16_t) *(it+4) << 8 | *(it+5);

            DLOG(INFO) << ip_addr.to_string() << ":" << to_string(port) <<  " received " << endl;


            pwp::peer recv_peer = {
                boost::asio::ip::address(ip_addr),
                port,
                ""
            };


            peer_list->push_back(recv_peer);

            ++i;
            cout << endl << "Finishing\n";
        }
        cout << endl << "Finished";

        return;   //Return valid peer fetched
    }


}