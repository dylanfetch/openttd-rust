/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_boundary_profile.hpp Diagnostic direct-service crossing counts. */
#ifndef RUST_STATION_BOUNDARY_PROFILE_HPP
#define RUST_STATION_BOUNDARY_PROFILE_HPP

enum class StationBoundary : uint8_t { Station, Links, Loading };

/* Disabled during timing. No callback allocates; output occurs at shutdown.
 * returned_bytes counts the ABI result envelope, excluding pointed-to storage. */
struct StationBoundaryProfile {
	bool enabled = std::getenv("OPENTTD_STATION_PROFILE") != nullptr;
	uint64_t calls[3]{};
	uint64_t returned_bytes[3]{};
	uint64_t grouped_reads = 0;
	uint64_t scoped_bodies = 0;

	~StationBoundaryProfile()
	{
		if (!this->enabled) return;
		const char *personal = std::getenv("HOME");
		if (personal == nullptr) return;
		if (FILE *file = std::fopen(fmt::format("{}/station-boundary-profile.json", personal).c_str(), "w")) {
			fmt::print(file, "{{\"station_calls\":{},\"station_returned_bytes\":{},\"links_calls\":{},\"links_returned_bytes\":{},\"loading_calls\":{},\"loading_returned_bytes\":{},\"grouped_reads\":{},\"scoped_bodies\":{}}}\n", this->calls[0], this->returned_bytes[0], this->calls[1], this->returned_bytes[1], this->calls[2], this->returned_bytes[2], this->grouped_reads, this->scoped_bodies);
			std::fclose(file);
		}
	}
};
inline StationBoundaryProfile _station_boundary_profile;

template <typename Result = void>
inline void CountStationBoundary(StationBoundary boundary, bool grouped = false) noexcept
{
	if (!_station_boundary_profile.enabled) return;
	auto index = static_cast<uint8_t>(boundary);
	++_station_boundary_profile.calls[index];
	if constexpr (!std::is_void_v<Result>) _station_boundary_profile.returned_bytes[index] += sizeof(Result);
	if (grouped) ++_station_boundary_profile.grouped_reads;
}

inline void CountStationScope() noexcept
{
	if (_station_boundary_profile.enabled) ++_station_boundary_profile.scoped_bodies;
}

#endif /* RUST_STATION_BOUNDARY_PROFILE_HPP */
