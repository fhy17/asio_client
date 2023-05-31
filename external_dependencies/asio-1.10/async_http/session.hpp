#ifndef __WIN_HTTP_SESSION_HPP
#define __WIN_HTTP_SESSION_HPP

#include "handle.hpp"


namespace http
{
	class session_t 
		: public handle_t
	{
	public:
		session_t(const std::wstring &agent);
		
		session_t(const session_t &) = delete;
		session_t &operator=(const session_t &) = delete;

	public:
		void set_timeout(
			const std::chrono::milliseconds &resolve, 
			const std::chrono::milliseconds &connect,
			const std::chrono::milliseconds &send, 
			const std::chrono::milliseconds &recv);
	};
}



#endif