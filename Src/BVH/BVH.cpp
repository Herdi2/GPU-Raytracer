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

void print_node_info(Array<BVHNode2> nodes) {
	
	IO::print(cpu_config.bvh_type == BVHType::BVH ? "BVH2 Node count: {}\n"_sv : "SBVH Node count: {}\n"_sv, nodes.size());
	// Per definition, since every space is split into two, or made a leaf
	IO::print(cpu_config.bvh_type == BVHType::BVH ? "BVH2 Average branching factor: 2 {}\n"_sv : "SBVH Average branching factor: 2{}\n"_sv); 
}

void print_node_info(Array<BVHNode4> nodes) {
	// Even though the amount of nodes loaded into the GPU is equal to
	// the amount of nodes for BVH2, the used nodes are less
	// Empty nodes are not removed from the node array
	size_t empty_node_count = 0;
	double child_count = 0;
	for (size_t i = 0; i < nodes.size(); i++) {
		const auto& node = nodes[i];
		if (node.get_count(0) == 0 && node.get_index(0) == 0 &&
			node.get_count(1) == 0 && node.get_index(1) == 0 &&
			node.get_count(2) == 0 && node.get_index(2) == 0 &&
			node.get_count(3) == 0 && node.get_index(3) == 0) {
			empty_node_count++;
		}
		else {
			child_count += node.get_child_count();
		}
	}
	IO::print("BVH4 Node count: {}\n"_sv, nodes.size() - empty_node_count);
	IO::print("BVH4 Average branching factor: {}\n"_sv, child_count / (nodes.size() - empty_node_count));
}

void print_node_info(Array<BVHNode8> nodes) {
	IO::print("BVH8 Node count: {}\n"_sv, nodes.size());
	double child_count = 0;
	for (size_t i = 0; i < nodes.size(); i++) {
		const auto& node = nodes[i];
		for (size_t j = 0; j < 8; j++) {
			if (node.meta[j] != 0) {
				child_count++;
			}
		}
	}
	IO::print("BVH8 Average branching factor: {}\n"_sv, child_count / nodes.size());
}

BVH2 BVH::create_from_triangles(const Array<Triangle> & triangles) {
	IO::print("Constructing BVH...\r"_sv);

	BVH2 bvh = BVH2(AlignedAllocator<64>::instance());

	// Only the SBVH uses SBVH as its starting point,
	// all other BVH types use the standard BVH as their starting point
	if (cpu_config.bvh_type == BVHType::SBVH) {
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
			print_node_info(bvh.nodes);
			return make_owned<BVH2>(std::move(bvh));
		case BVHType::SBVH: {
			print_node_info(bvh.nodes);
			return make_owned<BVH2>(std::move(bvh));
		}
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
		case BVHType::BVH8: {
			// Collapse binary BVH into 8-way Compressed Wide BVH
			OwnPtr<BVH8> bvh8 = make_owned<BVH8>();
			{
				ScopeTimer timer("BVH8 Converter"_sv);
				BVH8Converter(*bvh8.get(), bvh).convert();
			}
			print_node_info(bvh8.get()->nodes);
			return bvh8;
		}
		default: ASSERT_UNREACHABLE();
	}
	IO::exit(1);
}
