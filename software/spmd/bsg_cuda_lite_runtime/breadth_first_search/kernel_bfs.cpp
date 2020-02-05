//This is an empty kernel 

#include "threading/local_range.h"
#include "threading/num_threads.h"
#include "threading/thread_id.h"
#include "graph_formats/csr_blob.h"
#include "graph_algorithm/csr_setup_graph_data.hpp"

extern "C" {
#include "bsg_manycore.h"
#define BSG_TILE_GROUP_X_DIM bsg_tiles_X
#define BSG_TILE_GROUP_Y_DIM bsg_tiles_Y
#include "bsg_tile_group_barrier.h"
}

INIT_TILE_GROUP_BARRIER(r_barrier, c_barrier, 0, bsg_tiles_X-1, 0, bsg_tiles_Y-1)

using namespace formats;

#if 0
// read-only graph data
int32_t NODES, EDGES;
int32_t **F_NEIGH;
int32_t *F_DEGREE;
int32_t **B_NEIGH;
int32_t *B_DEGREE;

static int setup_graph_data(csr_blob_header_t *CSR)
{
    NODES = CSR->n_nodes;
    EDGES = CSR->n_edges;

    csr_blob_fwd_degrees(CSR, &F_DEGREE);

    int32_t *f_off;
    csr_blob_fwd_offsets(CSR, &f_off);
    F_NEIGH = (int32_t**)f_off;

    int32_t *f_edges;
    csr_blob_fwd_edges(CSR, &f_edges);
    
    int start, end;
    local_range(NODES, &start, &end);
        
    for (int i = start; i < end; i++) {
        F_NEIGH[i] = &f_edges[f_off[i]];
    }

    csr_blob_bck_degrees(CSR, &B_DEGREE);

    int32_t *b_off;
    csr_blob_bck_offsets(CSR, &b_off);
    B_NEIGH = (int32_t**)b_off;

    int32_t *b_edges;
    csr_blob_bck_edges(CSR, &b_edges);
    
    local_range(NODES, &start, &end);
        
    for (int i = start; i < end; i++) {
        B_NEIGH[i] = &b_edges[b_off[i]];
    }
    

    return 0;
}
#endif

extern "C" int bfs_dense_pull_dense_frontier_in_dense_frontier_out(
    csr_blob_header_t *CSR,
    int *visited,
    int *dense_frontier_in,
    int *dense_frontier_out) {

    csr_setup_graph_data(CSR);
    //setup_graph_data(CSR);
    
    int dst_s, dst_e;
    local_range(NODES, &dst_s, &dst_e);

    for (int dst = dst_s; dst < dst_e; dst++) {
        if (visited[dst] == 0) {        
            int32_t *neigh = B_NEIGH[dst];
            int degree = B_DEGREE[dst];
            for (int i = 0; i < degree; i++) {
                int src = neigh[i];
                if (dense_frontier_in[src]) {
                    dense_frontier_out[dst] = 1;
                    visited[dst] = 1;
                }
            }
        }
    }
    
    bsg_tile_group_barrier(&r_barrier, &c_barrier);
    return 0;
}

extern "C" int bfs_sparse_push_sparse_frontier_in_dense_frontier_out(
    csr_blob_header_t *CSR,
    int *visited,
    int *sparse_frontier_in,
    int *dense_frontier_out) {

    //setup_graph_data(CSR);
    csr_setup_graph_data(CSR);
    
    for (int i = thread_id(); i < NODES; i += num_threads()) {
        int src = sparse_frontier_in[i];
        if (src == -1) break;

        int32_t *neigh = F_NEIGH[src];
        int degree = F_DEGREE[src];
        for (int i = 0; i < degree; i++) {
            int dst = neigh[i];
            if (visited[dst] == 0) {
                dense_frontier_out[dst] = 1;
                visited[dst] = 1;
            }
        }            
    }
    
    bsg_tile_group_barrier(&r_barrier, &c_barrier);    
    return 0;
}
