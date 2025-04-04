#include <chenboot.h>
#include <sawpads.h>
#include <seedy.h>
#include "common.h"
#include <stdarg.h>

#define FRUSTUM_CULL 1
#define FRUSTUM_CULL_BLOCK 1
#define SPHERICAL_DISTANCE 1
#define MIPMAPPING 1
#define RENDER_DISTANCE_MIN 6
#define RENDER_DISTANCE_MAX 32
#define RENDER_DISTANCE_COOLDOWN VBLANKS_PER_SEC

#ifdef STANDALONE_EXE
#include "../obj/atlas.lz4.h"
#endif

/*

colouur multipliers for the keep guessing Nitori:
[-0.6, -0.5, -0.8]
[+0.6, +1.0, +0.8]

bounding box for Nitori
X,Z: 0.6
height: 1.8
eyes: 1.7

- first, then + ##### ZXY
*/

int main(void);
void yield(void);

const int16_t sintab[256] = {
0,101,201,301,401,501,601,700,799,897,995,1092,1189,1285,1380,1474,1567,1660,1751,1842,1931,2019,2106,2191,2276,2359,2440,2520,2598,2675,2751,2824,2896,2967,3035,3102,3166,3229,3290,3349,3406,3461,3513,3564,3612,3659,3703,3745,3784,3822,3857,3889,3920,3948,3973,3996,4017,4036,4052,4065,4076,4085,4091,4095,4096,4095,4091,4085,4076,4065,4052,4036,4017,3996,3973,3948,3920,3889,3857,3822,3784,3745,3703,3659,3612,3564,3513,3461,3406,3349,3290,3229,3166,3102,3035,2967,2896,2824,2751,2675,2598,2520,2440,2359,2276,2191,2106,2019,1931,1842,1751,1660,1567,1474,1380,1285,1189,1092,995,897,799,700,601,501,401,301,201,101,0,-101,-201,-301,-401,-501,-601,-700,-799,-897,-995,-1092,-1189,-1285,-1380,-1474,-1567,-1660,-1751,-1842,-1931,-2019,-2106,-2191,-2276,-2359,-2440,-2520,-2598,-2675,-2751,-2824,-2896,-2967,-3035,-3102,-3166,-3229,-3290,-3349,-3406,-3461,-3513,-3564,-3612,-3659,-3703,-3745,-3784,-3822,-3857,-3889,-3920,-3948,-3973,-3996,-4017,-4036,-4052,-4065,-4076,-4085,-4091,-4095,-4096,-4095,-4091,-4085,-4076,-4065,-4052,-4036,-4017,-3996,-3973,-3948,-3920,-3889,-3857,-3822,-3784,-3745,-3703,-3659,-3612,-3564,-3513,-3461,-3406,-3349,-3290,-3229,-3166,-3102,-3035,-2967,-2896,-2824,-2751,-2675,-2598,-2520,-2440,-2359,-2276,-2191,-2106,-2019,-1931,-1842,-1751,-1660,-1567,-1474,-1380,-1285,-1189,-1092,-995,-897,-799,-700,-601,-501,-401,-301,-201,-101,
};

extern const char license_text_txt[];
uint8_t fsys_level[LEVEL_LY][LEVEL_LZ][LEVEL_LX];
int render_distance = 12;
int last_rd_delta = 0;
int rd_delta_cooldown = 0;
FASTMEM int max_calced_di = 0;

#ifdef MIPMAPPING
uint32_t texture_to_color_map[128];
#endif

static void change_render_distance(int delta) {
	// prevent render distance from going below or above certain values
	if (render_distance <= RENDER_DISTANCE_MIN && delta < 0) return;
	if (render_distance >= RENDER_DISTANCE_MAX && delta > 0) return;
	// prevent oscillations
	if (last_rd_delta != delta && rd_delta_cooldown > 0) return;
	// ok
	last_rd_delta = delta;
	render_distance += delta;
	rd_delta_cooldown = RENDER_DISTANCE_COOLDOWN;
}

// Top, Side, Bottom, (reserved)
// Texcoord, TexPage, CLUT, (reserved)
FASTMEM uint8_t block_side_index[6];
FASTMEM uint32_t block_lighting[6];

static const uint8_t block_side_index_defaults[6] = {
	1, 1, 1, 1, 2, 0,
};
static const uint32_t block_lighting_defaults[6] = {
	0x010101*((0x80*8+5)/10), // -Z
	0x010101*((0x80*8+5)/10), // +Z
	0x010101*((0x80*6+5)/10), // -X
	0x010101*((0x80*6+5)/10), // +X
	0x010101*((0x80*5+5)/10), // -Y
	0x010101*((0x80*10+5)/10), // +Y
};

#include "block_info.h"

typedef struct mesh_data {
	int16_t x, y, z;
	int16_t tc;
	uint8_t face;
	uint8_t _unused1[7];
} mesh_data_t;

#include "meshes.h"

FASTMEM mesh_data_t mesh_data_block[24];

static void init_fastmem_tables(void)
{
	memcpy(block_side_index, block_side_index_defaults, sizeof(block_side_index));
	memcpy(block_lighting, block_lighting_defaults, sizeof(block_lighting));
	memcpy(mesh_data_block, mesh_data_block_defaults, sizeof(mesh_data_block));
}

void yield(void)
{
	// TODO: halt
}

extern char _end[];
void *_cur_brk = (void *)_end;
void __attribute__((externally_visible)) *sbrk (intptr_t incr)
{
	void *ret = _cur_brk;
	_cur_brk += incr;
	return ret;
}

int32_t mat_rt11, mat_rt12, mat_rt13;
int32_t mat_rt21, mat_rt22, mat_rt23;
int32_t mat_rt31, mat_rt32, mat_rt33;
int32_t mat_tr_x, mat_tr_y, mat_tr_z;
int32_t mat_hr11, mat_hr13;
int32_t mat_hr31, mat_hr33;

int32_t gte_ofx = 0x0000;
int32_t gte_ofy = 0x0000;
int32_t gte_zsf3 = 1;
int32_t gte_zsf4 = 1;

// FORMULA:
// gte_h = (VID_HEIGHT/2)/tan(fov_angle/2.0);

//int32_t gte_h = 289; // 45 deg
//int32_t gte_h = 208; // 60 deg
//int32_t gte_h = 156; // 75 deg
//int32_t gte_h = 120; // 90 deg
//int32_t gte_h = VID_HEIGHT/2; // 90 deg
//int32_t gte_h = 69; // 120 deg
//int32_t gte_h = 50; // 135 deg
//int32_t gte_h = 32; // 150 deg
int32_t gte_h = VID_HEIGHT/2;

int32_t cam_ry = 0x0000;
int32_t cam_rx = 0x0000;
int32_t cam_x = 0;
int32_t cam_y = 0;
int32_t cam_z = 0;
int32_t vel_x = 0;
int32_t vel_y = 0;
int32_t vel_z = 0;

#define MODE_INGAME 1
#define MODE_BLOCKSEL 2

int32_t joy_delay = 5;
int32_t mode = MODE_INGAME;
int16_t blocksel_id = 1;

uint32_t is_ticking = 0;
uint32_t ticks = 0;
uint32_t movement_ticks = 0;
uint32_t tex_update_ticks = 0;
uint32_t tex_update_blanks = 0;
uint32_t feet_sound_ticks = 0;
uint32_t vblank_counter_joy = 0;

#define OT_WORLD 2

int joy_buttons_old = 0;
int current_block[HOTBAR_MAX] = {0, 1, 4, 45, 18, 4, 3, 20, 8};
int hotbar_pos = 0;

int32_t fps_frames = 0;
int32_t fps_vblanks = 0;
int32_t fps_val = 0;
int32_t dma_size_used = 0;
uint32_t rendering_calc_cycles = 0;

static options_t options;

// ISR handler

chenboot_exception_frame_t *isr_handler_c(chenboot_exception_frame_t *sp)
{
	// If it's not an interrupt, spin
	while((sp->cause & 0x3C) != 0x00) {
	}

	if((PSXREG_I_STAT & (1<<0)) != 0) {
		// VBLANK
		vblank_counter++;
		vblank_counter_joy++;
		if (is_ticking > 0) {
			tex_update_blanks++;
			ticks++;
		}
		//
		PSXREG_I_STAT = ~(1<<0);
		cdrom_tick_vblank();
	}

	if((PSXREG_I_STAT & (1<<2)) != 0) {
		// CDROM
		PSXREG_I_STAT = ~(1<<2);
		cdrom_isr();
	}

	if((PSXREG_I_STAT & (1<<7)) != 0) {
		// JOY
		PSXREG_I_STAT = ~(1<<7);
		sawpads_isr_joy();
	}

	if((PSXREG_I_STAT & (1<<6)) != 0) {
		// TIMER2
		PSXREG_I_STAT = ~(1<<6);
		rendering_calc_cycles += 0x10000;
	}

	// Work around GTE bug
	if((sp->epc & 0xFE000000) == 0x4A000000) {
		sp->epc += 1; // skip op
	}

	return sp;
}

static inline bool is_face_lit(int32_t cx, int32_t cy, int32_t cz, uint32_t i) {
	switch (i) {
		case 0: return cz > 0 && world_get_top_opaque(cx, cz - 1) <= cy;
		case 2: return cx > 0 && world_get_top_opaque(cx - 1, cz) <= cy;
		case 1: return cz < LEVEL_LZ-1 && world_get_top_opaque(cx, cz + 1) <= cy;
		case 3: return cx < LEVEL_LX-1 && world_get_top_opaque(cx + 1, cz) <= cy;
		case 4: return world_get_top_opaque(cx, cz) < cy;
		case 5: return world_get_top_opaque(cx, cz) <= cy;
		default: return true;
	}
}

#ifdef MIPMAPPING
static int32_t mipmap_mask[] = {
	0xFFFF,
	0x7F7F,
	0x3F3F,
	0x1F1F,
	0x0F0F
};
#endif

