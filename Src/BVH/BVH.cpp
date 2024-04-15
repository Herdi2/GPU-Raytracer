#include "BVH.h"

#include "Core/IO.h"
#include "Core/Timer.h"
#include "Core/Allocators/AlignedAllocator.h"

#include "BVH/Builders/SAHBuilder.h"
#include "BVH/Builders/SBVHBuilder.h"
#include "BVH/Converters/BVH4Converter.h"
#include "BVH/Converters/BVH8Converter.h"

#include "BVH/BVHOptimizer.h"
#include <iostream>

#include "Core/IO.h"


void print_cwbvh(const BVH8& bvh, int node_index = 0) {
	const BVHNode8& node = bvh.nodes[node_index];

	for (int i = 0; i < 8; i++) {
		bool node_is_leaf = (node.meta[i] & 0b11111) < 24;
		if (node_is_leaf) {
			int first_triangle = node.meta[i] & 0b11111;

			for (int j = 0; j < __popcnt(node.meta[i] >> 5); j++) {
				printf("Node %i - Triangle: %i\n", node_index, node.base_index_triangle + first_triangle + j);
			}
		}
		else {
			int child_offset = node.meta[i] & 0b11111;
			int child_index = node.base_index_child + child_offset - 24;

			printf("Node %i - Child %i:\n", node_index, child_index);

			print_cwbvh(bvh, child_index);
		}
	}

	printf("\n");
}

void print_node_info(Array<BVHNode2> nodes) {
	String bvh_type = cpu_config.bvh_type == BVHType::BVH ? "BVH" : "SBVH";
	IO::print("{} Node count: {}\n"_sv, bvh_type, nodes.size());
	// Per definition, since every space is split into two, or made a leaf
	IO::print("{} Average branching factor: 2 {}\n"_sv, bvh_type); 
}

void print_node_info(Array<BVHNode4> nodes) {
	String bvh_type = cpu_config.bvh_type == BVHType::BVH4 ? "BVH4" : "SBVH4";
	double child_count = 0;
	for (size_t i = 0; i < nodes.size(); i++) {
		const auto& node = nodes[i];
		child_count += node.get_child_count();

	}
	IO::print("{} Node count: {}\n"_sv, bvh_type, nodes.size());
	IO::print("{} Average branching factor: {}\n"_sv, bvh_type, child_count / nodes.size());
}

void print_node_info(Array<BVHNode8> nodes) {
	String bvh_type = cpu_config.bvh_type == BVHType::BVH8 ? "BVH8" : "SBVH8";
	IO::print("{} Node count: {}\n"_sv, bvh_type, nodes.size());
	double child_count = 0;
	for (size_t i = 0; i < nodes.size(); i++) {
		const auto& node = nodes[i];
		for (size_t j = 0; j < 8; j++) {
			if (node.meta[j] != 0) {
				child_count++;
			}
		}
	}

	IO::print("{} Average branching factor: {}\n"_sv, bvh_type, child_count / nodes.size());
}

BVH2 BVH::create_from_triangles(const Array<Triangle> & triangles) {
	IO::print("Constructing BVH...\r"_sv);

	BVH2 bvh = BVH2(AlignedAllocator<64>::instance());

	// Only the SBVH uses SBVH as its starting point,
	// all other BVH types use the standard BVH as their starting point
	if (cpu_config.bvh_type == BVHType::SBVH || cpu_config.bvh_type == BVHType::SBVH4 || cpu_config.bvh_type == BVHType::SBVH8) {
		ScopeTimer timer("SBVH Construction"_sv);

		SBVHBuilder(bvh, triangles.size()).build(triangles);
	} else  {
		ScopeTimer timer("BVH Construction"_sv);

		SAHBuilder(bvh, triangles.size()).build(triangles);
	}

	if (cpu_config.enable_bvh_optimization) {
		BVHOptimizer::optimize(bvh);
	}

	return bvh;
}

OwnPtr<BVH> BVH::create_from_bvh2(BVH2 bvh) {
	switch (cpu_config.bvh_type) {
		case BVHType::BVH:
		case BVHType::SBVH: {
			print_node_info(bvh.nodes);
			return make_owned<BVH2>(std::move(bvh));
		}
		case BVHType::SBVH4:
		case BVHType::BVH4: {
			// Collapse binary BVH into 4-way BVH
			OwnPtr<BVH4> bvh4 = make_owned<BVH4>();
			{
				ScopeTimer timer("BVH4 Converter"_sv);
				BVH4Converter(*bvh4.get(), bvh).convert();
			}
			print_node_info(bvh4.get()->nodes);
			return bvh4;
		}
		case BVHType::SBVH8:
		case BVHType::BVH8: {
			// Collapse binary BVH into 8-way Compressed Wide BVH
			OwnPtr<BVH8> bvh8 = make_owned<BVH8>();
			{
				ScopeTimer timer("BVH8 Converter"_sv);
				BVH8Converter(*bvh8.get(), bvh).convert();
			}
			print_node_info(bvh8.get()->nodes);
			//print_cwbvh(*bvh8.get());
			return bvh8;
		}
		default: ASSERT_UNREACHABLE();
	}
	IO::exit(1);
}
