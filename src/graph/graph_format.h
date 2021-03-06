#ifndef GRAPH_FORMAT_H_
#define GRAPH_FORMAT_H_

#include <inttypes.h>

#include "loading_stats.h"
#include "db_graph.h"
#include "graph_file_reader.h"

// graph file format version
#define CTX_GRAPH_FILEFORMAT 6

// Stucture for specifying how to load data
typedef struct
{
  dBGraph *db_graph;
  bool boolean_covgs; // Update covg by at most 1
  bool must_exist_in_graph;
  const Edges *must_exist_in_edges;
  // if empty_colours is true an error is thrown if a kmer from a graph file
  // is already in the graph
  bool empty_colours;
} GraphLoadingPrefs;

#define LOAD_GPREFS_INIT(graph) {  \
  .db_graph = (graph),             \
  .boolean_covgs = false,          \
  .must_exist_in_graph = false,    \
  .must_exist_in_edges = NULL,     \
  .empty_colours = false}

extern bool greader_zero_covg_error, greader_missing_covg_error;

void graph_header_alloc(GraphFileHeader *header, size_t num_of_cols);
void graph_header_dealloc(GraphFileHeader *header);

void graph_header_print(const GraphFileHeader *header);

// Merge headers and set intersect name (if intersect_gname != NULL)
void graph_reader_merge_headers(GraphFileHeader *hdr,
                                const GraphFileReader *files, size_t num_files,
                                const char *intersect_gname);

// Return number of bytes read or die() with error
size_t graph_file_read_header(FILE *fh, GraphFileHeader *header, const char *path);

// Returns number of bytes read
size_t graph_file_read_kmer(FILE *fh, const GraphFileHeader *h, const char *path,
                            BinaryKmer *bkmer, Covg *covgs, Edges *edges);

// if only_load_if_in_colour is >= 0, only kmers with coverage in existing
// colour only_load_if_in_colour will be loaded.
// if clean_colours != 0 an error is thrown if a node already exists
// returns the number of colours in the binary
// If stats != NULL, updates:
//   stats->num_kmers_loaded
//   stats->total_bases_read
// If header is != NULL, header will be stored there.  Be sure to free.
size_t graph_load(GraphFileReader *file, const GraphLoadingPrefs prefs,
                  LoadingStats *stats);

// Load all files into colour 0
void graph_files_load_flat(GraphFileReader *gfiles, size_t num_files,
                           GraphLoadingPrefs prefs, LoadingStats *stats);

// Load a kmer and write to a file one kmer at a time
// Optionally filter a against the graph currently loaded
//   (i.e. only keep nodes and edges that are in the graph)
// Same functionality as graph_files_merge, but faster if dealing with only one
// input file. Reads in and dumps one kmer at a time
size_t graph_stream_filter(const char *out_ctx_path, const GraphFileReader *file,
                           const dBGraph *db_graph, const GraphFileHeader *hdr,
                           const Edges *only_load_if_in_edges);

size_t graph_stream_filter_mkhdr(const char *out_ctx_path, GraphFileReader *file,
                                 const dBGraph *db_graph,
                                 const Edges *only_load_if_in_edges,
                                 const char *intersect_gname);

size_t graph_files_merge(const char *out_ctx_path,
                         GraphFileReader *files, size_t num_files,
                         bool kmers_loaded, bool colours_loaded,
                         const Edges *only_load_if_in_edges,
                         GraphFileHeader *hdr, dBGraph *db_graph);

// if intersect only load kmers that are already in the hash table
// returns number of kmers written
size_t graph_files_merge_mkhdr(const char *out_ctx_path,
                               GraphFileReader *files, size_t num_files,
                               bool kmers_loaded, bool colours_loaded,
                               const Edges *only_load_if_in_edges,
                               const char *intersect_gname, dBGraph *db_graph);

//
// Writing
//

/*!
  Write kmers from the graph to a file. The file header should already have been
  written.
  @return Number of bytes written
 */
size_t graph_write_empty(const dBGraph *db_graph, FILE *fh, size_t num_of_cols);

/*!
  Overwrite kmers in an existing file.
  @param first_graphcol first colour in the dBGraph to read from
  @param first_filecol first colour in the file to write into
  @param ngraphcols Number of colours to write to file
  @param nfilecols Total number of colours in file
  @param mmap_ptr Memory mapped file pointer
  @param hdrsize Size of file header i.e. byte pos of first kmer in file
 */
void graph_update_mmap_kmers(const dBGraph *db_graph,
                             size_t first_graphcol, size_t ngraphcols,
                             size_t first_filecol, size_t nfilecols,
                             char *mmap_ptr, size_t hdrsize);

// Returns number of bytes written
size_t graph_write_header(FILE *fh, const GraphFileHeader *header);

size_t graph_write_kmer(FILE *fh, size_t num_bkmer_words, size_t num_cols,
                        const BinaryKmer bkmer, const Covg *covgs,
                        const Edges *edges);

// Dump all kmers with all colours to given file. Returns num of kmers written
size_t graph_write_all_kmers(FILE *fh, const dBGraph *db_graph);

// If you don't want to/care about graph_info, pass in NULL
// If you want to print all nodes pass condition as NULL
// start_col is ignored unless colours is NULL
// returns number of nodes dumped
uint64_t graph_file_save_mkhdr(const char *path, const dBGraph *graph,
                               uint32_t version,
                               const Colour *colours, Colour start_col,
                               size_t num_of_cols);

// Pass your own header
uint64_t graph_file_save(const char *path, const dBGraph *db_graph,
                         const GraphFileHeader *header, size_t intocol,
                         const Colour *colours, Colour start_col,
                         size_t num_of_cols);

void graph_writer_print_status(uint64_t nkmers, size_t ncols,
                               const char *path, uint32_t version);

#endif /* GRAPH_FORMAT_H_ */
