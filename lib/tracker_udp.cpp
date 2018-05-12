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
        std::vector<uint8_t> pid = from_int_to_bint(c.protocol_id);
        std::vector<uint8_t> action = {0,0,0,0};
        std::vector<uint8_t> tid = from_int_to_bint(c.transaction_id);
        
        req.clear();
        req.insert( req.end(), pid.begin(), pid.end() );
        req.insert( req.end(), action.begin(), action.end() );
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
            udp::resolver::query query(tracker_domain, std::to_string(port));   //Specify IP and Port
            
            udp::endpoint recv_endpoint = *resolver.resolve(query);

            udp::socket socket(_io_service);    //Create the socket

            boost::system::error_code error;

            LOG(INFO) << "Testing : " << tracker_domain << "\t " << std::to_string(port);
            
            //Try connection
            socket.open(udp::v4());

            std::vector<uint8_t> req(64);

            connect_request c;
            c.transaction_id = trans_id;


            get_connect_request(c, req);
            cout << endl << "Connect request : ";
            std::cout.write((char*)req.data(), req.size());

            cout << endl << "Sending request...";
            socket.send_to(boost::asio::buffer(req), recv_endpoint);

            cout << endl << "Waiting for response...";
            boost::array<char, 128> recv_buf;
            udp::endpoint sender_endpoint;
            size_t len = socket.receive_from(
                boost::asio::buffer(recv_buf), sender_endpoint);

            cout << endl << "UDP - Reqponse : ";
            std::cout.write(recv_buf.data(), len);
            cout << endl;

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


}