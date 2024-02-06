
#pragma once
#ifndef _SIMPLIFY_MOD_TEX_H
#define _SIMPLIFY_MOD_TEX_H

#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <cstdio>

#include "meshoptimizer_mod.h"

/*uv data and position data are seperated, two simplicifation remap will be generated.*/
size_t meshopt_simplify_mod_texDebug(unsigned int* destination_pos, unsigned int* destination_uv, unsigned int* simplification_remap, 
        unsigned int* simplification_remap_uv, const unsigned int* indices_pos, size_t index_count, 
		const unsigned int* indices_uv, size_t index_uv_count, const float* vertex_positions_data, size_t vertex_count, const float* uv_data,
		size_t uv_count, int vertex_positions_stride, int uv_stride, size_t target_index_count, 
		float target_error, float* out_result_error, float block_extend, float block_bottom[3]);

#endif