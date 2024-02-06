// This file is part of meshoptimizer library; see meshoptimizer.h for version/license details
#include "simplify_mod_tex.h"
#include <iostream>
using namespace std;

#ifndef TRACE
#define TRACE 0
#endif

#if TRACE
#include <stdio.h>
#endif

#if TRACE
#define TRACESTATS(i) stats[i]++;
#else
#define TRACESTATS(i) (void)0
#endif

// This work is based on:
// Michael Garland and Paul S. Heckbert. Surface simplification using quadric error metrics. 1997
// Michael Garland. Quadric-based polygonal surface simplification. 1999
// Peter Lindstrom. Out-of-Core Simplification of Large Polygonal Models. 2000
// Matthias Teschner, Bruno Heidelberger, Matthias Mueller, Danat Pomeranets, Markus Gross. Optimized Spatial Hashing for Collision Detection of Deformable Objects. 2003
// Peter Van Sandt, Yannis Chronis, Jignesh M. Patel. Efficiently Searching In-Memory Sorted Arrays: Revenge of the Interpolation Search? 2019

/* Simplification method on positions + texture
   input data: x, y, z, s, t*/

namespace meshopt_tex
{

struct EdgeAdjacency
{
	struct Edge
	{
		unsigned int next;
		unsigned int prev;
	};

	unsigned int* counts;
	unsigned int* offsets;
	Edge* data;


};

static void prepareEdgeAdjacency(EdgeAdjacency& adjacency, size_t index_count, size_t vertex_count, meshopt_Allocator& allocator)
{
	adjacency.counts = allocator.allocate<unsigned int>(vertex_count);
	adjacency.offsets = allocator.allocate<unsigned int>(vertex_count);
	adjacency.data = allocator.allocate<EdgeAdjacency::Edge>(index_count);
}

static void updateEdgeAdjacency(EdgeAdjacency& adjacency, const unsigned int* indices, size_t index_count, size_t vertex_count, const unsigned int* remap)
{
	size_t face_count = index_count / 3;

	// fill edge counts
	memset(adjacency.counts, 0, vertex_count * sizeof(unsigned int));

	for (size_t i = 0; i < index_count; ++i)
	{
		unsigned int v = remap ? remap[indices[i]] : indices[i];
		assert(v < vertex_count);

		adjacency.counts[v]++;
	}

	// fill offset table
	unsigned int offset = 0;

	for (size_t i = 0; i < vertex_count; ++i)
	{
		adjacency.offsets[i] = offset;
		offset += adjacency.counts[i];
	}

	assert(offset == index_count);

	// fill edge data
	for (size_t i = 0; i < face_count; ++i)
	{
		unsigned int a = indices[i * 3 + 0], b = indices[i * 3 + 1], c = indices[i * 3 + 2];

		if (remap)
		{
			a = remap[a];
			b = remap[b];
			c = remap[c];
		}

		adjacency.data[adjacency.offsets[a]].next = b;
		adjacency.data[adjacency.offsets[a]].prev = c;
		adjacency.offsets[a]++;

		adjacency.data[adjacency.offsets[b]].next = c;
		adjacency.data[adjacency.offsets[b]].prev = a;
		adjacency.offsets[b]++;

		adjacency.data[adjacency.offsets[c]].next = a;
		adjacency.data[adjacency.offsets[c]].prev = b;
		adjacency.offsets[c]++;
	}

	// fix offsets that have been disturbed by the previous pass
	for (size_t i = 0; i < vertex_count; ++i)
	{
		assert(adjacency.offsets[i] >= adjacency.counts[i]);

		adjacency.offsets[i] -= adjacency.counts[i];
	}
}

struct PositionHasher
{
	const float* vertex_positions;
	size_t vertex_stride_float;

	size_t hash(unsigned int index) const
	{
		const unsigned int* key = reinterpret_cast<const unsigned int*>(vertex_positions + index * vertex_stride_float);

		// Optimized Spatial Hashing for Collision Detection of Deformable Objects
		return (key[0] * 73856093) ^ (key[1] * 19349663) ^ (key[2] * 83492791);
	}

	bool equal(unsigned int lhs, unsigned int rhs) const
	{
		return memcmp(vertex_positions + lhs * vertex_stride_float, vertex_positions + rhs * vertex_stride_float, sizeof(float) * 3) == 0;
	}
};

static size_t hashBuckets2(size_t count)
{
	size_t buckets = 1;
	while (buckets < count + count / 4)
		buckets *= 2;

	return buckets;
}

template <typename T, typename Hash>
static T* hashLookup2(T* table, size_t buckets, const Hash& hash, const T& key, const T& empty)
{
	assert(buckets > 0);
	assert((buckets & (buckets - 1)) == 0);

	size_t hashmod = buckets - 1;
	size_t bucket = hash.hash(key) & hashmod;

	for (size_t probe = 0; probe <= hashmod; ++probe)
	{
		T& item = table[bucket];

		if (item == empty)
			return &item;

		if (hash.equal(item, key))
			return &item;

		// hash collision, quadratic probing
		bucket = (bucket + probe + 1) & hashmod;
	}

	assert(false && "Hash table is full"); // unreachable
	return 0;
}

static void buildPositionRemap(unsigned int* remap, unsigned int* wedge, const float* vertex_positions_data, size_t vertex_count, size_t vertex_positions_stride, meshopt_Allocator& allocator)
{
	PositionHasher hasher = {vertex_positions_data, vertex_positions_stride / sizeof(float)};

	size_t table_size = hashBuckets2(vertex_count);
	unsigned int* table = allocator.allocate<unsigned int>(table_size);
	memset(table, -1, table_size * sizeof(unsigned int));

	// build forward remap: for each vertex, which other (canonical) vertex does it map to?
	// we use position equivalence for this, and remap vertices to other existing vertices
	for (size_t i = 0; i < vertex_count; ++i)
	{
		unsigned int index = unsigned(i);
		unsigned int* entry = hashLookup2(table, table_size, hasher, index, ~0u);

		if (*entry == ~0u)
			*entry = index;

		remap[index] = *entry;
	}

	// build wedge table: for each vertex, which other vertex is the next wedge that also maps to the same vertex?
	// entries in table form a (cyclic) wedge loop per vertex; for manifold vertices, wedge[i] == remap[i] == i
	for (size_t i = 0; i < vertex_count; ++i)
		wedge[i] = unsigned(i);

	for (size_t i = 0; i < vertex_count; ++i){
		if (remap[i] != i)
		{
			unsigned int r = remap[i];

			wedge[i] = wedge[r];
			wedge[r] = unsigned(i);
		}
	}
		
}

/* building position map using positions and uv coordinates */
static void buildPositionRemap(unsigned int* remap, unsigned int* wedge, 
			const float* vertex_positions_data, size_t vertex_count, const float* uvs_data, size_t uv_positions_stride, 
			size_t vertex_positions_stride, meshopt_Allocator& allocator)
{
	/*make the positions_uvs data*/
	float* vert_uv = (float*)malloc(vertex_count * 5 * sizeof(float));
	for(int i = 0; i < vertex_count; ++i)
	{
		memcpy((vert_uv) + 5 * i , (vertex_positions_data) + 3 * i , sizeof(float) * 3);
		memcpy((vert_uv) + 5 * i  + 3 , (uvs_data) + 2 * i , sizeof(float) * 2);
	}
	PositionHasher hasher = {vert_uv, (vertex_positions_stride + uv_positions_stride) / sizeof(float)};

	size_t table_size = hashBuckets2(vertex_count);
	unsigned int* table = allocator.allocate<unsigned int>(table_size);
	memset(table, -1, table_size * sizeof(unsigned int));

	// build forward remap: for each vertex, which other (canonical) vertex does it map to?
	// we use position equivalence for this, and remap vertices to other existing vertices
	for (size_t i = 0; i < vertex_count; ++i)
	{
		unsigned int index = unsigned(i);
		unsigned int* entry = hashLookup2(table, table_size, hasher, index, ~0u);

		if (*entry == ~0u)
			*entry = index;

		remap[index] = *entry;
	}

	// build wedge table: for each vertex, which other vertex is the next wedge that also maps to the same vertex?
	// entries in table form a (cyclic) wedge loop per vertex; for manifold vertices, wedge[i] == remap[i] == i
	for (size_t i = 0; i < vertex_count; ++i)
		wedge[i] = unsigned(i);

	for (size_t i = 0; i < vertex_count; ++i)
		if (remap[i] != i)
		{
			unsigned int r = remap[i];

			wedge[i] = wedge[r];
			wedge[r] = unsigned(i);
		}

	free(vert_uv);
}	

enum VertexKind
{
	Kind_Manifold,    // not on an attribute seam, not on any boundary
	Kind_Border,      // not on an attribute seam, has exactly two open edges
	Kind_Border_Fake, // has exactly two open edges, boudary edge caused by the hole (non water tight mesh model)
	Kind_Seam,        // on an attribute seam with exactly two attribute seam edges
	Kind_Complex,     // none of the above; these vertices can move as long as all wedges move to the target vertex
	Kind_Locked,      // none of the above; these vertices can't move

