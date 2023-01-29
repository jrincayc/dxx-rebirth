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
 * Header for morph.c
 *
 */

#pragma once

#ifdef __cplusplus
#include <cstdint>
#include "dxxsconf.h"
#include "vecmat.h"
#include "pack.h"
#include "polyobj.h"
#include "dsx-ns.h"

#ifdef dsx
#include "object.h"
#include <array>
#include <memory>
#include <new>
#include <span>

namespace dcx {

struct morph_data : prohibit_void_ptr<morph_data>
{
	using ptr = std::unique_ptr<morph_data>;
	enum
	{
		MAX_VECS = 5000u,
	};
	struct max_vectors
	{
		const std::size_t count;
	};
	static void *operator new(std::size_t bytes) = delete;	/* require caller to use placement-form to specify the number of vectors to allocate */
	static void operator delete(void *p)
	{
		::operator delete(p);
	}
	static void operator delete(void *p, max_vectors)
	{
		::operator delete(p);
	}
	enum class submodel_state : uint8_t
	{
		invisible,
		animating,
		visible,
	};
	object_base *const obj;                      // object which is morphing
	const object_signature_t Morph_sig;
	typename object_base::control_type morph_save_control_type;
	typename object_base::movement_type morph_save_movement_type;
	uint8_t n_submodels_active;
	std::array<submodel_state, MAX_SUBMODELS> submodel_active;         // which submodels are active
	const max_vectors max_vecs;
	physics_info morph_save_phys_info;
	std::array<int, MAX_SUBMODELS>
		n_morphing_points,       // how many active points in each part
		submodel_startpoints;    // first point for each submodel
	static ptr create(object_base &, const polymodel &, polygon_model_index);
	std::span<fix> get_morph_times();
	std::span<vms_vector> get_morph_vecs();
	std::span<vms_vector> get_morph_deltas();
private:
	/* Advise gcc to inline this `operator new` because if it is not inline,
	 * then -Wmismatched-new-delete issues a spurious warning when it inlines
	 * the body of `operator delete(void *, max_vectors)` and does not inline
	 * this method.  Possibly relevant gcc bug reports:
	 *
	 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=100485	`False positive in -Wmismatched-new-delete`
	 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=103993	`-Wismatched-new-delete due to difference in inlining decisions`
	 */
	static inline void *operator new(std::size_t bytes, max_vectors);
	explicit morph_data(object_base &o, max_vectors);
};

struct d_level_unique_morph_object_state;

morph_data::ptr *find_morph_data(d_level_unique_morph_object_state &LevelUniqueMorphObjectState, object_base &obj);
}

namespace dsx {
void morph_start(d_level_unique_morph_object_state &, d_level_shared_polygon_model_state &, object_base &obj);

//process the morphing object for one frame
void do_morph_frame(object &obj);

//called at the start of a level
void init_morphs(d_level_unique_morph_object_state &MorphObjectState);
}
#endif

#endif
