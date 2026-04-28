#pragma once

#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <iostream>
#include "candle.hpp"

namespace algoforge {

class CandlePublisher {
public:
  CandlePublisher(zmq::context_t& ctx, const std::string& endpoint)
    : socket_(ctx, zmq::socket_type::push)
{
    int sndhwm = 0;  // infinite send buffer — never block
    socket_.set(zmq::sockopt::sndhwm, sndhwm);
    socket_.bind(endpoint);
}
    void publish(const Candle& c) {
        nlohmann::json j;
        j["symbol"]       = c.symbol;
        j["open"]         = c.open;
        j["high"]         = c.high;
        j["low"]          = c.low;
        j["close"]        = c.close;
        j["volume"]       = c.volume;
        j["timestamp"]    = c.timestamp;
        j["interval_sec"] = c.interval_sec;

        std::string msg = j.dump();

        try {
            socket_.send(zmq::buffer(msg), zmq::send_flags::dontwait);
        } catch (const zmq::error_t& e) {
            // No subscriber connected — skip silently
        }
    }

private:
    zmq::socket_t socket_;
};

} // namespace algoforge