	Kind_Count 
};

// manifold vertices can collapse onto anything
// border/seam vertices can only be collapsed onto border/seam respectively
// complex vertices can collapse onto complex/locked
// a rule of thumb is that collapsing kind A into kind B preserves the kind B in the target vertex
// for example, while we could collapse Complex into Manifold, this would mean the target vertex isn't Manifold anymore
const unsigned char kCanCollapse[Kind_Count][Kind_Count] = {
	{1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0},
	{0, 0, 1, 0, 0, 0},
	{0, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 1, 1},
	{0, 0, 0, 0, 0, 0},
};
/* TODO Hack ! */
/*const unsigned char kCanCollapse[Kind_Count][Kind_Count] = {
	{1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0},
	{1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
};*/

// if a vertex is manifold or seam, adjoining edges are guaranteed to have an opposite edge
// note that for seam edges, the opposite edge isn't present in the attribute-based topology
// but is present if you consider a position-only mesh variant
const unsigned char kHasOpposite[Kind_Count][Kind_Count] = {
	{1, 1, 1, 1, 0, 1},
	{1, 0, 0, 1, 0, 0},
	{1, 0, 0, 1, 0, 0},
	{1, 1, 1, 1, 0, 1},
	{0, 0, 0, 0, 0, 0},
	{1, 0, 0, 1, 0, 0},
};

static bool hasEdge(const EdgeAdjacency& adjacency, unsigned int a, unsigned int b)
{
	unsigned int count = adjacency.counts[a];
	const EdgeAdjacency::Edge* edges = adjacency.data + adjacency.offsets[a];

	for (size_t i = 0; i < count; ++i)
		if (edges[i].next == b)
			return true;

	return false;
}

static void classifyVertices(unsigned char* result, unsigned int* loop, unsigned int* loopback, size_t vertex_count, const EdgeAdjacency& adjacency, const unsigned int* remap, const unsigned int* wedge)
{
	memset(loop, -1, vertex_count * sizeof(unsigned int));
	memset(loopback, -1, vertex_count * sizeof(unsigned int));
	
	// incoming & outgoing open edges: ~0u if no open edges, i if there are more than 1
	// note that this is the same data as required in loop[] arrays; loop[] data is only valid for border/seam
	// but here it's okay to fill the data out for other types of vertices as well
	unsigned int* openinc = loopback;
	unsigned int* openout = loop;
	
	for (size_t i = 0; i < vertex_count; ++i)
	{
		unsigned int vertex = unsigned(i);

		unsigned int count = adjacency.counts[vertex];
		const EdgeAdjacency::Edge* edges = adjacency.data + adjacency.offsets[vertex];

		for (size_t j = 0; j < count; ++j)
		{
			unsigned int target = edges[j].next;

			if (!hasEdge(adjacency, target, vertex))
			{
				openinc[target] = (openinc[target] == ~0u) ? vertex : target;
				openout[vertex] = (openout[vertex] == ~0u) ? target : vertex;
			}
		}
	}
	

#if TRACE
	size_t stats[4] = {};
#endif

	for (size_t i = 0; i < vertex_count; ++i)
	{
		if (remap[i] == i)
		{
			if (wedge[i] == i)
			{
				// no attribute seam, need to check if it's manifold
				unsigned int openi = openinc[i], openo = openout[i];

				// note: we classify any vertices with no open edges as manifold
				// this is technically incorrect - if 4 triangles share an edge, we'll classify vertices as manifold
				// it's unclear if this is a problem in practice
				if (openi == ~0u && openo == ~0u)
				{
					result[i] = Kind_Manifold;
				}
				else if (openi != i && openo != i)
				{
					result[i] = Kind_Border;
				}
				else
				{
					result[i] = Kind_Locked;
					TRACESTATS(0);
				}
			}
			else if (wedge[wedge[i]] == i)
			{
				// attribute seam; need to distinguish between Seam and Locked
				unsigned int w = wedge[i];
				unsigned int openiv = openinc[i], openov = openout[i];
				unsigned int openiw = openinc[w], openow = openout[w];

				// seam should have one open half-edge for each vertex, and the edges need to "connect" - point to the same vertex post-remap
				if (openiv != ~0u && openiv != i && openov != ~0u && openov != i &&
					openiw != ~0u && openiw != w && openow != ~0u && openow != w)
				{
					if (remap[openiv] == remap[openow] && remap[openov] == remap[openiw])
					{
						result[i] = Kind_Seam;
					}
					else
					{
						result[i] = Kind_Locked;
						TRACESTATS(1);
					}
				}
				else
				{
					result[i] = Kind_Locked;
					TRACESTATS(2);
				}
			}
			else
			{
				// more than one vertex maps to this one; we don't have classification available
				result[i] = Kind_Locked;
				TRACESTATS(3);
			}
		}
		else
		{
			assert(remap[i] < i);

			result[i] = result[remap[i]];
		}
	}

#if TRACE
	printf("locked: many open edges %d, disconnected seam %d, many seam edges %d, many wedges %d\n",
		int(stats[0]), int(stats[1]), int(stats[2]), int(stats[3]));
#endif
}

struct Vector3
{
	float x, y, z;
};

struct Vector2
{
	float s, t;
};

struct Vector5
{
	float x, y, z, s, t;
};

/*x, y, z, s, t quadric data*/
struct Quadric5
{
	float a00, a11, a22, a33, a44;
	float a10, a20, a30, a40;
	float a21, a31, a41;
	float a32, a42, a43;
	float b0, b1, b2, b3, b4, c;
	float w;
};

Vector5 sub_vec5(const Vector5& v1, const Vector5& v2){
	Vector5 v;
	v.x = v1.x - v2.x;
	v.y = v1.y - v2.y;
	v.z = v1.z - v2.z;
	v.s = v1.s - v2.s;
	v.t = v1.t - v2.t;
	return v;
}

Vector5 add_vec5(const Vector5& v1, const Vector5& v2){
	Vector5 v;
	v.x = v1.x + v2.x;
	v.y = v1.y + v2.y;
	v.z = v1.z + v2.z;
	v.s = v1.s + v2.s;
	v.t = v1.t + v2.t;
	return v;
}

static float normalize(Vector5& v)
{
	float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.s * v.s + v.t * v.t);

