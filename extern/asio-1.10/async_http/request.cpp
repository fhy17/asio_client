#include "request.hpp"

#include <cassert>

#include "url.hpp"
#include "connection.hpp"



namespace http
{
	const wchar_t request_type_verb[][32] = 
	{
		L"GET",
		L"POST"
	};


	request_t::request_t(const connection_t &con, request_type type)
		: handle_t(::WinHttpOpenRequest(con, request_type_verb[(std::uint32_t)type], con.url().path(), 0, WINHTTP_NO_REFERER, 
		WINHTTP_DEFAULT_ACCEPT_TYPES, con.url().is_secure() ? WINHTTP_FLAG_SECURE : 0))
	{
		if( !is_valid() )
			throw http_error_t();

		auto func = ::WinHttpSetStatusCallback(*this, &request_t::callback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);
		assert(func != WINHTTP_INVALID_STATUS_CALLBACK);
		if( func == WINHTTP_INVALID_STATUS_CALLBACK )
			throw http_error_t("set http callback error");
	}

	request_t::~request_t()
	{
		::WinHttpSetStatusCallback(*this, 0, 0, 0);
		register_callback(nullptr, nullptr, nullptr, nullptr, nullptr);
	}

	void request_t::send_request(const std::wstring &headers, const std::pair<const void *, std::uint32_t> &request_data)
	{
		assert(response_complete_ != 0 &&
			read_ != 0 &&
			error_ != 0);

		BOOL suc = ::WinHttpSendRequest(*this, 
			headers.c_str(), (DWORD)headers.length(), 
			(void *)request_data.first, request_data.second, (DWORD)headers.length() + request_data.second, 
			(DWORD_PTR)this);
		assert(suc);
		if( !suc )
			throw http_error_t();
	}

	void request_t::register_callback(const handle_header_complete_t &header_complete, const handle_response_complete_t &response_complete, 
		const handle_read_t &read, const handle_redirect_t &redirect, const handle_error_t &error)
	{
		header_complete_	= header_complete;
		response_complete_	= response_complete;
		read_				= read;
		redirect_			= redirect;
		error_				= error;
	}

	void CALLBACK request_t::callback(HINTERNET handle, DWORD_PTR context, DWORD code, void* info, DWORD length)
	{
		if( 0 != context )
		{
			request_t* pT = reinterpret_cast<request_t *>(context);

			try
			{
				bool suc = pT->_on_callback(code, info, length);
			}
			catch(std::exception &e)
			{
				pT->error_(std::string("http error: ") + e.what());
				pT->response_complete_(false);
			}
		}
	}


	namespace
	{
		void handle_default(request_t *req, const void* info, DWORD length)
		{
			
		}

		void handle_header_available(request_t *req, const void* info, DWORD length)
		{
			query::status_code status_code;
			if( !req->query_http_header(status_code) )
				throw http_error_t();

			auto code = _wtoi((const wchar_t *)status_code.buffer());
			if( HTTP_STATUS_OK != code )
				throw http_error_t(error_code(code));

			http::query::raw_headers accept;
			req->query_http_header(accept);

			if( !::WinHttpReadData(*req, &req->buf_[0], (DWORD)req->buf_.size(), 0) ) // async result
				throw http_error_t();

			std::uint32_t size = 0;
			query::content_length content_length;
			if( !req->query_http_header(content_length) )
				size = 0;
			else
				size = _wtoi((const wchar_t *) content_length.buffer());

			req->header_complete_(std::cref(*req), size);

		}

		void handle_read_complete(request_t *req, const void* info, DWORD length)
		{
			if( 0 < length )
			{
				if( !req->read_(req->buf_.data(), length) )
					throw http_error_t("user exit");

				if( !::WinHttpReadData(*req, &req->buf_[0], (DWORD)req->buf_.size(), 0) ) // async result
					throw http_error_t();
			}
			else
			{
				req->response_complete_(true);
			}
		}

		void handle_send_complete(request_t *req, const void* info, DWORD length)
		{
			if( !::WinHttpReceiveResponse(*req, 0) ) // reserved
				throw http_error_t();
		}

		void handle_redirect(request_t *req, const void* info, DWORD length)
		{
			req->redirect_(reinterpret_cast<const wchar_t *>(info), length);
		}

		void handle_ssl_error(request_t *req, const void* info, DWORD length)
		{
			throw http_error_t(ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED);
		}

		void handle_error(request_t *req, const void* info, DWORD length)
		{
			const WINHTTP_ASYNC_RESULT *result = reinterpret_cast<const WINHTTP_ASYNC_RESULT *>(info);
			throw http_error_t(result->dwError);
		}

		struct
		{
			size_t code_;
			typedef void (*status_handle)(request_t *, const void*, DWORD);
			status_handle handle_;
		}handle_code[] = 
		{
			{ WINHTTP_CALLBACK_STATUS_RESOLVING_NAME,			&handle_default },
			{ WINHTTP_CALLBACK_STATUS_NAME_RESOLVED,			&handle_default },
			{ WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER,		&handle_default },
			{ WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER,		&handle_default },
			{ WINHTTP_CALLBACK_STATUS_SENDING_REQUEST,			&handle_default },
			{ WINHTTP_CALLBACK_STATUS_REQUEST_SENT,				&handle_default },
			{ WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE,		&handle_default },
			{ WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED,		&handle_default },
			{ WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION,		&handle_default },
			{ WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED,		&handle_default },
			{ WINHTTP_CALLBACK_STATUS_HANDLE_CREATED,			&handle_default },
			{ WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING,			&handle_default },
			{ WINHTTP_CALLBACK_STATUS_DETECTING_PROXY,			&handle_default },
			{ WINHTTP_CALLBACK_STATUS_REDIRECT,					&handle_redirect },
			{ WINHTTP_CALLBACK_STATUS_INTERMEDIATE_RESPONSE,	&handle_default },
			{ WINHTTP_CALLBACK_STATUS_SECURE_FAILURE,			&handle_ssl_error },
			{ WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE,		&handle_header_available },
			{ WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE,			&handle_default },
			{ WINHTTP_CALLBACK_STATUS_READ_COMPLETE,			&handle_read_complete },
			{ WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE,			&handle_default },
			{ WINHTTP_CALLBACK_STATUS_REQUEST_ERROR,			&handle_error },
			{ WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE,		&handle_send_complete },
		};
	}

	bool request_t::_on_callback(DWORD code, const void* info, DWORD length)
	{
		for(size_t i = 0; i != _countof(handle_code); ++i)
		{
			if( code == handle_code[i].code_ )
			{
				handle_code[i].handle_(this, info, length);
				return true;
			}
		}
		
		assert(0);
		return false;
	}
}