static inline void draw_one_quad(
	uint32_t command, int di,
	int32_t x00, int32_t y00, int32_t z00, int32_t t00,
	int32_t x01, int32_t y01, int32_t z01, int32_t t01,
	int32_t x10, int32_t y10, int32_t z10, int32_t t10,
	int32_t x11, int32_t y11, int32_t z11, int32_t t11,
	int32_t cl, int32_t tp)
{
	int32_t xy00 = ((x00&0xFFFF)|(y00<<16));
	int32_t xy01 = ((x01&0xFFFF)|(y01<<16));
	int32_t xy10 = ((x10&0xFFFF)|(y10<<16));

	asm volatile ("mtc2 %0, $0\n" : "+r"(xy00) : : );
	asm volatile ("mtc2 %0, $1\n" : "+r"(z00) : : );
	asm volatile ("mtc2 %0, $2\n" : "+r"(xy01) : : );
	asm volatile ("mtc2 %0, $3\n" : "+r"(z01) : : );
	asm volatile ("mtc2 %0, $4\n" : "+r"(xy10) : : );
	asm volatile ("mtc2 %0, $5\n" : "+r"(z10) : : );

	// Apply transformation
	asm volatile ("cop2 0x00280030\nnop\n" :::); // RTPT

	// Determine face
	asm volatile ("cop2 0x01400006\nnop\n" :::); // NCLIP
	int32_t backface_mac0;
	asm volatile ("mfc2 %0, $24\nnop\n" : "=r"(backface_mac0) : : );
	// Backface cull
	if(backface_mac0 > 0) { return; }

	// Get transformed vertices
	uint32_t sxy00;
	uint32_t sxy01;
	uint32_t sxy10;
	uint32_t sxy11;
	asm volatile ("mfc2 %0, $12\nnop\n" : "=r"(sxy00) : : );
	asm volatile ("mfc2 %0, $13\nnop\n" : "=r"(sxy01) : : );
	asm volatile ("mfc2 %0, $14\nnop\n" : "=r"(sxy10) : : );

#if 0
	// Clamp oversize polys
	if(((int16_t)(sxy0>>16)) < -512) { continue; }
	if(((int16_t)(sxy0>>16)) > 512) { continue; }
	if(((int16_t)(sxy0&0xFFFF)) < -512) { continue; }
	if(((int16_t)(sxy0&0xFFFF)) > 512) { continue; }
	if(((int16_t)(sxy1>>16)) < -512) { continue; }
	if(((int16_t)(sxy1>>16)) > 512) { continue; }
	if(((int16_t)(sxy1&0xFFFF)) < -512) { continue; }
	if(((int16_t)(sxy1&0xFFFF)) > 512) { continue; }
	if(((int16_t)(sxy2>>16)) < -512) { continue; }
	if(((int16_t)(sxy2>>16)) > 512) { continue; }
	if(((int16_t)(sxy2&0xFFFF)) < -512) { continue; }
	if(((int16_t)(sxy2&0xFFFF)) > 512) { continue; }
#endif

	// Apply 4th point
	int32_t xy11 = ((x11&0xFFFF)|(y11<<16));
	asm volatile ("mtc2 %0, $0\n" : "+r"(xy11) : : );
	asm volatile ("mtc2 %0, $1\n" : "+r"(z11) : : );
	asm volatile ("nop\n");
	asm volatile ("cop2 0x00180001\nnop\n" :::); // RTPS
	asm volatile ("mfc2 %0, $14\nnop\n" : "=r"(sxy11) : : );

#if 0
	if(((int16_t)(sxy3>>16)) < -512) { continue; }
	if(((int16_t)(sxy3>>16)) > 512) { continue; }
	if(((int16_t)(sxy3&0xFFFF)) < -512) { continue; }
	if(((int16_t)(sxy3&0xFFFF)) > 512) { continue; }
#endif

	// Draw a quad
	DMA_PUSH(9, OT_WORLD + di);
	dma_buffer[dma_pos++] = command;
	dma_buffer[dma_pos++] = (sxy00);
	dma_buffer[dma_pos++] = (t00) | (cl<<16);
	dma_buffer[dma_pos++] = (sxy01);
	dma_buffer[dma_pos++] = (t01) | (tp<<16);
	dma_buffer[dma_pos++] = (sxy10);
	dma_buffer[dma_pos++] = (t10);
	dma_buffer[dma_pos++] = (sxy11);
	dma_buffer[dma_pos++] = (t11);

}

static inline void draw_one_quad_notex(
	uint32_t command, int di,
	int32_t x00, int32_t y00, int32_t z00,
	int32_t x01, int32_t y01, int32_t z01,
	int32_t x10, int32_t y10, int32_t z10,
	int32_t x11, int32_t y11, int32_t z11,
	uint32_t color)
{
	int32_t xy00 = ((x00&0xFFFF)|(y00<<16));
	int32_t xy01 = ((x01&0xFFFF)|(y01<<16));
	int32_t xy10 = ((x10&0xFFFF)|(y10<<16));

	asm volatile ("mtc2 %0, $0\n" : "+r"(xy00) : : );
	asm volatile ("mtc2 %0, $1\n" : "+r"(z00) : : );
	asm volatile ("mtc2 %0, $2\n" : "+r"(xy01) : : );
	asm volatile ("mtc2 %0, $3\n" : "+r"(z01) : : );
	asm volatile ("mtc2 %0, $4\n" : "+r"(xy10) : : );
	asm volatile ("mtc2 %0, $5\n" : "+r"(z10) : : );

	// Apply transformation
	asm volatile ("cop2 0x00280030\nnop\n" :::); // RTPT

	// Determine face
	asm volatile ("cop2 0x01400006\nnop\n" :::); // NCLIP
	int32_t backface_mac0;
	asm volatile ("mfc2 %0, $24\nnop\n" : "=r"(backface_mac0) : : );
	// Backface cull
	if(backface_mac0 > 0) { return; }

	// Get transformed vertices
	uint32_t sxy00;
	uint32_t sxy01;
	uint32_t sxy10;
	uint32_t sxy11;
	asm volatile ("mfc2 %0, $12\nnop\n" : "=r"(sxy00) : : );
	asm volatile ("mfc2 %0, $13\nnop\n" : "=r"(sxy01) : : );
	asm volatile ("mfc2 %0, $14\nnop\n" : "=r"(sxy10) : : );

	// Apply 4th point
	int32_t xy11 = ((x11&0xFFFF)|(y11<<16));
	asm volatile ("mtc2 %0, $0\n" : "+r"(xy11) : : );
	asm volatile ("mtc2 %0, $1\n" : "+r"(z11) : : );
	asm volatile ("nop\n");
	asm volatile ("cop2 0x00180001\nnop\n" :::); // RTPS
	asm volatile ("mfc2 %0, $14\nnop\n" : "=r"(sxy11) : : );

	// Draw a quad
	uint32_t nc = (command & 0xFF000000);
	nc |= (((command & 0xFF) * (color & 0xFF00FF)) >> 7) & 0xFF00FF;
	nc |= (((command & 0xFF) * (color & 0xFF00)) >> 7) & 0xFF00;

	DMA_PUSH(5, OT_WORLD + di);
	dma_buffer[dma_pos++] = nc;
	dma_buffer[dma_pos++] = (sxy00);
	dma_buffer[dma_pos++] = (sxy01);
	dma_buffer[dma_pos++] = (sxy10);
	dma_buffer[dma_pos++] = (sxy11);

}

void draw_quads(int32_t cx, int32_t cy, int32_t cz, int di, const mesh_data_t *mesh_data, const block_info_t *bi, int face_count, uint32_t facemask, bool semitrans)
{
	int32_t ox = cx*0x0100;
	int32_t oy = cy*0x0100;
	int32_t oz = cz*0x0100;
	int mi = 0;

	for (int i = 0; i < face_count; i++, mi+=4) {
		if(((1<<mesh_data[mi+0].face)&facemask) == 0) {
			continue;
		}

		const block_info_t *block_data = &bi[mesh_data[mi+0].face & 0x07];
#ifdef MIPMAPPING
		int32_t miplevel = (facemask&0x80) ? ((di - 9) >> 3) : 0;
#endif

		int32_t x00 = ox+mesh_data[mi+0].x;
		int32_t y00 = oy+mesh_data[mi+0].y;
		int32_t z00 = oz+mesh_data[mi+0].z;
		int32_t x01 = ox+mesh_data[mi+1].x;
		int32_t y01 = oy+mesh_data[mi+1].y;
		int32_t z01 = oz+mesh_data[mi+1].z;
		int32_t x10 = ox+mesh_data[mi+2].x;
		int32_t y10 = oy+mesh_data[mi+2].y;
		int32_t z10 = oz+mesh_data[mi+2].z;
		int32_t x11 = ox+mesh_data[mi+3].x;
		int32_t y11 = oy+mesh_data[mi+3].y;
		int32_t z11 = oz+mesh_data[mi+3].z;

		uint32_t lighting = block_lighting[i];
		if(!is_face_lit(cx, cy, cz, i)) {
			lighting >>= 1;
			lighting &= 0x7F7F7F;
		} else {
			lighting &= 0xFFFFFF;
		}


#ifdef MIPMAPPING
		if (miplevel >= 3) {
			uint32_t command;
			if(semitrans) {
				command = 0x2A000000 | lighting;
			} else {
				command = 0x28000000 | lighting;
			}
			draw_one_quad_notex(
				command, di,
				x00, y00, z00,
				x01, y01, z01,
				x10, y10, z10,
				x11, y11, z11,
				texture_to_color_map[block_data->idx]);
			continue;
		}
#endif

		int32_t t00 = mesh_data[mi+0].tc+block_data->tc;
		int32_t t01 = mesh_data[mi+1].tc+block_data->tc;
		int32_t t10 = mesh_data[mi+2].tc+block_data->tc;
		int32_t t11 = mesh_data[mi+3].tc+block_data->tc;
		int32_t cl = block_data->cl;
		int32_t tp = block_data->tp;

#ifdef MIPMAPPING
		if (miplevel > 0) {
			int32_t svm = mipmap_mask[miplevel];
			t00 = (t00 >> miplevel) & svm;
			t01 = (t01 >> miplevel) & svm;
			t10 = (t10 >> miplevel) & svm;
			t11 = (t11 >> miplevel) & svm;
		}
#endif

		uint32_t command;
		if(semitrans) {
			command = 0x2E000000 | lighting;
		} else {
			command = 0x2C000000 | lighting;
		}

		draw_one_quad(
			command, di,
			x00, y00, z00, t00,
			x01, y01, z01, t01,
			x10, y10, z10, t10,
			x11, y11, z11, t11,
			cl, tp);
	}
}