	if (length > 0)
	{
		v.x /= length;
		v.y /= length;
		v.z /= length;
		v.s /= length;
		v.t /= length;
	}

	return length;
}

Vector5 nml_vec5(const Vector5& v1){
	Vector5 nml;
	float norm = sqrt(v1.x * v1.x + v1.y * v1.y + v1.z * v1.z + v1.s * v1.s + v1.t * v1.t);

	nml.x = v1.x / norm;
	nml.y = v1.y / norm;
	nml.z = v1.z / norm;
	nml.s = v1.s / norm;
	nml.t = v1.t / norm;

	return nml;
}

float product_vec5(const Vector5& v1, const Vector5& v2){
	float res =  v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.s * v2.s + v1.t * v2.t;
	return res;
}

float product_vec5(const float v1[5], const float v2[5]){
	float res =  v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2] + v1[3] * v2[3] + v1[4] * v2[4];
	return res;
}

void outproduct5(const Vector5& v1, const Vector5& v2, float r[5][5])
{
	float a[5] = {v1.x, v1.y, v1.z, v1.s, v1.t};
	float b[5] = {v2.x, v2.y, v2.z, v2.s, v2.t};
	for(int i = 0; i < 5; i++)
		for(int j = 0; j < 5; j++)
			r[i][j] = a[i] * b[j];
}

Vector5 prod_matvec5(const float m[5][5], const Vector5& v1)
{
	float v[5] = {v1.x, v1.y, v1.z, v1.s, v1.t};
	Vector5 r;
	r.x = product_vec5(m[0],v);
	r.y = product_vec5(m[1],v);
	r.z = product_vec5(m[2],v);
	r.s = product_vec5(m[3],v);
	r.t = product_vec5(m[4],v);
	return r;
}

Vector5 mul_vec5(const Vector5& v, float scaler){
	Vector5 res;
	res.x = scaler * v.x;
	res.y = scaler * v.y;
	res.z = scaler * v.z;
	res.s = scaler * v.s;
	res.t = scaler * v.t;
	return res;
}

Quadric5 symmetric_vec5(const Vector5 &v){
	Quadric5 res;
	res.a00 = v.x * v.x;
	res.a11 = v.y * v.y;
	res.a22 = v.z * v.z;
	res.a33 = v.s * v.s;
	res.a44 = v.t * v.t;

	res.a10 = v.x * v.y;
	res.a20 = v.x * v.z;
	res.a21 = v.y * v.z;
	res.a30 = v.s * v.x;
	res.a31 = v.s * v.y;
	res.a32 = v.s * v.z;
	res.a40 = v.x * v.t;
	res.a41 = v.y * v.t;
	res.a42 = v.z * v.t;
	res.a43 = v.s * v.t;

	return res;
}

Quadric5 sub_symmetricM(const Quadric5& v1, const Quadric5& v2){
	Quadric5 diff;
	diff.a00 = v1.a00 - v2.a00;
	diff.a11 = v1.a11 - v2.a11;
	diff.a22 = v1.a22 - v2.a22;
	diff.a33 = v1.a33 - v2.a33;
	diff.a44 = v1.a44 - v2.a44;

	diff.a10 = v1.a10 - v2.a10;
	diff.a20 = v1.a20 - v2.a20;
	diff.a21 = v1.a21 - v2.a21;
	diff.a30 = v1.a30 - v2.a30;
	diff.a31 = v1.a31 - v2.a31;
	diff.a32 = v1.a32 - v2.a32;
	diff.a40 = v1.a40 - v2.a40;
	diff.a41 = v1.a41 - v2.a41;
	diff.a42 = v1.a42 - v2.a42;
	diff.a43 = v1.a43 - v2.a43;

	return diff;
}


/*x, y, z, s, t quadric data add operation*/
static void quadric5Add(Quadric5& Q, const Quadric5& R)
{
	Q.a00 += R.a00;
	Q.a11 += R.a11;
	Q.a22 += R.a22;
	Q.a33 += R.a33;
	Q.a44 += R.a44;

	Q.a10 += R.a10;
	Q.a20 += R.a20;
	Q.a30 += R.a30;
	Q.a40 += R.a40;

	Q.a21 += R.a21;
	Q.a31 += R.a31;
	Q.a41 += R.a41;

	Q.a32 += R.a32;
	Q.a42 += R.a42;
	Q.a43 += R.a43;

	Q.b0 += R.b0;
	Q.b1 += R.b1;
	Q.b2 += R.b2;
	Q.b3 += R.b3;
	Q.b4 += R.b4;

	Q.c += R.c;
	Q.w += R.w;
}

/*quadirc5 error*/
static float quadric5Error(const Quadric5& Q, const Vector5& v)
{
	float rx = Q.b0;
	float ry = Q.b1;
	float rz = Q.b2;
	float rs = Q.b3;
	float rt = Q.b4;

	rx += Q.a10 * v.y + Q.a30 * v.s;
	ry += Q.a21 * v.z + Q.a31 * v.s;
	rz += Q.a20 * v.x + Q.a32 * v.s;
	rs += Q.a43 * v.t;
	rt += Q.a40 * v.x + Q.a41 * v.y + Q.a42 * v.z;
	
	rx *= 2;
	ry *= 2;
	rz *= 2;
	rs *= 2;
	rt *= 2;

	rx += Q.a00 * v.x;
	ry += Q.a11 * v.y;
	rz += Q.a22 * v.z;
	rs += Q.a33 * v.s;
	rt += Q.a44 * v.t;

	float r = Q.c;
	r += rx * v.x;
	r += ry * v.y;
	r += rz * v.z;
	r += rs * v.s;
	r += rt * v.t;

	float s = Q.w == 0.f ? 0.f : 1.f / Q.w;

	//return fabsf(r) * s;
	return fabsf(r);
}

/* compute orthonormal basis of the p q */
void ComputeE1E2(const Vector5 &p, const Vector5 &q, const Vector5 &r, Vector5 &e1, Vector5 &e2){
	//compute e1
	e1 = sub_vec5(q, p);
	e1 = nml_vec5(e1);
	
	//compute e2
	Vector5 diff = sub_vec5(r, p);
	float product[5][5] = {0.0};
	outproduct5(e1, diff, product); 
	Vector5 mul = prod_matvec5(product, e1);
	e2 = sub_vec5(diff, mul);
	e2 = nml_vec5(e2);
	
}

/*compute the quadric from e1 e2*/
Quadric5 computeQuadricfromE1E2(const Vector5& p, const Vector5& e1, const Vector5& e2){
	//compute A
	//Matrix A
	Quadric5 A;
	A.a00 = 1.0;
	A.a11 = 1.0;
	A.a22 = 1.0;
	A.a33 = 1.0;
	A.a44 = 1.0;

	A.a10 = 0.0;
	A.a20 = 0.0;
	A.a21 = 0.0;
	A.a30 = 0.0;
	A.a31 = 0.0;
	A.a32 = 0.0;
	A.a40 = 0.0;
	A.a41 = 0.0;
	A.a42 = 0.0;
	A.a43 = 0.0;

	Quadric5 tmp1 = symmetric_vec5(e1);
	Quadric5 tmp2 = symmetric_vec5(e2);
	A = sub_symmetricM(A, tmp1);
	A = sub_symmetricM(A, tmp2);

	//b
	float pe1 = product_vec5(p, e1);
	float pe2 = product_vec5(p, e2);

	Vector5 pe_1 = mul_vec5(e1, pe1);
	Vector5 pe_2 = mul_vec5(e2, pe2);

	Vector5 b_tmp = sub_vec5(add_vec5(pe_1, pe_2) , p);

	A.b0 = b_tmp.x;
	A.b1 = b_tmp.y;
	A.b2 = b_tmp.z;
	A.b3 = b_tmp.s;
	A.b4 = b_tmp.t;

	//c
	A.c = product_vec5(p, p) - pe1 * pe1 - pe2 * pe2;

	return A;
}

