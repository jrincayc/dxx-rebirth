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
 * Functions for managing the pig files.
 *
 */

#include <stdio.h>
#include <string.h>

#include "pstypes.h"
#include "strutil.h"
#include "inferno.h"
#include "gr.h"
#include "u_mem.h"
#include "iff.h"
#include "dxxerror.h"
#include "sounds.h"
#include "digi.h"
#include "bm.h"
#include "hash.h"
#include "common/2d/bitmap.h"
#include "args.h"
#include "palette.h"
#include "gamepal.h"
#include "physfsx.h"
#include "rle.h"
#include "piggy.h"
#include "gamemine.h"
#include "textures.h"
#include "texmerge.h"
#include "paging.h"
#include "game.h"
#include "text.h"
#include "vclip.h"
#include "makesig.h"
#include "console.h"
#include "compiler-cf_assert.h"
#include "compiler-range_for.h"
#include "d_range.h"
#include "partial_range.h"
#include <memory>

#if defined(DXX_BUILD_DESCENT_I)
#include "custom.h"
#include "snddecom.h"
#define DEFAULT_PIGFILE_REGISTERED      "descent.pig"

#elif defined(DXX_BUILD_DESCENT_II)
#define DEFAULT_PIGFILE_REGISTERED      "groupa.pig"
#define DEFAULT_PIGFILE_SHAREWARE       "d2demo.pig"
#define DEFAULT_HAMFILE_REGISTERED      "descent2.ham"
#define DEFAULT_HAMFILE_SHAREWARE       "d2demo.ham"

#define D1_PALETTE "palette.256"

#define DEFAULT_SNDFILE ((Piggy_hamfile_version < 3)?DEFAULT_HAMFILE_SHAREWARE:(GameArg.SndDigiSampleRate==SAMPLE_RATE_22K)?"descent2.s22":"descent2.s11")

#define MAC_ALIEN1_PIGSIZE      5013035
#define MAC_ALIEN2_PIGSIZE      4909916
#define MAC_FIRE_PIGSIZE        4969035
#define MAC_GROUPA_PIGSIZE      4929684 // also used for mac shareware
#define MAC_ICE_PIGSIZE         4923425
#define MAC_WATER_PIGSIZE       4832403

unsigned Num_aliases;
std::array<alias, MAX_ALIASES> alias_list;

int Piggy_hamfile_version = 0;
#endif

static std::unique_ptr<ubyte[]> BitmapBits;
static std::unique_ptr<ubyte[]> SoundBits;

struct SoundFile
{
	char    name[15];
};

#if defined(DXX_BUILD_DESCENT_II)
namespace {
#endif
hashtable AllBitmapsNames;
hashtable AllDigiSndNames;
std::array<pig_bitmap_offset, MAX_BITMAP_FILES> GameBitmapOffset;
#if defined(DXX_BUILD_DESCENT_II)
}
#endif

unsigned Num_bitmap_files;
int Num_sound_files = 0;

namespace dsx {
std::array<digi_sound, MAX_SOUND_FILES> GameSounds;
static std::array<int, MAX_SOUND_FILES> SoundOffset;
GameBitmaps_array GameBitmaps;
}

#if defined(DXX_BUILD_DESCENT_I)
#define DBM_FLAG_LARGE 	128		// Flags added onto the flags struct in b
static
#endif
std::array<BitmapFile, MAX_BITMAP_FILES> AllBitmaps;
static std::array<SoundFile, MAX_SOUND_FILES> AllSounds;

#define DBM_FLAG_ABM    64 // animated bitmap

static int Piggy_bitmap_cache_size;
static int Piggy_bitmap_cache_next;
static uint8_t *Piggy_bitmap_cache_data;
static std::array<uint8_t, MAX_BITMAP_FILES> GameBitmapFlags;
static std::array<uint16_t, MAX_BITMAP_FILES> GameBitmapXlat;

#if defined(DXX_BUILD_DESCENT_I)
#define PIGGY_BUFFER_SIZE (2048*1024)
#elif defined(DXX_BUILD_DESCENT_II)
#define PIGFILE_ID              MAKE_SIG('G','I','P','P') //PPIG
#define PIGFILE_VERSION         2
#define PIGGY_BUFFER_SIZE (2400*1024)
#endif
#define PIGGY_SMALL_BUFFER_SIZE (1400*1024)		// size of buffer when CGameArg.SysLowMem is set

static RAIIPHYSFS_File Piggy_fp;

ubyte bogus_bitmap_initialized=0;
std::array<uint8_t, 64 * 64> bogus_data;
namespace dsx {
digi_sound bogus_sound;
}

#if defined(DXX_BUILD_DESCENT_I)
grs_bitmap bogus_bitmap;
int MacPig = 0;	// using the Macintosh pigfile?
int PCSharePig = 0; // using PC Shareware pigfile?
static std::array<int, MAX_SOUND_FILES> SoundCompressed;
#elif defined(DXX_BUILD_DESCENT_II)
char Current_pigfile[FILENAME_LEN] = "";
int Pigfile_initialized=0;

static std::unique_ptr<ubyte[]> Bitmap_replacement_data;

#define BM_FLAGS_TO_COPY (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT \
                         | BM_FLAG_NO_LIGHTING | BM_FLAG_RLE | BM_FLAG_RLE_BIG)
#endif

#define DBM_NUM_FRAMES  63

namespace {

struct DiskBitmapHeader
{
	char name[8];
	ubyte dflags;           // bits 0-5 anim frame num, bit 6 abm flag
	ubyte width;            // low 8 bits here, 4 more bits in wh_extra
	ubyte height;           // low 8 bits here, 4 more bits in wh_extra
#if defined(DXX_BUILD_DESCENT_II)
	ubyte wh_extra;         // bits 0-3 width, bits 4-7 height
#endif
	ubyte flags;
	ubyte avg_color;
	int offset;
} __pack__;
#if defined(DXX_BUILD_DESCENT_I)
static_assert(sizeof(DiskBitmapHeader) == 0x11, "sizeof(DiskBitmapHeader) must be 0x11");
#elif defined(DXX_BUILD_DESCENT_II)
static_assert(sizeof(DiskBitmapHeader) == 0x12, "sizeof(DiskBitmapHeader) must be 0x12");

#define DISKBITMAPHEADER_D1_SIZE 17 // no wh_extra
#endif

struct DiskSoundHeader
{
	char name[8];
	int length;
	int data_length;
	int offset;
} __pack__;

}

namespace dsx {
namespace {
#if defined(DXX_BUILD_DESCENT_II)
#define D1_MAX_TMAP_NUM 1630 // 1621 in descent.pig Mac registered

/* the inverse of the d2 Textures array, but for the descent 1 pigfile.
 * "Textures" looks up a d2 bitmap index given a d2 tmap_num.
 * "d1_tmap_nums" looks up a d1 tmap_num given a d1 bitmap. "-1" means "None".
 */
using d1_tmap_nums_t = std::array<short, D1_MAX_TMAP_NUM>;
static std::unique_ptr<d1_tmap_nums_t> d1_tmap_nums;

static void free_d1_tmap_nums()
{
	d1_tmap_nums.reset();
}
#if DXX_USE_EDITOR
static int piggy_is_substitutable_bitmap(char * name, char (&subst_name)[32]);
static void piggy_write_pigfile(const char *filename);
static void write_int(int i,PHYSFS_File *file);
#endif
#endif
}
}

namespace dcx {
namespace {

static void get_bitmap_name_from_header(std::array<char, 13> &output_name, const DiskBitmapHeader &bmh)
{
	if (bmh.dflags & DBM_FLAG_ABM)
		snprintf(output_name.data(), output_name.size(), "%.8s#%d", bmh.name, bmh.dflags & DBM_NUM_FRAMES);
	else
	{
		memcpy(output_name.data(), bmh.name, 8);
		output_name[8] = 0;
	}
}

static int piggy_is_needed(const int soundnum)
{
	if (!CGameArg.SysLowMem)
		return 1;
	constexpr std::size_t upper_bound = Sounds.size();
	for (const auto i : AltSounds)
	{
		if (i < upper_bound && Sounds[i] == soundnum)
			return 1;
	}
	return 0;
}

}
}

/*
 * reads a DiskBitmapHeader structure from a PHYSFS_File
 */
namespace dsx {
static void DiskBitmapHeader_read(DiskBitmapHeader *dbh, PHYSFS_File *fp)
{
	PHYSFS_read(fp, dbh->name, 8, 1);
	dbh->dflags = PHYSFSX_readByte(fp);
	dbh->width = PHYSFSX_readByte(fp);
	dbh->height = PHYSFSX_readByte(fp);
#if defined(DXX_BUILD_DESCENT_II)
	dbh->wh_extra = PHYSFSX_readByte(fp);
#endif
	dbh->flags = PHYSFSX_readByte(fp);
	dbh->avg_color = PHYSFSX_readByte(fp);
	dbh->offset = PHYSFSX_readInt(fp);
}
}

/*
 * reads a DiskSoundHeader structure from a PHYSFS_File
 */
static void DiskSoundHeader_read(DiskSoundHeader *dsh, PHYSFS_File *fp)
{
	PHYSFS_read(fp, dsh->name, 8, 1);
	dsh->length = PHYSFSX_readInt(fp);
	dsh->data_length = PHYSFSX_readInt(fp);
	dsh->offset = PHYSFSX_readInt(fp);
}

#if defined(DXX_BUILD_DESCENT_II)
/*
 * reads a descent 1 DiskBitmapHeader structure from a PHYSFS_File
 */
static void DiskBitmapHeader_d1_read(DiskBitmapHeader *dbh, PHYSFS_File *fp)
{
	PHYSFS_read(fp, dbh->name, 8, 1);
	dbh->dflags = PHYSFSX_readByte(fp);
	dbh->width = PHYSFSX_readByte(fp);
	dbh->height = PHYSFSX_readByte(fp);
	dbh->wh_extra = 0;
	dbh->flags = PHYSFSX_readByte(fp);
	dbh->avg_color = PHYSFSX_readByte(fp);
	dbh->offset = PHYSFSX_readInt(fp);
}
#endif

void swap_0_255(grs_bitmap &bmp)
{
	auto a = [](uint8_t &c) {
		if (c == 0)
			c = 255;
		else if (c == 255)
			c = 0;
	};
	auto d = bmp.get_bitmap_data();
	std::for_each(d, d + (bmp.bm_h * bmp.bm_w), a);
}