static int get_model(int block) {
       if (block == 44) return 2;
       return (block == 6) || (block >= 37 && block <= 40) ? 1 : 0;
}

void draw_block(int32_t cx, int32_t cy, int32_t cz, int di, int block, uint32_t facemask, bool transparent)
{
	switch (get_model(block)) {
		case 0:
#ifdef MIPMAPPING
			if (block != 18 && block != 20) {
				facemask |= 0x80;
			}
#endif
			draw_quads(cx, cy, cz, di, mesh_data_block, block_info[block], 6, facemask, transparent || ((block&(~1)) == 8));
			break;
		case 1:
			draw_quads(cx, cy, cz, di, mesh_data_plant, block_info[block], 4, 0xFF7F, false);
			break;
		case 2:
#ifdef MIPMAPPING
			facemask |= 0x80;
#endif
			draw_quads(cx, cy, cz, di, mesh_data_slab, block_info[block], 6, facemask, transparent);
			break;
	}
}

#ifdef SPHERICAL_DISTANCE
static inline int get_block_render_distance(int32_t dx, int32_t dy, int32_t dz) {
        int x = (dx + dy + dz) << 7;

	asm volatile ("mtc2 %0, $9\n" : "+r"(dx) : : );
	asm volatile ("mtc2 %0, $10\n" : "+r"(dy) : : );
	asm volatile ("mtc2 %0, $11\n" : "+r"(dz) : : );
	asm volatile ("cop2 0x00A00428\nnop\n" :::); // SQR
	asm volatile ("mfc2 %0, $9\nnop\n" : "=r"(dx) : : );
	asm volatile ("mfc2 %0, $10\nnop\n" : "=r"(dy) : : );
	asm volatile ("mfc2 %0, $11\nnop\n" : "=r"(dz) : : );

        int xs = ((dx + dy + dz) * 9) << 12;

        x = x - (((x*x) - xs) / (x<<1));
        x = x - (((x*x) - xs) / (x<<1));
        return ((x + 32) >> 6);
}
#else
#define get_block_render_distance(dx, dy, dz) ((dx)+(dy)+(dz))
#endif

inline void draw_block_in_level(int32_t cx, int32_t cy, int32_t cz, int32_t di, uint32_t nfmask)
{
#ifdef SPHERICAL_DISTANCE
	if (di > max_calced_di) return;
#endif
	int block = world_get_block_unsafe(cx, cy, cz);
	switch (block) {
		case 0:
			return;
		case 6: case 37: case 38: case 39: case 40:
			draw_block(cx, cy, cz, di, block, 0xFF7F, false);
			return;
		default: {
			uint32_t fmask = world_get_render_faces_unsafe(cx, cy, cz) & nfmask;
			if(fmask != 0) {
				draw_block(cx, cy, cz, di, block, fmask | 0xFF00, false);
			}
		} return;
	}
}

inline void draw_blocks_in_range(int32_t cam_cx, int32_t cam_cy, int32_t cam_cz, int32_t cd)
{
	int dymin = - cd;
	int dymax = + cd;
	int cymin = cam_cy + dymin;
	int cymax = cam_cy + dymax;
	if(cymin < 0) { dymin += -cymin; }
	if(cymax > LEVEL_LY) { dymax += LEVEL_LY-cymax; }
	for(int dy = dymin; dy <= dymax; dy++) {
		int cy = cam_cy + dy;
		//if(cy < 0 || cy >= LEVEL_LY) { continue; }
		int ady = (dy < 0 ? -dy : dy);
		uint32_t nfmask = 0xFF00;
		if(dy > 0) { nfmask |= 0x10; }
		if(dy < 0) { nfmask |= 0x20; }

		int dzmin = - (cd-ady);
		int dzmax = + (cd-ady);
		int czmin = cam_cz + dzmin;
		int czmax = cam_cz + dzmax;
		if(czmin < 0) { dzmin += -czmin; }
		if(czmax > LEVEL_LZ) { dzmax += LEVEL_LZ-1-czmax; }

	for(int dz = dzmin; dz <= dzmax; dz++) {
		int cz = cam_cz + dz;
		//if(cz < 0 || cz >= LEVEL_LZ) { continue; }
		int adz = (dz < 0 ? -dz : dz);
		nfmask &= ~0x03;
		if(dz > 0) { nfmask |= 0x01; }
		if(dz < 0) { nfmask |= 0x02; }

		int adx = cd-adz-ady;
		if(adx >= 0) {
			int cx1 = cam_cx - adx;
			int cx2 = cam_cx + adx;
			if(cx1 >= 0 && cx1 < LEVEL_LX) {
				draw_block_in_level(cx1, cy, cz, cd, nfmask|0x08);
			}
			if(cx2 != cx1 && cx2 >= 0 && cx2 < LEVEL_LX) {
				draw_block_in_level(cx2, cy, cz, cd, nfmask|0x04);
			}
		}
	}
	}
}

