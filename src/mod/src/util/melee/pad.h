#pragma once

#include "hsd/pad.h"
#include "melee/player.h"
#include "util/vector.h"
#include <cmath>
#include <gctypes.h>

// 0.2750
constexpr auto DEADZONE = 22;

// 1.0
constexpr auto STICK_MAX = 80;

// 0.3
constexpr auto TRIGGER_DEADZONE = 42;

// 1.0
constexpr auto TRIGGER_MAX = 140;

constexpr auto XSMASH_THRESHOLD = .8000f;

constexpr auto YSMASH_THRESHOLD = .6625f;

namespace detail {

const PADStatus &get_input_impl(int port, int offset);
const PADStatus &get_nana_input_impl(const Player *nana, int offset);

} // namespace detail

template<int offset>
inline const PADStatus &get_input(int port)
{
	constexpr auto real_offset = (((offset - 1) % PAD_QNUM) + PAD_QNUM) % PAD_QNUM;
	return detail::get_input_impl(port, real_offset);
}

template<int offset>
inline const PADStatus &get_nana_input(const Player *nana)
{
	constexpr auto real_offset = (((offset - 1) % NANA_BUFFER) + NANA_BUFFER) % NANA_BUFFER;
	return detail::get_nana_input_impl(nana, real_offset);
}

template<int offset>
inline const PADStatus &get_character_input(const Player *player)
{
	if (!player->is_backup_climber)
		return get_input<offset>(player->port);
	else
		return get_nana_input<offset>(player);
}

inline bool check_ucf_xsmash(const Player *player)
{
	// Designed by tauKhan
	const auto &prev_input = get_character_input<-2>(player);
	const auto &current_input = get_character_input<0>(player);
	const auto delta = current_input.stick.x - prev_input.stick.x;
	return delta * delta > 75 * 75;
}

constexpr int abs_coord_to_int(float x)
{
	// Small bias converts Nana coords back to the corresponding Popo coords
	return static_cast<int>(std::abs(x) * 80 - .0001f) + 1;
}

inline vec2 convert_hw_coords(const vec2b &hw)
{
	// Convert hardware stick coordinates to what Melee uses
	auto hw_signed = vec2c(hw - vec2b(128, 128));

	HSD_PadClamp(&hw_signed.x, &hw_signed.y, HSD_PadLibData.clamp_stickShift,
				                 HSD_PadLibData.clamp_stickMin,
				                 HSD_PadLibData.clamp_stickMax);

	return hw_signed.map<vec2>([](auto x) {
		return std::abs(x) > DEADZONE ? (float)x / HSD_PadLibData.scale_stick : 0.f;
	});
}

constexpr bool is_rim_coord(const vec2 &coords)
{
	const auto converted = vec2i(abs_coord_to_int(coords.x) + 1,
	                             abs_coord_to_int(coords.y) + 1);
	return converted.length_sqr() > 80 * 80;
}

constexpr s8 popo_to_nana(float x)
{
	return x >= 0 ? (s8)(x * 127) : (s8)(x * 128);
}

constexpr float popo_to_nana_float(float x)
{
	return x >= 0 ? (float)((s8)(x * 127)) / 127 : (float)((s8)(x * 128)) / 128;
}

constexpr vec2c popo_to_nana(const vec2 &coords)
{
	return coords.map<vec2c>([](auto x) { return popo_to_nana(x); });
}