#include "tracker.h"
#include "tracker_udp.hpp"
#include "peer.h" //from int to bint

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


    void udp_manager(const std::string tracker_url, tracker::TParameter param){
        using namespace std;
        using namespace boost::asio::ip;


        cout << endl << "Called UDP procedure " << endl;


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

            cout << endl << "Testing : " << recv_endpoint.address().to_string() << "\t " << std::to_string(port);
            

            socket.connect(recv_endpoint);

            //Try connection
            //socket.open(udp::v4());

            std::vector<uint8_t> req(16);

            connect_request c;
            c.transaction_id = trans_id;


            get_connect_request(c, req);

            //assert(req.size() == 16);

            if(!socket.is_open()){
                cout << endl << "Socket closed";
                return;
            }


            cout << endl << "Connect request : ";
            cout << string_to_hex(req);
            cout << endl << "Sending request...";
            socket.send_to(boost::asio::buffer(req), recv_endpoint);

            if(_io_service.stopped()){
                _io_service.reset();
                _io_service.run();
            }

            cout << endl << "Sended " << "bytes \nWaiting for response...";
            std::vector<uint8_t> recv_buf(16);
            udp::endpoint sender_endpoint;
            size_t len = socket.receive_from(boost::asio::buffer(recv_buf), sender_endpoint);


            if(_io_service.stopped()){
                _io_service.reset();
                _io_service.run();
            }



            cout << endl << "UDP - Reqponse : ";

            uint32_t trans_id_peer;
            uint64_t conn_id;

            verify_connect_resp(recv_buf, trans_id_peer, conn_id);


            cout << endl << "Transaction ID : " << std::to_string(trans_id_peer) << endl << "Connection ID : " << std::to_string(conn_id) << endl;

            
            cout << endl << "Sending announce...";

            req.resize(98);
            get_announce_req(req, param, conn_id);
            cout << endl << string_to_hex(req) << endl << endl;

            socket.send_to(boost::asio::buffer(req), recv_endpoint);

            cout << endl << "Sended " << "bytes \nWaiting for response...";
            recv_buf.resize(512);
            len = socket.receive_from(boost::asio::buffer(recv_buf), sender_endpoint);


            cout << endl << "Response : " << endl << string_to_hex(recv_buf) << endl << endl;


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
            cout << endl << "Error UDP : " << e.what() << endl;
            return;
        }
    }




    void verify_connect_resp(std::vector<uint8_t>& resp, uint32_t& trans_id, uint64_t& conn_id){

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
    }


    void get_announce_req(std::vector<uint8_t>& req, const tracker::TParameter& param, uint64_t conn_id){
        using namespace std;

        vector<uint8_t>::iterator it;
        uint32_t trans_id = get_transaction_id();
        req.clear();

        //Connection ID
        vector<uint8_t> c_id_v(8);
        c_id_v = from_int64_to_bint(conn_id);

        const vector<uint8_t> action = {0,0,0,1};

        //Transaction ID
        vector<uint8_t> t_id_v(4);
        t_id_v = from_int_to_bint(trans_id);

        //Copy info_hash
        vector<uint8_t> info_hash_v(20);

        for(int i=0; i<20; ++i){
            info_hash_v[i] = (uint8_t)param.info_hash_raw[i];
        }

        //Peer ID
        vector<uint8_t> peer_id_v(param.peer_id.begin(), param.peer_id.begin());

        vector<uint8_t> downloaded_v = from_int64_to_bint(param.downloaded);
        vector<uint8_t> left_v = from_int64_to_bint(param.left);
        vector<uint8_t> uploaded_v = from_int64_to_bint(param.uploaded);

        vector<uint8_t> event_v = {0,0,0,2};  //Started
        vector<uint8_t> ip_v = {0,0,0,0};  //Default

        vector<uint8_t> key_v = {0,55,1,0};  //Test Key

        vector<uint8_t> numwant_v = from_int_to_bint(param.numwant); 

        vector<uint8_t> port = {25, 255};   //Test Port


        req.insert( req.end(), c_id_v.begin(), c_id_v.end() );
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


    }



}