void draw_world(void)
{
	int cam_cx = cam_x >> 8;
	int cam_cy = cam_y >> 8;
	int cam_cz = cam_z >> 8;
	int cam_real_dist = render_distance;
	int cd = (cam_real_dist + 3) & (~3);
	int cdy = cd;
	/*
	for(int cd = cam_dist; cd != 0; cd--) {
		draw_blocks_in_range(cam_cx, cam_cy, cam_cz, cd);
	}
	*/

	max_calced_di = get_block_render_distance(cam_real_dist, 0, 0) + 1;

	int cymin = cam_cy - cdy;
	int cymax = cam_cy + cdy;
	if(cymin < 0) { cymin = 0; }
	if(cymax >= LEVEL_LY) { cymax = LEVEL_LY-1; }

	int czmin = cam_cz - cd;
	int czmax = cam_cz + cd;
	if(czmin < 0) { czmin = 0; }
	if(czmax >= LEVEL_LZ) { czmax = LEVEL_LZ-1; }

	int cxmin = cam_cx - cd;
	int cxmax = cam_cx + cd;
	if(cxmin < 0) { cxmin = 0; }
	if(cxmax >= LEVEL_LX) { cxmax = LEVEL_LX-1; }

	cymin = (cymin+3)&~3;
	cymax = (cymax+0)&~3;
	czmin = (czmin+3)&~3;
	czmax = (czmax+0)&~3;
	cxmin = (cxmin+3)&~3;
	cxmax = (cxmax+0)&~3;

	int xrange = ((cxmax+4-cxmin)>>2);
	int yrange = ((cymax+4-cymin)>>2);
	int zrange = ((czmax+4-czmin)>>2);

#if FRUSTUM_CULL
	// Frustum cull centre sphere distances
	const int32_t cull_centre_chunk = 0x376cf6; // (0x200<<12) * sqrt(3)
	const int32_t cull_centre_block = 0xddb3e; // (0x80<<12) * sqrt(3)

	// Frustum cull planes
	int xmul = gte_h;
	int xdiv = VID_WIDTH/2;
	int ymul = gte_h;
	int ydiv = VID_HEIGHT/2;
	int cull0xstep_raw = mat_rt31+mat_rt21*ymul/ydiv;
	int cull0ystep_raw = mat_rt32+mat_rt22*ymul/ydiv;
	int cull0zstep_raw = mat_rt33+mat_rt23*ymul/ydiv;
	int cull1xstep_raw = mat_rt31-mat_rt21*ymul/ydiv;
	int cull1ystep_raw = mat_rt32-mat_rt22*ymul/ydiv;
	int cull1zstep_raw = mat_rt33-mat_rt23*ymul/ydiv;
	int cull2xstep_raw = mat_rt31+mat_rt11*xmul/xdiv;
	int cull2ystep_raw = mat_rt32+mat_rt12*xmul/xdiv;
	int cull2zstep_raw = mat_rt33+mat_rt13*xmul/xdiv;
	int cull3xstep_raw = mat_rt31-mat_rt11*xmul/xdiv;
	int cull3ystep_raw = mat_rt32-mat_rt12*xmul/xdiv;
	int cull3zstep_raw = mat_rt33-mat_rt13*xmul/xdiv;

#define CULL_PLANE_SETUP(X) \
		int cull##X##xstep1 = cull##X##xstep_raw<<8; \
		int cull##X##ystep1 = cull##X##ystep_raw<<8; \
		int cull##X##zstep1 = cull##X##zstep_raw<<8; \
		int cull##X##xstep1_5 = (cull##X##xstep1*3+1)>>1; \
		int cull##X##ystep1_5 = (cull##X##ystep1*3+1)>>1; \
		int cull##X##zstep1_5 = (cull##X##zstep1*3+1)>>1; \
		int cull##X##xstep4 = cull##X##xstep1*4; \
		int cull##X##ystep4 = cull##X##ystep1*4; \
		int cull##X##zstep4 = cull##X##zstep1*4; \
		int cull##X##xmin = ((cxmin<<8)+0x200-cam_x)*cull##X##xstep_raw; \
		int cull##X##ymin = ((cymin<<8)+0x200-cam_y)*cull##X##ystep_raw; \
		int cull##X##zmin = ((czmin<<8)+0x200-cam_z)*cull##X##zstep_raw; \
 \
		int cull##X##yrange = yrange*cull##X##ystep4; \
		int bcull##X##yrange = 4*cull##X##ystep1; \
		cull##X##ystep4 -= zrange*cull##X##zstep4; \
		cull##X##zstep4 -= xrange*cull##X##xstep4; \
		cull##X##ystep1 -= 4*cull##X##zstep1; \
		cull##X##zstep1 -= 4*cull##X##xstep1; \
 \
		int cull##X##step1_5 = cull##X##ystep1_5+cull##X##zstep1_5+cull##X##xstep1_5; \
		int cull##X = cull##X##ymin+cull##X##zmin+cull##X##xmin; \

	CULL_PLANE_SETUP(0)
	CULL_PLANE_SETUP(1)
	CULL_PLANE_SETUP(2)
	CULL_PLANE_SETUP(3)
#endif

	for(int iy = 0, cy = cymin, dy = cymin-cam_cy;
		iy < yrange; //cy < cymax+1;
		iy++, cy+=4, dy+=4
#if FRUSTUM_CULL
		,cull0 += cull0ystep4
		,cull1 += cull1ystep4
		,cull2 += cull2ystep4
		,cull3 += cull3ystep4
#endif
		) {
	for(int iz = 0, cz = czmin, dz = czmin-cam_cz;
		iz < zrange; //cz < czmax+1;
		iz++, cz+=4, dz+=4
#if FRUSTUM_CULL
		,cull0 += cull0zstep4
		,cull1 += cull1zstep4
		,cull2 += cull2zstep4
		,cull3 += cull3zstep4
#endif
		) {
	for(int ix = 0, cx = cxmin, dx = cxmin-cam_cx;
		ix < xrange; //cx < cxmax+1;
		ix++, cx+=4, dx+=4
#if FRUSTUM_CULL
		,cull0 += cull0xstep4
		,cull1 += cull1xstep4
		,cull2 += cull2xstep4
		,cull3 += cull3xstep4
#endif
		) {

#if FRUSTUM_CULL
		// Frustum culling
		if(cull0 < -cull_centre_chunk) { continue; }
		if(cull1 < -cull_centre_chunk) { continue; }
		if(cull2 < -cull_centre_chunk) { continue; }
		if(cull3 < -cull_centre_chunk) { continue; }
#endif

		uint32_t vismask = world_get_vis_blocks_unsafe(cx, cy, cz);
		if(vismask == 0) {
			continue;
		}

#if FRUSTUM_CULL && FRUSTUM_CULL_BLOCK
		int bcull0 = cull0 - cull0step1_5;
		int bcull1 = cull1 - cull1step1_5;
		int bcull2 = cull2 - cull2step1_5;
		int bcull3 = cull3 - cull3step1_5;

		int min_bcull01 = (bcull0 < bcull1 ? bcull0 : bcull1);
		int min_bcull23 = (bcull2 < bcull3 ? bcull2 : bcull3);
		int min_bcull = (min_bcull01 < min_bcull23 ? min_bcull01 : min_bcull23);

		if(min_bcull > cull_centre_chunk) {
#endif
			// Chunk fully in frustum

			for(int biy = 0, bcy = cy, bdy = dy;
				biy < 4;
				biy++, bcy++, bdy++
				) {

				if((vismask & 15) == 0) {
					if (vismask == 0) break;
					vismask >>= 4;
					continue;
				}
				int ady = (bdy < 0 ? -bdy : bdy);
				if (ady > cam_real_dist) continue;
				uint32_t nfmask = 0xFF00;
				if     (bdy > 0) { nfmask |= 0x10; }
				else if(bdy < 0) { nfmask |= 0x20; }
			for(int biz = 0, bcz = cz, bdz = dz;
				biz < 4;
				biz++, bcz++, bdz++
				, vismask>>=1
				) {

				if((vismask & 1) == 0) {
					if (vismask == 0) { biy = 4; break; }
					continue;
				}
				int adz = (bdz < 0 ? -bdz : bdz);
				if (adz > cam_real_dist) continue;
				nfmask &= ~0x03;
				if     (bdz > 0) { nfmask |= 0x01; }
				else if(bdz < 0) { nfmask |= 0x02; }
			for(int bix = 0, bcx = cx, bdx = dx;
				bix < 4;
				bix++, bcx++, bdx++
				) {

				int adx = (bdx < 0 ? -bdx : bdx);
				if (adx > cam_real_dist) continue;
				nfmask &= ~0x0C;
				if(bdx > 0)      { nfmask |= 0x04; }
				else if(bdx < 0) { nfmask |= 0x08; }
				draw_block_in_level(bcx, bcy, bcz, get_block_render_distance(adx, ady, adz), nfmask);
			}
			}
			}

#if FRUSTUM_CULL && FRUSTUM_CULL_BLOCK
		} else {
			// Blocks may need culling

			for(int biy = 0, bcy = cy, bdy = dy;
				biy < 4;
				biy++, bcy++, bdy++
				,bcull0 += cull0ystep1
				,bcull1 += cull1ystep1
				,bcull2 += cull2ystep1
				,bcull3 += cull3ystep1
				) {

				if((vismask & 15) == 0) {
					if (vismask == 0) break;
					bcull0 += (cull0zstep1<<2) + (cull0xstep1<<4);
					bcull1 += (cull1zstep1<<2) + (cull1xstep1<<4);
					bcull2 += (cull2zstep1<<2) + (cull2xstep1<<4);
					bcull3 += (cull3zstep1<<2) + (cull3xstep1<<4);
					vismask >>= 4;
					continue;
				}
				int ady = (bdy < 0 ? -bdy : bdy);
				if (ady > cam_real_dist) continue;
				uint32_t nfmask = 0xFF00;
				if     (bdy > 0) { nfmask |= 0x10; }
				else if(bdy < 0) { nfmask |= 0x20; }
			for(int biz = 0, bcz = cz, bdz = dz;
				biz < 4;
				biz++, bcz++, bdz++
				, vismask>>=1
				,bcull0 += cull0zstep1
				,bcull1 += cull1zstep1
				,bcull2 += cull2zstep1
				,bcull3 += cull3zstep1
				) {

				if((vismask & 1) == 0) {
					if (vismask == 0) { biy = 4; break; }
					bcull0 += cull0xstep1<<2;
					bcull1 += cull1xstep1<<2;
					bcull2 += cull2xstep1<<2;
					bcull3 += cull3xstep1<<2;
					continue;
				}
				int adz = (bdz < 0 ? -bdz : bdz);
				if (adz > cam_real_dist) continue;
				nfmask &= ~0x03;
				if     (bdz > 0) { nfmask |= 0x01; }
				else if(bdz < 0) { nfmask |= 0x02; }
			for(int bix = 0, bcx = cx, bdx = dx;
				bix < 4;
				bix++, bcx++, bdx++
				,bcull0 += cull0xstep1
				,bcull1 += cull1xstep1
				,bcull2 += cull2xstep1
				,bcull3 += cull3xstep1
				) {

				// Frustum culling
				if(bcull0 < -(0x200<<12)) { continue; }
				if(bcull1 < -(0x200<<12)) { continue; }
				if(bcull2 < -(0x200<<12)) { continue; }
				if(bcull3 < -(0x200<<12)) { continue; }

				int adx = (bdx < 0 ? -bdx : bdx);
				if (adx > cam_real_dist) continue;
				nfmask &= ~0x0C;
				if(bdx > 0)      { nfmask |= 0x04; }
				else if(bdx < 0) { nfmask |= 0x08; }
				draw_block_in_level(bcx, bcy, bcz, get_block_render_distance(adx, ady, adz), nfmask);
			}
			}
			}
		}
#endif
	}
	}
	}
}

static inline bool try_move(int32_t dx, int32_t dy, int32_t dz, bool do_move) {
	// camera pos is eye height
	int32_t ncx = cam_x + dx;
	int32_t ncy = cam_y + dy;
	int32_t ncz = cam_z + dz;

	if (!world_is_colliding_fixed(ncx - 77, ncy - 435, ncz - 77, ncx + 77, ncy + 26, ncz + 77)) {
		if(do_move) {
			cam_x = ncx;
			cam_y = ncy;
			cam_z = ncz;
		}
		return true;
	} else {
		return false;
	}
}

static void setup_matrix(bool with_y) {
	int yang = (cam_ry+0x80)>>8;
	int xang = with_y ? (cam_rx+0x80)>>8 : 0;
	int32_t rxc = sintab[(xang+0x40)&0xFF];
	int32_t rxs = sintab[(xang+0x00)&0xFF];
	int32_t ryc = sintab[(yang+0x40)&0xFF];
	int32_t rys = sintab[(yang+0x00)&0xFF];

	mat_rt11 =  ryc;
	mat_rt12 =  0x0000;
	mat_rt13 = -rys;
	mat_rt21 =  -((rys*rxs+0x800)>>12);
	mat_rt22 =  -(rxc);
	mat_rt23 =  -((ryc*rxs+0x800)>>12);
	mat_rt31 =  ((rys*rxc+0x800)>>12);
	mat_rt32 = -rxs;
	mat_rt33 =  ((ryc*rxc+0x800)>>12);

	// Adjust pixel ratio
#if VID_WIDTH != 320
	mat_rt11 = (mat_rt11*VID_WIDTH + 320/2)/320;
	mat_rt12 = (mat_rt12*VID_WIDTH + 320/2)/320;
	mat_rt13 = (mat_rt13*VID_WIDTH + 320/2)/320;
#endif

	mat_hr11 =  ryc;
	mat_hr13 = -rys;
	mat_hr31 =  rys;
	mat_hr33 =  ryc;
}

bool block_is_in_liquid_unsafe(int cx, int cy, int cz)
{
	int32_t cb = world_get_block_unsafe(cx, cy, cz);
	return cb >= 8 && cb <= 11;
}

