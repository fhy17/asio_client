#ifndef __WIN_HTTP_REQUEST_HPP
#define __WIN_HTTP_REQUEST_HPP


#include <array>
#include <vector>
#include <functional>
#include <cstdint>

#include "handle.hpp"
#include "query_header.hpp"

namespace http
{
	class connection_t;

	enum class request_type
	{
		get,
		post
	};

	class request_t 
		: public handle_t
	{
	public:
		typedef std::function<void(const request_t &, std::uint32_t)>	handle_header_complete_t;
		typedef std::function<void(bool suc)>							handle_response_complete_t;
		typedef std::function<bool(const char *buf, std::uint32_t len)>		handle_read_t;
		typedef std::function<void(const wchar_t *url, std::uint32_t len)>		handle_redirect_t;
		typedef std::function<void(const std::string &)>				handle_error_t;

	public:
		handle_header_complete_t header_complete_;
		handle_response_complete_t response_complete_;
		handle_read_t read_;
		handle_redirect_t redirect_;
		handle_error_t error_;

		std::array<char, 8192> buf_;

	public:
		request_t(const connection_t &con, request_type);
		~request_t();

		request_t(const request_t &) = delete;
		request_t &operator=(const request_t &) = delete;

	public:
		void send_request(const std::wstring &headers = L"", const std::pair<const void *, std::uint32_t> &request_data = {nullptr, 0});
		void register_callback(const handle_header_complete_t &header_complete, const handle_response_complete_t &response_complete, 
			const handle_read_t &read, const handle_redirect_t &redirect, const handle_error_t &error);

		template < int level >
		bool query_http_header(query::header_t<level> &val) const
		{
			auto suc = ::WinHttpQueryHeaders(*this, val.level(), val.name(), val.buffer(), val.length(), val.index());
			if( !suc && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER )
			{
				val.resize(*(val.length()));
				suc = ::WinHttpQueryHeaders(*this, val.level(), val.name(), val.buffer(), val.length(), val.index());
			}
			
			return suc == TRUE;
		}

	private:
		static void CALLBACK callback(HINTERNET handle, DWORD_PTR context, DWORD code, void* info, DWORD length);
		bool _on_callback(DWORD code, const void* info, DWORD length);
	};
}



#endif