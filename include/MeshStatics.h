#pragma once

#include "mesh_simplify/meshoptimizer.h"
#include <cfloat>
#include <cstring>
#include <stdint.h>

void meshopt_statistics(const char *name, const float* positions, const float *normals, const float *uvs, const float *positions_parent,
						const unsigned int *idx, const unsigned int * idx_p, const unsigned int * idx_t);