namespace dsx {

bitmap_index piggy_register_bitmap(grs_bitmap &bmp, const char *const name, const int in_file)
{
	bitmap_index temp;
	assert(Num_bitmap_files < AllBitmaps.size());

	temp.index = Num_bitmap_files;

	if (!in_file) {
#if defined(DXX_BUILD_DESCENT_II)
#if DXX_USE_EDITOR
		if ( GameArg.EdiMacData )
			swap_0_255(bmp);
#endif
#endif
		if (CGameArg.DbgNoCompressPigBitmap)
			gr_bitmap_rle_compress(bmp);
	}
#if defined(DXX_BUILD_DESCENT_II)
	else if (SoundOffset[Num_sound_files] == 0)
		SoundOffset[Num_sound_files] = -1;		// make sure this sound's data is not individually freed
#endif

	auto &abn = AllBitmaps[Num_bitmap_files].name;
	abn.back() = 0;
	strncpy(abn.data(), name, abn.size() - 1);
	hashtable_insert(&AllBitmapsNames, AllBitmaps[Num_bitmap_files].name.data(), Num_bitmap_files);
#if defined(DXX_BUILD_DESCENT_I)
	GameBitmaps[Num_bitmap_files] = bmp;
#endif
	if ( !in_file ) {
		GameBitmapOffset[Num_bitmap_files] = pig_bitmap_offset::None;
		GameBitmapFlags[Num_bitmap_files] = bmp.get_flags();
	}
	Num_bitmap_files++;

	return temp;
}

int piggy_register_sound( digi_sound * snd, const char * name, int in_file )
{
	int i;

	Assert( Num_sound_files < MAX_SOUND_FILES );

	strncpy( AllSounds[Num_sound_files].name, name, 12 );
	hashtable_insert( &AllDigiSndNames, AllSounds[Num_sound_files].name, Num_sound_files );
	GameSounds[Num_sound_files] = *snd;
#if defined(DXX_BUILD_DESCENT_I)
//added/moved on 11/13/99 by Victor Rachels to ready for changing freq
//#ifdef ALLEGRO
        GameSounds[Num_sound_files].freq = snd->freq;

#ifdef ALLEGRO
//end this section move - VR
	GameSounds[Num_sound_files].priority = 128;
	GameSounds[Num_sound_files].loop_start = 0;
	GameSounds[Num_sound_files].loop_end = GameSounds[Num_sound_files].len;
	GameSounds[Num_sound_files].param = -1;
#endif
#endif
	if ( !in_file ) {
		SoundOffset[Num_sound_files] = 0;       
	}
#if defined(DXX_BUILD_DESCENT_I)
	else if (SoundOffset[Num_sound_files] == 0)
		SoundOffset[Num_sound_files] = -1;		// make sure this sound's data is not individually freed
#endif

	i = Num_sound_files;
	Num_sound_files++;
	return i;
}

bitmap_index piggy_find_bitmap(const char * name)
{
	bitmap_index bmp;

	bmp.index = 0;

#if defined(DXX_BUILD_DESCENT_II)
	size_t namelen;
	const char *t;
	if ((t=strchr(name,'#'))!=NULL)
		namelen = t - name;
	else
		namelen = strlen(name);

	char temp[FILENAME_LEN];
	range_for (auto &i, partial_const_range(alias_list, Num_aliases))
		if (i.alias_name[namelen] == 0 && d_strnicmp(name, i.alias_name,namelen)==0) {
			if (t) {                //extra stuff for ABMs
				struct splitpath_t path;
				d_splitpath(i.file_name, &path);
				snprintf(temp, sizeof(temp), "%.*s%s\n", DXX_ptrdiff_cast_int(path.base_end - path.base_start), path.base_start, t);
				name = temp;
			}
			else
				name = i.file_name; 
			break;
		}
#endif

	int i;
	i = hashtable_search( &AllBitmapsNames, name );
	Assert( i != 0 );
	if ( i < 0 )
		return bmp;

	bmp.index = i;
	return bmp;
}

}

int piggy_find_sound(const char *name)
{
	int i;

	i = hashtable_search( &AllDigiSndNames, name );

	if ( i < 0 )
		return 255;

	return i;
}

