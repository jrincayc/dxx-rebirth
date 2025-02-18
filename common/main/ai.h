/*
 * Portions of this file are copyright Rebirth contributors and licensed as
 * described in COPYING.txt.
 * Portions of this file are copyright Parallax Software and licensed
 * according to the Parallax license below.
 * See COPYING.txt for license details.

THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Header file for AI system.
 *
 */

#pragma once

#include <cstddef>
#include <utility>
#include "dxxsconf.h"
#include "dsx-ns.h"
#include "fmtcheck.h"
#include "vecmat.h"

#define	PARALLAX	0		//	If !0, then special debugging info for Parallax eyes only enabled.

#ifdef __cplusplus
#include "pstypes.h"
#include "fwd-object.h"
#include "fwd-segment.h"
#ifdef dsx
#include "countarray.h"
#include "aistruct.h"
#include "valptridx.h"
#endif

namespace dcx {
struct point_seg;

#ifdef dsx
struct awareness_event
{
	segnum_t segnum;				// segment the event occurred in
	player_awareness_type_t type;	// type of event, defines behavior
	vms_vector pos;					// absolute 3 space location of event
};

struct d_level_unique_robot_awareness_state
{
	unsigned Num_awareness_events;
	std::array<awareness_event, 64> Awareness_events;
};

extern d_level_unique_robot_awareness_state LevelUniqueRobotAwarenessState;
#endif

}
struct PHYSFS_File;

#define PLAYER_AWARENESS_INITIAL_TIME   (3*F1_0)
#define MAX_PATH_LENGTH                 30          // Maximum length of path in ai path following.
#define MAX_DEPTH_TO_SEARCH_FOR_PLAYER  10
#define BOSS_GATE_MATCEN_NUM            -1

#define ROBOT_BRAIN 7
#define ROBOT_BOSS1 17

#define ROBOT_FIRE_AGITATION 94

#ifdef dsx

#define BOSS_D1      1
#define BOSS_SUPER   2
#if defined(DXX_BUILD_DESCENT_II)
#include "player-flags.h"
namespace dsx {
#define BOSS_D2     21 // Minimum D2 boss value.
#define BOSS_COOL   21
#define BOSS_WATER  22
#define BOSS_FIRE   23
#define BOSS_ICE    24
#define BOSS_ALIEN1 25
#define BOSS_ALIEN2 26

#define NUM_D2_BOSSES 8

using boss_flags_t = std::array<ubyte, NUM_D2_BOSSES>;
extern const boss_flags_t Boss_spew_more;     // Set byte if this boss can teleport
extern const boss_flags_t Boss_spews_bots_energy;     // Set byte if boss spews bots when hit by energy weapon.
extern const boss_flags_t Boss_spews_bots_matter;     // Set byte if boss spews bots when hit by matter weapon.
extern const boss_flags_t Boss_invulnerable_energy;   // Set byte if boss is invulnerable to energy weapons.
extern const boss_flags_t Boss_invulnerable_matter;   // Set byte if boss is invulnerable to matter weapons.
extern const boss_flags_t Boss_invulnerable_spot;     // Set byte if boss is invulnerable in all but a certain spot.  (Dot product fvec|vec_to_collision < BOSS_INVULNERABLE_DOT)
extern segnum_t Believed_player_seg;
extern const object *Ai_last_missile_camera;
}
#endif

namespace dsx {
void create_awareness_event(object &objp, player_awareness_type_t type, d_level_unique_robot_awareness_state &LevelUniqueRobotAwarenessState);         // object *objp can create awareness of player, amount based on "type"
ai_mode ai_behavior_to_mode(ai_behavior behavior);
void do_ai_robot_hit(vmobjptridx_t robot, player_awareness_type_t type);
void init_ai_object(vmobjptridx_t objp, ai_behavior initial_mode, imsegidx_t hide_segment);
}

namespace dcx {

struct d_level_shared_boss_state
{
	struct special_segment_array_t : public count_array_t<vcsegidx_t, 100> {};
	struct gate_segment_array_t : public special_segment_array_t {};
	struct teleport_segment_array_t : public special_segment_array_t {};
	gate_segment_array_t Gate_segs;
	teleport_segment_array_t Teleport_segs;
};

constexpr fix Boss_cloak_duration = F1_0*7;

extern vms_vector Believed_player_pos;
}