bool player_is_in_liquid(void)
{
	int32_t cam_cx = cam_x >> 8;
	int32_t cam_cy = cam_y >> 8;
	int32_t cam_cz = cam_z >> 8;

	if(cam_cx < 0 || cam_cy < 0 || cam_cz < 0 || cam_cx >= LEVEL_LX || cam_cy > LEVEL_LY || cam_cz >= LEVEL_LZ) {
		return false;
	}

	if(cam_cy > 0 && block_is_in_liquid_unsafe(cam_cx, cam_cy-1, cam_cz)) {
		return true;
	} else if(cam_cy < LEVEL_LY && block_is_in_liquid_unsafe(cam_cx, cam_cy, cam_cz)) {
		return true;
	} else {
		return false;
	}
}

void draw_everything(void)
{
	// Set up GTE parameters
	asm volatile ("ctc2 %0, $24\nnop\n" : "+r"(gte_ofx) : : );
	asm volatile ("ctc2 %0, $25\nnop\n" : "+r"(gte_ofy) : : );
	asm volatile ("ctc2 %0, $26\nnop\n" : "+r"(gte_h) : : );
	asm volatile ("ctc2 %0, $29\nnop\n" : "+r"(gte_zsf3) : : );
	asm volatile ("ctc2 %0, $30\nnop\n" : "+r"(gte_zsf4) : : );

	// Set up GTE matrix
	setup_matrix(true);

	mat_tr_x = -(cam_x*mat_rt11 + cam_y*mat_rt12 + cam_z*mat_rt13 + 0x800)>>12;
	mat_tr_y = -(cam_x*mat_rt21 + cam_y*mat_rt22 + cam_z*mat_rt23 + 0x800)>>12;
	mat_tr_z = -(cam_x*mat_rt31 + cam_y*mat_rt32 + cam_z*mat_rt33 + 0x800)>>12;

	uint32_t mat_rtp1 = (mat_rt11&0xFFFF)|(mat_rt12<<16);
	uint32_t mat_rtp2 = (mat_rt13&0xFFFF)|(mat_rt21<<16);
	uint32_t mat_rtp3 = (mat_rt22&0xFFFF)|(mat_rt23<<16);
	uint32_t mat_rtp4 = (mat_rt31&0xFFFF)|(mat_rt32<<16);
	uint32_t mat_rtp5 = (mat_rt33);

	asm volatile ("ctc2 %0, $0\n" : "+r"(mat_rtp1) : : );
	asm volatile ("ctc2 %0, $1\n" : "+r"(mat_rtp2) : : );
	asm volatile ("ctc2 %0, $2\n" : "+r"(mat_rtp3) : : );
	asm volatile ("ctc2 %0, $3\n" : "+r"(mat_rtp4) : : );
	asm volatile ("ctc2 %0, $4\n" : "+r"(mat_rtp5) : : );
	asm volatile ("ctc2 %0, $5\n" : "+r"(mat_tr_x) : : );
	asm volatile ("ctc2 %0, $6\n" : "+r"(mat_tr_y) : : );
	asm volatile ("ctc2 %0, $7\n" : "+r"(mat_tr_z) : : );

	// Set up DMA buffer
	gpu_dma_init();

	// Clear screen
	DMA_PUSH(8, DMA_ORDER_MAX-1);
	dma_buffer[dma_pos++] = 0x38FFD0B7;
	dma_buffer[dma_pos++] = ((-(VID_WIDTH/2))&0xFFFF)|((-(VID_HEIGHT/2))<<16);
	dma_buffer[dma_pos++] = 0x00FFD0B7;
	dma_buffer[dma_pos++] = ((+(VID_WIDTH/2))&0xFFFF)|((-(VID_HEIGHT/2))<<16);
	dma_buffer[dma_pos++] = 0x00FFF4E0;
	dma_buffer[dma_pos++] = ((-(VID_WIDTH/2))&0xFFFF)|((+(VID_HEIGHT/2))<<16);
	dma_buffer[dma_pos++] = 0x00FFF4E0;
	dma_buffer[dma_pos++] = ((+(VID_WIDTH/2))&0xFFFF)|((+(VID_HEIGHT/2))<<16);

	// Set FOV
	switch (options.fov_mode) {
		case 0: gte_h = 208; break;
		case 1: default: gte_h = VID_HEIGHT/2; break;
		case 2: gte_h = 69; break;
	}

	// Send VERY FIRST COMMANDS
	frame_start();

	// Load mesh data into GTE
	rendering_calc_cycles = 0;
	PSXREG_Tn_MODE(2) = 2 | (1 << 6) | (1 << 5) | (2 << 8);
	draw_world();
	rendering_calc_cycles = (rendering_calc_cycles + (PSXREG_Tn_VALUE(2) & 0xFFFF)) << 3;
	PSXREG_Tn_MODE(2) = (2 << 8);

	// Draw fog
#ifdef FOG_ENABLED
	if (options.fog_on) {
		DMA_PUSH(8, OT_WORLD + (max_calced_di * 15 / 16));
		dma_buffer[dma_pos++] = 0x3AFFD0B7;
		dma_buffer[dma_pos++] = ((-(VID_WIDTH/2))&0xFFFF)|((-(VID_HEIGHT/2))<<16);
		dma_buffer[dma_pos++] = 0x00FFD0B7;
		dma_buffer[dma_pos++] = ((+(VID_WIDTH/2))&0xFFFF)|((-(VID_HEIGHT/2))<<16);
		dma_buffer[dma_pos++] = 0x00FFF4E0;
		dma_buffer[dma_pos++] = ((-(VID_WIDTH/2))&0xFFFF)|((+(VID_HEIGHT/2))<<16);
		dma_buffer[dma_pos++] = 0x00FFF4E0;
		dma_buffer[dma_pos++] = ((+(VID_WIDTH/2))&0xFFFF)|((+(VID_HEIGHT/2))<<16);
	}
#endif

	int32_t cam_cx = cam_x >> 8;
	int32_t cam_cy = cam_y >> 8;
	int32_t cam_cz = cam_z >> 8;

	switch (mode) {
		case MODE_BLOCKSEL:
			draw_block_sel_menu(blocksel_id, block_sel_slots, sizeof(block_sel_slots));
			break;
		case MODE_INGAME:
			draw_current_block();
			draw_crosshair();
			break;
	}

	draw_hotbar();
	draw_text(TEXT_BORDER_X, TEXT_BORDER_Y, 0xFFFFFF, "0.30");
	if (options.debug_mode) {
		draw_text(TEXT_BORDER_X, TEXT_BORDER_Y + 10, 0xFFFFFF, "%d FPS, BS: %d, RT: %d", fps_val, dma_size_used, rendering_calc_cycles);
		draw_text(TEXT_BORDER_X, TEXT_BORDER_Y + 20, 0xFFFFFF, "RD: %d, J: %02X%02X%02X%02X", render_distance, sawpads_controller[0].axes[0], sawpads_controller[0].axes[1],
			sawpads_controller[0].axes[2], sawpads_controller[0].axes[3]);
		draw_text(TEXT_BORDER_X, TEXT_BORDER_Y + 30, 0xFFFFFF, "XYZ: %d.%02x / %d.%02x / %d.%02x",
			cam_x >> 8,
			cam_x & 0xFF,
			cam_y >> 8,
			cam_y & 0xFF,
			cam_z >> 8,
			cam_z & 0xFF
		);
	} else {
		if (options.show_fps) {
			draw_text(TEXT_BORDER_X, TEXT_BORDER_Y + 10, 0xFFFFFF, "%d FPS", fps_val);
		}
	}

	draw_liquid_overlay();

	/*
	gp0_command(0x720000FF);
	gp0_data_xy(rx1-8/2, ry1-8/2);
	gp0_command(0x72FFFFFF);
	gp0_data_xy(rx0-8/2, ry0-8/2);
	*/

	dma_size_used = gpu_dma_finish();

	switch (options.render_distance) {
		case 0:
			if (dma_size_used > 3500) {
				change_render_distance(-1);
			} else if (dma_size_used < 2500) {
				change_render_distance(+1);
			}
			break;
		case 1:
		default:
			if (dma_size_used > 8500) {
				change_render_distance(-1);
			} else if (dma_size_used < 5500) {
				change_render_distance(+1);
			}
			break;
		case 2:
			if (dma_size_used > 13000) {
				change_render_distance(-1);
			} else if (dma_size_used < 9000) {
				change_render_distance(+1);
			}
			break;
		case 3:
			if (dma_size_used > 19500) {
				change_render_distance(-1);
			} else if (dma_size_used < 14500) {
				change_render_distance(+1);
			}
			break;
	}
}

static int joy_has_analogs(void)
{
	return !(sawpads_controller[0].id == 0x41 && sawpads_controller[0].hid == 0x5A);
}

static int blocksel_jx0 = 0;
static int blocksel_jy0 = 0;
#define BLOCKSEL_JSTEP 0x280

