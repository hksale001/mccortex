#ifndef GRAPH_CACHE_H_
#define GRAPH_CACHE_H_

#include "khash.h"
#include "db_node.h"

// Build and store paths through the graph
// Must build one path at a time
// Cannot update an old path
// (this could be overcome by making paths steps a linkedlist
//  i.e. adding uint32_t next_in_path field to step, but is not needed atm)
// Warning: not thread safe! Do not use the same GraphCache in more than one
//          thread at the same time.

typedef struct
{
  const size_t first_node_id;
  uint32_t num_nodes;
  uint32_t first_step; // linked list of steps through this supernode

  // This is to speed up traversal of subsequent colours
  const dBNode prev_nodes[4], next_nodes[4];
  const uint8_t prev_bases, next_bases; // bases packed into 2bits per base
  const uint8_t num_prev:4, num_next:4;
} CacheSupernode;

typedef struct
{
  const uint32_t orient:1, supernode:31;
  const uint32_t pathid; // path that this step belongs to
  uint32_t next_step; // linked list of steps through a single supernode
} CacheStep;

typedef struct
{
  const uint32_t first_step;
  uint32_t num_steps;
} CachePath;

#include "objbuf_macro.h"
create_objbuf(cache_snode_buf, CacheSupernodeBuffer, CacheSupernode);
create_objbuf(cache_step_buf, CacheStepBuffer, CacheStep);
create_objbuf(cache_path_buf, CachePathBuffer, CachePath);

#define db_node_hash(x) kh_int64_hash_func((x.key << 1) | x.orient)
KHASH_INIT(SnodeIdHash, dBNode, uint32_t, 1, db_node_hash, db_nodes_are_equal)

typedef struct
{
  dBNodeBuffer          node_buf;
  CacheSupernodeBuffer  snode_buf;
  CacheStepBuffer       step_buf;
  CachePathBuffer       path_buf;

  // hash hkey_t->uint32_t (supernode_id)
  khash_t(SnodeIdHash) *snode_hash;

  const dBGraph *db_graph;
} GraphCache;

void supernode_cache_alloc(GraphCache *cache, const dBGraph *db_graph);
void supernode_cache_dealloc(GraphCache *cache);
void supernode_cache_reset(GraphCache *cache);

// Returns pathid
uint32_t supernode_cache_new_path(GraphCache *cache);

// Returns stepid
uint32_t supernode_cache_new_step(GraphCache *cache, dBNode node);

void supernode_stepptrs_qsort(GraphCache *cache,
                              CacheStep **list, size_t n);

// Get all nodes in a single step (supernode with orientation)
// Adds to the end of the node buffer (does not reset it)
void supernode_snode_fetch_nodes(const GraphCache *cache,
                                 const CacheSupernode *snode,
                                 Orientation orient,
                                 dBNodeBuffer *nbuf);

// Get all nodes in a path up to, but not including the given step
// Adds to the end of the node buffer (does not reset it)
void supernode_step_fetch_nodes(const GraphCache *cache,
                                const CacheStep *end_step,
                                dBNodeBuffer *nbuf);

static inline const dBNode* cache_supernode_first_node(const GraphCache *cache,
                                                       const CacheSupernode *snode)
{
  return cache->node_buf.data + snode->first_node_id;
}

static inline const dBNode* cache_supernode_last_node(const GraphCache *cache,
                                                      const CacheSupernode *snode)
{
  return cache->node_buf.data + snode->first_node_id + snode->num_nodes - 1;
}

#define cache_supernode_node(cache,nodeid) (&(cache)->node_buf.data[nodeid])
#define cache_supernode_snode(cache,snodeid) (&(cache)->snode_buf.data[snodeid])
#define cache_supernode_step(cache,stepid) (&(cache)->step_buf.data[stepid])
#define cache_supernode_path(cache,pathid) (&(cache)->path_buf.data[pathid])

#define cache_supernode_num_nodes(cache) ((cache)->node_buf.len)
#define cache_supernode_num_snodes(cache) ((cache)->snode_buf.len)
#define cache_supernode_num_steps(cache) ((cache)->step_buf.len)
#define cache_supernode_num_paths(cache) ((cache)->path_buf.len)

// Looks like 3p flank if steps don't have the same n-1 supernode
bool snode_cache_is_3p_flank(GraphCache *cache,
                             CacheStep ** steps, size_t num_steps);

// Remove duplicate paths
size_t snode_cache_remove_dupes(GraphCache *cache,
                                CacheStep **steps, size_t num_steps);

// Returns true if all nodes in supernode have given colour
bool cache_supernode_has_colour(const GraphCache *cache,
                                const CacheSupernode *snode,
                                size_t colour);

// Returns true if all nodes in path have given colour
bool cache_step_has_colour(const GraphCache *cache,
                           const CacheStep *endstep,
                           size_t colour);

// Returns NULL if not found
CacheSupernode* snode_cache_find_snode(GraphCache *cache, dBNode node);

Orientation snode_cache_get_supernode_orient(const GraphCache *cache,
                                             const CacheSupernode *snode,
                                             dBNode first_node);

#endif /* GRAPH_CACHE_H_ */