namespace dsx {

struct d_level_shared_boss_state : ::dcx::d_level_shared_boss_state
{
	// Time between cloaks
	using D1_Boss_cloak_interval = std::integral_constant<fix, F1_0 * 10>;
	using D1_Boss_teleport_interval = std::integral_constant<fix, F1_0 * 8>;
#if defined(DXX_BUILD_DESCENT_I)
	static constexpr D1_Boss_cloak_interval Boss_cloak_interval{};
	static constexpr D1_Boss_teleport_interval Boss_teleport_interval{};
#elif defined(DXX_BUILD_DESCENT_II)
	fix Boss_cloak_interval;
	fix Boss_teleport_interval;
#endif
};

extern d_level_shared_boss_state LevelSharedBossState;
void move_towards_segment_center(const d_level_shared_segment_state &, object_base &objp);
imobjptridx_t gate_in_robot(unsigned type, vmsegptridx_t segnum);
void do_ai_frame(vmobjptridx_t objp);
void do_ai_frame_all();
}
extern void create_all_paths(void);
namespace dsx {
void create_path_to_station(vmobjptridx_t objp, int max_length);
#if defined(DXX_BUILD_DESCENT_I)
#define ai_follow_path(o,pv,vec)	ai_follow_path(o,pv)
#else
#undef ai_follow_path
#endif
void ai_follow_path(vmobjptridx_t objp, const player_visibility_state player_visibility, const vms_vector *vec_to_player);
void ai_turn_towards_vector(const vms_vector &vec_to_player, object_base &obj, fix rate);
extern void init_ai_objects(void);
void create_n_segment_path(vmobjptridx_t objp, unsigned path_length, imsegidx_t avoid_seg);
void create_n_segment_path_to_door(vmobjptridx_t objp, unsigned path_length);
}
#endif
namespace dcx {
void make_random_vector(vms_vector &vec);
[[nodiscard]]
static inline vms_vector make_random_vector()
{
	vms_vector v;
	return make_random_vector(v), v;
}

}
#ifdef dsx
namespace dsx {
void init_robots_for_level();
#if defined(DXX_BUILD_DESCENT_II)
int polish_path(vmobjptridx_t objp, point_seg *psegs, int num_points);
void move_towards_player(object &objp, const vms_vector &vec_to_player);
#endif

// max_length is maximum depth of path to create.
// If -1, use default: MAX_DEPTH_TO_SEARCH_FOR_PLAYER
void attempt_to_resume_path(vmobjptridx_t objp);

// When a robot and a player collide, some robots attack!
void do_ai_robot_hit_attack(vmobjptridx_t robot, vmobjptridx_t player, const vms_vector &collision_point);
int ai_door_is_openable(
	vmobjptr_t,
#if defined(DXX_BUILD_DESCENT_II)
	player_flags,
#endif
	const shared_segment &segp, sidenum_t sidenum);
player_visibility_state player_is_visible_from_object(vmobjptridx_t objp, vms_vector &pos, fix field_of_view, const vms_vector &vec_to_player);
extern void ai_reset_all_paths(void);   // Reset all paths.  Call at the start of a level.
int ai_multiplayer_awareness(vmobjptridx_t objp, int awareness_level);

#if defined(DXX_BUILD_DESCENT_II)
// In escort.c
void do_escort_frame(vmobjptridx_t objp, const object &plrobj, player_visibility_state player_visibility);
void do_snipe_frame(vmobjptridx_t objp, fix dist_to_player, player_visibility_state player_visibility, const vms_vector &vec_to_player);
void do_thief_frame(vmobjptridx_t objp, fix dist_to_player, player_visibility_state player_visibility, const vms_vector &vec_to_player);
#endif
}

#if PARALLAX
extern void force_dump_ai_objects_all(const char *msg);
#else
static inline void force_dump_ai_objects_all(const char *msg)
{
	(void)msg;
}
#endif

