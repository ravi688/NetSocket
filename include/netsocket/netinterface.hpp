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

		IPv4Address() = default;
		IPv4Address(const std::string_view str);

		u8& operator[](u32 index) noexcept { return parts[index]; }
		const u8& operator[](u32 index) const noexcept { return parts[index]; }

		std::string str() const;

		decltype(auto) begin() { return parts.begin(); }
		decltype(auto) end() { return parts.end(); }
		decltype(auto) begin() const { return parts.cbegin(); }
		decltype(auto) end() const { return parts.cend(); }
	};
	NETSOCKET_API std::vector<std::pair<std::string, IPv4Address>> GetInterfaceIPv4Addresses();
	NETSOCKET_API std::string TrySelectingPhysicalInterfaceIPAddress(const std::vector<std::pair<std::string, IPv4Address>>& ipAddresses, std::string_view prefix = "");
}