namespace dsx {
static void piggy_close_file()
{
	if (Piggy_fp)
	{
		Piggy_fp.reset();
#if defined(DXX_BUILD_DESCENT_II)
		Current_pigfile[0] = 0;
#endif
	}
}

#if defined(DXX_BUILD_DESCENT_I)
int properties_init()
{
	int sbytes = 0;
	std::array<char, 13> temp_name;
	digi_sound temp_sound;
	DiskBitmapHeader bmh;
	DiskSoundHeader sndh;
	int N_sounds;
	int size;
	int pigsize;
	int retval;
	GameSounds = {};
	SoundOffset = {};

	static_assert(GameBitmapXlat.size() == GameBitmaps.size(), "size mismatch");
	for (unsigned i = 0; i < GameBitmaps.size(); ++i)
	{
		GameBitmapXlat[i] = i;
		GameBitmaps[i].set_flags(BM_FLAG_PAGED_OUT);
	}

	if ( !bogus_bitmap_initialized )	{
		ubyte c;
		bogus_bitmap_initialized = 1;
		c = gr_find_closest_color( 0, 0, 63 );
		bogus_data.fill(c);
		c = gr_find_closest_color( 63, 0, 0 );
		// Make a big red X !
		range_for (const unsigned i, xrange(64u))
		{
			bogus_data[i*64+i] = c;
			bogus_data[i*64+(63-i)] = c;
		}
		gr_init_bitmap(bogus_bitmap, bm_mode::linear, 0, 0, 64, 64, 64, bogus_data.data());
		piggy_register_bitmap(bogus_bitmap, "bogus", 1);
#ifdef ALLEGRO
		bogus_sound.len = 64*64;
#else
        bogus_sound.length = 64*64;
#endif
		bogus_sound.data = bogus_data.data();
//added on 11/13/99 by Victor Rachels to ready for changing freq
                bogus_sound.freq = 11025;
//end this section addition - VR
		GameBitmapOffset[0] = pig_bitmap_offset::None;
	}
	
	Piggy_fp = PHYSFSX_openReadBuffered(DEFAULT_PIGFILE_REGISTERED).first;
	if (!Piggy_fp)
	{
		if (!PHYSFSX_exists("BITMAPS.TBL",1) && !PHYSFSX_exists("BITMAPS.BIN",1))
			Error("Cannot find " DEFAULT_PIGFILE_REGISTERED " or BITMAPS.TBL");
		return 1;	// need to run gamedata_read_tbl
	}

	pigsize = PHYSFS_fileLength(Piggy_fp);
	unsigned Pigdata_start;
	int firstInt;
	switch (pigsize) {
		case D1_SHARE_BIG_PIGSIZE:
		case D1_SHARE_10_PIGSIZE:
		case D1_SHARE_PIGSIZE:
			PCSharePig = 1;
			Pigdata_start = 0;
			break;
		case D1_10_BIG_PIGSIZE:
		case D1_10_PIGSIZE:
			Pigdata_start = 0;
			break;
		default:
			firstInt = PHYSFSX_readInt(Piggy_fp );
			if(firstInt == 0x30474950) {  //Header PIG0
			  Pigdata_start = 4;
			  PCSharePig = 0;
			  break;
			} else {
			  Warning("Unknown size for " DEFAULT_PIGFILE_REGISTERED);
			  Int3();
			  PHYSFS_seek(Piggy_fp, 0);
			}
			[[fallthrough]];
		case D1_MAC_PIGSIZE:
		case D1_MAC_SHARE_PIGSIZE:
			MacPig = 1;
			[[fallthrough]];
		case D1_PIGSIZE:
		case D1_OEM_PIGSIZE:
			Pigdata_start = PHYSFSX_readInt(Piggy_fp );
			break;
	}
	
	HiresGFXAvailable = MacPig;	// for now at least

	if (PCSharePig)
		retval = PIGGY_PC_SHAREWARE;	// run gamedata_read_tbl in shareware mode
	else if (GameArg.EdiNoBm || (!PHYSFSX_exists("BITMAPS.TBL",1) && !PHYSFSX_exists("BITMAPS.BIN",1)))
	{
		properties_read_cmp(Vclip, Piggy_fp);	// Note connection to above if!!!
		range_for (auto &i, GameBitmapXlat)
		{
			i = PHYSFSX_readShort(Piggy_fp);
			if (PHYSFS_eof(Piggy_fp))
				break;
		}
		retval = 0;	// don't run gamedata_read_tbl
	}
	else
		retval = 1;	// run gamedata_read_tbl

	PHYSFS_seek(Piggy_fp, Pigdata_start);
	size = PHYSFS_fileLength(Piggy_fp) - Pigdata_start;

	const unsigned N_bitmaps = PHYSFSX_readInt(Piggy_fp);
	size -= sizeof(int);
	N_sounds = PHYSFSX_readInt(Piggy_fp);
	size -= sizeof(int);

	const unsigned header_size = (N_bitmaps * sizeof(DiskBitmapHeader)) + (N_sounds * sizeof(DiskSoundHeader));

	for (const unsigned i : xrange(N_bitmaps))
	{
		DiskBitmapHeader_read(&bmh, Piggy_fp);
		
		GameBitmapFlags[i+1] = bmh.flags & (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT | BM_FLAG_NO_LIGHTING | BM_FLAG_RLE);

		GameBitmapOffset[i+1] = static_cast<pig_bitmap_offset>(bmh.offset + header_size + (sizeof(int)*2) + Pigdata_start);
		Assert( (i+1) == Num_bitmap_files );

		//size -= sizeof(DiskBitmapHeader);
		get_bitmap_name_from_header(temp_name, bmh);

		grs_bitmap temp_bitmap{};
		const auto iwidth = (bmh.dflags & DBM_FLAG_LARGE) ? bmh.width + 256 : bmh.width;
		gr_init_bitmap(temp_bitmap, bm_mode::linear, 0, 0, 
			iwidth, bmh.height,
			iwidth, Piggy_bitmap_cache_data);
		temp_bitmap.add_flags(BM_FLAG_PAGED_OUT);
#if !DXX_USE_OGL
		temp_bitmap.avg_color = bmh.avg_color;
#endif

		if (MacPig)
		{
			// HACK HACK HACK!!!!!
			if (!d_strnicmp(bmh.name, "cockpit") || !d_strnicmp(bmh.name, "status") || !d_strnicmp(bmh.name, "rearview")) {
				temp_bitmap.bm_w = temp_bitmap.bm_rowsize = 640;
				if (GameBitmapFlags[i+1] & BM_FLAG_RLE)
					GameBitmapFlags[i+1] |= BM_FLAG_RLE_BIG;
			}
			if (!d_strnicmp(bmh.name, "cockpit") || !d_strnicmp(bmh.name, "rearview"))
				temp_bitmap.bm_h = 480;
		}
		
		piggy_register_bitmap(temp_bitmap, temp_name.data(), 1);
	}

	if (!MacPig)
	{
	for (unsigned i = 0; i < N_sounds; ++i)
	{
		DiskSoundHeader_read(&sndh, Piggy_fp);
		
		//size -= sizeof(DiskSoundHeader);
#ifdef ALLEGRO
		temp_sound.len = sndh.length;
#else
		temp_sound.length = sndh.length;
#endif

//added on 11/13/99 by Victor Rachels to ready for changing freq
                temp_sound.freq = 11025;
//end this section addition - VR
		temp_sound.data = reinterpret_cast<uint8_t *>(sndh.offset + header_size + (sizeof(int)*2)+Pigdata_start);
		SoundOffset[Num_sound_files] = sndh.offset + header_size + (sizeof(int)*2)+Pigdata_start;
		if (PCSharePig)
			SoundCompressed[Num_sound_files] = sndh.data_length;
		std::array<char, 9> temp_name_read;
		memcpy(temp_name_read.data(), sndh.name, temp_name_read.size() - 1);
		temp_name_read.back() = 0;
		piggy_register_sound(&temp_sound, temp_name_read.data(), 1);
                sbytes += sndh.length;
	}

		SoundBits = std::make_unique<ubyte[]>(sbytes + 16);
	}

#if 1	//def EDITOR
	Piggy_bitmap_cache_size	= size - header_size - sbytes + 16;
	Assert( Piggy_bitmap_cache_size > 0 );
#else
	Piggy_bitmap_cache_size = PIGGY_BUFFER_SIZE;
	if (CGameArg.SysLowMem)
		Piggy_bitmap_cache_size = PIGGY_SMALL_BUFFER_SIZE;
#endif
	BitmapBits = std::make_unique<ubyte[]>(Piggy_bitmap_cache_size);
	Piggy_bitmap_cache_data = BitmapBits.get();
	Piggy_bitmap_cache_next = 0;

	return retval;
}
#elif defined(DXX_BUILD_DESCENT_II)

//initialize a pigfile, reading headers
//returns the size of all the bitmap data
void piggy_init_pigfile(const char *filename)
{
	int i;
	std::array<char, 13> temp_name;
	DiskBitmapHeader bmh;
	int header_size, N_bitmaps;
#if DXX_USE_EDITOR
	int data_size;
#endif

	piggy_close_file();             //close old pig if still open

	auto &&[fp, physfserr] = PHYSFSX_openReadBuffered(filename);
#if !DXX_USE_EDITOR
	auto effective_filename = filename;
#endif
	//try pigfile for shareware
	if (!fp)
	{
		auto &&[fp2, physfserr2] = PHYSFSX_openReadBuffered(DEFAULT_PIGFILE_SHAREWARE);
		if (!fp2)
		{
#if DXX_USE_EDITOR
			static_cast<void>(physfserr);
			static_cast<void>(physfserr2);
			return;         //if editor, ok to not have pig, because we'll build one
#else
			Error("Failed to open required files <%s>, <" DEFAULT_PIGFILE_SHAREWARE ">: \"%s\", \"%s\"", filename, PHYSFS_getErrorByCode(physfserr), PHYSFS_getErrorByCode(physfserr2));
#endif
		}
		fp = std::move(fp2);
#if !DXX_USE_EDITOR
		effective_filename = DEFAULT_PIGFILE_SHAREWARE;
#endif
	}
	Piggy_fp = std::move(fp);
	if (Piggy_fp) {                         //make sure pig is valid type file & is up-to-date
		int pig_id,pig_version;

		pig_id = PHYSFSX_readInt(Piggy_fp);
		pig_version = PHYSFSX_readInt(Piggy_fp);
		if (pig_id != PIGFILE_ID || pig_version != PIGFILE_VERSION) {
			Piggy_fp.reset(); //out of date pig
			                        //..so pretend it's not here
#if DXX_USE_EDITOR
			return;         //if editor, ok to not have pig, because we'll build one
		#else
			Error("Cannot load PIG file: expected (id=%.8lx version=%.8x), found (id=%.8x version=%.8x) in \"%s\"", PIGFILE_ID, PIGFILE_VERSION, pig_id, pig_version, effective_filename);
		#endif
		}
	}

	strncpy(Current_pigfile, filename, sizeof(Current_pigfile) - 1);

	N_bitmaps = PHYSFSX_readInt(Piggy_fp);

	header_size = N_bitmaps * sizeof(DiskBitmapHeader);

	const unsigned data_start = header_size + PHYSFS_tell(Piggy_fp);
#if DXX_USE_EDITOR
	data_size = PHYSFS_fileLength(Piggy_fp) - data_start;
#endif
	Num_bitmap_files = 1;

	for (i=0; i<N_bitmaps; i++ )
	{
		int width;
		grs_bitmap *bm = &GameBitmaps[i + 1];
		
		DiskBitmapHeader_read(&bmh, Piggy_fp);
		get_bitmap_name_from_header(temp_name, bmh);
		width = bmh.width + (static_cast<short>(bmh.wh_extra & 0x0f) << 8);
		gr_init_bitmap(*bm, bm_mode::linear, 0, 0, width, bmh.height + (static_cast<short>(bmh.wh_extra & 0xf0) << 4), width, NULL);
		bm->set_flags(BM_FLAG_PAGED_OUT);
#if !DXX_USE_OGL
		bm->avg_color = bmh.avg_color;
#endif

		GameBitmapFlags[i+1] = bmh.flags & BM_FLAGS_TO_COPY;

		GameBitmapOffset[i+1] = pig_bitmap_offset{bmh.offset + data_start};
		Assert( (i+1) == Num_bitmap_files );
		piggy_register_bitmap(*bm, temp_name.data(), 1);
	}

#if DXX_USE_EDITOR
	Piggy_bitmap_cache_size = data_size + (data_size/10);   //extra mem for new bitmaps
	Assert( Piggy_bitmap_cache_size > 0 );
#else
	Piggy_bitmap_cache_size = PIGGY_BUFFER_SIZE;
	if (CGameArg.SysLowMem)
		Piggy_bitmap_cache_size = PIGGY_SMALL_BUFFER_SIZE;
#endif
	BitmapBits = std::make_unique<ubyte[]>(Piggy_bitmap_cache_size);
	Piggy_bitmap_cache_data = BitmapBits.get();
	Piggy_bitmap_cache_next = 0;

	Pigfile_initialized=1;
}

//reads in a new pigfile (for new palette)
//returns the size of all the bitmap data
void piggy_new_pigfile(char *pigname)
{
	int i;
	std::array<char, 13> temp_name;
	DiskBitmapHeader bmh;
	int header_size, N_bitmaps;
#if DXX_USE_EDITOR
	int must_rewrite_pig = 0;
#endif

	d_strlwr(pigname);

	if (d_strnicmp(Current_pigfile, pigname, sizeof(Current_pigfile)) == 0 // correct pig already loaded
	    && !Bitmap_replacement_data) // no need to reload: no bitmaps were altered
		return;

	if (!Pigfile_initialized) {                     //have we ever opened a pigfile?
		piggy_init_pigfile(pigname);            //..no, so do initialization stuff
		return;
	}
	else
		piggy_close_file();             //close old pig if still open

	Piggy_bitmap_cache_next = 0;            //free up cache

	strncpy(Current_pigfile, pigname, sizeof(Current_pigfile) - 1);

	auto &&[fp, physfserr] = PHYSFSX_openReadBuffered(pigname);
#if !DXX_USE_EDITOR
	const char *effective_filename = pigname;
#endif
	//try pigfile for shareware
	if (!fp)
	{
		auto &&[fp2, physfserr2] = PHYSFSX_openReadBuffered(DEFAULT_PIGFILE_SHAREWARE);
		if (!fp2)
		{
#if DXX_USE_EDITOR
			static_cast<void>(physfserr);
			static_cast<void>(physfserr2);
			return;         //if editor, ok to not have pig, because we'll build one
#else
			Error("Failed to open required files <%s>, <" DEFAULT_PIGFILE_SHAREWARE ">: \"%s\", \"%s\"", pigname, PHYSFS_getErrorByCode(physfserr), PHYSFS_getErrorByCode(physfserr2));
#endif
		}
		fp = std::move(fp2);
#if !DXX_USE_EDITOR
		effective_filename = DEFAULT_PIGFILE_SHAREWARE;
#endif
	}
	Piggy_fp = std::move(fp);
	if (Piggy_fp) {  //make sure pig is valid type file & is up-to-date
		int pig_id,pig_version;

		pig_id = PHYSFSX_readInt(Piggy_fp);
		pig_version = PHYSFSX_readInt(Piggy_fp);
		if (pig_id != PIGFILE_ID || pig_version != PIGFILE_VERSION) {
			Piggy_fp.reset();              //out of date pig
			                        //..so pretend it's not here
#if DXX_USE_EDITOR
			return;         //if editor, ok to not have pig, because we'll build one
		#else
			Error("Cannot load PIG file: expected (id=%.8lx version=%.8x), found (id=%.8x version=%.8x) in \"%s\"", PIGFILE_ID, PIGFILE_VERSION, pig_id, pig_version, effective_filename);
		#endif
		}
		N_bitmaps = PHYSFSX_readInt(Piggy_fp);

		header_size = N_bitmaps * sizeof(DiskBitmapHeader);

		const unsigned data_start = header_size + PHYSFS_tell(Piggy_fp);

		for (i=1; i<=N_bitmaps; i++ )
		{
			grs_bitmap *bm = &GameBitmaps[i];
			int width;
			
			DiskBitmapHeader_read(&bmh, Piggy_fp);
			get_bitmap_name_from_header(temp_name, bmh);
#if DXX_USE_EDITOR
			//Make sure name matches
			if (strcmp(temp_name.data(), AllBitmaps[i].name.data()))
			{
				//Int3();       //this pig is out of date.  Delete it
				must_rewrite_pig=1;
			}
#endif
	
			AllBitmaps[i].name = temp_name;

			width = bmh.width + (static_cast<short>(bmh.wh_extra & 0x0f) << 8);
			gr_set_bitmap_data(*bm, NULL);	// free ogl texture
			gr_init_bitmap(*bm, bm_mode::linear, 0, 0, width, bmh.height + (static_cast<short>(bmh.wh_extra & 0xf0) << 4), width, NULL);
			bm->set_flags(BM_FLAG_PAGED_OUT);
#if !DXX_USE_OGL
			bm->avg_color = bmh.avg_color;
#endif

			GameBitmapFlags[i] = bmh.flags & BM_FLAGS_TO_COPY;
	
			GameBitmapOffset[i] = pig_bitmap_offset{bmh.offset + data_start};
		}
	}
	else
		N_bitmaps = 0;          //no pigfile, so no bitmaps

#if !DXX_USE_EDITOR

	Assert(N_bitmaps == Num_bitmap_files-1);

	#else

	if (must_rewrite_pig || (N_bitmaps < Num_bitmap_files-1)) {
		int size;

		//re-read the bitmaps that aren't in this pig

		for (i=N_bitmaps+1;i<Num_bitmap_files;i++) {
			if (const auto p = strchr(AllBitmaps[i].name.data(), '#'))
			{   // this is an ABM == animated bitmap
				char abmname[FILENAME_LEN];
				unsigned fnum;
				int iff_error;          //reference parm to avoid warning message
				palette_array_t newpal;
				char basename[FILENAME_LEN];
				unsigned nframes;

				const std::size_t len = p - AllBitmaps[i].name.data();
				cf_assert(len < AllBitmaps[i].name.size());
				memcpy(basename, AllBitmaps[i].name.data(), len);
				basename[len] = 0;

				snprintf(abmname, sizeof(abmname), "%.8s.abm", basename);

				std::array<std::unique_ptr<grs_main_bitmap>, MAX_BITMAPS_PER_BRUSH> bm;
				iff_error = iff_read_animbrush(abmname,bm,&nframes,newpal);

				if (iff_error != IFF_NO_ERROR)  {
					Error("File %s - IFF error: %s",abmname,iff_errormsg(iff_error));
				}
			
				for (fnum=0;fnum<nframes; fnum++)       {
					char tempname[20];
					int SuperX;

					snprintf(tempname, sizeof(tempname), "%s#%u", basename, fnum);

					//SuperX = (GameBitmaps[i+fnum].bm_flags&BM_FLAG_SUPER_TRANSPARENT)?254:-1;
					SuperX = (GameBitmapFlags[i+fnum]&BM_FLAG_SUPER_TRANSPARENT)?254:-1;
					//above makes assumption that supertransparent color is 254

					gr_remap_bitmap_good(*bm[fnum].get(), newpal, iff_has_transparency ? iff_transparent_color : -1, SuperX);

#if !DXX_USE_OGL
					bm[fnum]->avg_color = compute_average_pixel(bm[fnum].get());
#endif

					if ( GameArg.EdiMacData )
						swap_0_255(*bm[fnum].get());

					if (CGameArg.DbgNoCompressPigBitmap)
						gr_bitmap_rle_compress(*bm[fnum].get());

					if (bm[fnum]->get_flag_mask(BM_FLAG_RLE))
						size = *reinterpret_cast<const int *>(bm[fnum]->bm_data);
					else
						size = bm[fnum]->bm_w * bm[fnum]->bm_h;

					memcpy( &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next],bm[fnum]->bm_data,size);
					d_free(bm[fnum]->bm_mdata);
					bm[fnum]->bm_mdata = &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next];
					Piggy_bitmap_cache_next += size;

					GameBitmaps[i+fnum] = std::move(*bm[fnum]);
				}

				i += nframes-1;         //filled in multiple bitmaps
			}
			else {          //this is a BBM

				grs_bitmap n;
				palette_array_t newpal;
				int iff_error;
				char bbmname[FILENAME_LEN];
				int SuperX;

				snprintf(bbmname, sizeof(bbmname), "%.8s.bbm", AllBitmaps[i].name.data());
				iff_error = iff_read_bitmap(bbmname, n, &newpal);

				if (iff_error != IFF_NO_ERROR)          {
					Error("File %s - IFF error: %s",bbmname,iff_errormsg(iff_error));
				}

				SuperX = (GameBitmapFlags[i]&BM_FLAG_SUPER_TRANSPARENT)?254:-1;
				//above makes assumption that supertransparent color is 254

				gr_remap_bitmap_good(n, newpal, iff_has_transparency ? iff_transparent_color : -1, SuperX);

#if !DXX_USE_OGL
				n.avg_color = compute_average_pixel(&n);
#endif

				if ( GameArg.EdiMacData )
					swap_0_255(n);

				if (CGameArg.DbgNoCompressPigBitmap)
					gr_bitmap_rle_compress(n);

				if (n.get_flag_mask(BM_FLAG_RLE))
					size = *reinterpret_cast<const int *>(n.bm_data);
				else
					size = n.bm_w * n.bm_h;

				memcpy( &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next],n.bm_data,size);
				d_free(n.bm_mdata);
				n.bm_mdata = &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next];
				Piggy_bitmap_cache_next += size;

