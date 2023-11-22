// Подключаем заголовочный файл <sdkddkver.h> в системе Windows,
// чтобы избежать предупреждения о неизвестной версии Platform SDK,
// когда используем заголовочные файлы библиотеки Boost.Asio
#ifdef WIN32
#include <sdkddkver.h>
#endif

#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <boost/asio/signal_set.hpp>

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <random>
#include <list>
#include "sdk.h"
#include "http_server.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <exchange.pb.h>

/// @brief Генаратор рандомной строки заданной длины
/// @param length длина строки
/// @return рандомная строка
std::string RandomString(size_t length){
    const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwyz";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);

    std::string random_string;

    for(size_t i = 0; i < length; ++i){
        random_string += characters[distribution(generator)];
    }

    return random_string;
}

/// @brief Генератор рандомного целого беззнакового числа
/// @return рандомное целое беззнаковое число
uint32_t RandomUnsignedInt32Number(){
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<uint32_t> distribution;
    return distribution(generator);
}

/// @brief Генератор рандомного беззнакового целого числа в заданном диапазоне
/// @param min минимальное значение
/// @param max максимальное число
/// @return рандомное беззнаковое целое число
uint32_t RandomNumber(uint32_t min, uint32_t max){
    const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwyz";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<uint32_t> distribution(min, max);
    return distribution(generator);
}

namespace {
	namespace net = boost::asio;
	using namespace std::literals;
	namespace sys = boost::system;
	namespace http = boost::beast::http;

	/// @brief Запуск функции на заданном числе потоков
	/// @tparam Fn тип функции
	/// @param n число потоков
	/// @param n fn функция
	template<typename Fn>
	void RunWorkers(unsigned n, const Fn& fn) {
		n = std::max(1u, n);
		std::vector<std::jthread> workers;
		workers.reserve(n - 1);
		// Запускаем n-1 рабочих потоков, выполняющих функцию fn
		while (--n) {
			workers.emplace_back(fn);
		}
		fn();
	}

	namespace beast = boost::beast;
	namespace http = beast::http;
	// Ответ, тело которого представлено в виде строки
	using StringResponse = http::response<http::string_body>;
	// Запрос, тело которого представлено в виде строки
	using StringRequest = http::request<http::string_body>;

	// Чтобы использовать литералы ""s и ""sv стандартной библиотеки, применим std::literals.
	using namespace std::literals;

	// Структура ContentType задаёт область видимости для констант, задающей значения HTTP-заголовка Content-Type
	struct ContentType {
		ContentType() = delete;
		constexpr static std::string_view TEXT_HTML = "text/html"sv;
		// При необходимости внутрь ContentType можно добавить и другие типы контента
	};

