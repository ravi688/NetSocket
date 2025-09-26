#include <netsocket/assert.hpp>
#undef _ASSERT
#include <spdlog/spdlog.h>
#include <common/third_party/debug_break.h>

namespace netsocket
{
	void __netsocket_assert(bool condition, const std::string_view description)
	{
		if(!condition)
		{
			spdlog::critical("Assertion Failure: {}", description);
			debug_break();
		}
	}
}