				GameBitmaps[i] = n;
			}
		}

		//@@Dont' do these things which are done when writing
		//@@for (i=0; i < Num_bitmap_files; i++ )       {
		//@@    bitmap_index bi;
		//@@    bi.index = i;
		//@@    PIGGY_PAGE_IN( bi );
		//@@}
		//@@
		//@@piggy_close_file();

		piggy_write_pigfile(pigname);

		Current_pigfile[0] = 0;                 //say no pig, to force reload

		piggy_new_pigfile(pigname);             //read in just-generated pig


	}
	#endif  //ifdef EDITOR
}

#define HAMFILE_ID              MAKE_SIG('!','M','A','H') //HAM!
#define HAMFILE_VERSION 3
//version 1 -> 2:  save marker_model_num
//version 2 -> 3:  removed sound files

#define SNDFILE_ID              MAKE_SIG('D','N','S','D') //DSND
#define SNDFILE_VERSION 1

int read_hamfile()
{
	int ham_id;
	int sound_offset = 0;
	int shareware = 0;

	auto ham_fp = PHYSFSX_openReadBuffered(DEFAULT_HAMFILE_REGISTERED).first;
	
	if (!ham_fp)
	{
		ham_fp = PHYSFSX_openReadBuffered(DEFAULT_HAMFILE_SHAREWARE).first;
		if (ham_fp)
		{
			shareware = 1;
			GameArg.SndDigiSampleRate = SAMPLE_RATE_11K;
			if (CGameArg.SndDisableSdlMixer)
			{
				digi_close();
				digi_init();
			}
		}
	}

	if (!ham_fp) {
		return 0;
	}

	//make sure ham is valid type file & is up-to-date
	ham_id = PHYSFSX_readInt(ham_fp);
	Piggy_hamfile_version = PHYSFSX_readInt(ham_fp);
	if (ham_id != HAMFILE_ID)
		Error("Failed to load ham file " DEFAULT_HAMFILE_REGISTERED ": expected ham_id=%.8lx, found ham_id=%.8x version=%.8x\n", HAMFILE_ID, ham_id, Piggy_hamfile_version);
#if 0
	if (ham_id != HAMFILE_ID || Piggy_hamfile_version != HAMFILE_VERSION) {
		Must_write_hamfile = 1;
		PHYSFS_close(ham_fp);						//out of date ham
		return 0;
	}
#endif

	if (Piggy_hamfile_version < 3) // hamfile contains sound info, probably PC demo
	{
		sound_offset = PHYSFSX_readInt(ham_fp);
		
		if (shareware) // deal with interactive PC demo
		{
			GameArg.GfxSkipHiresGFX = 1;
			//CGameArg.SysLowMem = 1;
		}
	}

	#if 1 //ndef EDITOR
	{
		bm_read_all(Vclip, ham_fp);
		//PHYSFS_read( ham_fp, GameBitmapXlat, sizeof(ushort)*MAX_BITMAP_FILES, 1 );
		range_for (auto &i, GameBitmapXlat)
		{
			i = PHYSFSX_readShort(ham_fp);
			if (PHYSFS_eof(ham_fp))
				break;
		}
	}
	#endif

	if (Piggy_hamfile_version < 3) {
		int N_sounds;
		int sound_start;
		int header_size;
		int i;
		DiskSoundHeader sndh;
		digi_sound temp_sound;
		char temp_name_read[16];
		int sbytes = 0;
		static int justonce = 1;

		if (!justonce)
		{
			return 1;
		}
		justonce = 0;

		PHYSFSX_fseek(ham_fp, sound_offset, SEEK_SET);
		N_sounds = PHYSFSX_readInt(ham_fp);

		sound_start = PHYSFS_tell(ham_fp);

		header_size = N_sounds * sizeof(DiskSoundHeader);

		//Read sounds

		for (i=0; i<N_sounds; i++ ) {
			DiskSoundHeader_read(&sndh, ham_fp);
			temp_sound.length = sndh.length;
			temp_sound.data = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(sndh.offset + header_size + sound_start));
			SoundOffset[Num_sound_files] = sndh.offset + header_size + sound_start;
			memcpy( temp_name_read, sndh.name, 8 );
			temp_name_read[8] = 0;
			piggy_register_sound( &temp_sound, temp_name_read, 1 );
			if (piggy_is_needed(i))
				sbytes += sndh.length;
		}
		SoundBits = std::make_unique<ubyte[]>(sbytes + 16);
	}
	return 1;
}

void read_sndfile(const int required)
{
	int snd_id,snd_version;
	int N_sounds;
	int sound_start;
	int header_size;
	int i;
	DiskSoundHeader sndh;
	digi_sound temp_sound;
	char temp_name_read[16];
	int sbytes = 0;

	const auto filename = DEFAULT_SNDFILE;
	auto &&[snd_fp, physfserr] = PHYSFSX_openReadBuffered(filename);
	if (!snd_fp)
	{
		if (required)
			Error("Cannot open sound file: %s: %s", filename, PHYSFS_getErrorByCode(physfserr));
		return;
	}

	//make sure soundfile is valid type file & is up-to-date
	snd_id = PHYSFSX_readInt(snd_fp);
	snd_version = PHYSFSX_readInt(snd_fp);
	if (snd_id != SNDFILE_ID || snd_version != SNDFILE_VERSION) {
		if (required)
			Error("Cannot load sound file: expected (id=%.8lx version=%.8x), found (id=%.8x version=%.8x) in \"%s\"", SNDFILE_ID, SNDFILE_VERSION, snd_id, snd_version, filename);
		return;
	}

	N_sounds = PHYSFSX_readInt(snd_fp);

	sound_start = PHYSFS_tell(snd_fp);
	header_size = N_sounds*sizeof(DiskSoundHeader);

	//Read sounds

	for (i=0; i<N_sounds; i++ ) {
		DiskSoundHeader_read(&sndh, snd_fp);
		temp_sound.length = sndh.length;
		temp_sound.data = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(sndh.offset + header_size + sound_start));
		SoundOffset[Num_sound_files] = sndh.offset + header_size + sound_start;
		memcpy( temp_name_read, sndh.name, 8 );
		temp_name_read[8] = 0;
		piggy_register_sound( &temp_sound, temp_name_read, 1 );
		if (piggy_is_needed(i))
			sbytes += sndh.length;
	}
	SoundBits = std::make_unique<ubyte[]>(sbytes + 16);
}

