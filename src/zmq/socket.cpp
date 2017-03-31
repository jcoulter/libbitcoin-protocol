/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/protocol/zmq/socket.hpp>

#include <cstdint>
#include <string>
#include <zmq.h>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/protocol/zmq/authenticator.hpp>
#include <bitcoin/protocol/zmq/certificate.hpp>
#include <bitcoin/protocol/zmq/identifiers.hpp>
#include <bitcoin/protocol/zmq/message.hpp>
#include <bitcoin/protocol/zmq/zeromq.hpp>

namespace libbitcoin {
namespace protocol {
namespace zmq {

static constexpr int32_t zmq_true = 1;
static constexpr int32_t zmq_fail = -1;
static constexpr int32_t zmq_send_buffer = 1000;
static constexpr int32_t zmq_receive_buffer = 1000;
static constexpr int32_t zmq_linger_milliseconds = 0;

int32_t socket::to_socket_type(role socket_role)
{
    switch (socket_role)
    {
        case role::pair: return ZMQ_PAIR;
        case role::publisher: return ZMQ_PUB;
        case role::subscriber: return ZMQ_SUB;
        case role::requester: return ZMQ_REQ;
        case role::replier: return ZMQ_REP;
        case role::dealer: return ZMQ_DEALER;
        case role::router: return ZMQ_ROUTER;
        case role::puller: return ZMQ_PULL;
        case role::pusher: return ZMQ_PUSH;
        case role::extended_publisher: return ZMQ_XPUB;
        case role::extended_subscriber: return ZMQ_XSUB;
        case role::streamer: return ZMQ_STREAM;
        default: return -1;
    }
}

// zmq_term terminates blocking operations but blocks until each socket in the
// context is explicitly closed. Socket close kills transfers after linger.
socket::socket(void* zmq_socket)
  : self_(zmq_socket),
    send_buffer_(zmq_send_buffer),
    receive_buffer_(zmq_receive_buffer),
    identifier_(reinterpret_cast<identifier>(zmq_socket))
{
    if (self_ == nullptr)
        return;

    // Because self is only set on construct, sockets are not restartable.
    if (!set(ZMQ_SNDHWM, send_buffer_) ||
        !set(ZMQ_RCVHWM, receive_buffer_) ||
        !set(ZMQ_LINGER, zmq_linger_milliseconds))
    {
        stop();
    }
}

socket::socket(context& context, role socket_role)
  : socket(zmq_socket(context.self(), to_socket_type(socket_role)))
{
}

socket::~socket()
{
    stop();
}

bool socket::stop()
{
    return self_ == nullptr || zmq_close(self_) != zmq_fail;
}

socket::operator const bool() const
{
    return self_ != nullptr;
}

void* socket::self()
{
    return self_;
}

// To preserve identity the id survives after the socket is destroyed.
identifier socket::id() const
{
    return identifier_;
}

code socket::bind(const config::endpoint& address)
{
    if (zmq_bind(self_, address.to_string().c_str()) == zmq_fail)
        return get_last_error();

    return error::success;
}

code socket::connect(const config::endpoint& address)
{
    if (zmq_connect(self_, address.to_string().c_str()) == zmq_fail)
        return get_last_error();

    return error::success;
}

// private
bool socket::set(int32_t option, int32_t value)
{
    return zmq_setsockopt(self_, option, &value, sizeof(value)) != zmq_fail;
}

// private
bool socket::set(int32_t option, const std::string& value)
{
    if (value.empty())
        return true;

    const auto buffer = value.c_str();
    return zmq_setsockopt(self_, option, buffer, value.size()) != zmq_fail;
}

// For NULL security, ZAP calls are only made for non-empty domain.
// For PLAIN/CURVE, calls are always made if ZAP handler is present.
bool socket::set_authentication_domain(const std::string& domain)
{
    return set(ZMQ_ZAP_DOMAIN, domain);
}

// Defines whether the socket will act as server for CURVE security.
bool socket::set_curve_server()
{
    return set(ZMQ_CURVE_SERVER, zmq_true);
}

// Sets socket's long term server key, must set this on CURVE client sockets.
bool socket::set_curve_client(const config::sodium& server_public_key)
{
    return set(ZMQ_CURVE_SERVERKEY, server_public_key.to_string());
}

// Sets socket's long term public key, must set this on CURVE client sockets.
bool socket::set_public_key(const config::sodium& key)
{
    return set(ZMQ_CURVE_PUBLICKEY, key.to_string());
}

// You must set this on both CURVE client and server sockets.
bool socket::set_private_key(const config::sodium& key)
{
    return set(ZMQ_CURVE_SECRETKEY, key.to_string());
}

// Use on client for both set_public_key and set_private_key from a cert.
// If CURVE is not required by server, call set_certificate({ null_hash })
// to generate an arbitrary client certificate for a secure socket.
bool socket::set_certificate(const certificate& certificate)
{
    return certificate &&
        set_public_key(certificate.public_key().to_string()) &&
        set_private_key(certificate.private_key().to_string());
}

bool socket::set_socks_proxy(const config::authority& socks_proxy)
{
    return !socks_proxy || set(ZMQ_SOCKS_PROXY, socks_proxy.to_string());
}

code socket::send(message& packet)
{
    return packet.send(*this);
}

code socket::receive(message& packet)
{
    return packet.receive(*this);
}

} // namespace zmq
} // namespace protocol
} // namespace libbitcoin
