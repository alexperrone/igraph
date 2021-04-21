/*

  Copyright 2017 The Johns Hopkins University Applied Physics Laboratory LLC. All Rights Reserved.

  Truss algorithm for cohesive subgroups.

  Author: Alex Perrone
  Date: 2017-08-03

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

#include <igraph.h>
/*
#include <truss.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
*/
#include "test_utilities.inc"

//void print_results(const igraph_t *graph, igraph_vector_int_t *v, FILE *f);

void print_and_destroy(igraph_t *graph, igraph_vector_int_t *truss) {
    long int i;
    igraph_integer_t from, to;
    printf("fromNode,toNode,truss\n");
    for (i=0; i < igraph_vector_int_size(truss); i++) {
        igraph_edge(graph, i, &from, &to);
        printf("%d,%d,%li\n", from, to, (long int) VECTOR(*truss)[i]);
    }

    igraph_vector_int_destroy(truss);
    igraph_destroy(graph);

}

int main() {

    igraph_t graph;
    igraph_vector_t v;
    igraph_vector_int_t truss;
  
    igraph_real_t edges[] = { 0,1, 0,2, 0,3, 0,4,
      1,2, 1,3, 1,4, 2,3, 2,4, 3,4, 3,6, 3,11,
      4,5, 4,6, 5,6, 5,7, 5,8, 5,9, 6,7, 6,10, 6,11,
      7,8, 7,9, 8,9, 8,10 };

    long int n = sizeof(edges) / sizeof(igraph_real_t);

    igraph_vector_view(&v, edges, n);

    igraph_create(&graph, &v, 0, IGRAPH_UNDIRECTED);
  
    // Compute the truss of the edges.
    igraph_vector_int_init(&truss, igraph_ecount(&graph));
    igraph_truss(&graph, &truss);
    print_and_destroy(&graph, &truss);
  
    VERIFY_FINALLY_STACK();
  
    return 0;
}