int properties_init(void)
{
	int ham_ok=0,snd_ok=0;
	for (unsigned i = 0; i < MAX_SOUND_FILES; ++i)
	{
		GameSounds[i].length = 0;
		GameSounds[i].data = NULL;
		SoundOffset[i] = 0;
	}

	for (unsigned i = 0; i < GameBitmapXlat.size(); ++i)
	{
		GameBitmapXlat[i] = i;
	}

	if ( !bogus_bitmap_initialized )        {
		ubyte c;

		bogus_bitmap_initialized = 1;
		c = gr_find_closest_color( 0, 0, 63 );
		bogus_data.fill(c);
		c = gr_find_closest_color( 63, 0, 0 );
		// Make a big red X !
		range_for (const unsigned i, xrange(64u))
		{
			bogus_data[i*64+i] = c;
			bogus_data[i*64+(63-i)] = c;
		}
		gr_init_bitmap(GameBitmaps[Num_bitmap_files], bm_mode::linear, 0, 0, 64, 64, 64, bogus_data.data());
		piggy_register_bitmap(GameBitmaps[Num_bitmap_files], "bogus", 1);
		bogus_sound.length = 64*64;
		bogus_sound.data = bogus_data.data();
		GameBitmapOffset[0] = pig_bitmap_offset::None;
	}

	snd_ok = ham_ok = read_hamfile();

	if (Piggy_hamfile_version >= 3)
	{
		read_sndfile(1);
	}

	return (ham_ok && snd_ok);               //read ok
}
#endif

#if defined(DXX_BUILD_DESCENT_I)
void piggy_read_sounds(int pc_shareware)
{
	uint8_t * ptr;
	int i;

	if (MacPig)
	{
		// Read Mac sounds converted to RAW format (too messy to read them directly from the resource fork code-wise)
		char soundfile[32] = "Sounds/sounds.array";
		// hack for Mac Demo
		if (auto array = PHYSFSX_openReadBuffered(soundfile).first)
		{
			/* Sounds.size() is the D2-compatible size, but this
			 * statement must read a D1 length, so use MAX_SOUNDS.
			 */
			if (PHYSFS_read(array, Sounds, MAX_SOUNDS, 1) != 1)	// make the 'Sounds' index array match with the sounds we're about to read in
			{
				con_printf(CON_URGENT,"Warning: Can't read Sounds/sounds.array: %s", PHYSFS_getLastError());
				return;
			}
		}
		else if (PHYSFSX_fsize(DEFAULT_PIGFILE_REGISTERED) == D1_MAC_SHARE_PIGSIZE)
		{
			con_printf(CON_URGENT,"Warning: Missing Sounds/sounds.array for Mac data files");
			return;
		}

		for (i = 0; i < MAX_SOUND_FILES; i++)
		{
			snprintf(soundfile, sizeof(soundfile), "SND%04d.raw", i);
			if (ds_load(0, soundfile) == 255)
				break;
		}

		return;
	}

	ptr = SoundBits.get();

	std::vector<uint8_t> lastbuf;
	for (i=0; i<Num_sound_files; i++ )
	{
		digi_sound *snd = &GameSounds[i];

		if ( SoundOffset[i] > 0 )
		{
			if ( piggy_is_needed(i) )
			{
				PHYSFSX_fseek( Piggy_fp, SoundOffset[i], SEEK_SET );

				// Read in the sound data!!!
				snd->data = ptr;
#ifdef ALLEGRO
				ptr += snd->len;
#else
				ptr += snd->length;
#endif
		//Arne's decompress for shareware on all soundcards - Tim@Rikers.org
				if (pc_shareware)
				{
					const auto compressed_length = SoundCompressed[i];
					lastbuf.resize(compressed_length);
					PHYSFS_read(Piggy_fp, lastbuf.data(), compressed_length, 1);
					sound_decompress(lastbuf.data(), compressed_length, snd->data);
				}
				else
#ifdef ALLEGRO
					PHYSFS_read( Piggy_fp, snd->data, snd->len, 1 );
#else
					PHYSFS_read( Piggy_fp, snd->data, snd->length, 1 );
#endif
			}
		}
	}
}
#elif defined(DXX_BUILD_DESCENT_II)
void piggy_read_sounds(void)
{
	uint8_t * ptr;
	int i;

	ptr = SoundBits.get();
	auto fp = PHYSFSX_openReadBuffered(DEFAULT_SNDFILE).first;
	if (!fp)
		return;

	for (i=0; i<Num_sound_files; i++ )      {
		digi_sound *snd = &GameSounds[i];

		if ( SoundOffset[i] > 0 )       {
			if ( piggy_is_needed(i) )       {
				PHYSFSX_fseek( fp, SoundOffset[i], SEEK_SET );

				// Read in the sound data!!!
				snd->data = ptr;
				ptr += snd->length;
				PHYSFS_read( fp, snd->data, snd->length, 1 );
			}
			else
				snd->data = reinterpret_cast<uint8_t *>(-1);
		}
	}
}
#endif

void piggy_bitmap_page_in( bitmap_index bitmap )
{
	grs_bitmap * bmp;
	int i,org_i;

	org_i = 0;

	i = bitmap.index;
	Assert( i >= 0 );
	Assert( i < MAX_BITMAP_FILES );
	Assert( i < Num_bitmap_files );
	Assert( Piggy_bitmap_cache_size > 0 );

	if ( i < 1 ) return;
	if ( i >= MAX_BITMAP_FILES ) return;
	if ( i >= Num_bitmap_files ) return;

	if (GameBitmapOffset[i] == pig_bitmap_offset::None)
		return;		// A read-from-disk bitmap!!!

	if (CGameArg.SysLowMem)
	{
		org_i = i;
		i = GameBitmapXlat[i];          // Xlat for low-memory settings!
	}

	bmp = &GameBitmaps[i];

	if (bmp->get_flag_mask(BM_FLAG_PAGED_OUT))
	{
		pause_game_world_time p;

	ReDoIt:
		PHYSFS_seek(Piggy_fp, static_cast<unsigned>(GameBitmapOffset[i]));

		gr_set_bitmap_flags(*bmp, GameBitmapFlags[i]);
#if defined(DXX_BUILD_DESCENT_I)
		gr_set_bitmap_data (*bmp, &Piggy_bitmap_cache_data [Piggy_bitmap_cache_next]);
#endif

		if (bmp->get_flag_mask(BM_FLAG_RLE))
		{
			int zsize = PHYSFSX_readInt(Piggy_fp);
#if defined(DXX_BUILD_DESCENT_I)

			// GET JOHN NOW IF YOU GET THIS ASSERT!!!
			Assert( Piggy_bitmap_cache_next+zsize < Piggy_bitmap_cache_size );
			if ( Piggy_bitmap_cache_next+zsize >= Piggy_bitmap_cache_size ) {
				piggy_bitmap_page_out_all();
				goto ReDoIt;
			}
			memcpy( &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next], &zsize, sizeof(int) );
			Piggy_bitmap_cache_next += sizeof(int);
			PHYSFS_read( Piggy_fp, &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next], 1, zsize-4 );
			if (MacPig)
			{
				rle_swap_0_255(*bmp);
				memcpy(&zsize, bmp->bm_data, 4);
			}
			Piggy_bitmap_cache_next += zsize-4;
#elif defined(DXX_BUILD_DESCENT_II)
			int pigsize = PHYSFS_fileLength(Piggy_fp);

			// GET JOHN NOW IF YOU GET THIS ASSERT!!!
			//Assert( Piggy_bitmap_cache_next+zsize < Piggy_bitmap_cache_size );
			if ( Piggy_bitmap_cache_next+zsize >= Piggy_bitmap_cache_size ) {
				Int3();
				piggy_bitmap_page_out_all();
				goto ReDoIt;
			}
			PHYSFS_read( Piggy_fp, &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next+4], 1, zsize-4 );
			PUT_INTEL_INT(&Piggy_bitmap_cache_data[Piggy_bitmap_cache_next], zsize);
			gr_set_bitmap_data(*bmp, &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next]);

#ifndef MACDATA
			switch (pigsize) {
			default:
				if (!GameArg.EdiMacData)
					break;
				[[fallthrough]];
			case MAC_ALIEN1_PIGSIZE:
			case MAC_ALIEN2_PIGSIZE:
			case MAC_FIRE_PIGSIZE:
			case MAC_GROUPA_PIGSIZE:
			case MAC_ICE_PIGSIZE:
			case MAC_WATER_PIGSIZE:
				rle_swap_0_255(*bmp);
				memcpy(&zsize, bmp->bm_data, 4);
				break;
			}
#endif

			Piggy_bitmap_cache_next += zsize;
			if ( Piggy_bitmap_cache_next+zsize >= Piggy_bitmap_cache_size ) {
				Int3();
				piggy_bitmap_page_out_all();
				goto ReDoIt;
			}
#endif

		} else {
			// GET JOHN NOW IF YOU GET THIS ASSERT!!!
			Assert( Piggy_bitmap_cache_next+(bmp->bm_h*bmp->bm_w) < Piggy_bitmap_cache_size );
			if ( Piggy_bitmap_cache_next+(bmp->bm_h*bmp->bm_w) >= Piggy_bitmap_cache_size ) {
				piggy_bitmap_page_out_all();
				goto ReDoIt;
			}
			PHYSFS_read( Piggy_fp, &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next], 1, bmp->bm_h*bmp->bm_w );
#if defined(DXX_BUILD_DESCENT_I)
			Piggy_bitmap_cache_next+=bmp->bm_h*bmp->bm_w;
			if (MacPig)
				swap_0_255(*bmp);
