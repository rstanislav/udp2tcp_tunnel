#include "common.h"

#include "asio_helper.h"
#include "net_common.h"
#include "udp_service.h"
#include "underlying_transport_impl.h"

#include "auto_cancel.h"

namespace
{
udp::socket connected_udp_socket(
    io_service& io,
    optional<network::endpoint> const& local_bind)
{
    udp::socket sock(io);
    sock.open(udp::v4());

    sock.set_option(udp::socket::reuse_address(true));
    sock.set_option(udp::socket::broadcast    (true));

    if (local_bind)
        sock.bind(udp::endpoint(*local_bind));

    return sock;
}

///////////////////////////////////////////////////////////////////////////
struct udp_transfer_strategy
{
    udp_transfer_strategy(udp::endpoint const& remote_peer)
        : remote_peer_(remote_peer)
    {
    }

    template<class buffers_sequence, class callback>
    void async_send(udp::socket& sock, buffers_sequence& buf_seq, callback const& cb) const
    {
        typename buffers_sequence::value_type msg = buf_seq.front();
        buf_seq.pop_front();

        sock.async_send_to(msg, msg.dest() ? *msg.dest() : remote_peer_, cb);
    }

    template<class buffer, class callback>
    void async_recv(udp::socket& sock, buffer const& buf, callback const& cb) const
    {
        sock.async_receive(buf, cb);
    }

private:
    udp::endpoint remote_peer_;
};

//template<class T>
//struct scope_swap
//{
//    scope_swap(T* resource, optional<T> new_value)
//        : resource_ (*resource)
//        , old_value_(*resource)
//        , swap_     (new_value)
//    {
//        if (swap_)
//            resource_ = *new_value;
//    }

//   ~scope_swap()
//    {
//        if (swap_)
//            resource_ = old_value_;
//    }

//private:
//    T&      resource_;
//    T       old_value_;
//    bool    swap_;
//};

} // 'anonymous'


namespace network
{

///////////////////////////////////////////////////////////////////////////
struct udp_socket::impl
{
    typedef
        underlying_transport_impl<udp, udp_transfer_strategy>
        underlying_transport;

    impl(
        io_service& io,
        optional<endpoint>  const& local_bind,
        optional<endpoint>  const& remote_server,
        on_receive_f        const& on_receive,
        on_error_f          const& on_error)

    :  strat_    (remote_server ? *remote_server : endpoint())
    ,  transport_(
          underlying_transport::ptr_t(
              underlying_transport::create(
                move(connected_udp_socket(io, local_bind)),
                on_receive,
                on_error,
                &strat_)))
    {
    }

    void send(const void* data, size_t size, optional<endpoint> const& dest)
    {
        transport_->send(data, size, dest);
    }

private:
    udp_transfer_strategy                   strat_;
    auto_cancel_ptr<underlying_transport>   transport_;
};


///////////////////////////////////////////////////////////////////////////
udp_socket::udp_socket(
    io_service& io,
    optional<endpoint> const& local_bind,
    optional<endpoint> const& remote_server,
    on_receive_f const& on_receive,
    on_error_f   const& on_error)

    :  pimpl_(new udp_socket::impl(io, local_bind, remote_server, on_receive, on_error))
{
}

void udp_socket::send(const void* data, size_t size, optional<endpoint> const& destination)
{
    pimpl_->send(data, size, destination);
}

} // namespace network
