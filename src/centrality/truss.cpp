/*

  Copyright 2017 The Johns Hopkins University Applied Physics Laboratory LLC. All Rights Reserved.
  Copyright 2021 The igraph team.

  Truss algorithm for cohesive subgroups.

  Author: Alex Perrone
  Date: 2017-08-03
  Minor edits: The igraph team, 2021

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc.,  51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301 USA

*/

#include "igraph_adjlist.h"
#include "igraph_community.h"
#include "core/exceptions.h"
#include "igraph_error.h"
#include "igraph_interface.h"
#include "igraph_motifs.h"

#include <iostream>
#include <vector>
#include <unordered_set>

using namespace std;

/* helper functions */
static int igraph_truss_i_unpack(const igraph_vector_int_t *tri, long int num_triangles, igraph_vector_t *unpacked_tri);
static void igraph_truss_i_compute_support(const igraph_vector_t *eid, igraph_vector_int_t *support);
static int igraph_i_trussness(const igraph_t *graph, igraph_vector_int_t *support,
  igraph_vector_int_t *trussness);

/**
 * \function trussness
 * \brief Find the "trussness" for every edge in the graph. This
 * indicates the highest k-truss that the edge occurs in.
 *
  * </para><para>
 * A k-truss is a subgraph in which every edge occurs in at least k-2 triangles in the subgraph.
 *
 * </para><para>
 * This function returns the highest k for each edge. If you are interested in
 * a particular k-truss subgraph, you can subset the graph using to those eids
 * which are >= k because each k-truss is a subgraph of a (k–1)-truss
 * (thus to get * all 4-trusses, take k >= 4 because the 5-trusses, 6-trusses, etc need to be included).
 *
 * </para><para>The current implementation of this function iteratively
 * decrements support of each edge using O(|E|) space and O(|E|^1.5) time.
 *
 * \param graph The input graph.
 * \param truss Pointer to initialized vector of truss values that will
 * indicate the highest k-truss each edge occurs in. It will be resized as
 * needed.
 * \return Error code.
 *
 * Time complexity: It should be O(|E|^1.5). See Algorithm 2 in:
 * Wang, Jia, and James Cheng. "Truss decomposition in massive networks."
 * Proceedings of the VLDB Endowment 5.9 (2012): 812-823.
*
 */
int igraph_trussness(const igraph_t* graph, igraph_vector_int_t* trussness){
  long int num_triangles;
  igraph_vector_int_t triangles, support;
  igraph_vector_t eid, unpacked_triangles;

  /* Manage the stack to make it memory safe: do not change the order of
   * initialization of the following four vectors */
  IGRAPH_VECTOR_INT_INIT_FINALLY(&support, igraph_ecount(graph));
  IGRAPH_VECTOR_INIT_FINALLY(&eid, 0);
  IGRAPH_VECTOR_INIT_FINALLY(&unpacked_triangles, 0);
  IGRAPH_VECTOR_INT_INIT_FINALLY(&triangles, 0);

  // List the triangles as vertex triplets.
  IGRAPH_CHECK(igraph_list_triangles(graph, &triangles));
  num_triangles = igraph_vector_int_size(&triangles);

  // Unpack the triangles from vertex list to edge list.
  IGRAPH_CHECK(igraph_truss_i_unpack(&triangles, num_triangles, &unpacked_triangles));
  igraph_vector_int_destroy(&triangles);
  IGRAPH_FINALLY_CLEAN(1);

  // Get the edge ids of the unpacked triangles. Note: a given eid can occur
  // multiple times in this list if it is in multiple triangles.
  IGRAPH_CHECK(igraph_get_eids(graph, &eid, &unpacked_triangles, 0, 0, 1));
  igraph_vector_destroy(&unpacked_triangles);
  IGRAPH_FINALLY_CLEAN(1);

  // Compute the support of the edges.
  igraph_truss_i_compute_support(&eid, &support);
  igraph_vector_destroy(&eid);
  IGRAPH_FINALLY_CLEAN(1);

  // Compute the trussness of the edges.
  IGRAPH_CHECK(igraph_i_trussness(graph, &support, trussness));
  igraph_vector_int_destroy(&support);
  IGRAPH_FINALLY_CLEAN(1);

  return IGRAPH_SUCCESS;
}

// Unpack the triangles as a vector of vertices to be a vector of edges.
// So, instead of the triangle specified as vertices [1, 2, 3], return the
// edges as [1, 2, 1, 3, 2, 3] so that the support can be computed.
static int igraph_truss_i_unpack(const igraph_vector_int_t *tri, long int num_triangles, igraph_vector_t *unpacked_tri) {
  long int i, j;

  IGRAPH_CHECK(igraph_vector_resize(unpacked_tri, 2 * num_triangles));

  for (i = 0, j = 0; i < num_triangles; i += 3, j += 6){
    VECTOR(*unpacked_tri)[j]   = VECTOR(*unpacked_tri)[j+2] = VECTOR(*tri)[i];
    VECTOR(*unpacked_tri)[j+1] = VECTOR(*unpacked_tri)[j+4] = VECTOR(*tri)[i+1];
    VECTOR(*unpacked_tri)[j+3] = VECTOR(*unpacked_tri)[j+5] = VECTOR(*tri)[i+2];
  }

  return IGRAPH_SUCCESS;
}

// Compute the edge support, i.e. number of triangles each edge occurs in.
// Time complexity: O(m), where m is the number of edges listed in eid.
static void igraph_truss_i_compute_support(const igraph_vector_t *eid, igraph_vector_int_t *support) {
  long int m = igraph_vector_size(eid);
  for (long int i = 0; i < m; ++i){
    VECTOR(*support)[(long int) VECTOR(*eid)[i]] += 1;
  }
}