#elif defined(DXX_BUILD_DESCENT_II)
			int pigsize = PHYSFS_fileLength(Piggy_fp);
			gr_set_bitmap_data(*bmp, &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next]);
			Piggy_bitmap_cache_next+=bmp->bm_h*bmp->bm_w;

#ifndef MACDATA
			switch (pigsize) {
			default:
				if (!GameArg.EdiMacData)
					break;
				[[fallthrough]];
			case MAC_ALIEN1_PIGSIZE:
			case MAC_ALIEN2_PIGSIZE:
			case MAC_FIRE_PIGSIZE:
			case MAC_GROUPA_PIGSIZE:
			case MAC_ICE_PIGSIZE:
			case MAC_WATER_PIGSIZE:
				swap_0_255(*bmp);
				break;
			}
#endif
#endif
		}

		//@@if ( bmp->bm_selector ) {
		//@@#if !defined(WINDOWS) && !defined(MACINTOSH)
		//@@	if (!dpmi_modify_selector_base( bmp->bm_selector, bmp->bm_data ))
		//@@		Error( "Error modifying selector base in piggy.c\n" );
		//@@#endif
		//@@}

		compute_average_rgb(bmp, bmp->avg_color_rgb);

	}

	if (CGameArg.SysLowMem)
	{
		if ( org_i != i )
			GameBitmaps[org_i] = GameBitmaps[i];
	}

//@@Removed from John's code:
//@@#ifndef WINDOWS
//@@    if ( bmp->bm_selector ) {
//@@            if (!dpmi_modify_selector_base( bmp->bm_selector, bmp->bm_data ))
//@@                    Error( "Error modifying selector base in piggy.c\n" );
//@@    }
//@@#endif

}
}

namespace dsx {
void piggy_bitmap_page_out_all()
{
	int i;
	
	Piggy_bitmap_cache_next = 0;

	texmerge_flush();
	rle_cache_flush();

	for (i=0; i<Num_bitmap_files; i++ ) {
		if (GameBitmapOffset[i] != pig_bitmap_offset::None)
		{	// Don't page out bitmaps read from disk!!!
			GameBitmaps[i].set_flags(BM_FLAG_PAGED_OUT);
			gr_set_bitmap_data(GameBitmaps[i], nullptr);
		}
	}

}
}

void piggy_load_level_data()
{
	piggy_bitmap_page_out_all();
	paging_touch_all(Vclip);
}

namespace dsx {
namespace {
#if defined(DXX_BUILD_DESCENT_II)
#if DXX_USE_EDITOR

static void piggy_write_pigfile(const char *filename)
{
	int bitmap_data_start, data_offset;
	DiskBitmapHeader bmh;
	int org_offset;
	char subst_name[32];
	int i;
	char tname[FILENAME_LEN];

	for (i=0; i < Num_bitmap_files; i++ ) {
		bitmap_index bi;
		bi.index = i;
		PIGGY_PAGE_IN( bi );
	}

	piggy_close_file();

	auto pig_fp = PHYSFSX_openWriteBuffered(filename).first;       //open PIG file
	if (!pig_fp)
		return;

	write_int(PIGFILE_ID,pig_fp);
	write_int(PIGFILE_VERSION,pig_fp);

	Num_bitmap_files--;
	PHYSFS_write( pig_fp, &Num_bitmap_files, sizeof(int), 1 );
	Num_bitmap_files++;

	bitmap_data_start = PHYSFS_tell(pig_fp);
	bitmap_data_start += (Num_bitmap_files - 1) * sizeof(DiskBitmapHeader);
	data_offset = bitmap_data_start;

	change_filename_extension(tname,filename,"lst");
	auto fp1 = PHYSFSX_openWriteBuffered(tname).first;
	change_filename_extension(tname,filename,"all");
	auto fp2 = PHYSFSX_openWriteBuffered(tname).first;

	for (i=1; i < Num_bitmap_files; i++ ) {
		grs_bitmap *bmp;

		const auto name = AllBitmaps[i].name.data();
		{
			char *p1;
			if (const auto p = strchr(name, '#')) {   // this is an ABM == animated bitmap
				int n;
				p1 = p; p1++; 
				n = atoi(p1);
				*p = 0;
				if (fp2 && n==0)
					PHYSFSX_printf( fp2, "%s.abm\n", name);
				memcpy(bmh.name, name, 8);
				Assert( n <= DBM_NUM_FRAMES );
				bmh.dflags = DBM_FLAG_ABM + n;
				*p = '#';
			} else {
				if (fp2)
					PHYSFSX_printf( fp2, "%s.bbm\n", name);
				memcpy(bmh.name, name, 8);
				bmh.dflags = 0;
			}
		}
		bmp = &GameBitmaps[i];

		assert(!bmp->get_flag_mask(BM_FLAG_PAGED_OUT));

		if (fp1)
			PHYSFSX_printf(fp1, "BMP: %s, size %d bytes", name, bmp->bm_rowsize * bmp->bm_h);
		org_offset = PHYSFS_tell(pig_fp);
		bmh.offset = data_offset - bitmap_data_start;
		PHYSFSX_fseek( pig_fp, data_offset, SEEK_SET );

		if (bmp->get_flag_mask(BM_FLAG_RLE))
		{
			const auto size = reinterpret_cast<const int *>(bmp->bm_data);
			PHYSFS_write( pig_fp, bmp->bm_data, sizeof(ubyte), *size );
			data_offset += *size;
			if (fp1)
				PHYSFSX_printf( fp1, ", and is already compressed to %d bytes.\n", *size );
		} else {
			PHYSFS_write( pig_fp, bmp->bm_data, sizeof(ubyte), bmp->bm_rowsize * bmp->bm_h );
			data_offset += bmp->bm_rowsize * bmp->bm_h;
			if (fp1)
				PHYSFSX_printf( fp1, ".\n" );
		}
		PHYSFSX_fseek( pig_fp, org_offset, SEEK_SET );
		Assert( GameBitmaps[i].bm_w < 4096 );
		bmh.width = (GameBitmaps[i].bm_w & 0xff);
		bmh.wh_extra = ((GameBitmaps[i].bm_w >> 8) & 0x0f);
		Assert( GameBitmaps[i].bm_h < 4096 );
		bmh.height = GameBitmaps[i].bm_h;
		bmh.wh_extra |= ((GameBitmaps[i].bm_h >> 4) & 0xf0);
		bmh.flags = GameBitmaps[i].get_flags();
		if (piggy_is_substitutable_bitmap(name, subst_name))
		{
			bitmap_index other_bitmap;
			other_bitmap = piggy_find_bitmap( subst_name );
			GameBitmapXlat[i] = other_bitmap.index;
			bmh.flags |= BM_FLAG_PAGED_OUT;
		} else {
			bmh.flags &= ~BM_FLAG_PAGED_OUT;
		}
		bmh.avg_color = compute_average_pixel(&GameBitmaps[i]);
		PHYSFS_write(pig_fp, &bmh, sizeof(DiskBitmapHeader), 1);	// Mark as a bitmap
	}
	PHYSFSX_printf( fp1, " Dumped %d assorted bitmaps.\n", Num_bitmap_files );
}

static void write_int(int i, PHYSFS_File *file)
{
	if (PHYSFS_write( file, &i, sizeof(i), 1) != 1)
		Error( "Error reading int in gamesave.c" );

}
#endif

/*
 * Functions for loading replacement textures
 *  1) From .pog files
 *  2) From descent.pig (for loading d1 levels)
 */
static void free_bitmap_replacements()
{
	Bitmap_replacement_data.reset();
}
#endif
}

void piggy_close()
{
	int i;

#if defined(DXX_BUILD_DESCENT_I)
	custom_close();
#endif
	piggy_close_file();
	BitmapBits.reset();
	SoundBits.reset();
	for (i = 0; i < Num_sound_files; i++)
		if (SoundOffset[i] == 0)
			d_free(GameSounds[i].data);
#if defined(DXX_BUILD_DESCENT_II)
	free_bitmap_replacements();
	free_d1_tmap_nums();
#endif
}

}

void remove_char( char * s, char c )
{
	char *p = strchr(s,c);
	if (p) *p = '\0';
}