namespace dsx {
void start_boss_death_sequence(object &objp);
extern void ai_init_boss_for_ship(void);
void init_ai_for_ship();

#if defined(DXX_BUILD_DESCENT_II)
extern vms_vector Last_fired_upon_player_pos;

#define MIN_ESCORT_DISTANCE     (F1_0*40)

#define SNIPE_RETREAT_TIME  (F1_0*5)
#define SNIPE_ABORT_RETREAT_TIME (SNIPE_RETREAT_TIME/2) // Can abort a retreat with this amount of time left in retreat
#define SNIPE_ATTACK_TIME   (F1_0*10)
#define SNIPE_WAIT_TIME     (F1_0*5)
#define SNIPE_FIRE_TIME     (F1_0*2)

#define THIEF_PROBABILITY   16384   // 50% chance of stealing an item at each attempt

extern void  create_buddy_bot(void);

imobjptridx_t boss_spew_robot(const object_base &objp, const vms_vector &pos);

// Amount of time since the current robot was last processed for things such as movement.
// It is not valid to use FrameTime because robots do not get moved every frame.

// --------- John: These variables must be saved as part of gamesave. ---------
// ------ John: End of variables which must be saved as part of gamesave. -----

// These globals are set by a call to find_vector_intersection, which is a slow routine,
// so we don't want to call it again (for this object) unless we have to.

// -- unused, 08/07/95 -- extern void ai_turn_randomly(vms_vector *vec_to_player, object *obj, fix rate, int previous_visibility);
void init_ai_frame(player_flags, const control_info &Controls);

[[nodiscard]]
std::size_t create_bfs_list(vmobjptr_t robot, vcsegidx_t start_seg, player_flags, segnum_t *bfs_list, std::size_t max_segs);

template <std::size_t N>
	[[nodiscard]]
std::size_t create_bfs_list(const vmobjptr_t &robot, const vcsegidx_t &start_seg, const player_flags powerup_flags, std::array<segnum_t, N> &bfs_list)
{
	return create_bfs_list(robot, start_seg, powerup_flags, bfs_list.data(), N);
}
extern void init_thief_for_level();

void start_robot_death_sequence(object &objp);
void buddy_message_str(const char * str) __attribute_nonnull();
void buddy_message(const char * format, ... ) __attribute_format_printf(1, 2);
#define buddy_message(F,...)	dxx_call_printf_checked(buddy_message,buddy_message_str,(),(F),##__VA_ARGS__)

#define SPECIAL_REACTOR_ROBOT   65
extern void special_reactor_stuff(void);
#endif
}

namespace dcx {
struct point_seg_array_t : public std::array<point_seg, MAX_POINT_SEGS> {};
extern point_seg_array_t        Point_segs;
extern point_seg_array_t::iterator        Point_segs_free_ptr;
static inline std::size_t operator-(point_seg_array_t::iterator i, point_seg_array_t &p)
{
	return std::distance(p.begin(), i);
}

enum class create_path_random_flag : uint8_t
{
	nonrandom,
	random,
};

enum class create_path_safety_flag : uint8_t
{
	unsafe,
	safe,
};

enum class create_path_result : uint8_t
{
	early,
	finished,
};

}

namespace dsx {

#if defined(DXX_BUILD_DESCENT_II)
void create_path_to_segment(vmobjptridx_t objp, segnum_t goalseg, unsigned max_length, create_path_safety_flag safety_flag);
#endif

void create_path_to_segment(vmobjptridx_t objp, unsigned max_length, create_path_safety_flag safety_flag, icsegidx_t);
void create_path_to_believed_player_segment(vmobjptridx_t objp, unsigned max_length, create_path_safety_flag safety_flag);
void create_path_to_guidebot_player_segment(vmobjptridx_t objp, unsigned max_length, create_path_safety_flag safety_flag);
std::pair<create_path_result, unsigned> create_path_points(vmobjptridx_t objp, vcsegidx_t start_seg, icsegidx_t end_seg, point_seg_array_t::iterator point_segs, unsigned max_depth, create_path_random_flag random_flag, create_path_safety_flag safety_flag, icsegidx_t avoid_seg);

int ai_save_state(PHYSFS_File * fp);
int ai_restore_state(PHYSFS_File *fp, int version, int swap);

#if DXX_USE_EDITOR
void player_follow_path(object &objp);
void check_create_player_path();
#endif
}
#endif

#endif