/* internal function doing the computations once the support is defined */
static int igraph_i_trussness(const igraph_t *graph, igraph_vector_int_t *support,
  igraph_vector_int_t *trussness){

  igraph_integer_t fromVertex, toVertex, e1, e2;
  igraph_adjlist_t adjlist;
  igraph_vector_int_t *fromNeighbors, *toNeighbors, *q1, *q2;
  igraph_vector_int_t commonNeighbors;
  bool e1_complete, e2_complete;
  int j, n, level, newLevel, max;
  long int i, seed, ncommon, nedges;

  // C++ data structures
  vector<bool> completed;
  vector< unordered_set<long int> > vec;
  unordered_set<long int>::iterator it;

  // Allocate memory for result
  nedges = igraph_vector_int_size(support);
  IGRAPH_CHECK(igraph_vector_int_resize(trussness, nedges));
  if (nedges == 0)
      return IGRAPH_SUCCESS;

  // Get max possible value = max entry in support.
  // This cannot be computed if there are no edges, hence the above if
  max = igraph_vector_int_max(support);

  // Initialize completed edges.
  IGRAPH_HANDLE_EXCEPTIONS(completed.resize(nedges));

  // The vector of levels. Each level of the vector is a set of edges initially
  // at that level of support, where support is # of triangles the edge is in.
  IGRAPH_HANDLE_EXCEPTIONS(vec.resize(max + 1));

  // Add each edge to its appropriate level of support.
  for (i = 0; i < nedges; ++i) {
    IGRAPH_HANDLE_EXCEPTIONS(vec[VECTOR(*support)[i]].insert(i));  // insert edge i into its support level
  }


  // Record the trussness of edges at level 0. These edges are not part
  // of any triangles, so there's not much to do and we "complete" them
  for (it = vec[0].begin(); it != vec[0].end(); ++it){
    VECTOR(*trussness)[*it] = 2;
    completed[*it] = 1;
  }

  // Initialize variables needed below.
  IGRAPH_CHECK(igraph_adjlist_init(graph, &adjlist, IGRAPH_ALL, IGRAPH_NO_LOOPS,
                                   IGRAPH_MULTIPLE));
  IGRAPH_FINALLY(igraph_adjlist_destroy, &adjlist);
  IGRAPH_VECTOR_INT_INIT_FINALLY(&commonNeighbors, 0);

  /* Sort each vector of neighbors, so it's done once for all */
  igraph_adjlist_sort(&adjlist);

  // Move through the levels, one level at a time, starting at first level.
  for (level = 1; level <= max; ++level){

    /* Track down edges one at a time */
    while (!vec[level].empty()){
      seed = *vec[level].begin();  // pull out the first edge
      vec[level].erase(seed);  // remove the first element

      /* Find the vertices of this edge */
      igraph_edge(graph, seed, &fromVertex, &toVertex);

      /* Find neighbors of both vertices. If they run into each other,
       * there is a triangle. Because we sorted the adjacency list already,
       * we don't need to sort it for every edge here */
      fromNeighbors = igraph_adjlist_get(&adjlist, fromVertex);
      toNeighbors = igraph_adjlist_get(&adjlist, toVertex);
      q1 = fromNeighbors;
      q2 = toNeighbors;

      if (igraph_vector_int_size(q1) > igraph_vector_int_size(q2)){
        // case: #fromNeighbors > #toNeigbors, so make q1 the smaller set.
        q1 = toNeighbors;
        q2 = fromNeighbors;
      }

      // Intersect the neighbors.
      igraph_vector_int_intersect_sorted(q1, q2, &commonNeighbors);

      /* Go over the overlapping neighbors and check each */
      ncommon = igraph_vector_int_size(&commonNeighbors);
      for (j = 0; j < ncommon; j++){
        n = VECTOR(commonNeighbors)[j];  // the common neighbor
        igraph_get_eid(graph, &e1, fromVertex, n, IGRAPH_UNDIRECTED, 1);
        igraph_get_eid(graph, &e2, toVertex, n, IGRAPH_UNDIRECTED, 1);

        e1_complete = completed[e1] == 1;
        e2_complete = completed[e2] == 1;

        if (!e1_complete && !e2_complete){
          // Demote this edge, if higher than current level.
          if (VECTOR(*support)[e1] > level){
            VECTOR(*support)[e1] -= 1;  // decrement the level
            newLevel = VECTOR(*support)[e1];
            vec[newLevel].insert(e1);
            vec[newLevel + 1].erase(e1);  // the old level
          }
          // Demote this edge, if higher than current level.
          if (VECTOR(*support)[e2] > level){
            VECTOR(*support)[e2] -= 1;  // decrement the level
            newLevel = VECTOR(*support)[e2];
            vec[newLevel].insert(e2);
            vec[newLevel + 1].erase(e2);  // the old level
          }
        }
      }
      // Record this edge; its level is its trussness.
      VECTOR(*trussness)[seed] = level + 2;
      completed[seed] = 1; // mark as complete
      igraph_vector_int_clear(&commonNeighbors);
    }  // end while
  }  // end for-loop over levels

  // Clean up.
  igraph_vector_int_destroy(&commonNeighbors);
  igraph_adjlist_destroy(&adjlist);
  IGRAPH_FINALLY_CLEAN(2);

  return IGRAPH_SUCCESS;
}