#if defined(DXX_BUILD_DESCENT_II)
namespace dsx {
namespace {

#if DXX_USE_EDITOR
static const BitmapFile *piggy_does_bitmap_exist_slow(const char *const name)
{
	range_for (auto &i, partial_const_range(AllBitmaps, Num_bitmap_files))
	{
		if (!strcmp(i.name.data(), name))
			return &i;
	}
	return nullptr;
}

constexpr char gauge_bitmap_names[][9] = {
	"gauge01",
	"gauge02",
	"gauge06",
	"targ01",
	"targ02",
	"targ03",
	"targ04",
	"targ05",
	"targ06",
	"gauge18",
#if defined(DXX_BUILD_DESCENT_I)
	"targ01pc",
	"targ02pc",
	"targ03pc",
	"gaug18pc"
#elif defined(DXX_BUILD_DESCENT_II)
	"gauge01b",
	"gauge02b",
	"gauge06b",
	"targ01b",
	"targ02b",
	"targ03b",
	"targ04b",
	"targ05b",
	"targ06b",
	"gauge18b",
	"gauss1",
	"helix1",
	"phoenix1"
#endif
};

static const char (*piggy_is_gauge_bitmap(const char *const base_name))[9]
{
	range_for (auto &i, gauge_bitmap_names)
	{
		if (!d_stricmp(base_name, i))
			return &i;
	}
	return nullptr;
}

static int piggy_is_substitutable_bitmap(char * name, char (&subst_name)[32])
{
	int frame;
	char * p;
	char base_name[ 16 ];
	
	strcpy( subst_name, name );
	p = strchr( subst_name, '#' );
	if ( p ) {
		frame = atoi( &p[1] );
		*p = 0;
		strcpy( base_name, subst_name );
		if ( !piggy_is_gauge_bitmap( base_name )) {
			snprintf(subst_name, sizeof(subst_name), "%s#%d", base_name, frame + 1);
			if ( piggy_does_bitmap_exist_slow( subst_name )  ) {
				if ( frame & 1 ) {
					snprintf(subst_name, sizeof(subst_name), "%s#%d", base_name, frame - 1);
					return 1;
				}
			}
		}
	}
	strcpy( subst_name, name );
	return 0;
}
#endif

/* returns nonzero if d1_tmap_num references a texture which isn't available in d2. */
int d1_tmap_num_unique(uint16_t d1_tmap_num)
{
	switch (d1_tmap_num) {
	case   0: case   2: case   4: case   5: case   6: case   7: case   9:
	case  10: case  11: case  12: case  17: case  18:
	case  20: case  21: case  25: case  28:
	case  38: case  39: case  41: case  44: case  49:
	case  50: case  55: case  57: case  88:
	case 132: case 141: case 147:
	case 154: case 155: case 158: case 159:
	case 160: case 161: case 167: case 168: case 169:
	case 170: case 171: case 174: case 175: case 185:
	case 193: case 194: case 195: case 198: case 199:
	case 200: case 202: case 210: case 211:
	case 220: case 226: case 227: case 228: case 229: case 230:
	case 240: case 241: case 242: case 243: case 246:
	case 250: case 251: case 252: case 253: case 257: case 258: case 259:
	case 260: case 263: case 266: case 283: case 298:
	case 315: case 317: case 319: case 320: case 321:
	case 330: case 331: case 332: case 333: case 349:
	case 351: case 352: case 353: case 354:
	case 355: case 357: case 358: case 359:
	case 362: case 370: return 1;
	default: return 0;
	}
}

}

void load_bitmap_replacements(const char *level_name)
{
	char ifile_name[FILENAME_LEN];
	//first, free up data allocated for old bitmaps
	free_bitmap_replacements();

	change_filename_extension(ifile_name, level_name, ".POG" );
	if (auto ifile = PHYSFSX_openReadBuffered(ifile_name).first)
	{
		int id,version;
		unsigned n_bitmaps;
		int bitmap_data_size;

		id = PHYSFSX_readInt(ifile);
		version = PHYSFSX_readInt(ifile);

		if (id != MAKE_SIG('G','O','P','D') || version != 1) {
			return;
		}

		n_bitmaps = PHYSFSX_readInt(ifile);

		const auto indices = std::make_unique<uint16_t[]>(n_bitmaps);
		range_for (auto &i, unchecked_partial_range(indices.get(), n_bitmaps))
			i = PHYSFSX_readShort(ifile);

		bitmap_data_size = PHYSFS_fileLength(ifile) - PHYSFS_tell(ifile) - sizeof(DiskBitmapHeader) * n_bitmaps;
		Bitmap_replacement_data = std::make_unique<ubyte[]>(bitmap_data_size);

		range_for (const auto i, unchecked_partial_range(indices.get(), n_bitmaps))
		{
			DiskBitmapHeader bmh;
			grs_bitmap *bm = &GameBitmaps[i];
			int width;

			DiskBitmapHeader_read(&bmh, ifile);

			width = bmh.width + (static_cast<short>(bmh.wh_extra & 0x0f) << 8);
			gr_set_bitmap_data(*bm, NULL);	// free ogl texture
			gr_init_bitmap(*bm, bm_mode::linear, 0, 0, width, bmh.height + (static_cast<short>(bmh.wh_extra & 0xf0) << 4), width, NULL);
#if !DXX_USE_OGL
			bm->avg_color = bmh.avg_color;
#endif
			bm->bm_data = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(bmh.offset));

			gr_set_bitmap_flags(*bm, bmh.flags & BM_FLAGS_TO_COPY);

			GameBitmapOffset[i] = pig_bitmap_offset::None; // don't try to read bitmap from current pigfile
		}

		PHYSFS_read(ifile,Bitmap_replacement_data,1,bitmap_data_size);

		range_for (const auto i, unchecked_partial_range(indices.get(), n_bitmaps))
		{
			grs_bitmap *bm = &GameBitmaps[i];
			gr_set_bitmap_data(*bm, &Bitmap_replacement_data[reinterpret_cast<uintptr_t>(bm->bm_data)]);
		}
		last_palette_loaded_pig[0]= 0;  //force pig re-load
		texmerge_flush();       //for re-merging with new textures
	}
}

namespace {

/* calculate table to translate d1 bitmaps to current palette,
 * return -1 on error
 */
static int get_d1_colormap(palette_array_t &d1_palette, std::array<color_palette_index, 256> &colormap)
{
	auto palette_file = PHYSFSX_openReadBuffered(D1_PALETTE).first;
	if (!palette_file || PHYSFS_fileLength(palette_file) != 9472)
		return -1;
	PHYSFS_read( palette_file, &d1_palette[0], sizeof(d1_palette[0]), d1_palette.size() );
	build_colormap_good(d1_palette, colormap);
	// don't change transparencies:
	colormap[254] = color_palette_index{254};
	colormap[255] = TRANSPARENCY_COLOR;
	return 0;
}

#define JUST_IN_CASE 132 /* is enough for d1 pc registered */
static void bitmap_read_d1( grs_bitmap *bitmap, /* read into this bitmap */
                     PHYSFS_File *d1_Piggy_fp, /* read from this file */
                     int bitmap_data_start, /* specific to file */
                     DiskBitmapHeader *bmh, /* header info for bitmap */
                     uint8_t **next_bitmap, /* where to write it (if 0, use malloc) */
					 palette_array_t &d1_palette, /* what palette the bitmap has */
					 std::array<color_palette_index, 256> &colormap) /* how to translate bitmap's colors */
{
	int zsize, pigsize = PHYSFS_fileLength(d1_Piggy_fp);
	uint8_t *data;
	int width;

	width = bmh->width + (static_cast<short>(bmh->wh_extra & 0x0f) << 8);
	gr_set_bitmap_data(*bitmap, NULL);	// free ogl texture
	gr_init_bitmap(*bitmap, bm_mode::linear, 0, 0, width, bmh->height + (static_cast<short>(bmh->wh_extra & 0xf0) << 4), width, NULL);
#if !DXX_USE_OGL
	bitmap->avg_color = bmh->avg_color;
#endif
	gr_set_bitmap_flags(*bitmap, bmh->flags & BM_FLAGS_TO_COPY);

	PHYSFSX_fseek(d1_Piggy_fp, bitmap_data_start + bmh->offset, SEEK_SET);
	if (bmh->flags & BM_FLAG_RLE) {
		zsize = PHYSFSX_readInt(d1_Piggy_fp);
		PHYSFSX_fseek(d1_Piggy_fp, -4, SEEK_CUR);
	} else
		zsize = bitmap->bm_h * bitmap->bm_w;

	if (next_bitmap) {
		data = *next_bitmap;
		*next_bitmap += zsize;
	} else {
		MALLOC(data, ubyte, zsize + JUST_IN_CASE);
	}
	if (!data) return;

	PHYSFS_read(d1_Piggy_fp, data, 1, zsize);
	gr_set_bitmap_data(*bitmap, data);
	switch(pigsize) {
	case D1_MAC_PIGSIZE:
	case D1_MAC_SHARE_PIGSIZE:
		if (bmh->flags & BM_FLAG_RLE)
			rle_swap_0_255(*bitmap);
		else
			swap_0_255(*bitmap);
	}
	if (bmh->flags & BM_FLAG_RLE)
		rle_remap(*bitmap, colormap);
	else
		gr_remap_bitmap_good(*bitmap, d1_palette, TRANSPARENCY_COLOR, -1);
	if (bmh->flags & BM_FLAG_RLE) { // size of bitmap could have changed!
		int new_size;
		memcpy(&new_size, bitmap->bm_data, 4);
		if (next_bitmap) {
			*next_bitmap += new_size - zsize;
		} else {
			Assert( zsize + JUST_IN_CASE >= new_size );
			bitmap->bm_mdata = reinterpret_cast<uint8_t *>(d_realloc(bitmap->bm_mdata, new_size));
			Assert(bitmap->bm_data);
		}
	}
}

#define D1_MAX_TEXTURES 800

static void bm_read_d1_tmap_nums(PHYSFS_File *d1pig)
{
	int i, d1_index;

	PHYSFSX_fseek(d1pig, 8, SEEK_SET);
	d1_tmap_nums = std::make_unique<d1_tmap_nums_t>();
	d1_tmap_nums->fill(-1);
	for (i = 0; i < D1_MAX_TEXTURES; i++) {
		d1_index = PHYSFSX_readShort(d1pig);
		Assert(d1_index >= 0 && d1_index < D1_MAX_TMAP_NUM);
		(*d1_tmap_nums)[d1_index] = i;
		if (PHYSFS_eof(d1pig))
			break;
	}
}

// this function is at the same position in the d1 shareware piggy loading 
// algorithm as bm_load_sub in main/bmread.c
static int get_d1_bm_index(char *filename, PHYSFS_File *d1_pig) {
	int i, N_bitmaps;
	DiskBitmapHeader bmh;
	if (strchr (filename, '.'))
		*strchr (filename, '.') = '\0'; // remove extension
	PHYSFSX_fseek (d1_pig, 0, SEEK_SET);
	N_bitmaps = PHYSFSX_readInt (d1_pig);
	PHYSFSX_fseek (d1_pig, 8, SEEK_SET);
	for (i = 1; i <= N_bitmaps; i++) {
		DiskBitmapHeader_d1_read(&bmh, d1_pig);
		if (!d_strnicmp(bmh.name, filename, 8))
			return i;
	}
	return -1;
}

// imitate the algorithm of gamedata_read_tbl in main/bmread.c
static void read_d1_tmap_nums_from_hog(PHYSFS_File *d1_pig)
{
#define LINEBUF_SIZE 600
	int reading_textures = 0;
	short texture_count = 0;
	int bitmaps_tbl_is_binary = 0;
	int i;

	auto &&[bitmaps, physfserr] = PHYSFSX_openReadBuffered("bitmaps.tbl");
	if (!bitmaps) {
		auto &&[bitmapbin, physfserr2] = PHYSFSX_openReadBuffered ("bitmaps.bin");
		if (!bitmapbin)
		{
			Warning("Failed to open \"bitmaps.tbl\", \"bitmaps.bin\": \"%s\", \"%s\".  Descent 1 textures will not be read.", PHYSFS_getErrorByCode(physfserr), PHYSFS_getErrorByCode(physfserr2));
			return;
		}
		bitmaps = std::move(bitmapbin);
		bitmaps_tbl_is_binary = 1;
	}

	d1_tmap_nums = std::make_unique<d1_tmap_nums_t>();
	d1_tmap_nums->fill(-1);

	for (PHYSFSX_gets_line_t<LINEBUF_SIZE> inputline; PHYSFSX_fgets (inputline, bitmaps);)
	{
		char *arg;

		if (bitmaps_tbl_is_binary)
			decode_text_line((inputline));
		else
			while (inputline[(i=strlen(inputline))-2]=='\\')
				PHYSFSX_fgets(inputline,bitmaps,i-2); // strip comments
		REMOVE_EOL(inputline);
                if (strchr(inputline, ';')!=NULL) REMOVE_COMMENTS(inputline);
		if (strlen(inputline) == LINEBUF_SIZE-1) {
			Warning("Possible line truncation in BITMAPS.TBL");
			return;
		}
		arg = strtok( inputline, space_tab );
                if (arg && arg[0] == '@') {
			arg++;
			//Registered_only = 1;
		}

                while (arg != NULL) {
			if (*arg == '$')
				reading_textures = 0; // default
			if (!strcmp(arg, "$TEXTURES")) // BM_TEXTURES
				reading_textures = 1;
			else if (! d_stricmp(arg, "$ECLIP") // BM_ECLIP
				   || ! d_stricmp(arg, "$WCLIP")) // BM_WCLIP
					texture_count++;
			else // not a special token, must be a bitmap!
				if (reading_textures) {
					while (*arg == '\t' || *arg == ' ')
						arg++;//remove unwanted blanks
					if (*arg == '\0')
						break;
					if (d1_tmap_num_unique(texture_count)) {
						int d1_index = get_d1_bm_index(arg, d1_pig);
						if (d1_index >= 0 && d1_index < D1_MAX_TMAP_NUM) {
							(*d1_tmap_nums)[d1_index] = texture_count;
							//int d2_index = d2_index_for_d1_index(d1_index);
						}
				}
				Assert (texture_count < D1_MAX_TEXTURES);
				texture_count++;
			}

			arg = strtok (NULL, equal_space);
		}
	}
}

/* If the given d1_index is the index of a bitmap we have to load
 * (because it is unique to descent 1), then returns the d2_index that
 * the given d1_index replaces.
 * Returns -1 if the given d1_index is not unique to descent 1.
 */
static short d2_index_for_d1_index(short d1_index)
{
	Assert(d1_index >= 0 && d1_index < D1_MAX_TMAP_NUM);
	if (! d1_tmap_nums || (*d1_tmap_nums)[d1_index] == -1
	    || ! d1_tmap_num_unique((*d1_tmap_nums)[d1_index]))
  		return -1;

	return Textures[convert_d1_tmap_num((*d1_tmap_nums)[d1_index])].index;
}

}