void blocksel_update(int mmul)
{
	if (joy_has_analogs())
	{
		blocksel_jx0 += (int)(int8_t)(sawpads_controller[0].axes[2]) * mmul;
		blocksel_jy0 += (int)(int8_t)(sawpads_controller[0].axes[3]) * mmul;

		while (blocksel_jx0 < -BLOCKSEL_JSTEP) {
			blocksel_jx0 += BLOCKSEL_JSTEP;
			if ((blocksel_id % 9) == 0) blocksel_id += 8;
			else blocksel_id--;
		}

		while (blocksel_jx0 > BLOCKSEL_JSTEP) {
			blocksel_jx0 -= BLOCKSEL_JSTEP;
			if ((blocksel_id % 9) == 8) blocksel_id -= 8;
			else blocksel_id++;
		}

		while (blocksel_jy0 < -BLOCKSEL_JSTEP) {
			blocksel_jy0 += BLOCKSEL_JSTEP;
			blocksel_id -= 9;
		}

		while (blocksel_jy0 > BLOCKSEL_JSTEP) {
			blocksel_jy0 -= BLOCKSEL_JSTEP;
			blocksel_id += 9;
		}
	}

	if (joy_pressed != 0) {
		if ((joy_pressed & (PAD_T | PAD_S | PAD_O | PAD_X)) != 0) {
			mode = MODE_INGAME;
			return;
		}

		if ((joy_pressed & PAD_UP) != 0) blocksel_id -= 9;
		if ((joy_pressed & PAD_LEFT) != 0) {
			if ((blocksel_id % 9) == 0) blocksel_id += 8;
			else blocksel_id--;
		}
		if ((joy_pressed & PAD_RIGHT) != 0) {
			if ((blocksel_id % 9) == 8) blocksel_id -= 8;
			else blocksel_id++;
		}
		if ((joy_pressed & PAD_DOWN) != 0) blocksel_id += 9;
	}

	while (blocksel_id < 0) blocksel_id += sizeof(block_sel_slots);
	while (blocksel_id >= (int16_t)sizeof(block_sel_slots)) blocksel_id -= sizeof(block_sel_slots);

	current_block[hotbar_pos] = block_sel_slots[blocksel_id];
}

void draw_status_prog_frame(int progress, int max) {
	gpu_dma_init();
	draw_status_progress(progress, max);
	gpu_dma_finish();
}

void wgen_stage_frame(const char* format) {
	gpu_dma_init();
	frame_start();
	draw_status_progress(0, 1);
	draw_status_window(0, format);
	gpu_dma_finish();
	frame_flip();
}

void world_main_prepare(void)
{
	gpu_dma_init();
	frame_start();
	draw_status_window(0, "Building terrain");
	gpu_dma_finish();
	frame_flip();

	vel_x = vel_y = vel_z = 0;
	world_init();
	wait_for_next_vblank();
}

void world_main_load(int slot)
{
	gpu_dma_init();
	frame_start();
	draw_status_window(0, "Loading level");
	gpu_dma_finish();
	frame_flip();

	level_info info;
	// fill with placeholders
	info.options = options;

	int ret = load_level(slot, &info, fsys_level, LEVEL_LX*LEVEL_LY*LEVEL_LZ, draw_status_prog_frame);
	if (ret >= 0) {
		cam_x = info.cam_x;
		cam_y = info.cam_y;
		cam_z = info.cam_z;
		cam_rx = info.cam_rx;
		cam_ry = info.cam_ry;
		memcpy(current_block, info.hotbar_blocks, HOTBAR_MAX);
		hotbar_pos = info.hotbar_pos;
		options = info.options;
		world_main_prepare();
	} else {
		// TODO: add error msgs
		gpu_dma_init();
		frame_start();
		draw_status_window(0, "Error: %s", save_get_error_string(ret));
		gpu_dma_finish();
		frame_flip();
		wait_for_vblanks(90);
	}
}

void world_main_save(int slot)
{
	gpu_dma_init();
	frame_start();
	draw_status_window(0, "Saving level..");
	gpu_dma_finish();
	frame_flip();

	level_info info;
	info.xsize = LEVEL_LX;
	info.ysize = LEVEL_LY;
	info.zsize = LEVEL_LZ;
	info.cam_x = cam_x;
	info.cam_y = cam_y;
	info.cam_z = cam_z;
	info.cam_rx = cam_rx;
	info.cam_ry = cam_ry;
	memcpy(info.hotbar_blocks, current_block, HOTBAR_MAX);
	info.hotbar_pos = hotbar_pos;
	info.options = options;

	int ret = save_level(slot, &info, fsys_level, draw_status_prog_frame);
	gpu_dma_init();
	frame_start();
	if (ret >= 0) {
		draw_status_window(0, "Level saved!");
	} else {
		draw_status_window(0, "Error: %s", save_get_error_string(ret));
	}
	gpu_dma_finish();
	frame_flip();
	// TODO FIXME: The cache at Y=10 corrupts every time we save a level?
	// At least on Mednafen it does...
	world_init();
	wait_for_vblanks(30);
}

void world_main_generate(int mode)
{
	memset(fsys_level,0,LEVEL_LX*LEVEL_LY*LEVEL_LZ);

	world_generate(mode, fsys_level, LEVEL_LX, LEVEL_LY, LEVEL_LZ, (*(volatile uint32_t *)0x1F801120), wgen_stage_frame, draw_status_prog_frame);

	cam_ry = 0x0000;
	cam_rx = 0x0000;
	cam_x = 0x80 + ((LEVEL_LX>>1)<<8);
	cam_y = 0x80 + (LEVEL_LY<<8);
	for (int i = LEVEL_LY - 1; i >= 0; i--) {
		if (!world_is_walkable(world_get_block(LEVEL_LX>>1, i, LEVEL_LZ>>1))) {
			cam_y = 0x80 + ((i+3)<<8);
			break;
		}
	}
	cam_z = 0x80 + ((LEVEL_LZ>>1)<<8);

	world_main_prepare();
}

