#pragma once

#include <common/defines.h> // for u8
#include <netsocket/defines.hpp> // for NETSOCKET_API

#include <vector>
#include <array>
#include <string>
#include <utility>

namespace netsocket
{
	struct NETSOCKET_API IPv4Address
	{
		std::array<u8, 4> parts;

		u8& operator[](u32 index) noexcept { return parts[index]; }
		const u8& operator[](u32 index) const noexcept { return parts[index]; }

		std::string str() const;
	};
	NETSOCKET_API std::vector<std::pair<std::string, IPv4Address>> GetInterfaceIPv4Addresses();
	NETSOCKET_API std::string TrySelectingPhysicalInterfaceIPAddress(const std::vector<std::pair<std::string, IPv4Address>>& ipAddresses);
}
