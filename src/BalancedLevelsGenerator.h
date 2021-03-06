/***************************************************************************
 *  Thunderegg, a library for solving Poisson's equation on adaptively 
 *  refined block-structured Cartesian grids
 *
 *  Copyright (C) 2019  Thunderegg Developers. See AUTHORS.md file at the
 *  top-level directory.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 ***************************************************************************/

#ifndef BALANCELEVELGENERATOR_H
#define BALANCELEVELGENERATOR_H
#include "Domain.h"
#include "OctTree.h"
#include <deque>
#include <iostream>
#include <memory>
#include <mpi.h>
#include <set>
#include <vector>
#include <zoltan.h>
template <size_t D> class BalancedLevelsGenerator
{
	private:
	void extractLevel(const Tree<D> &t, int level, int n);
	void balanceLevel(int level);
	void balanceLevelWithLower(int level);

	public:
	using DomainMap = std::map<int, std::shared_ptr<Domain<D>>>;
	std::vector<DomainMap> levels;
	BalancedLevelsGenerator(const Tree<D> &t, int n)
	{
		levels.resize(t.num_levels);
		for (int i = 1; i <= t.num_levels; i++) {
			extractLevel(t, i, n);
		}
	}
	void zoltanBalance()
	{
		balanceLevel(levels.size());
		for (int i = levels.size() - 1; i >= 1; i--) {
			balanceLevelWithLower(i);
		}
	}
};