	/// @brief Создаёт StringResponse с заданными параметрами
	/// @param status http статус ответа
	/// @param body тело ответа
	/// @param http_version 1.1 или 1.0
	/// @param keep_alive
	/// @param method метод запроса
	/// @param content_type тип тела ответа
	/// @return строковый http ответ
	StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
		bool keep_alive,
		http::verb method,
		std::string_view content_type = ContentType::TEXT_HTML) {
		StringResponse response(status, http_version);
		response.set(http::field::content_type, content_type);
		if (method != http::verb::head) {
			response.body() = body;
		}
		response.content_length(body.size());
		response.keep_alive(keep_alive);
		return response;
	}

	// хэш-таблица соответствия токена в запросе и номера блока данных на сервере
    std::unordered_map<std::string_view, size_t> hash_to_block_num;
	
	// хэш-таблица соответствия номера блоку токену
    std::unordered_map<size_t, std::string> block_num_to_hash;
	
	// хэш-таблица соответствия токена размеру блока
    std::unordered_map<std::string, size_t> hash_to_block_size;
	
	// хэш-таблица соответствия номера блока блоку данных
    std::unordered_map<size_t, std::string> block_num_to_block;
	
	// хранилище токенов
    std::list<std::string> hashes;
	
	// максимальный размер токена
    const size_t MAX_HASH_SIZE{128};
	
	// максимальный размер блока данных на сервере
    const size_t MAX_BLOCK_SIZE{1000000};

	/// @brief Номер блока данных по токену
	/// @param hash токен
	/// @param номер блока данных
    size_t GetBlockNumber(const std::string& hash){
        if(hash_to_block_num.find(hash) == hash_to_block_num.end()){
            hashes.emplace_back(hash);
            auto block_num{RandomUnsignedInt32Number()};
            hash_to_block_num[hashes.back()] = block_num;
            block_num_to_hash[block_num] = hash;
        }
        return hash_to_block_num[hash];
    }

	/// @brief Размер блока данных по токену
	/// @param hash токен
	/// @param размер блока данных
    size_t GetBlockSize(const std::string& hash){
        if(hash_to_block_size.find(hash) == hash_to_block_size.end()){
            hashes.emplace_back(hash);
            hash_to_block_size[hashes.back()] = RandomNumber(1, MAX_BLOCK_SIZE);
        }
        return hash_to_block_size[hash];
    }

	/// @brief Имитация обращение к БД
	/// @param block_num
	/// @param buffer буфер
	/// @param buffer_size размер буфера
	/// @return размер записанного в буфер блока данных
    int GetBlockData(size_t block_num, char* buffer, size_t buffer_size){
        if(block_num_to_block.find(block_num) == block_num_to_block.end()){
            block_num_to_block[block_num] = RandomString(GetBlockSize(block_num_to_hash[block_num]));
        }
        const auto& block = block_num_to_block.at(block_num);
        if(buffer_size < block.size()){
            return 0;
        }
        std::memcpy(buffer, block.data(), block.size());
        return block.size();
    }

	/// @brief Формировщик ответа сервера
	/// @brief client_to_server распаршенный запрос клиента
	/// @brief server_to_client ответ
    void GetServerResponse(const Exchange::ClientToServer& client_to_server, Exchange::ServerToClient& server_to_client){
        for(int i = 0; i < client_to_server.hashes_size(); ++i){
            std::string hash{client_to_server.hashes().at(i)};
            if(hash.size() != MAX_HASH_SIZE){
                continue;
            }
            Exchange::HashAndBlock hash_and_block;
            auto block_num = GetBlockNumber(hash);
            static std::string block(MAX_BLOCK_SIZE, '1');
            auto block_size = GetBlockData(block_num, block.data(), block.size());
            hash_and_block.set_hash(std::move(hash));
            hash_and_block.set_block(block.data(), block_size);
            server_to_client.mutable_hash_and_block()->Add(std::move(hash_and_block));
        }
    }

	/// @brief Обработка запроса на сервер
	/// @param req запрос на сервер 
	/// @return строковый Http ответ на запрос
	StringResponse HandleRequest(StringRequest&& req) {
		const auto text_response = [&req](http::status status, std::string_view text) {
			if (req.method() == http::verb::get || req.method() == http::verb::head) {
				return MakeStringResponse(status, text, req.version(), req.keep_alive(), req.method());
			}
			return MakeStringResponse(http::status::method_not_allowed, "Invalid method", req.version(), req.keep_alive(), req.method());
		};

        Exchange::ClientToServer client_to_server;
        Exchange::ServerToClient server_to_client;
        try {
            if(client_to_server.ParseFromArray( req.body().data(), req.body().size())){
                std::cout << "Parse Ok, hash count "sv << client_to_server.hashes_size() << std::endl;
                GetServerResponse(client_to_server, server_to_client);
            }else{
                std::cout << "Parse error"sv << std::endl;
                return text_response(http::status::bad_request, "Parse error"sv);
            }
        }
        catch (...) {
            std::cout << "Parse error by exception"sv << std::endl;
            return text_response(http::status::bad_request, "Parse error by exception"sv);
        }

        size_t size_output = server_to_client.ByteSizeLong();
        char *msg = new char[size_output];
        server_to_client.SerializeToArray(msg, size_output);

        return text_response(http::status::ok, {msg, size_output});
	};
}

int main() {
	using namespace std::literals;
	namespace net = boost::asio;

	const unsigned int num_threads = std::thread::hardware_concurrency();

	net::io_context ioc(num_threads);

	const auto address = net::ip::make_address("0.0.0.0");
    constexpr int port = 8080;
	http_server::ServerHttp(ioc, { address, port }, [](auto&& req, auto&& sender) {
		sender(HandleRequest(std::forward<decltype(req)>(req)));
		});

	std::cout << "Server has started..."sv << std::endl;

	RunWorkers(num_threads, [&ioc] {
		ioc.run();
		});
}