#define D1_BITMAPS_SIZE 300000
void load_d1_bitmap_replacements()
{
	DiskBitmapHeader bmh;
	int pig_data_start, bitmap_header_start, bitmap_data_start;
	int N_bitmaps;
	short d1_index, d2_index;
	uint8_t * next_bitmap;
	palette_array_t d1_palette;
	char *p;
	int pigsize;

	auto &&[d1_Piggy_fp, physfserr] = PHYSFSX_openReadBuffered(D1_PIGFILE);
#define D1_PIG_LOAD_FAILED "Failed loading " D1_PIGFILE
	if (!d1_Piggy_fp) {
		Warning("Failed to open " D1_PIGFILE ": %s", PHYSFS_getErrorByCode(physfserr));
		return;
	}

	//first, free up data allocated for old bitmaps
	free_bitmap_replacements();

	std::array<color_palette_index, 256> colormap;
	if (get_d1_colormap( d1_palette, colormap ) != 0)
		Warning("Could not load descent 1 color palette");

	pigsize = PHYSFS_fileLength(d1_Piggy_fp);
	switch (pigsize) {
	case D1_SHARE_BIG_PIGSIZE:
	case D1_SHARE_10_PIGSIZE:
	case D1_SHARE_PIGSIZE:
	case D1_10_BIG_PIGSIZE:
	case D1_10_PIGSIZE:
		pig_data_start = 0;
		// OK, now we need to read d1_tmap_nums by emulating d1's gamedata_read_tbl()
		read_d1_tmap_nums_from_hog(d1_Piggy_fp);
		break;
	default:
		Warning("Unknown size for " D1_PIGFILE);
		Int3();
		[[fallthrough]];
	case D1_PIGSIZE:
	case D1_OEM_PIGSIZE:
	case D1_MAC_PIGSIZE:
	case D1_MAC_SHARE_PIGSIZE:
		pig_data_start = PHYSFSX_readInt(d1_Piggy_fp );
		bm_read_d1_tmap_nums(d1_Piggy_fp); //was: bm_read_all_d1(fp);
		//for (i = 0; i < 1800; i++) GameBitmapXlat[i] = PHYSFSX_readShort(d1_Piggy_fp);
		break;
	}

	PHYSFSX_fseek( d1_Piggy_fp, pig_data_start, SEEK_SET );
	N_bitmaps = PHYSFSX_readInt(d1_Piggy_fp);
	{
		int N_sounds = PHYSFSX_readInt(d1_Piggy_fp);
		int header_size = N_bitmaps * DISKBITMAPHEADER_D1_SIZE
			+ N_sounds * sizeof(DiskSoundHeader);
		bitmap_header_start = pig_data_start + 2 * sizeof(int);
		bitmap_data_start = bitmap_header_start + header_size;
	}

	Bitmap_replacement_data = std::make_unique<ubyte[]>(D1_BITMAPS_SIZE);
	if (!Bitmap_replacement_data) {
		Warning(D1_PIG_LOAD_FAILED);
		return;
	}

	next_bitmap = Bitmap_replacement_data.get();

	for (d1_index = 1; d1_index <= N_bitmaps; d1_index++ ) {
		d2_index = d2_index_for_d1_index(d1_index);
		// only change bitmaps which are unique to d1
		if (d2_index != -1) {
			PHYSFSX_fseek(d1_Piggy_fp, bitmap_header_start + (d1_index-1) * DISKBITMAPHEADER_D1_SIZE, SEEK_SET);
			DiskBitmapHeader_d1_read(&bmh, d1_Piggy_fp);

			bitmap_read_d1( &GameBitmaps[d2_index], d1_Piggy_fp, bitmap_data_start, &bmh, &next_bitmap, d1_palette, colormap );
			Assert(next_bitmap - Bitmap_replacement_data.get() < D1_BITMAPS_SIZE);
			GameBitmapOffset[d2_index] = pig_bitmap_offset::None; // don't try to read bitmap from current d2 pigfile
			GameBitmapFlags[d2_index] = bmh.flags;

			auto &abname = AllBitmaps[d2_index].name;
			if ((p = strchr(abname.data(), '#')) /* d2 BM is animated */
			     && !(bmh.dflags & DBM_FLAG_ABM) ) { /* d1 bitmap is not animated */
				int i, len = p - abname.data();
				for (i = 0; i < Num_bitmap_files; i++)
					if (i != d2_index && ! memcmp(abname.data(), AllBitmaps[i].name.data(), len))
					{
						gr_set_bitmap_data(GameBitmaps[i], NULL);	// free ogl texture
						GameBitmaps[i] = GameBitmaps[d2_index];
						GameBitmapOffset[i] = pig_bitmap_offset::None;
						GameBitmapFlags[i] = bmh.flags;
					}
			}
		}
	}
	last_palette_loaded_pig[0]= 0;  //force pig re-load

	texmerge_flush();       //for re-merging with new textures
}

/*
 * Find and load the named bitmap from descent.pig
 * similar to read_extra_bitmap_iff
 */
grs_bitmap *read_extra_bitmap_d1_pig(const char *name, grs_bitmap &n)
{
	{
		int pig_data_start, bitmap_header_start, bitmap_data_start;
		int N_bitmaps;
		palette_array_t d1_palette;
		int pigsize;
		auto &&[d1_Piggy_fp, physfserr] = PHYSFSX_openReadBuffered(D1_PIGFILE);
		if (!d1_Piggy_fp)
		{
			Warning("Failed to open " D1_PIGFILE ": %s", PHYSFS_getErrorByCode(physfserr));
			return nullptr;
		}

		std::array<color_palette_index, 256> colormap;
		if (get_d1_colormap( d1_palette, colormap ) != 0)
			Warning("Could not load descent 1 color palette");

		pigsize = PHYSFS_fileLength(d1_Piggy_fp);
		switch (pigsize) {
		case D1_SHARE_BIG_PIGSIZE:
		case D1_SHARE_10_PIGSIZE:
		case D1_SHARE_PIGSIZE:
		case D1_10_BIG_PIGSIZE:
		case D1_10_PIGSIZE:
			pig_data_start = 0;
			break;
		default:
			Warning("Unknown size for " D1_PIGFILE);
			Int3();
			[[fallthrough]];
		case D1_PIGSIZE:
		case D1_OEM_PIGSIZE:
		case D1_MAC_PIGSIZE:
		case D1_MAC_SHARE_PIGSIZE:
			pig_data_start = PHYSFSX_readInt(d1_Piggy_fp );

			break;
		}

		PHYSFSX_fseek( d1_Piggy_fp, pig_data_start, SEEK_SET );
		N_bitmaps = PHYSFSX_readInt(d1_Piggy_fp);
		{
			int N_sounds = PHYSFSX_readInt(d1_Piggy_fp);
			int header_size = N_bitmaps * DISKBITMAPHEADER_D1_SIZE
				+ N_sounds * sizeof(DiskSoundHeader);
			bitmap_header_start = pig_data_start + 2 * sizeof(int);
			bitmap_data_start = bitmap_header_start + header_size;
		}

		for (unsigned i = 1;; ++i)
		{
			if (i > N_bitmaps)
			{
				con_printf(CON_DEBUG, "could not find bitmap %s", name);
				return nullptr;
			}
			DiskBitmapHeader bmh;
			DiskBitmapHeader_d1_read(&bmh, d1_Piggy_fp);
			if (!d_strnicmp(bmh.name, name, 8))
			{
				bitmap_read_d1(&n, d1_Piggy_fp, bitmap_data_start, &bmh, 0, d1_palette, colormap);
				break;
			}
		}
	}

#if !DXX_USE_OGL
	n.avg_color = 0;	//compute_average_pixel(n);
#endif
	return &n;
}

}
#endif

namespace dcx {
/*
 * reads a bitmap_index structure from a PHYSFS_File
 */
void bitmap_index_read(PHYSFS_File *fp, bitmap_index &bi)
{
	bi.index = PHYSFSX_readShort(fp);
}

/*
 * reads n bitmap_index structs from a PHYSFS_File
 */
void bitmap_index_read_n(PHYSFS_File *fp, const partial_range_t<bitmap_index *> r)
{
	range_for (auto &i, r)
		i.index = PHYSFSX_readShort(fp);
}
}