template <size_t D>
inline void BalancedLevelsGenerator<D>::extractLevel(const Tree<D> &t, int level, int nx)
{
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	if (rank == 0) {
		Node<D>         child = *t.levels.at(level);
		std::deque<int> q;
		std::set<int>   qed;
		q.push_back(child.id);
		qed.insert(child.id);

		while (!q.empty()) {
			std::shared_ptr<Domain<D>> d_ptr(new Domain<D>());
			Domain<D> &                d = *d_ptr;
			Node<D>                    n = t.nodes.at(q.front());
			q.pop_front();

			d.n            = nx;
			d.id           = n.id;
			d.lengths      = n.lengths;
			d.starts       = n.starts;
			d.child_id     = n.child_id;
			d.refine_level = n.level;
			if (n.level < level) {
				d.parent_id = n.id;
			} else {
				d.parent_id = n.parent;
			}
			if (d.parent_id != -1) {
				d.oct_on_parent = 0;
				while (t.nodes.at(n.parent).child_id[d.oct_on_parent] != n.id) {
					d.oct_on_parent++;
				}
			}

			// set and enqueue nbrs
			for (Side<D> s : Side<D>::getValues()) {
				if (n.nbrId(s) == -1 && n.parent != -1 && t.nodes.at(n.parent).nbrId(s) != -1) {
					Node<D> parent = t.nodes.at(n.parent);
					Node<D> nbr    = t.nodes.at(parent.nbrId(s));
					auto    octs   = Orthant<D>::getValuesOnSide(s);
					int     quad   = 0;
					while (parent.childId(octs[quad]) != n.id) {
						quad++;
					}
					d.getNbrInfoPtr(s) = new CoarseNbrInfo<D>(nbr.id, quad);
					if (!qed.count(nbr.id)) {
						q.push_back(nbr.id);
						qed.insert(nbr.id);
					}
				} else if (n.level < level && n.nbrId(s) != -1
				           && t.nodes.at(n.nbrId(s)).hasChildren()) {
					Node<D> nbr  = t.nodes.at(n.nbrId(s));
					auto    octs = Orthant<D>::getValuesOnSide(s.opposite());
					std::array<int, Orthant<D>::num_orthants / 2> nbr_ids;
					for (size_t i = 0; i < Orthant<D>::num_orthants / 2; i++) {
						int id     = nbr.childId(octs[i]);
						nbr_ids[i] = id;
						if (!qed.count(id)) {
							q.push_back(id);
							qed.insert(id);
						}
					}
					d.getNbrInfoPtr(s) = new FineNbrInfo<D>(nbr_ids);
				} else if (n.nbrId(s) != -1) {
					int id = n.nbrId(s);
					if (!qed.count(id)) {
						q.push_back(id);
						qed.insert(id);
					}
					d.getNbrInfoPtr(s) = new NormalNbrInfo<D>(id);
				}
			}
			levels[level - 1][d.id] = d_ptr;
		}
	}
	for (auto &p : levels[level - 1]) {
		p.second->setPtrs(levels[level - 1]);
	}
}
template <size_t D> inline void BalancedLevelsGenerator<D>::balanceLevel(int level)
{
	struct Zoltan_Struct *zz = Zoltan_Create(MPI_COMM_WORLD);

	// parameters
	Zoltan_Set_Param(zz, "LB_METHOD", "HYPERGRAPH");  /* Zoltan method: HYPERGRAPH */
	Zoltan_Set_Param(zz, "LB_APPROACH", "PARTITION"); /* Zoltan method: "BLOCK" */
	Zoltan_Set_Param(zz, "NUM_GID_ENTRIES", "1");     /* global ID is 1 integer */
	Zoltan_Set_Param(zz, "NUM_LID_ENTRIES", "0");     /* don't use local IDs */
	Zoltan_Set_Param(zz, "OBJ_WEIGHT_DIM", "0");      /* we omit object weights */
	Zoltan_Set_Param(zz, "AUTO_MIGRATE", "FALSE");    /* we omit object weights */
	Zoltan_Set_Param(zz, "DEBUG_LEVEL", "0");         /* we omit object weights */

	// Query functions
	// Number of Vertices
	auto numObjFn = [](void *data, int *ierr) -> int {
		DomainMap &map = *(DomainMap *) data;
		*ierr          = ZOLTAN_OK;
		return map.size();
	};
	Zoltan_Set_Num_Obj_Fn(zz, numObjFn, &levels[level - 1]);

	// List of vertices
	auto objListFn
	= [](void *data, int num_gid_entries, int num_lid_entries, ZOLTAN_ID_PTR global_ids,
	     ZOLTAN_ID_PTR local_ids, int wgt_dim, float *obj_wgts, int *ierr) {
		  DomainMap &map = *(DomainMap *) data;
		  *ierr          = ZOLTAN_OK;
		  int pos        = 0;
		  for (auto p : map) {
			  global_ids[pos] = p.first;
			  pos++;
		  }
	  };
	Zoltan_Set_Obj_List_Fn(zz, objListFn, &levels[level - 1]);

	// Construct hypergraph
	struct CompressedVertex {
		std::vector<int> vertices;
		std::vector<int> ptrs;
		std::vector<int> edges;
	};
	CompressedVertex graph;
	for (auto &p : levels[level - 1]) {
		Domain<D> &d = *p.second;
		graph.vertices.push_back(d.id);
		graph.ptrs.push_back(graph.edges.size());
		for (Side<D> s : Side<D>::getValues()) {
			int edge_id = -1;
			if (d.hasNbr(s)) {
				switch (d.getNbrType(s)) {
					case NbrType::Normal:
						if (s.isLowerOnAxis()) {
							edge_id = d.getNormalNbrInfo(s).id;
						} else {
							edge_id = d.id;
						}
						break;
					case NbrType::Fine:
						edge_id = d.id;
						break;
					case NbrType::Coarse:
						edge_id = d.getCoarseNbrInfo(s).id;
						break;
				}
			}
			graph.edges.push_back(edge_id * Side<D>::num_sides + s.toInt());
		}
	}

	// set graph functions
	Zoltan_Set_HG_Size_CS_Fn(zz,
	                         [](void *data, int *num_lists, int *num_pins, int *format, int *ierr) {
		                         CompressedVertex &graph = *(CompressedVertex *) data;
		                         *ierr                   = ZOLTAN_OK;
		                         *num_lists              = graph.vertices.size();
		                         *num_pins               = graph.edges.size();
		                         *format                 = ZOLTAN_COMPRESSED_VERTEX;
	                         },
	                         &graph);
	Zoltan_Set_HG_CS_Fn(zz,
	                    [](void *data, int num_gid_entries, int num_vtx_edge, int num_pins,
	                       int format, ZOLTAN_ID_PTR vtxedge_GID, int *vtxedge_ptr,
	                       ZOLTAN_ID_PTR pin_GID, int *ierr) {
		                    CompressedVertex &graph = *(CompressedVertex *) data;
		                    *ierr                   = ZOLTAN_OK;
		                    copy(graph.vertices.begin(), graph.vertices.end(), vtxedge_GID);
		                    copy(graph.ptrs.begin(), graph.ptrs.end(), vtxedge_ptr);
		                    copy(graph.edges.begin(), graph.edges.end(), pin_GID);
	                    },
	                    &graph);

	Zoltan_Set_Obj_Size_Fn(zz,
	                       [](void *data, int num_gid_entries, int num_lid_entries,
	                          ZOLTAN_ID_PTR global_id, ZOLTAN_ID_PTR local_id, int *ierr) {
		                       DomainMap &map = *(DomainMap *) data;
		                       *ierr          = ZOLTAN_OK;
		                       return map[*global_id]->serialize(nullptr);
	                       },
	                       &levels[level - 1]);
	Zoltan_Set_Pack_Obj_Fn(zz,
	                       [](void *data, int num_gid_entries, int num_lid_entries,
	                          ZOLTAN_ID_PTR global_id, ZOLTAN_ID_PTR local_id, int dest, int size,
	                          char *buf, int *ierr) {
		                       DomainMap &map = *(DomainMap *) data;
		                       *ierr          = ZOLTAN_OK;
		                       map[*global_id]->serialize(buf);
		                       map.erase(*global_id);
	                       },
	                       &levels[level - 1]);
	Zoltan_Set_Unpack_Obj_Fn(
	zz,
	[](void *data, int num_gid_entries, ZOLTAN_ID_PTR global_id, int size, char *buf, int *ierr) {
		DomainMap &map = *(DomainMap *) data;
		*ierr          = ZOLTAN_OK;
		map[*global_id].reset(new Domain<D>());
		map[*global_id]->deserialize(buf);
	},
	&levels[level - 1]);
	// Zoltan_Set_Obj_Size_Multi_Fn(zz, DomainZoltanHelper::object_sizes, this);
	// Zoltan_Set_Pack_Obj_Multi_Fn(zz, DomainZoltanHelper::pack_objects, this);
	// Zoltan_Set_Unpack_Obj_Multi_Fn(zz, DomainZoltanHelper::unpack_objects, this);

	////////////////////////////////////////////////////////////////
	// Zoltan can now partition the objects in this collection.
	// In this simple example, we assume the number of partitions is
	// equal to the number of processes.  Process rank 0 will own
	// partition 0, process rank 1 will own partition 1, and so on.
	////////////////////////////////////////////////////////////////

	int           changes;
	int           numGidEntries;
	int           numLidEntries;
	int           numImport;
	ZOLTAN_ID_PTR importGlobalIds;
	ZOLTAN_ID_PTR importLocalIds;
	int *         importProcs;
	int *         importToPart;
	int           numExport;
	ZOLTAN_ID_PTR exportGlobalIds;
	ZOLTAN_ID_PTR exportLocalIds;
	int *         exportProcs;
	int *         exportToPart;

	int rc = Zoltan_LB_Partition(zz, &changes, &numGidEntries, &numLidEntries, &numImport,
	                             &importGlobalIds, &importLocalIds, &importProcs, &importToPart,
	                             &numExport, &exportGlobalIds, &exportLocalIds, &exportProcs,
	                             &exportToPart);

	// update ranks of neighbors before migrating
	for (int i = 0; i < numExport; i++) {
		int curr_id   = exportGlobalIds[i];
		int dest_rank = exportProcs[i];
		levels[level - 1][curr_id]->updateRank(dest_rank);
	}

	rc = Zoltan_Migrate(zz, numImport, importGlobalIds, importLocalIds, importProcs, importToPart,
	                    numExport, exportGlobalIds, exportLocalIds, exportProcs, exportToPart);

	if (rc != ZOLTAN_OK) {
		std::cerr << "zoltan error\n";
		Zoltan_Destroy(&zz);
		exit(0);
	}
	Zoltan_Destroy(&zz);
#if DD_DEBUG
	std::cout << "I have " << levels[level - 1].size() << " domains: ";

	int  prev  = -100;
	bool range = false;
	for (auto &p : levels[level - 1]) {
		int curr = p.second->id;
		if (curr != prev + 1 && !range) {
			std::cout << curr << "-";
			range = true;
		} else if (curr != prev + 1 && range) {
			std::cout << prev << " " << curr << "-";
		}
		prev = curr;
	}

	std::cout << prev << "\n";
	std::cout << std::endl;
#endif
	for (auto &p : levels[level - 1]) {
		p.second->setPtrs(levels[level - 1]);
	}
}
template <size_t D> inline void BalancedLevelsGenerator<D>::balanceLevelWithLower(int level)
{
	struct Zoltan_Struct *zz = Zoltan_Create(MPI_COMM_WORLD);

	// parameters
	Zoltan_Set_Param(zz, "LB_METHOD", "HYPERGRAPH");  /* Zoltan method: HYPERGRAPH */
	Zoltan_Set_Param(zz, "LB_APPROACH", "PARTITION"); /* Zoltan method: "BLOCK" */
	Zoltan_Set_Param(zz, "NUM_GID_ENTRIES", "1");     /* global ID is 1 integer */
	Zoltan_Set_Param(zz, "NUM_LID_ENTRIES", "0");     /* don't use local IDs */
	Zoltan_Set_Param(zz, "OBJ_WEIGHT_DIM", "1");      /* we omit object weights */
	Zoltan_Set_Param(zz, "EDGE_WEIGHT_DIM", "0");     /* we omit object weights */
	Zoltan_Set_Param(zz, "AUTO_MIGRATE", "FALSE");    /* we omit object weights */
	Zoltan_Set_Param(zz, "DEBUG_LEVEL", "0");         /* we omit object weights */

	// Query functions
	// Number of Vertices
	struct Levels {
		DomainMap *upper;
		DomainMap *lower;
	};
	Levels levels = {&this->levels[level - 1], &this->levels[level]};
	Zoltan_Set_Num_Obj_Fn(zz,
	                      [](void *data, int *ierr) -> int {
		                      Levels *levels = (Levels *) data;
		                      *ierr          = ZOLTAN_OK;
		                      return levels->upper->size() + levels->lower->size();
	                      },
	                      &levels);

	// List of vertices
	Zoltan_Set_Obj_List_Fn(zz,
	                       [](void *data, int num_gid_entries, int num_lid_entries,
	                          ZOLTAN_ID_PTR global_ids, ZOLTAN_ID_PTR local_ids, int wgt_dim,
	                          float *obj_wgts, int *ierr) {
		                       Levels *levels = (Levels *) data;
		                       *ierr          = ZOLTAN_OK;
		                       int pos        = 0;
		                       for (auto p : *levels->upper) {
			                       global_ids[pos] = p.first;
			                       obj_wgts[pos]   = 1;
			                       pos++;
		                       }
		                       for (auto p : *levels->lower) {
			                       global_ids[pos] = p.first;
			                       obj_wgts[pos]   = 0;
			                       pos++;
		                       }
	                       },
	                       &levels);

	// Construct hypergraph
	struct CompressedVertex {
		std::vector<int> vertices;
		std::vector<int> ptrs;
		std::vector<int> edges;
	};
	CompressedVertex graph;
	// process coarse level
	for (auto &p : this->levels[level - 1]) {
		Domain<D> &d = *p.second;
		graph.vertices.push_back(d.id);
		graph.ptrs.push_back(graph.edges.size());
		// patch to patch communication
		for (Side<D> s : Side<D>::getValues()) {
			int edge_id = -1;
			if (d.hasNbr(s)) {
				switch (d.getNbrType(s)) {
					case NbrType::Normal:
						if (s.isLowerOnAxis()) {
							edge_id = d.getNormalNbrInfo(s).id;
						} else {
							edge_id = d.id;
						}
						break;
					case NbrType::Fine:
						edge_id = d.id;
						break;
					case NbrType::Coarse:
						edge_id = d.getCoarseNbrInfo(s).id;
						break;
				}
			}
			graph.edges.push_back(edge_id * Side<D>::num_sides + s.toInt());
		}
		// level to level communication
		graph.edges.push_back(-d.id - 1);
	}
	// process fine level
	for (auto &p : this->levels[level]) {
		Domain<D> &d = *p.second;
		graph.vertices.push_back(d.id);
		graph.ptrs.push_back(graph.edges.size());
		graph.edges.push_back(-d.parent_id - 1);
	}

	// set graph functions
	Zoltan_Set_HG_Size_CS_Fn(zz,
	                         [](void *data, int *num_lists, int *num_pins, int *format, int *ierr) {
		                         CompressedVertex &graph = *(CompressedVertex *) data;
		                         *ierr                   = ZOLTAN_OK;
		                         *num_lists              = graph.vertices.size();
		                         *num_pins               = graph.edges.size();
		                         *format                 = ZOLTAN_COMPRESSED_VERTEX;
	                         },
	                         &graph);
	Zoltan_Set_HG_CS_Fn(zz,
	                    [](void *data, int num_gid_entries, int num_vtx_edge, int num_pins,
	                       int format, ZOLTAN_ID_PTR vtxedge_GID, int *vtxedge_ptr,
	                       ZOLTAN_ID_PTR pin_GID, int *ierr) {
		                    CompressedVertex &graph = *(CompressedVertex *) data;
		                    *ierr                   = ZOLTAN_OK;
		                    copy(graph.vertices.begin(), graph.vertices.end(), vtxedge_GID);
		                    copy(graph.ptrs.begin(), graph.ptrs.end(), vtxedge_ptr);
		                    copy(graph.edges.begin(), graph.edges.end(), pin_GID);
	                    },
	                    &graph);

	Zoltan_Set_Obj_Size_Fn(zz,
	                       [](void *data, int num_gid_entries, int num_lid_entries,
	                          ZOLTAN_ID_PTR global_id, ZOLTAN_ID_PTR local_id, int *ierr) {
		                       Levels *levels = (Levels *) data;
		                       *ierr          = ZOLTAN_OK;
		                       return levels->upper->at(*global_id)->serialize(nullptr);
	                       },
	                       &levels);

	// fixed objects
	Zoltan_Set_Num_Fixed_Obj_Fn(zz,
	                            [](void *data, int *ierr) -> int {
		                            Levels *levels = (Levels *) data;
		                            *ierr          = ZOLTAN_OK;
		                            return levels->lower->size();
	                            },
	                            &levels);

	Zoltan_Set_Fixed_Obj_List_Fn(zz,
	                             [](void *data, int num_fixed_obj, int num_gid_entries,
	                                ZOLTAN_ID_PTR fixed_gids, int *fixed_parts, int *ierr) {
		                             Levels *levels = (Levels *) data;
		                             *ierr          = ZOLTAN_OK;
		                             int rank;
		                             MPI_Comm_rank(MPI_COMM_WORLD, &rank);
		                             int pos = 0;
		                             for (auto p : *levels->lower) {
			                             fixed_gids[pos]  = p.first;
			                             fixed_parts[pos] = rank;
			                             pos++;
		                             }
	                             },
	                             &levels);
	// pack and unpack
	// pack and unpack
	Zoltan_Set_Pack_Obj_Fn(zz,
	                       [](void *data, int num_gid_entries, int num_lid_entries,
	                          ZOLTAN_ID_PTR global_id, ZOLTAN_ID_PTR local_id, int dest, int size,
	                          char *buf, int *ierr) {
		                       Levels *levels = (Levels *) data;
		                       *ierr          = ZOLTAN_OK;
		                       levels->upper->at(*global_id)->serialize(buf);
		                       levels->upper->erase(*global_id);
	                       },
	                       &levels);
	Zoltan_Set_Unpack_Obj_Fn(
	zz,
	[](void *data, int num_gid_entries, ZOLTAN_ID_PTR global_id, int size, char *buf, int *ierr) {
		Levels *levels = (Levels *) data;
		*ierr          = ZOLTAN_OK;
		(*levels->upper)[*global_id].reset(new Domain<D>());
		(*levels->upper)[*global_id]->deserialize(buf);
	},
	&levels);

	////////////////////////////////////////////////////////////////
	// Zoltan can now partition the objects in this collection.
	// In this simple example, we assume the number of partitions is
	// equal to the number of processes.  Process rank 0 will own
	// partition 0, process rank 1 will own partition 1, and so on.
	////////////////////////////////////////////////////////////////

	int           changes;
	int           numGidEntries;
	int           numLidEntries;
	int           numImport;
	ZOLTAN_ID_PTR importGlobalIds;
	ZOLTAN_ID_PTR importLocalIds;
	int *         importProcs;
	int *         importToPart;
	int           numExport;
	ZOLTAN_ID_PTR exportGlobalIds;
	ZOLTAN_ID_PTR exportLocalIds;
	int *         exportProcs;
	int *         exportToPart;

	int rc = Zoltan_LB_Partition(zz, &changes, &numGidEntries, &numLidEntries, &numImport,
	                             &importGlobalIds, &importLocalIds, &importProcs, &importToPart,
	                             &numExport, &exportGlobalIds, &exportLocalIds, &exportProcs,
	                             &exportToPart);

	// update ranks of neighbors before migrating
	for (int i = 0; i < numExport; i++) {
		int curr_id   = exportGlobalIds[i];
		int dest_rank = exportProcs[i];
		levels.upper->at(curr_id)->updateRank(dest_rank);
	}

	rc = Zoltan_Migrate(zz, numImport, importGlobalIds, importLocalIds, importProcs, importToPart,
	                    numExport, exportGlobalIds, exportLocalIds, exportProcs, exportToPart);

	if (rc != ZOLTAN_OK) {
		std::cerr << "zoltan error\n";
		Zoltan_Destroy(&zz);
		exit(0);
	}
	Zoltan_Destroy(&zz);
#if DD_DEBUG
	std::cout << "I have " << levels.upper->size() << " domains: ";

	int  prev  = -100;
	bool range = false;
	for (auto &p : *levels.upper) {
		int curr = p.second->id;
		if (curr != prev + 1 && !range) {
			std::cout << curr << "-";
			range = true;
		} else if (curr != prev + 1 && range) {
			std::cout << prev << " " << curr << "-";
		}
		prev = curr;
	}

	std::cout << prev << "\n";
	std::cout << std::endl;
#endif
	for (auto &p : this->levels[level - 1]) {
		p.second->setPtrs(this->levels[level - 1]);
	}
}
#endif
