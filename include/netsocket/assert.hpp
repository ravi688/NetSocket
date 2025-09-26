#pragma once

#include <netsocket/defines.hpp>

#define netsocket_assert(condition) netsocket::__netsocket_assert(condition, #condition)

#ifdef NETSOCKET_DEBUG
#	define netsocket_debug_assert(condition) netsocket::__netsocket_assert(condition, #condition)
#else
#	define netsocket_debug_assert(condition)
#endif // NETSOCKET_DEBUG

#include <string_view>

namespace netsocket
{
	void __netsocket_assert(bool condition, const std::string_view description);
}