void player_update(int mmul)
{
	int jx0 = 0x00;
	int jy0 = 0x00;
	int jx1 = 0x00;
	int jy1 = 0x00;

	int prev_feet_x = cam_x >> 8;
	int prev_feet_y = (cam_y - 443) >> 8;
	int prev_feet_z = cam_z >> 8;
	int prev_vel_y = vel_y;

	int has_mouse = sawpads_controller[1].id == 0x12 && sawpads_controller[1].hid == 0x5A;
	int has_analogs = joy_has_analogs();

	if (joy_delay > 0) {
		joy_delay--;
		return;
	}

	if ((joy_pressed & PAD_START) != 0) {
		int is_menu_open = 1;
		is_ticking = 0;

#ifdef SAVING_ENABLED
		while (is_menu_open) switch (gui_menu(7, 0, "Options", "Generate new level", "Save level..", "Load level..", NULL, "Credits", "Back to game")) {
#else
		while (is_menu_open) switch (gui_menu(5, 0, "Options", "Generate new level", NULL, "Credits", "Back to game")) {
#endif
			case 0:
				if (gui_options_menu(&options)) is_menu_open = 0;
				break;
			case 1: {
				int wgen_mode = gui_worldgen_menu();
				if (wgen_mode < 0) break;
				world_main_generate(wgen_mode);
				is_menu_open = 0; break;
			} break;
#ifdef SAVING_ENABLED
			case 2: {
				int slot = gui_menu(6, 0, "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Cancel");
				if (slot < 0 || slot >= 5) break;
				world_main_save(slot+1);
				is_menu_open = 0; break;
			}
			case 3: {
				int slot = gui_menu(6, 0, "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Cancel");
				if (slot < 0 || slot >= 5) break;
				world_main_load(slot+1);
				is_menu_open = 0; break;
			}
			case 5: {
#else
			case 3: {
#endif
				gui_terrible_text_viewer(license_text_txt);
				is_menu_open = 0; break;
			}
			default:
				is_menu_open = 0;
				break;
		}
		joy_delay = 5;
		is_ticking = 1;
		return;
	}

	int use_dpad = options.move_dpad || !has_analogs;
	if (use_dpad) {
		if (!has_analogs && ((sawpads_controller[0].buttons & PAD_T) == 0)) {
			if ((sawpads_controller[0].buttons & PAD_UP) == 0) jy1 = -0x3F;
			if ((sawpads_controller[0].buttons & PAD_DOWN) == 0) jy1 = 0x3F;
			if ((sawpads_controller[0].buttons & PAD_LEFT) == 0) jx1 = -0x3F;
			if ((sawpads_controller[0].buttons & PAD_RIGHT) == 0) jx1 = 0x3F;
		} else {
			if ((sawpads_controller[0].buttons & PAD_UP) == 0) jy0 = -0x7F;
			if ((sawpads_controller[0].buttons & PAD_DOWN) == 0) jy0 = 0x7F;
			if ((sawpads_controller[0].buttons & PAD_LEFT) == 0) jx0 = -0x7F;
			if ((sawpads_controller[0].buttons & PAD_RIGHT) == 0) jx0 = 0x7F;
		}
	} else {
		jx0 = (int)(int8_t)(sawpads_controller[0].axes[2]);
		jy0 = (int)(int8_t)(sawpads_controller[0].axes[3]);
		jx1 = (int)(int8_t)(sawpads_controller[0].axes[0]);
		jy1 = (int)(int8_t)(sawpads_controller[0].axes[1]);

//		if (has_mouse) {
//			jx1 += (int)(int8_t)(sawpads_controller[1].axes[0]);
//			jy1 += (int)(int8_t)(sawpads_controller[1].axes[1]);
//		}

	}

	if (jx1 != 0) {
		cam_ry += (jx1<<3) * mmul;
		while (cam_ry < -0x8000) cam_ry += 0x10000;
		while (cam_ry > 0x8000) cam_ry -= 0x10000;
	}

	if (jy1 != 0) {
		cam_rx += (jy1<<3) * mmul;
		if (cam_rx < -0x4000) cam_rx = -0x4000;
		if (cam_rx > 0x4000) cam_rx = 0x4000;
	}

	if ((joy_pressed & PAD_R2) != 0 || (has_mouse && (joy_pressed & (PAD_MOUSE_R << 16)) != 0)) {
		int32_t sel_cx = -1;
		int32_t sel_cy = -1;
		int32_t sel_cz = -1;
		int32_t sel_valid = world_cast_ray(
			cam_x, cam_y, cam_z,
			mat_rt31, mat_rt32, mat_rt33,
			&sel_cx, &sel_cy, &sel_cz,
			10, false);

		if (sel_valid >= 0 && world_get_block(sel_cx, sel_cy, sel_cz) != 0) {
			int32_t bl = world_get_block(sel_cx, sel_cy, sel_cz);
			if (bl == 46) {
				for (int32_t cdx = -2; cdx <= 2; cdx++)
				for (int32_t cdy = -2; cdy <= 2; cdy++)
				for (int32_t cdz = -2; cdz <= 2; cdz++)
					world_set_block(sel_cx+cdx, sel_cy+cdy, sel_cz+cdz, 0, 1);
			} else {
				world_set_block(sel_cx, sel_cy, sel_cz, 0, 1);
			}
			if (!world_is_walkable(bl)) {
				if (options.sound_on) {
					sound_play(sound_get_id(bl), 0x3FFF, 0x3FFF);
				}
			}
		}
	}

	if ((joy_pressed & PAD_O) != 0) {
		int32_t sel_cx = -1;
		int32_t sel_cy = -1;
		int32_t sel_cz = -1;
		int32_t sel_valid = world_cast_ray(
			cam_x, cam_y, cam_z,
			mat_rt31, mat_rt32, mat_rt33,
			&sel_cx, &sel_cy, &sel_cz,
			10, false);

		if (sel_valid >= 0) {
			int32_t sel_block = world_get_block(sel_cx, sel_cy, sel_cz);
			for (int i = 0; i <= HOTBAR_MAX; i++) {
				if (i == HOTBAR_MAX)
					current_block[hotbar_pos] = world_get_block(sel_cx, sel_cy, sel_cz);
				else if (current_block[i] == sel_block) {
					hotbar_pos = i;
					break;
				}

			}
		}
	}

	if ((joy_pressed & PAD_L2) != 0 || (has_mouse && (joy_pressed & (PAD_MOUSE_L << 16)) != 0)) {
		int32_t sel_cx = -1;
		int32_t sel_cy = -1;
		int32_t sel_cz = -1;
		int32_t sel_valid = world_cast_ray(
			cam_x, cam_y, cam_z,
			mat_rt31, mat_rt32, mat_rt33,
			&sel_cx, &sel_cy, &sel_cz,
			10, true);

		if (sel_valid >= 0 && try_move(0, 0, 0, false)) {
			int32_t b_set = 0;

			if (sel_valid == FACE_YP && current_block[hotbar_pos] == 44) {
				int sel_cb = world_get_block(sel_cx, sel_cy - 1, sel_cz);
				if (sel_cb == 44) {
					world_set_block(sel_cx, sel_cy - 1, sel_cz, 43, 1);
					b_set = 1;
				}
			}

			if (b_set == 0) {
				world_set_block(sel_cx, sel_cy, sel_cz, current_block[hotbar_pos], 1);
			}

			if (!world_is_walkable(current_block[hotbar_pos])) {
				if (options.sound_on) {
					sound_play(sound_get_id(current_block[hotbar_pos]), 0x3FFF, 0x3FFF);
				}
			}
		}
	}

	if ((joy_pressed & PAD_S) != 0) {
		blocksel_id = 0;
		for (uint32_t i = 0; i < sizeof(block_sel_slots); i++) {
			if (current_block[hotbar_pos] == block_sel_slots[i]) {
				blocksel_id = i;
				break;
			}
		}
		mode = MODE_BLOCKSEL;
	}

	if ((joy_pressed & PAD_L1) != 0) { hotbar_pos--; if (hotbar_pos < 0) hotbar_pos = HOTBAR_MAX-1; }
	if ((joy_pressed & PAD_R1) != 0) hotbar_pos = (hotbar_pos + 1) % HOTBAR_MAX;
	setup_matrix(false);
	int32_t lvx = jx0 >> 1;
	int32_t lvy = 0;
	int32_t lvz = -(jy0 >> 1);

	if ((sawpads_controller[0].buttons & PAD_R3) == 0) { } else { lvx >>= 1; lvz >>= 1; }

#if 0
	int32_t gvx = 0;
	int32_t gvy = 0;
	int32_t gvz = 0;
	gvx = (lvx*mat_rt11 + lvy*mat_rt21 + lvz*mat_rt31 + 0x800)>>12;
	gvy = (lvx*mat_rt12 + lvy*mat_rt22 + lvz*mat_rt32 + 0x800)>>12;
	gvz = (lvx*mat_rt13 + lvy*mat_rt23 + lvz*mat_rt33 + 0x800)>>12;
	cam_x += gvx;
	cam_y += gvy;
	cam_z += gvz;
#else
	int32_t gvx = 0;
	int32_t gvz = 0;
	gvx = (lvx*mat_hr11 + lvz*mat_hr31 + 0x800)>>12;
	gvz = (lvx*mat_hr13 + lvz*mat_hr33 + 0x800)>>12;
	if (options.pro_jumps) {
		int acc_x = gvx;
		int acc_z = gvz;

		// Get normalised accel vector
		int nacc_x = acc_x;
		int nacc_z = acc_z;
		int nacc_len = 1;
		while(nacc_len*nacc_len < nacc_x*nacc_x + nacc_z*nacc_z) {
			nacc_len <<= 1;
		}
		nacc_len >>= 1;
		while(nacc_len*nacc_len < nacc_x*nacc_x + nacc_z*nacc_z) {
			nacc_len++;
		}
		nacc_len--;
		nacc_x = (nacc_x<<12) / nacc_len;
		nacc_z = (nacc_z<<12) / nacc_len;

		// Cap acceleration
		if(nacc_len > (0x1000>>6)) {
			acc_x = (nacc_x+(1<<5))>>6;
			acc_z = (nacc_z+(1<<5))>>6;
		}

		// Apply friction
		if (!try_move(0, -16, 0, false)) {
			vel_x -= ((vel_x+1)>>1) + (vel_x>>31);
			vel_z -= ((vel_z+1)>>1) + (vel_z>>31);
		}

		// Get speed cap
		int speed_cap_dot = vel_x*nacc_x + vel_z*nacc_z;

		int speed_cap = 0x100;
		if(speed_cap_dot < speed_cap*speed_cap) {
			vel_x += acc_x;
			vel_z += acc_z;
		}
	} else {
		vel_x = (vel_x + gvx) / 3;
		vel_z = (vel_z + gvz) / 3;
	}

	bool in_liquid = player_is_in_liquid();

	if (mmul >= 1 && (sawpads_controller[0].buttons & PAD_X) == 0) {
		if (in_liquid || !try_move(0, -16, 0, false)) {
			if (options.pro_jumps) {
				vel_y = in_liquid ? 48 : 96;
			} else {
				vel_y = in_liquid ? 32 : 64;
			}
		}
	}

	for (int i = 0; i < mmul; i++) {
		if (vel_y == 0 || try_move(0, vel_y, 0, true)) {
			if (vel_y > -192) vel_y -= ((ABS(vel_y) >> 4) + 4) >> (in_liquid ? 2 : 0);
			if (vel_y < -192) vel_y = -192;
		} else if (vel_y > 0) {
			vel_y = 0;
		} else if (vel_y < 0) {
			vel_y /= 2;
			while (!try_move(0, vel_y, 0, true)) {
				if(vel_y == 0) {
					while (vel_y < 128 && !try_move(0, vel_y, 0, false)) {
						vel_y += 4;
					}
					cam_y += (vel_y>>1)+1;
					vel_y = 0;
					break;
				}
				vel_y /= 2;
			}
		}

		if (vel_x == 0 && vel_z == 0) {
			// pass
		} else if (try_move(vel_x, 0, vel_z, true)) {
			// pass
		} else if (try_move(vel_x, 128, vel_z, true)) {
			// pass
		} else {
			while (vel_x != 0 && !try_move(vel_x, 0, 0, true)) {
				vel_x /= 2;
			}
			while (vel_z != 0 && !try_move(0, 0, vel_z, true)) {
				vel_z /= 2;
			}
		}
	}

	int feet_x = cam_x >> 8;
	int feet_y = (cam_y - 443) >> 8;
	int feet_z = cam_z >> 8;
	int force_feet_sound = (prev_vel_y < -8 && vel_y == 0);

	feet_sound_ticks += mmul*3;
	if (force_feet_sound || prev_feet_x != feet_x || prev_feet_y != feet_y || prev_feet_z != feet_z) {
		int bid = world_get_block(feet_x, feet_y, feet_z);
		if (force_feet_sound) feet_sound_ticks = VBLANKS_PER_SEC;
		if (!world_is_walkable(bid) && feet_sound_ticks >= VBLANKS_PER_SEC) {
			feet_sound_ticks %= VBLANKS_PER_SEC;
			if (options.sound_on) {
				sound_play(sound_get_id(bid), 0x1FFF, 0x1FFF);
			}
		}
	}
#endif
}

static void load_texture_atlas(uint8_t *cmp_data, int cmp_data_len)
{
	uint8_t *decmp_data = lz4_alloc_and_unpack(cmp_data, cmp_data_len, (320/4) * 256 * 2 + (128 * 4));
#ifdef MIPMAPPING
	memcpy(texture_to_color_map, decmp_data, 128 * 4);
#endif
	gpu_dma_load(decmp_data + 512, 768, 256, 320/4, 256, 0);
	free(decmp_data);
}

int main(void)
{
	int i;
	int x, y, xc, yc;

	// Disable DMA
	PSXREG_DPCR &= ~0x08888888;

	// Configure ISR
	chenboot_isr_disable();
	PSXREG_I_MASK = 0;
	PSXREG_I_STAT = 0;
	chenboot_isr_install(isr_handler_c);
	chenboot_isr_enable();

	// Enable GTE
	asm volatile (
		"\tmfc0 $t0, $12\n"
		"\tlui $t1, 0x4000\n"
		"\tori $t1, $t1, 0x7F01\n"
		"\tor $t0, $t0, $t1\n"
		"\tmtc0 $t0, $12\n"

		:::"t0","t1"
	);

	// Reset GPU
	gp1_command(0x00000000); // Reset
	//gp1_command(0x04000001); // DMA mode: FIFO (1)
	gp1_command(0x04000002); // DMA mode: DMA to GPU (2)
	gp1_command(0x05000000 | ((0)<<0) | ((0)<<10)); // Display start (x,y)
#if VID_WIDTH == 640
	gp1_command(0x06000000 | ((0x260)<<0) | ((0x260+VID_WIDTH*4)<<12)); // X screen range
#elif VID_WIDTH == 320
	gp1_command(0x06000000 | ((0x260)<<0) | ((0x260+VID_WIDTH*8)<<12)); // X screen range
#else
#error "Given VID_WIDTH not supported - cannot set a valid video mode!"
#endif

#ifdef TV_PAL
	gp1_command(0x07000000 | ((0xA3-VID_HEIGHT/2)<<0) | ((0xA3+VID_HEIGHT/2)<<10)); // Y screen range
#else
	gp1_command(0x07000000 | ((0x88-VID_HEIGHT/2)<<0) | ((0x88+VID_HEIGHT/2)<<10)); // Y screen range
#endif

	// Set display mode
	gp1_command(0x08000000
#if VID_WIDTH == 640
		| (3<<0) | (0<<6) // X resolution (640)
#elif VID_WIDTH == 320
		| (1<<0) | (0<<6) // X resolution (320)
#else
#error "Given VID_WIDTH not supported - cannot set a valid video mode!"
#endif
#if VID_HEIGHT == 240
		| (0<<2) | (0<<5) // Y resolution / interlace (240/OFF)
#else
#error "Given VID_HEIGHT not supported - cannot set a valid video mode!"
#endif
#ifdef TV_PAL
		| (1<<3) // TV standard (PAL)
#else
		| (0<<3) // TV standard (NTSC)
#endif
		| (0<<4) // Colour depth (15bpp)
	);

	// Set up drawing modes
	gp0_command(0xE1000600); // Texpage
	gp0_command(0xE2000000); // Texwindow
	gp0_command(0xE3000000 | ((0)<<0) | ((0)<<10)); // XY1 draw range
	gp0_command(0xE4000000 | ((VID_WIDTH-1)<<0) | ((VID_HEIGHT-1)<<10)); // XY2 draw range
	gp0_command(0xE5000000 | ((VID_WIDTH/2)<<0) | ((VID_HEIGHT/2)<<11)); // Draw offset
	//gp0_command(0xE5000000 | ((0)<<0) | ((0)<<10)); // Draw offset
	gp0_command(0xE6000000); // Mask bit setting

	// Display enable: OFF (1)
	gp1_command(0x03000001);

	// Clear screen
	gp0_command(0x01000000);
	gp0_command(0x02000000);
	gp0_data(((0)<<0) | ((0)<<16)); // X/Y
	gp0_data(((VID_WIDTH)<<0) | ((VID_HEIGHT)<<16)); // W/H

	// Enable VBLANK interrupt
	vblank_counter = 0;
	PSXREG_I_MASK |= (1<<0);

	// DMA a texture
	gpu_dma_load((uint32_t*) (&font_raw[64]), 64 * 0xE, 256, 128*VID_WIDTH_MULTIPLIER/4, 64, 0);

	// Write font CLUT
	gp1_command(0x04000001); // DMA mode: FIFO (1)
	gp0_command(0xA0000000);
	gp0_data_xy(768 + 320/4, 384);
	gp0_data_xy(2, 1);
	gp0_data(0xFFFF0000); // semi-transparency toggled via command
	gp0_command(0x01000000);
	gp1_command(0x04000002); // DMA mode: DMA to GPU (2)

	// Draw initial message
	gpu_dma_init();
	frame_start();
	
	draw_status_window(1, "Initializing..");
	gpu_dma_finish();
	frame_flip();

	// Display enable: ON (1)
	gp1_command(0x03000000);
	init_fastmem_tables();
	wait_for_next_vblank();

	// Prepare options
	memset(&options, 0, sizeof(options_t));
	options.render_distance = 1;
	options.sound_on = 1;
	options.music_on = 1;
#ifdef FOG_ENABLED
	options.fog_on = 1;
#endif
	options.fov_mode = 1;

	// Prepare joypad
	PSXREG_JOY_CTRL = 0x0010;
	PSXREG_JOY_MODE = 0x000D;
	PSXREG_JOY_BAUD = 0x0088;

	// Enable joypad ISR
	PSXREG_I_STAT = ~(1<<7);
	PSXREG_I_MASK |= (1<<7);

//	sawpads_unlock_dualshock();

	// Initialize CD data
#ifndef STANDALONE_EXE
	cdrom_init(draw_status_prog_frame);
#endif

	// Load CD assets
	draw_status_prog_frame(0, 2);
#ifndef STANDALONE_EXE
	sound_init();
	draw_status_prog_frame(1, 2);
#endif
	{
#ifndef STANDALONE_EXE
		file_record_t *atlas_file = cdrom_get_file("ATLAS.LZ4");
		uint8_t *atlas_lz4 = malloc(atlas_file->size);
		cdrom_read_record(atlas_file, atlas_lz4);
		load_texture_atlas(atlas_lz4, atlas_file->size);
		free(atlas_lz4);
#else
		load_texture_atlas(atlas_lz4, sizeof(atlas_lz4));
#endif
	}
	draw_status_prog_frame(2, 2);

	if (!cdrom_has_songs()) {
		seedy_drive_stop();
	}

#ifdef STANDALONE_EXE
	while (vblank_counter < VBLANKS_PER_SEC*1) { }
#endif

	sawpads_do_read();
	options.debug_mode = ((sawpads_controller[0].buttons & PAD_SELECT) == 0) ? 1 : 0;
//	options.debug_mode = 1;

	// Ensure notice is displayed for long enough
	while (vblank_counter < VBLANKS_PER_SEC*8) {
		sawpads_do_read();
		if (sawpads_controller[0].buttons != 0xFFFF) {
			break;
		}
		wait_for_next_vblank();
		gpu_dma_init();
		draw_text(TEXT_BORDER_X, VID_HEIGHT - 10 - TEXT_BORDER_Y, 0xFFFFFF, "Press any button to continue");
		gpu_dma_finish();
	}

    int is_menu_open = 1;
		is_ticking = 0;

#ifdef SAVING_ENABLED
		while (is_menu_open) switch (gui_menu(7, 0, "Options", "Generate new level", "Save level..", "Load level..", NULL, "Credits", "Back to game")) {
#else
		while (is_menu_open) switch (gui_menu(5, 0, "Options", "Generate new level", NULL, "Credits", "Back to game")) {
#endif
			case 0:
				if (gui_options_menu(&options)) is_menu_open = 0;
				break;
			case 1: {
				int wgen_mode = gui_worldgen_menu();
				if (wgen_mode < 0) break;
				world_main_generate(wgen_mode);
				is_menu_open = 0; break;
			} break;
#ifdef SAVING_ENABLED
			case 2: {
				int slot = gui_menu(6, 0, "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Cancel");
				if (slot < 0 || slot >= 5) break;
				world_main_save(slot+1);
				is_menu_open = 0; break;
			}
			case 3: {
				int slot = gui_menu(6, 0, "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Cancel");
				if (slot < 0 || slot >= 5) break;
				world_main_load(slot+1);
				is_menu_open = 0; break;
			}
			case 5: {
#else
			case 3: {
#endif
				gui_terrible_text_viewer(license_text_txt);
				is_menu_open = 0; break;
			}
			default:
				is_menu_open = 0;
				break;
		}
		joy_delay = 5;
		is_ticking = 1;

	ticks = movement_ticks = 0;
	for(;;) {
		yield();

		if(vblank_counter > 0) {
			uint32_t frames_to_run = vblank_counter;
			//vblank_counter -= frames_to_run;

			is_ticking = 1;
			draw_everything();

			rd_delta_cooldown -= vblank_counter;
			if (rd_delta_cooldown < 0) rd_delta_cooldown = 0;

			fps_frames++;
			fps_vblanks += vblank_counter;
			if (fps_vblanks >= VBLANKS_PER_SEC) {
				fps_val = fps_frames * VBLANKS_PER_SEC / fps_vblanks;
				fps_frames = 0;
				fps_vblanks = 0;
			}

			// count the vblanks and reset counter
			vblank_counter = 0;
			int mmul = ticks - movement_ticks;
			movement_ticks = ticks;
#if defined(LAVA_ANIMATION_START) || defined(WATER_ANIMATION_START)
			uint32_t last_tut = tex_update_ticks;
			while (tex_update_blanks >= VBLANKS_PER_SEC/10) {
				tex_update_blanks -= VBLANKS_PER_SEC/10;
				tex_update_ticks++;
			}
			if (tex_update_ticks != last_tut) {
				int water_tex = (tex_update_ticks % WATER_ANIMATION_FRAMES) + WATER_ANIMATION_START - (8<<4);
				int lava_tex = ((tex_update_ticks >> 1) % LAVA_ANIMATION_FRAMES) + LAVA_ANIMATION_START - (8<<4);
//				if (lava_tex >= LAVA_ANIMATION_FRAMES) lava_tex = ((LAVA_ANIMATION_FRAMES * 2 - 2) - lava_tex);
//				lava_tex += LAVA_ANIMATION_START;
				for (int i = 0; i < 6; i++) {
					block_info[8][i] = (block_info_t)QUAD(water_tex & 15, water_tex >> 4);
					block_info[9][i] = (block_info_t)QUAD(water_tex & 15, water_tex >> 4);
					block_info[10][i] = (block_info_t)QUAD(lava_tex & 15, lava_tex >> 4);
					block_info[11][i] = (block_info_t)QUAD(lava_tex & 15, lava_tex >> 4);
				}
			}
#endif

			switch (mode) {
				case MODE_INGAME:
					player_update(mmul);
					break;
				case MODE_BLOCKSEL:
					blocksel_update(mmul);
					break;
			}

			world_update(ticks, &vblank_counter);
			cdrom_tick_song_player(mmul, options.music_on);
			frame_flip_nosync();
			if (vblank_counter_joy > 0) {
				joy_update(vblank_counter_joy, 1);
				vblank_counter_joy = 0;
			}
		}
	}

	return 0;
}

