#include "App.h"
#include "WebSocketProtocol.h"
#include "ucesb.pb.h"
#include "zmqpp/message.hpp"
#include "zmqpp/poller.hpp"
#include "zmqpp/socket_types.hpp"

#include <google/protobuf/util/json_util.h>
#include <zmqpp/zmqpp.hpp>
#include <thread>
#include <mutex>

/* Global state, but we have two threads so we are careful
 * In general the threads don't interact */


/* Data managed by the WebSocket thread
 * We use wsLoop->defer() to call WebSocket commands from the other thread */
struct us_listen_socket_t *listen_socket;
uWS::App wsApp;
uWS::Loop* wsLoop;
std::string zmq_string("tcp://localhost:4242");
std::atomic<int> clients;

/* Data managed by the ZeroMQ thread */
// string of the last message published for new clients
std::string lastmsg;
std::mutex lastmsg_mutex;
//
void zmq_thread()
{
    zmqpp::context zmq_context;
    zmqpp::socket zmq_subber(zmq_context, zmqpp::socket_type::sub);
    zmqpp::poller zmq_poller;

    zmq_subber.connect(zmq_string);
    zmq_subber.subscribe("");
    zmqpp::message msg;

    zmq_poller.add(zmq_subber);

    frs_monitor::UcesbReport proto;

    while(true)
    {
        if(!zmq_poller.poll(10000))
        {
            // don't report timeout if ucesb said it was exiting
            if (lastmsg.find("exit") != lastmsg.npos)
                continue;

            {
                std::lock_guard<std::mutex> guard(lastmsg_mutex);
                lastmsg = "{ \"message\": \"timeout\" }";
            }

            wsLoop->defer([=] {
                wsApp.publish("ucesb", lastmsg, uWS::OpCode::TEXT, true);
            });

            continue;
        }

        zmq_subber.receive(msg);

        if (msg.is_signal()) continue;

        std::string envelope, argument, output;
        msg >> envelope;

        if (envelope == "stat" || envelope == "init" || envelope == "dead") {
            msg >> argument;
            proto.ParseFromString(argument);
            google::protobuf::util::JsonPrintOptions options;
            std::string json;
            MessageToJsonString(proto, &json, options);
            output = "{ \"message\": \"" + envelope + "\", \"data\": " + json + "}";
        }
        else {
            output = "{ \"message\": \"" + envelope + "\" }";
        }

        {
            std::lock_guard<std::mutex> guard(lastmsg_mutex);
            lastmsg = output;
        }

        wsLoop->defer([=] {
            wsApp.publish("ucesb", output, uWS::OpCode::TEXT, true);
        });
    }
}

int main(int argc, char** argv) {
    /* ws->getUserData returns one of these */
    struct PerSocketData {

    };

    if (argc == 2)    {
        std::string arg(argv[1]);
        int protopos = arg.find("://");
        std::string proto, address, port;
        if (protopos != arg.npos) {
            proto = arg.substr(0, protopos);
            arg = arg.substr(protopos + 3);
        }
        address = arg;
        int portpos = arg.find(":");
        if (portpos != arg.npos) {
            address = arg.substr(0, portpos);
            port = arg.substr(portpos + 1);
        }

        if (proto.empty()) proto = "tcp";
        if (port.empty()) port = "4242";
        if (address.empty()) {
            std::cerr << "Invalid connection string specified" << std::endl;
            std::cerr << "Usage " << argv[0] << " [tcp://]server[:port]" << std::endl;
            return 1;
        }

        zmq_string = proto + "://" + address + ":" + port;
    }

    std::cout << "ucesb WebSocket Relay (v1) Starting..." << std::endl;
    std::cout << "Connecting to ucesb socket " << zmq_string << std::endl;

    wsLoop = uWS::Loop::get();

    wsApp.get("/debug", [](auto* res, auto* req) {
    std::unique_lock<std::mutex> guard(lastmsg_mutex, std::defer_lock);
    if (guard.try_lock())
    {
        if (!lastmsg.empty())
        {
            res->writeHeader("Content-Type", "application/json");
            res->end(lastmsg);
        }
        else
        {
            res->writeHeader("Content-Type", "text/plain");
            res->end("no message");
        }
        guard.unlock();
    }
    });

    /* Very simple WebSocket broadcasting server */
    wsApp.ws<PerSocketData>("/*", {
        /* Settings */
        .compression = uWS::DEDICATED_COMPRESSOR_3KB,
        .maxPayloadLength = 16 * 1024 * 1024,
        .idleTimeout = 10,
        .maxBackpressure = 1 * 1024 * 1024,
        /* Handlers */
        .upgrade = nullptr,
        .open = [](auto *ws) {
            /* Let's make every connection subscribe to the "ucesb" topic */
            ws->subscribe("ucesb");
            /* Send the previus cached status */
            std::unique_lock<std::mutex> guard(lastmsg_mutex, std::defer_lock);
            if (guard.try_lock())
            {
                if (!lastmsg.empty())
                    ws->send(lastmsg, uWS::OpCode::TEXT, true);
                guard.unlock();
            }
            clients++;
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            /* Simply broadcast every single message we get */
            //ws->publish("broadcast", message, opCode, true);
            if (message == "ping") {
                ws->send("pong", opCode);
            }
        },
        .drain = [](auto *ws) {
            /* Check getBufferedAmount here */
        },
        .ping = [](auto *ws) {

        },
        .pong = [](auto *ws) {

        },
        .close = [](auto *ws, int code, std::string_view message) {
            /* We automatically unsubscribe from any topic here */
            clients--;
        }
    }).listen(9001, [](auto *token) {
        listen_socket = token;
        if (token) {
            std::cout << "WebSocket server listening on port " << 9001 << std::endl;
        }
    });

    // Now we have created the WebSocket app we can spawn the ZeroMQ thread
    // (as the globals are valid)
    //
    std::thread zmq(zmq_thread);
    std::thread terminal;

    if (isatty(fileno(stdout))) {
        terminal = std::thread([]() {
            while(true) {
                std::cout << "\rLast message: ???, WebSocket Clients: " << clients.load();
                std::cout.flush();
                sleep(5);
            }
        });
    }

    // And run the WebSocket on this now
    wsApp.run();
}
