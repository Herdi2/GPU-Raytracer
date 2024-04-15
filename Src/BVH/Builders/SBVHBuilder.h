#pragma once
#include "BVH/BVH.h"
#include "BVHPartitions.h"

#include "Core/Array.h"
#include "Core/BitArray.h"

struct PrimitiveRef;

struct SBVHBuilder {
	BVH2 & sbvh;

	Array<PrimitiveRef> indices[3];

	// Scatch memory
	Array<float> sah;
	BitArray indices_going_left;

	long split_ratio[2] = { 0, 0 }; // split_ratio[0] = amount of object splits, split_ratio[1] = amount of spatial splits

	float inv_root_surface_area;

	SBVHBuilder(BVH2 & sbvh, size_t triangle_count) : sbvh(sbvh), sah(triangle_count), indices_going_left(triangle_count) { }

	long * build(const Array<Triangle> & triangles); // SAH-based object + spatial splits, Stich et al. 2009 (Triangles only)

private:
	int build_sbvh(int node_index, const Array<Triangle> & triangles, int first_index, int index_count);
};
