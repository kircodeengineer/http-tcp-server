#pragma once
#include "sdk.h"
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace http_server {

    namespace net = boost::asio;
    using tcp = net::ip::tcp;
    namespace beast = boost::beast;
    namespace sys = boost::system;
    namespace http = beast::http;

    void ReportError(beast::error_code ec, std::string_view what);

    class SessionBase {
    protected:
        using HttpRequest = http::request<http::string_body>;
    public:
        SessionBase() = delete;

        SessionBase& operator=(const SessionBase&) = delete;

        void Run();

    protected:
        explicit SessionBase(tcp::socket&& socket);

        ~SessionBase() = default;

        template<typename Body, typename Fields>
        void Write(http::response<Body, Fields>&& response) {
            auto self_response = std::make_shared<http::response<Body, Fields>>(std::move(response));

            auto self = GetSharedThis();

            http::async_write(stream_, *self_response,
                [self_response, self](beast::error_code ec, std::size_t bytes_written) {
                    self->OnWrite(self_response->need_eof(), ec, bytes_written);
                });
        }

    private:
        /// @brief Асинхронное чтение запроса. Может быть вызван несколько раз.
        void Read();

        void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read);

        void OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written);

        void Close();

        virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;
        virtual void HandleRequest(HttpRequest&& request) = 0;
    private:
        // tcp_stream содержит внутри себя сокет и добавляет поддержку таймаутов
        beast::tcp_stream stream_;
        beast::flat_buffer buffer_;
        HttpRequest request_;
    };


    /// @brief Этот класс будет отвечать за сеанс асинхронного обмена данными с клиентом. 
    /// @tparam RequestHandler
    template <typename RequestHandler>
    class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
    public:
        template <typename Handler>
        Session(tcp::socket&& socket, Handler&& request_handler) :
            SessionBase(std::move(socket)),
            request_handler_(std::forward<Handler>(request_handler))
        {};
    private:

        void HandleRequest(HttpRequest&& request) override {
            request_handler_(std::move(request), [self = this->shared_from_this()](auto&& response){
                self->Write(std::move(response));
            });
        }

        std::shared_ptr<SessionBase> GetSharedThis() override {
            return this->shared_from_this();
        }
    private:
        RequestHandler request_handler_;
    };

    /// @brief Слушатель асинхронно принимает входящие TCP-соединения. 
    /// @tparam RequestHandler тип функции-обработчика запросов
    template <typename RequestHandler>
    class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {

    private:
        // Ссылка на io_context, управляющий асинхронными операциями.
        // Контекст требуется для конструирования сокета, поэтому нужно сохранить ссылку на этот контекст.
        net::io_context& ioc_;

        // приём соединений клиентов
        tcp::acceptor acceptor_;

        // обработчик запросов
        RequestHandler request_handler_;

    public:
        template <typename Handler>
        Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& request_handler)
            : ioc_(ioc),
            acceptor_(net::make_strand(ioc)),
            request_handler_(std::forward<Handler>(request_handler))
        {
            acceptor_.open(endpoint.protocol());

            acceptor_.set_option(net::socket_base::reuse_address(true));

            acceptor_.bind(endpoint);

            acceptor_.listen(net::socket_base::max_listen_connections);
        }

        void Run() {
            DoAccept();
        }

    private:
        void DoAccept() {
            acceptor_.async_accept(
                net::make_strand(ioc_),

                beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this())
            );

        }

        void OnAccept(beast::error_code ec, tcp::socket socket) {
            using namespace std::literals;

            if (ec) {
                return ReportError(ec, "accept"sv);
            }

            // Асинхронно обрабатываем сессии
            AsyncRunSession(std::move(socket));

            // Принимаем новое сообщение
            DoAccept();
        }

        void AsyncRunSession(tcp::socket&& socket) {
            std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
        }
    };

    /// @brief Вспомогательная функция для запуска сервера
    /// @tparam RequestHandler
    /// @param ioc
    /// @param endpoint
    /// @param request_handler
    template <typename RequestHandler>
    void ServerHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
        using MyListener = Listener<std::decay_t<RequestHandler>>;

        std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
    }
}; // namespace http_server