/*rescale position and texture coordinates*/
static float rescaleUV(Vector2* result_tex, const float* uv_data, size_t uv_count, size_t uv_stride, float extent)
{
	//rescale the uv coordinates
	size_t uv_stride_float = uv_stride / sizeof(float);

	float mint[2] = {FLT_MAX, FLT_MAX};
	float maxt[2] = {-FLT_MAX, -FLT_MAX};

	for (size_t i = 0; i < uv_count; ++i)
	{
		const float* v = uv_data + i * uv_stride_float;

		if (result_tex)
		{
			result_tex[i].s = v[0];
			result_tex[i].t = v[1];
			// cout<<"uv : "<<v[0]<<" "<<result_tex[i].s<<endl;
		}

		for (int j = 0; j < 2; ++j)
		{
			float vj = v[j];

			mint[j] = mint[j] > vj ? vj : mint[j];
			maxt[j] = maxt[j] < vj ? vj : maxt[j];
		}
	}

	float extent_uv = 0.f;

	extent_uv = (maxt[0] - mint[0]) < extent_uv ? extent_uv : (maxt[0] - mint[0]);
	extent_uv = (maxt[1] - mint[1]) < extent_uv ? extent_uv : (maxt[1] - mint[1]);
	
	return extent_uv;
}

static float rescalePositions(Vector3* result, const float* vertex_positions_data, size_t vertex_count, size_t vertex_positions_stride)
{
	size_t vertex_stride_float = vertex_positions_stride / sizeof(float);

	float minv[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
	float maxv[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

	for (size_t i = 0; i < vertex_count; ++i)
	{
		const float* v = vertex_positions_data + i * vertex_stride_float;

		if (result)
		{
			result[i].x = v[0];
			result[i].y = v[1];
			result[i].z = v[2];
		}

		for (int j = 0; j < 3; ++j)
		{
			float vj = v[j];

			minv[j] = minv[j] > vj ? vj : minv[j];
			maxv[j] = maxv[j] < vj ? vj : maxv[j];
		}
	}

	float extent = 0.f;

	extent = (maxv[0] - minv[0]) < extent ? extent : (maxv[0] - minv[0]);
	extent = (maxv[1] - minv[1]) < extent ? extent : (maxv[1] - minv[1]);
	extent = (maxv[2] - minv[2]) < extent ? extent : (maxv[2] - minv[2]);

	if (result)
	{
		float scale = extent == 0 ? 0.f : 1.f / extent;

		for (size_t i = 0; i < vertex_count; ++i)
		{
			result[i].x = (result[i].x - minv[0]) * scale;
			result[i].y = (result[i].y - minv[1]) * scale;
			result[i].z = (result[i].z - minv[2]) * scale;
		}
		
	}
	
	return extent;
}

static void rescalePositions(Vector3 *result,
				const float *vertex_positions_data,
				size_t vertex_count,
				size_t vertex_positions_stride, float extent,
				float offset[3])
{
	size_t vertex_stride_float = vertex_positions_stride / sizeof(float);

	float scale = 1.f / extent;

	for (size_t i = 0; i < vertex_count; ++i) {
		const float *v = vertex_positions_data + i * vertex_stride_float;

		if (result) {
			result[i].x = (v[0] - offset[0]) * scale;
			result[i].y = (v[1] - offset[1]) * scale;
			result[i].z = (v[2] - offset[2]) * scale;
		}
	}
}


struct Collapse
{
	unsigned int v0;
	unsigned int v1;

	//store uv index
	unsigned int t0;
	unsigned int t1;

	union
	{
		unsigned int bidi;
		float error;
		unsigned int errorui;
	};
};

static float normalize(Vector3& v)
{
	float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);

	if (length > 0)
	{
		v.x /= length;
		v.y /= length;
		v.z /= length;
	}

	return length;
}

static void quadric5FromTriangle(Quadric5& Q, const Vector5& p, const Vector5& q, const Vector5& r)
{
	Vector5 e1, e2;
	
	ComputeE1E2(p, q, r, e1, e2);
	
	Q = computeQuadricfromE1E2(p, e1, e2);
}

/*  two index buffer: indices_pos, indices_uv
	fill the face quadric5 by index
	Quadric5* quadrics : the quadric computed by positions and textures
	unsigned int * indices : position index
	unsigned int * indices : texture index
	size_t index_count: index count
	Vector3 vertex_positions: positions value
	
*/
static void fillFaceQuadric5s(Quadric5* quadrics, const unsigned int* indices, const unsigned int* indices_tex,
							size_t index_count, const Vector3* vertex_positions, const Vector2* uv_positions, 
							const unsigned int* remap, const unsigned int* remap_uv)
{
	/*build vector 5 with position and uvs*/
	for (size_t i = 0; i < index_count / 3; i ++)
	{
		unsigned int i0 = indices[3 * i + 0];
		unsigned int i1 = indices[3 * i + 1];
		unsigned int i2 = indices[3 * i + 2];

		unsigned int j0 = indices_tex[3 * i + 0];
		unsigned int j1 = indices_tex[3 * i + 1];
		unsigned int j2 = indices_tex[3 * i + 2];

		Vector5 p, q, r;
		p.x = vertex_positions[i0].x;
		p.y = vertex_positions[i0].y;
		p.z = vertex_positions[i0].z;
		p.s = uv_positions[j0].s;
		p.t = uv_positions[j0].t;

		q.x = vertex_positions[i1].x;
		q.y = vertex_positions[i1].y;
		q.z = vertex_positions[i1].z;
		q.s = uv_positions[j1].s;
		q.t = uv_positions[j1].t;

		r.x = vertex_positions[i2].x;
		r.y = vertex_positions[i2].y;
		r.z = vertex_positions[i2].z;
		r.s = uv_positions[j2].s;
		r.t = uv_positions[j2].t;

		Quadric5 Q;
		quadric5FromTriangle(Q, p, q, r);
		
		quadric5Add(quadrics[remap[i0]], Q);
		quadric5Add(quadrics[remap[i1]], Q);
		quadric5Add(quadrics[remap[i2]], Q);
		
	}
}

static void fillEdgeQuadric5s(Quadric5* vertex_quadrics, const unsigned int* indices, const unsigned int* indices_uv, size_t index_count, 
			const Vector3* vertex_positions, const Vector2 * uv_positions, const unsigned int* remap, const unsigned int* remap_uv,
			const unsigned char* vertex_kind, const unsigned int* loop, const unsigned int* loopback)
{
	for (size_t i = 0; i < index_count; i += 3)
	{
		static const int next[3] = {1, 2, 0};

		for (int e = 0; e < 3; ++e)
		{
			unsigned int i0 = indices[i + e];
			unsigned int i1 = indices[i + next[e]];
			
			unsigned int t0 = indices_uv[i + e];
			unsigned int t1 = indices_uv[i + next[e]];

			unsigned char k0 = vertex_kind[i0];
			unsigned char k1 = vertex_kind[i1];

			// check that either i0 or i1 are border/seam and are on the same edge loop
			// note that we need to add the error even for edged that connect e.g. border & locked
			// if we don't do that, the adjacent border->border edge won't have correct errors for corners
			if (k0 != Kind_Border && k0 != Kind_Seam && k0 != Kind_Border_Fake &&
				k1 != Kind_Border && k1 != Kind_Seam && k1 != Kind_Border_Fake)
				continue;

			if ((k0 == Kind_Border || k0 == Kind_Seam || k0 == Kind_Border_Fake) && loop[i0] != i1)
				continue;

			if ((k1 == Kind_Border || k1 == Kind_Seam || k1 == Kind_Border_Fake) && loopback[i1] != i0)
				continue;

			// seam edges should occur twice (i0->i1 and i1->i0) - skip redundant edges
			if (kHasOpposite[k0][k1] && remap[i1] > remap[i0])
				continue;

			unsigned int i2 = indices[i + next[next[e]]];
			unsigned int t2 = indices_uv[i + next[next[e]]];

			// we try hard to maintain border edge geometry; seam edges can move more freely
			// due to topological restrictions on collapses, seam quadrics slightly improves collapse structure but aren't critical
			const float kEdgeWeightSeam = 1.f;
			const float kEdgeWeightBorder = 10.f;

			float edgeWeight = (k0 == Kind_Border || k1 == Kind_Border || k0 == Kind_Border_Fake || k1 == Kind_Border_Fake) ? kEdgeWeightBorder : kEdgeWeightSeam;

			Quadric5 Q;
			//build vector5
			Vector5 p, q, r;
			p.x = vertex_positions[i0].x;
			p.y = vertex_positions[i0].y;
			p.z = vertex_positions[i0].z;
			p.s = uv_positions[t0].s;
			p.t = uv_positions[t0].t;

			q.x = vertex_positions[i1].x;
			q.y = vertex_positions[i1].y;
			q.z = vertex_positions[i1].z;
			q.s = uv_positions[t1].s;
			q.t = uv_positions[t1].t;

			r.x = vertex_positions[i2].x;
			r.y = vertex_positions[i2].y;
			r.z = vertex_positions[i2].z;
			r.s = uv_positions[t2].s;
			r.t = uv_positions[t2].t;

			quadric5FromTriangle(Q, p, q, r);

			quadric5Add(vertex_quadrics[remap[i0]], Q);
			quadric5Add(vertex_quadrics[remap[i1]], Q);
		}
	}
}

// does triangle ABC flip when C is replaced with D?
static bool hasTriangleFlip(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d)
{
	Vector3 eb = {b.x - a.x, b.y - a.y, b.z - a.z};
	Vector3 ec = {c.x - a.x, c.y - a.y, c.z - a.z};
	Vector3 ed = {d.x - a.x, d.y - a.y, d.z - a.z};

	Vector3 nbc = {eb.y * ec.z - eb.z * ec.y, eb.z * ec.x - eb.x * ec.z, eb.x * ec.y - eb.y * ec.x};
	Vector3 nbd = {eb.y * ed.z - eb.z * ed.y, eb.z * ed.x - eb.x * ed.z, eb.x * ed.y - eb.y * ed.x};

	return nbc.x * nbd.x + nbc.y * nbd.y + nbc.z * nbd.z < 0;
}

static bool hasTriangleFlips(const EdgeAdjacency& adjacency, const Vector3* vertex_positions, const unsigned int* collapse_remap, unsigned int i0, unsigned int i1)
{
	assert(collapse_remap[i0] == i0);
	assert(collapse_remap[i1] == i1);

	const Vector3& v0 = vertex_positions[i0];
	const Vector3& v1 = vertex_positions[i1];

	const EdgeAdjacency::Edge* edges = &adjacency.data[adjacency.offsets[i0]];
	size_t count = adjacency.counts[i0];

	for (size_t i = 0; i < count; ++i)
	{
		unsigned int a = collapse_remap[edges[i].next];
		unsigned int b = collapse_remap[edges[i].prev];

		// skip triangles that get collapsed
		// note: this is mathematically redundant as if either of these is true, the dot product in hasTriangleFlip should be 0
		if (a == i1 || b == i1)
			continue;

		// early-out when at least one triangle flips due to a collapse
		if (hasTriangleFlip(vertex_positions[a], vertex_positions[b], v0, v1))
			return true;
	}

	return false;
}

/*add texture indices*/
static size_t pickEdgeCollapses(Collapse* collapses, const unsigned int* indices, const unsigned int* indices_uv, size_t index_count, const unsigned int* remap, const unsigned char* vertex_kind, const unsigned int* loop)
{
	size_t collapse_count = 0;

	for (size_t i = 0; i < index_count / 3; ++ i)
	{
		static const int next[3] = {1, 2, 0};

		for (int e = 0; e < 3; ++e)
		{
			unsigned int i0 = indices[3 * i + e];
			unsigned int i1 = indices[3 * i + next[e]];

			//store the texture indices
			unsigned int t0 = i0;
			unsigned int t1 = i1;

			// this can happen either when input has a zero-length edge, or when we perform collapses for complex
			// topology w/seams and collapse a manifold vertex that connects to both wedges onto one of them
			// we leave edges like this alone since they may be important for preserving mesh integrity
			if (remap[i0] == remap[i1])
				continue;

			unsigned char k0 = vertex_kind[i0];
			unsigned char k1 = vertex_kind[i1];

			// the edge has to be collapsible in at least one direction
			if (!(kCanCollapse[k0][k1] | kCanCollapse[k1][k0]))
				continue;

			// manifold and seam edges should occur twice (i0->i1 and i1->i0) - skip redundant edges
			if (kHasOpposite[k0][k1] && remap[i1] > remap[i0])
				continue;

			// two vertices are on a border or a seam, but there's no direct edge between them
			// this indicates that they belong to twou different edge loops and we should not collapse this edge
			// loop[] tracks half edges so we only need to check i0->i1
			if (k0 == k1 && (k0 == Kind_Border || k0 == Kind_Seam || k0 == Kind_Border_Fake) && loop[i0] != i1)
				continue;

			// edge can be collapsed in either direction - we will pick the one with minimum error
			// note: we evaluate error later during collapse ranking, here we just tag the edge as bidirectional
			if (kCanCollapse[k0][k1] & kCanCollapse[k1][k0])
			{
				Collapse c = {i0, i1, t0, t1, {/* bidi= */ 1}};
				collapses[collapse_count++] = c;
				
			}
			else
			{
				// edge can only be collapsed in one direction
				unsigned int e0 = kCanCollapse[k0][k1] ? i0 : i1;
				unsigned int e1 = kCanCollapse[k0][k1] ? i1 : i0;
					
				unsigned int u0 = e0;
				unsigned int u1 = e1;

				Collapse c = {e0, e1, u0, u1, {/* bidi= */ 0}};
				collapses[collapse_count++] = c;
			}
		}
	}

	return collapse_count;
}

/*add the uv_data*/
static void rankEdgeCollapses(Collapse* collapses, size_t collapse_count, const Vector3* vertex_positions, 
							const Vector2* uv_data, const Quadric5* vertex_quadrics, const unsigned int* remap)
{
	for (size_t i = 0; i < collapse_count; ++i)
	{
		Collapse& c = collapses[i];

		unsigned int i0 = c.v0;
		unsigned int i1 = c.v1;

		unsigned int t0 = c.t0;
		unsigned int t1 = c.t1;


		// most edges are bidirectional which means we need to evaluate errors for two collapses
		// to keep this code branchless we just use the same edge for unidirectional edges
		unsigned int j0 = c.bidi ? i1 : i0;
		unsigned int j1 = c.bidi ? i0 : i1;

		unsigned int u0 = c.bidi ? t1 : t0;
		unsigned int u1 = c.bidi ? t0 : t1;

		const Quadric5& qi = vertex_quadrics[remap[i0]];
		const Quadric5& qj = vertex_quadrics[remap[j0]];

		//generate the vector5
		Vector5 v_ei, v_ej;
		v_ei.x = vertex_positions[i1].x;
		v_ei.y = vertex_positions[i1].y;
		v_ei.z = vertex_positions[i1].z;
		v_ei.s = uv_data[t1].s;
		v_ei.t = uv_data[t1].t;

		v_ej.x = vertex_positions[j1].x;
		v_ej.y = vertex_positions[j1].y;
		v_ej.z = vertex_positions[j1].z;
		v_ej.s = uv_data[u1].s;
		v_ej.t = uv_data[u1].t;

		
		//i1 j1
		float ei = quadric5Error(qi, v_ei);
		float ej = quadric5Error(qj, v_ej);
		//std::cout<<"q5 error check "<<ei<<" "<<ej<<std::endl;
		// pick edge direction with minimal error
		c.v0 = ei <= ej ? i0 : j0;
		c.v1 = ei <= ej ? i1 : j1;

		c.t0 = c.v0;
		c.t1 = c.v1;

		c.error = ei <= ej ? ei : ej;
	}
}

static void sortEdgeCollapses(unsigned int* sort_order, const Collapse* collapses, size_t collapse_count)
{
	const int sort_bits = 11;

	// fill histogram for counting sort
	unsigned int histogram[1 << sort_bits];
	memset(histogram, 0, sizeof(histogram));

	for (size_t i = 0; i < collapse_count; ++i)
	{
		// skip sign bit since error is non-negative
		unsigned int key = (collapses[i].errorui << 1) >> (32 - sort_bits);

		histogram[key]++;
	}

	// compute offsets based on histogram data
	size_t histogram_sum = 0;

	for (size_t i = 0; i < 1 << sort_bits; ++i)
	{
		size_t count = histogram[i];
		histogram[i] = unsigned(histogram_sum);
		histogram_sum += count;
	}

	assert(histogram_sum == collapse_count);

	// compute sort order based on offsets
	for (size_t i = 0; i < collapse_count; ++i)
	{
		// skip sign bit since error is non-negative
		unsigned int key = (collapses[i].errorui << 1) >> (32 - sort_bits);

		sort_order[histogram[key]++] = unsigned(i);
	}
}

/* Edge collapse based on quadric 5 */
static size_t performEdgeCollapses(	unsigned int* collapse_remap,
									unsigned int* collapse_remap_uv,
									unsigned char* collapse_locked, 
									unsigned char* collapse_locked_uv,
									Quadric5* vertex_quadrics, 
									const Collapse* collapses, 
									size_t collapse_count, 
									const unsigned int* collapse_order, 
									const unsigned int* remap, 
									const unsigned int* remap_uv,
									const unsigned int* wedge, 
									const unsigned int* wedge_uv,
									const unsigned char* vertex_kind, 
									const Vector3* vertex_positions, 
									const Vector2* uv_data,
									const EdgeAdjacency& adjacency, 
									size_t triangle_collapse_goal, 
									float error_limit, 
									float& result_error)
{
	size_t edge_collapses = 0;
	size_t triangle_collapses = 0;

	// most collapses remove 2 triangles; use this to establish a bound on the pass in terms of error limit
	// note that edge_collapse_goal is an estimate; triangle_collapse_goal will be used to actually limit collapses
	size_t edge_collapse_goal = triangle_collapse_goal / 2;

#if TRACE
	size_t stats[4] = {};
#endif
	int k = 0, j = 0;
	int cout_collapse = 0;
	for (size_t i = 0; i < collapse_count; ++i)
	{
		const Collapse& c = collapses[collapse_order[i]];

		TRACESTATS(0);

		if (c.error > error_limit)
			break;

		if (triangle_collapses >= triangle_collapse_goal)
			break;


		// we limit the error in each pass based on the error of optimal last collapse; since many collapses will be locked
		// as they will share vertices with other successfull collapses, we need to increase the acceptable error by some factor
		float error_goal = edge_collapse_goal < collapse_count ? 1.5f * collapses[collapse_order[edge_collapse_goal]].error : FLT_MAX;

		// on average, each collapse is expected to lock 6 other collapses; to avoid degenerate passes on meshes with odd
		// topology, we only abort if we got over 1/6 collapses accordingly.
		if (c.error > error_goal && triangle_collapses > triangle_collapse_goal / 6)
			break;

		unsigned int i0 = c.v0;
		unsigned int i1 = c.v1;

		unsigned int t0 = c.t0;
		unsigned int t1 = c.t1;

		unsigned int r0 = remap[i0];
		unsigned int r1 = remap[i1];

		unsigned int u0 = remap_uv[t0];
		unsigned int u1 = remap_uv[t1];

		// we don't collapse vertices that had source or target vertex involved in a collapse
		// it's important to not move the vertices twice since it complicates the tracking/remapping logic
		// it's important to not move other vertices towards a moved vertex to preserve error since we don't re-rank collapses mid-pass
		if (collapse_locked[r0] | collapse_locked[r1])
		{
			TRACESTATS(1);
			continue;
		}

		if (hasTriangleFlips(adjacency, vertex_positions, collapse_remap, r0, r1))
		{
			// adjust collapse goal since this collapse is invalid and shouldn't factor into error goal
			edge_collapse_goal++;

			TRACESTATS(2);
			continue;
		}

		assert(collapse_remap[r0] == r0);
		assert(collapse_remap[r1] == r1);

		quadric5Add(vertex_quadrics[r1], vertex_quadrics[r0]);
		
		if (vertex_kind[i0] == Kind_Complex)
		{
			//position
			unsigned int v = i0;
			//uv
			unsigned int t = t0;

			do
			{   
				collapse_remap[v] = r1;
				v = wedge[v];

				collapse_remap_uv[t] = u1;
				t = wedge_uv[t];
				
			} while (v != i0);
		}
		else if (vertex_kind[i0] == Kind_Seam)
		{

			//remap v0 to v1 and seam pair of v0 to seam pair of v1
			unsigned int s0 = wedge[i0];
			unsigned int s1 = wedge[i1];

			unsigned int q0 = wedge_uv[t0];
			unsigned int q1 = wedge_uv[t1];

			assert(s0 != i0 && s1 != i1);
			assert(wedge[s0] == i0 && wedge[s1] == i1);

			assert(q0 != t0 && q1 != t1);
			assert(wedge_uv[q0] == t0 && wedge_uv[q1] == t1);

			collapse_remap[i0] = i1;
			collapse_remap[s0] = s1;

			collapse_remap_uv[t0] = t1;
			collapse_remap_uv[q0] = q1;
			
		}
		else
		{
			assert(wedge[i0] == i0);
			collapse_remap[i0] = i1;

			assert(wedge_uv[t0] == t0);
			collapse_remap_uv[t0] = t1;
		}
		
		collapse_locked[r0] = 1;
		collapse_locked[r1] = 1;

		collapse_locked_uv[u0] = 1;
		collapse_locked_uv[u1] = 1;

		// border edges collapse 1 triangle, other edges collapse 2 or more
		triangle_collapses += (vertex_kind[i0] == Kind_Border || vertex_kind[i0] == Kind_Border_Fake) ? 1 : 2;
		edge_collapses++;

		result_error = result_error < c.error ? c.error : result_error;
		cout_collapse++;
	}
	//std::cout<<"collaspse count: "<<cout_collapse<<std::endl;

#if TRACE
	float error_goal_perfect = edge_collapse_goal < collapse_count ? collapses[collapse_order[edge_collapse_goal]].error : 0.f;

	printf("removed %d triangles, error %e (goal %e); evaluated %d/%d collapses (done %d, skipped %d, invalid %d)\n",
		int(triangle_collapses), sqrtf(result_error), sqrtf(error_goal_perfect),
		int(stats[0]), int(collapse_count), int(edge_collapses), int(stats[1]), int(stats[2]));
#endif

	return edge_collapses;
}

static size_t remapIndexBuffer(unsigned int* indices, size_t index_count, const unsigned int* collapse_remap)
{
	size_t write = 0;

	for (size_t i = 0; i < index_count; i += 3)
	{
		unsigned int v0 = collapse_remap[indices[i + 0]];
		unsigned int v1 = collapse_remap[indices[i + 1]];
		unsigned int v2 = collapse_remap[indices[i + 2]];

		// we never move the vertex twice during a single pass
		assert(collapse_remap[v0] == v0);
		assert(collapse_remap[v1] == v1);
		assert(collapse_remap[v2] == v2);

		if (v0 != v1 && v0 != v2 && v1 != v2)
		{
			indices[write + 0] = v0;
			indices[write + 1] = v1;
			indices[write + 2] = v2;
			write += 3;
		}
	}

	return write;
}

static size_t remapIndexBufferTex(unsigned int* indices, size_t index_count, const unsigned int* collapse_remap,
									unsigned int* indices_uv, const unsigned int* collapse_remap_uv)
{
	size_t write = 0;

	for (size_t i = 0; i < index_count; i += 3)
	{
		unsigned int v0 = collapse_remap[indices[i + 0]];
		unsigned int v1 = collapse_remap[indices[i + 1]];
		unsigned int v2 = collapse_remap[indices[i + 2]];

		unsigned int u0 = collapse_remap_uv[indices_uv[i + 0]];
		unsigned int u1 = collapse_remap_uv[indices_uv[i + 1]];
		unsigned int u2 = collapse_remap_uv[indices_uv[i + 2]];

		// we never move the vertex twice during a single pass
		assert(collapse_remap[v0] == v0);
		assert(collapse_remap[v1] == v1);
		assert(collapse_remap[v2] == v2);

		assert(collapse_remap_uv[u0] == u0);
		assert(collapse_remap_uv[u1] == u1);
		assert(collapse_remap_uv[u2] == u2);

		if (v0 != v1 && v0 != v2 && v1 != v2 && u0 != u1 && u0 != u2 && u2 != u0 )
		{
			indices[write + 0] = v0;
			indices[write + 1] = v1;
			indices[write + 2] = v2;
			

			indices_uv[write + 0] = u0;
			indices_uv[write + 1] = u1;
			indices_uv[write + 2] = u2;

			write += 3;
		}
	}

	return write;
}

// Didier
// Keep track of overall simplification remap by composition at each pass
static void updateSimplificationRemap(unsigned int *simplification_remap, size_t vertex_count, const unsigned int *collapse_remap)
{
	for (size_t i = 0; i < vertex_count; ++i)
	{
		unsigned int idx = simplification_remap[i];
		simplification_remap[i] = collapse_remap[idx];
		
	}	
	
}

static void remapEdgeLoops(unsigned int* loop, size_t vertex_count, const unsigned int* collapse_remap)
{
	for (size_t i = 0; i < vertex_count; ++i)
	{
		if (loop[i] != ~0u)
		{
			unsigned int l = loop[i];
			unsigned int r = collapse_remap[l];

			// i == r is a special case when the seam edge is collapsed in a direction opposite to where loop goes
			loop[i] = (i == r) ? loop[l] : r;
		}
	}
}

void GetFakeBorderKind(unsigned char* vertex_kind, meshopt_tex::Vector3* position, size_t vertex_count){
	for(size_t i = 0; i < vertex_count; ++i){
		if(vertex_kind[i] == Kind_Border){
		//printf("border vertex: %f, %f, %f\n", position[i].x, position[i].y, position[i].z);
		if((position[i].x > 0.1 && position[i].x < 0.9) && 
			(position[i].y > 0.1 && position[i].y < 0.9) &&
			(position[i].z > 0.1 && position[i].z < 0.9))
			vertex_kind[i] = Kind_Border_Fake;
		}
	}
}


} 

