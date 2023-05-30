#include "session.hpp"
#include <cassert>


namespace http
{
	session_t::session_t(const std::wstring &agent)
		: handle_t(::WinHttpOpen(agent.c_str(),
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		WINHTTP_FLAG_ASYNC))
	{
		if( !is_valid() )
			throw http_error_t();
	}

	void session_t::set_timeout(
		const std::chrono::milliseconds &resolve,
		const std::chrono::milliseconds &connect,
		const std::chrono::milliseconds &send,
		const std::chrono::milliseconds &recv)
	{
		assert(is_valid());

		auto suc = ::WinHttpSetTimeouts(*this, (int)resolve.count(), (int)connect.count(), 
			(int)send.count(), (int)recv.count());
		assert(suc);
	}
}