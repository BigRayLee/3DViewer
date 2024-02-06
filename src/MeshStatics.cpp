#include "MeshStatics.h"

/*mesh stastics*/
void meshopt_statistics(const char *name, const float* positions, const float *normals, const float *uvs, const float *positions_parent,
						const unsigned int *idx, const unsigned int * idx_p, const unsigned int * idx_t){
	{
		// uint32_t *idx = data.indices + mesh.index_offset;
		// float *pos = (float*)(data.positions + mesh.vertex_offset);
		// uint32_t nidx = mesh.index_count;
		// uint32_t nvtx = mesh.vertex_count;
		
		const int kCacheSize = 16;
		meshopt_VertexCacheStatistics vcs = meshopt_analyzeVertexCache(
					idx, 10, 20, kCacheSize, 0, 0);
		// meshopt_VertexFetchStatistics vfs = meshopt_analyzeVertexFetch(
		// 			idx, nidx, nvtx, sizeof(Vec3));
		// meshopt_OverdrawStatistics os = meshopt_analyzeOverdraw(
		// 			idx, nidx, pos, nvtx, sizeof(Vec3));
		// meshopt_VertexCacheStatistics vcs_nv = meshopt_analyzeVertexCache(
		// 		idx, nidx, nvtx, 32, 32, 32);
		// meshopt_VertexCacheStatistics vcs_amd = meshopt_analyzeVertexCache(
		// 		idx, nidx, nvtx, 14, 64, 128);
		// meshopt_VertexCacheStatistics vcs_intel = meshopt_analyzeVertexCache(
		// 		idx, nidx, nvtx, 128, 0, 0);

		// printf("%-9s: ACMR %.2f ATVR %.2f (NV %.2f AMD %.2f Intel %.2f) "
		// 	"Overfetch %.2f Overdraw %.2f\n", name, vcs.acmr, vcs.atvr, 
		// 	vcs_nv.atvr, vcs_amd.atvr, vcs_intel.atvr, vfs.overfetch, 
		// 	os.overdraw);
		}

	}