//#ifndef NDEBUG
unsigned char* meshopt_uv_simplifyDebugKind = 0;
unsigned int* meshopt_uv_simplifyDebugLoop = 0;
unsigned int* meshopt_uv_simplifyDebugLoopBack = 0;
//#endif

/* Mesh simplifictaion with texture,
   destination_position: the result vertex index
   unsigned int* desination_pos: final reslut
   unsigned int* simplification_remap: simplification remap
   unsigned int* indices_pos: position index
   size_t index_count: index count number
   const unsigned int* indices_uv: uv index
   const float* vertex_positions_data: positions data
   size_t vertex_count: position vertex count
   const float* uv_data: uv coordinates data
   size_t uv_count: texture coordinates count
   size_t vertex_positions_stride: position stride
   size_t uv_stride: uv stride
   size_t target_index_count: target number of mesh simplification
   float target error: mesh simplification error
   float *out_result_error: 
*/
size_t meshopt_simplify_mod_texDebug(unsigned int* destination_pos, unsigned int* destination_uv, unsigned int* simplification_remap, 
        unsigned int* simplification_remap_uv, const unsigned int* indices_pos, size_t index_count, 
		const unsigned int* indices_uv, size_t index_uv_count, const float* vertex_positions_data, size_t vertex_count, const float* uv_data,
		size_t uv_count, int vertex_positions_stride, int uv_stride, size_t target_index_count, 
		float target_error, float* out_result_error, float block_extend, float block_bottom[3])
{
	using namespace meshopt_tex;
    size_t result_count_uv;
	//std::cout<<"Quadric 5 based mesh simpilification with texture"<<std::endl;
	assert(index_count % 3 == 0);
	assert(vertex_positions_stride > 0 && vertex_positions_stride <= 256);
	assert(vertex_positions_stride % sizeof(float) == 0);
	assert(target_index_count <= index_count);
	assert(uv_count == vertex_count);

	meshopt_Allocator allocator;

	unsigned int* result = destination_pos;
	unsigned int* result_uv = destination_uv;

	// build adjacency information
	EdgeAdjacency adjacency = {};
	prepareEdgeAdjacency(adjacency, index_count, vertex_count, allocator);
	updateEdgeAdjacency(adjacency, indices_pos, index_count, vertex_count, NULL);
	
	// build position remap that maps each vertex to the one with identical position
	unsigned int* remap = allocator.allocate<unsigned int>(vertex_count);
	unsigned int* wedge = allocator.allocate<unsigned int>(vertex_count);
	buildPositionRemap(remap, wedge, vertex_positions_data, vertex_count, vertex_positions_stride, allocator);

    //build uv remap that maps each vertex to the one with identical position
    unsigned int* remap_uv = allocator.allocate<unsigned int>(uv_count);
    unsigned int* wedge_uv = allocator.allocate<unsigned int>(uv_count);
    buildPositionRemap(remap_uv, wedge_uv, uv_data, uv_count, uv_stride, allocator);

	// buildPositionRemap(remap, wedge, vertex_positions_data, vertex_count, uv_data, uv_stride, vertex_positions_stride, allocator);
	// remap_uv = remap;
	// wedge_uv = wedge; 
	
	// Didier
	// init simplification remap
	if (simplification_remap != NULL){
        memcpy(simplification_remap, remap, vertex_count * sizeof(unsigned int));
        memcpy(simplification_remap_uv, remap_uv, uv_count * sizeof(unsigned int));
    }
	
	// classify vertices; vertex kind determines collapse rules, see kCanCollapse
	unsigned char* vertex_kind = allocator.allocate<unsigned char>(vertex_count);
	unsigned int* loop = allocator.allocate<unsigned int>(vertex_count);
	unsigned int* loopback = allocator.allocate<unsigned int>(vertex_count);
	
	classifyVertices(vertex_kind, loop, loopback, vertex_count, adjacency, remap, wedge);

	#if TRACE
	size_t unique_positions = 0;
	for (size_t i = 0; i < vertex_count; ++i)
		unique_positions += remap[i] == i;

	printf("position remap: %d vertices => %d positions\n", int(vertex_count), int(unique_positions));

	size_t kinds[Kind_Count] = {};
	for (size_t i = 0; i < vertex_count; ++i)
		kinds[vertex_kind[i]] += remap[i] == i;

	printf("kinds: manifold %d, border %d, fakeborder %d, seam %d, complex %d, locked %d\n",
	       int(kinds[Kind_Manifold]), int(kinds[Kind_Border]), int(kinds[Kind_Border_Fake]), int(kinds[Kind_Seam]), int(kinds[Kind_Complex]), int(kinds[Kind_Locked]));	   
	#endif

	Vector3* vertex_positions = allocator.allocate<Vector3>(vertex_count);
    Vector2* uv_positions = allocator.allocate<Vector2>(uv_count);
	// float extent = rescalePositionsTex(vertex_positions, uv_positions, vertex_positions_data, uv_data,
	// 							        vertex_count, vertex_positions_stride, uv_count, uv_stride);
	rescalePositions(vertex_positions, vertex_positions_data, vertex_count, vertex_positions_stride, block_extend, block_bottom);
	rescaleUV(uv_positions, uv_data, uv_count, uv_stride, block_extend);
	
	GetFakeBorderKind(vertex_kind, vertex_positions, vertex_count);
	
	target_error /= block_extend; 
	
	Quadric5* vertex_quadrics = allocator.allocate<Quadric5>(vertex_count);
	memset(vertex_quadrics, 0, vertex_count * sizeof(Quadric5));
	
	fillFaceQuadric5s(vertex_quadrics, indices_pos, indices_uv, index_count, vertex_positions, uv_positions, remap, remap_uv);
	fillEdgeQuadric5s(vertex_quadrics, indices_pos, indices_uv, index_count, vertex_positions, uv_positions, remap, remap_uv,
					  vertex_kind, loop, loopback);

	if (result != indices_pos)
		memcpy(result, indices_pos, index_count * sizeof(unsigned int));
	
	if (result_uv != indices_uv)
		memcpy(result_uv, indices_uv, index_uv_count * sizeof(unsigned int));

#if TRACE
	size_t pass_count = 0;
#endif

	Collapse* edge_collapses = allocator.allocate<Collapse>(index_count);
	unsigned int* collapse_order = allocator.allocate<unsigned int>(index_count);
	unsigned int* collapse_remap = allocator.allocate<unsigned int>(vertex_count);
	unsigned char* collapse_locked = allocator.allocate<unsigned char>(vertex_count);

    unsigned int* collapse_remap_uv = allocator.allocate<unsigned int>(uv_count);
    unsigned char* collapse_locked_uv = allocator.allocate<unsigned char>(uv_count);

	size_t result_count = index_count;
	float result_error = 0;

	// target_error input is linear; we need to adjust it to match quadricError units
	float error_limit = target_error * target_error;
	//std::cout<<"target index number: "<<target_index_count<<" result index: "<<result_count<<std::endl;
	while (result_count > target_index_count)
	{
		// note: throughout the simplification process adjacency structure reflects welded topology for result-in-progress
		updateEdgeAdjacency(adjacency, result, result_count, vertex_count, remap);

		size_t edge_collapse_count = pickEdgeCollapses(edge_collapses, result, indices_uv, result_count, remap, vertex_kind, loop);

		// no edges can be collapsed any more due to topology restrictions
		if (edge_collapse_count == 0)
			break;

		rankEdgeCollapses(edge_collapses, edge_collapse_count, vertex_positions, uv_positions, vertex_quadrics, remap);
		
#if TRACE > 1
		dumpEdgeCollapses(edge_collapses, edge_collapse_count, vertex_kind);
#endif

		sortEdgeCollapses(collapse_order, edge_collapses, edge_collapse_count);
		
		size_t triangle_collapse_goal = (result_count - target_index_count) / 3;

        //vertex position collapse remap 
		for (size_t i = 0; i < vertex_count; ++i)
			collapse_remap[i] = unsigned(i);

		memset(collapse_locked, 0, vertex_count);

        //uv coordinates
        for(size_t i = 0; i < uv_count; ++i)
            collapse_remap_uv[i] = unsigned(i);
        
        memset(collapse_locked_uv, 0, uv_count);

#if TRACE
		printf("pass %d: ", int(pass_count++));
#endif

		size_t collapses = performEdgeCollapses(collapse_remap, collapse_remap_uv, collapse_locked, collapse_locked_uv, vertex_quadrics, edge_collapses, edge_collapse_count, 
				collapse_order, remap, remap_uv, wedge, wedge_uv, vertex_kind, vertex_positions, uv_positions, adjacency, triangle_collapse_goal, error_limit, result_error);
		
		// no edges can be collapsed any more due to hitting the error limit or triangle collapse limit
		if (collapses == 0)
			break;

		// Didier
		// Keep track of global simplification map 
		if (simplification_remap != NULL | simplification_remap_uv != NULL){
            updateSimplificationRemap(simplification_remap, vertex_count, collapse_remap);
            updateSimplificationRemap(simplification_remap_uv, uv_count, collapse_remap_uv);
        }
		
        remapEdgeLoops(loop, vertex_count, collapse_remap);
		remapEdgeLoops(loopback, vertex_count, collapse_remap);
		
        // remapEdgeLoops(loop, uv_count, collapse_remap_uv);
		// remapEdgeLoops(loopback, uv_count, collapse_remap_uv);
		
		
		// size_t new_count = remapIndexBuffer(result, result_count, collapse_remap);
        // size_t new_count_uv = remapIndexBuffer(result_uv, result_count, collapse_remap_uv);
		size_t new_count = remapIndexBufferTex(result, result_count, collapse_remap, result_uv, collapse_remap_uv);
		
		assert(new_count < result_count);

		result_count = new_count;
        result_count_uv = new_count;
	}

	//destination = result;

#if TRACE
	printf("result: %d triangles, error: %e; total %d passes\n", int(result_count), sqrtf(result_error), int(pass_count));
#endif

#if TRACE > 1
	dumpLockedCollapses(result, result_count, vertex_kind);
#endif

#ifndef NDEBUG
	if (meshopt_uv_simplifyDebugKind)
		memcpy(meshopt_uv_simplifyDebugKind, vertex_kind, vertex_count);

	if (meshopt_uv_simplifyDebugLoop)
		memcpy(meshopt_uv_simplifyDebugLoop, loop, vertex_count * sizeof(unsigned int));

	if (meshopt_uv_simplifyDebugLoopBack)
		memcpy(meshopt_uv_simplifyDebugLoopBack, loopback, vertex_count * sizeof(unsigned int));
#endif

	// result_error is quadratic; we need to remap it back to linear
	if (out_result_error)
		*out_result_error = sqrtf(result_error);
	//std::cout<<"final count: "<<result_count<<std::endl;

	return result_count;
}
