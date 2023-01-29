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
 * Bitmap and Palette loading functions.
 *
 */

#pragma once

#include <physfs.h>
#include "maths.h"
#include "fwd-vclip.h"
#include "fwd-game.h"
#include "fwd-piggy.h"
#include "fwd-robot.h"
#include "d_array.h"
#include "inferno.h"
#include <cstdint>

namespace dcx {

struct grs_bitmap;

enum class tmapinfo_flag : uint8_t
{
	lava = 1 << 0,		//this material blows up when hit
	/* if DXX_BUILD_DESCENT_II */
	water = 1 << 1,		//this material is water
	force_field = 1 << 2,	//this is force field - flares don't stick
	goal_blue = 1 << 3,	//this is used to remap the blue goal
	goal_red = 1 << 4,	//this is used to remap the red goal
	goal_hoard = 1 << 5,//this is used to remap the goals
	/* endif */
};

enum class tmapinfo_flags : uint8_t;

static constexpr uint8_t operator&(tmapinfo_flags flags, tmapinfo_flag mask)
{
	return static_cast<uint8_t>(flags) & static_cast<uint8_t>(mask);
}

static constexpr tmapinfo_flags &operator|=(tmapinfo_flags &flags, tmapinfo_flag mask)
{
	return flags = static_cast<tmapinfo_flags>(static_cast<uint8_t>(flags) | static_cast<uint8_t>(mask));
}

static constexpr tmapinfo_flag operator|(tmapinfo_flag a, tmapinfo_flag b)
{
	return static_cast<tmapinfo_flag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

}

#ifdef dsx
namespace dsx {
#if defined(DXX_BUILD_DESCENT_I)
constexpr std::integral_constant<unsigned, 800> MAX_TEXTURES{};
#elif defined(DXX_BUILD_DESCENT_II)
constexpr std::integral_constant<unsigned, 1200> MAX_TEXTURES{};
#endif

struct tmap_info : prohibit_void_ptr<tmap_info>
{
#if defined(DXX_BUILD_DESCENT_I)
	d_fname filename;
	tmapinfo_flags flags;
	fix			lighting;		// 0 to 1
	fix			damage;			//how much damage being against this does
	unsigned eclip_num;		//if not -1, the eclip that changes this   
#define N_COCKPIT_BITMAPS 4
#elif defined(DXX_BUILD_DESCENT_II)
	fix     lighting;  //how much light this casts
	fix     damage;    //how much damage being against this does (for lava)
	uint16_t eclip_num; //the eclip that changes this, or -1
	short   destroyed; //bitmap to show when destroyed, or -1
	short   slide_u,slide_v;    //slide rates of texture, stored in 8:8 fix
	tmapinfo_flags flags;
#if DXX_USE_EDITOR
	d_fname filename;       //used by editor to remap textures
	#endif

#define TMAP_INFO_SIZE 20   // how much space it takes up on disk
#define N_COCKPIT_BITMAPS 6
#endif
};
}

namespace dcx {
extern int Num_object_types;

struct player_ship;
//right now there's only one player ship, but we can have another by
//adding an array and setting the pointer to the active ship.
extern struct player_ship only_player_ship;
constexpr struct player_ship *Player_ship = &only_player_ship;
extern unsigned Num_cockpits;
}

namespace dsx {
#if DXX_USE_EDITOR
using tmap_xlate_table_array = std::array<short, MAX_TEXTURES>;
extern tmap_xlate_table_array tmap_xlate_table;
#endif

/* This is level-unique because hoard mode assumes it can overwrite a
 * texture.
 */
struct d_level_unique_tmap_info_state
{
	using TmapInfo_array = std::array<tmap_info, MAX_TEXTURES>;
	unsigned Num_tmaps;
	TmapInfo_array TmapInfo;
};

extern d_level_unique_tmap_info_state LevelUniqueTmapInfoState;
// Initializes the palette, bitmap system...
void gamedata_close();
}
void bm_close();

// Initializes the Texture[] array of bmd_bitmap structures.
void init_textures();

#ifdef dsx

namespace dsx {

int gamedata_init(d_level_shared_robot_info_state &LevelSharedRobotInfoState);

#if defined(DXX_BUILD_DESCENT_I)

#define OL_ROBOT 				1
#define OL_HOSTAGE 			2
#define OL_POWERUP 			3
#define OL_CONTROL_CENTER	4
#define OL_PLAYER				5
#define OL_CLUTTER			6		//some sort of misc object
#define OL_EXIT				7		//the exit model for external scenes

#define	MAX_OBJTYPE			100

extern int Num_total_object_types;		//	Total number of object types, including robots, hostages, powerups, control centers, faces
extern int8_t	ObjType[MAX_OBJTYPE];		// Type of an object, such as Robot, eg if ObjType[11] == OL_ROBOT, then object #11 is a robot
extern std::array<polygon_model_index, MAX_OBJTYPE> ObjId;			// ID of a robot, within its class, eg if ObjType[11] == 3, then object #11 is the third robot
extern fix	ObjStrength[MAX_OBJTYPE];	// initial strength of each object

constexpr std::integral_constant<unsigned, 210> MAX_OBJ_BITMAPS{};

#elif defined(DXX_BUILD_DESCENT_II)

extern int Robot_replacements_loaded;
constexpr std::integral_constant<unsigned, 610> MAX_OBJ_BITMAPS{};
extern unsigned N_ObjBitmaps;
#endif

enum class object_bitmap_index : uint16_t
{
	None = UINT16_MAX
};
extern enumerated_array<bitmap_index, MAX_OBJ_BITMAPS, object_bitmap_index> ObjBitmaps;
extern std::array<object_bitmap_index, MAX_OBJ_BITMAPS> ObjBitmapPtrs;

}

#endif

extern int  Num_object_subtypes;     // Number of possible IDs for the current type of object to be placed

extern int First_multi_bitmap_num;
void compute_average_rgb(grs_bitmap *bm, std::array<fix, 3> &rgb);

namespace dsx {
extern enumerated_array<bitmap_index, N_COCKPIT_BITMAPS, cockpit_mode_t> cockpit_bitmap;
void load_robot_replacements(const d_fname &level_name);
#if defined(DXX_BUILD_DESCENT_I) || (defined(DXX_BUILD_DESCENT_II) && DXX_USE_EDITOR)
// Initializes all bitmaps from BITMAPS.TBL file.
int gamedata_read_tbl(d_level_shared_robot_info_state &LevelSharedRobotInfoState, d_vclip_array &Vclip, int pc_shareware);
#endif

void bm_read_all(d_level_shared_robot_info_state &LevelSharedRobotInfoState, d_vclip_array &Vclip, PHYSFS_File * fp);
#if defined(DXX_BUILD_DESCENT_I)
void properties_read_cmp(d_level_shared_robot_info_state &LevelSharedRobotInfoState, d_vclip_array &Vclip, PHYSFS_File * fp);
#endif
int ds_load(int skip, const char * filename );
int compute_average_pixel(grs_bitmap *n);

#if defined(DXX_BUILD_DESCENT_II)
int load_exit_models();
//these values are the number of each item in the release of d2
//extra items added after the release get written in an additional hamfile
constexpr std::integral_constant<unsigned, 66> N_D2_ROBOT_TYPES{};
constexpr std::integral_constant<unsigned, 1145> N_D2_ROBOT_JOINTS{};
constexpr std::integral_constant<unsigned, 422> N_D2_OBJBITMAPS{};
constexpr std::integral_constant<unsigned, 502> N_D2_OBJBITMAPPTRS{};
constexpr std::integral_constant<unsigned, 62> N_D2_WEAPON_TYPES{};
#endif
}

#endif
