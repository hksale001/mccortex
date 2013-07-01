#include "global.h"
#include <time.h>
#include <pthread.h>

#include "util.h"
#include "file_util.h"
#include "db_graph.h"
#include "add_read_paths.h"
#include "binary_format.h"
#include "path_format.h"
#include "graph_walker.h"
#include "shaded_caller.h"

static const char usage[] =
"usage: ctx_call <threads> <mem> <in.ctx> <out.bubbles.gz>\n";

int main(int argc, char* argv[])
{
  if(argc != 5) print_usage(usage, NULL);

  char *num_threads_arg = argv[1];
  char *mem_arg = argv[2];
  char *input_ctx_path = argv[3];
  char *out_path = argv[4];

  size_t mem_to_use = 0;
  uint32_t num_of_threads = 0;

  if(!parse_entire_uint(num_threads_arg, &num_of_threads))
    print_usage(usage, "Invalid number of threads: %s", num_threads_arg);

  if(!test_file_readable(input_ctx_path))
    print_usage(usage, "Cannot read input file: %s", input_ctx_path);

  if(!mem_to_integer(mem_arg, &mem_to_use) || mem_to_use == 0)
    print_usage(usage, "Invalid memory argument: %s", mem_arg);

  if(!test_file_writable(out_path))
    print_usage(usage, "Cannot write output file: %s", out_path);

  // Probe binary to get kmer_size
  boolean is_binary = false;
  uint32_t kmer_size, num_of_cols;
  uint64_t num_kmers;

  if(!binary_probe(input_ctx_path, &is_binary, &kmer_size, &num_of_cols, &num_kmers))
    print_usage(usage, "Cannot read binary file: %s", input_ctx_path);
  else if(!is_binary)
    print_usage(usage, "Input binary file isn't valid: %s", input_ctx_path);

  // probe paths file
  char *input_paths_file = malloc(strlen(input_ctx_path)+4);
  paths_format_filename(input_ctx_path, input_paths_file);
  boolean valid_paths_file = false;
  uint64_t ctp_num_paths = 0, ctp_num_path_bytes = 0, ctp_num_path_kmers = 0;
  uint32_t ctp_kmer_size = 0, ctp_num_of_cols = 0;

  if(!paths_format_probe(input_paths_file, &valid_paths_file,
                         &ctp_kmer_size, &ctp_num_of_cols, &ctp_num_paths,
                         &ctp_num_path_bytes, &ctp_num_path_kmers))
  {
    print_usage(usage, "Cannot find .ctp file: %s", input_paths_file);
  }

  if(!valid_paths_file)
    die("Invalid .ctp file: %s", input_paths_file);
  if(ctp_num_of_cols != num_of_cols)
    die("Number of colours in .ctp does not match .ctx");
  if(ctp_kmer_size != kmer_size)
    die("Kmer size in .ctp does not match .ctx");

  // Decide on memory
  size_t req_num_kmers = num_kmers*(1.0/IDEAL_OCCUPANCY);
  size_t i, hash_kmers;
  size_t hash_mem = hash_table_mem(req_num_kmers, &hash_kmers);

  size_t graph_mem = hash_mem +
                     hash_kmers * sizeof(Edges) + // edges
                     hash_kmers * sizeof(uint64_t) * 2 + // kmer_paths
                     round_bits_to_bytes(hash_kmers) * num_of_cols + // in col
                     round_bits_to_bytes(hash_kmers) * 2; // visited fw/rv

  size_t thread_mem = round_bits_to_bytes(hash_kmers) * 2 * num_of_threads;

  if(graph_mem+thread_mem > mem_to_use) {
    print_usage(usage, "Not enough memory; hash table: %zu; threads: %zu",
                graph_mem, thread_mem);
  }

  // Allocate memory
  dBGraph db_graph;
  db_graph_alloc(&db_graph, kmer_size, num_of_cols, hash_kmers);

  // size_t path_mem = mem_to_use - graph_mem - thread_mem;
  size_t path_mem = ctp_num_path_bytes;

  char graph_mem_str[100], per_thread_mem_str[100], path_mem_str[100];
  bytes_to_str(graph_mem, 1, graph_mem_str);
  bytes_to_str(thread_mem / num_of_threads, 1, per_thread_mem_str);
  bytes_to_str(path_mem, 1, path_mem_str);

  message("[memory]  graph: %s;  threads: %i x %s;  paths: %s\n",
          graph_mem_str, num_of_threads, per_thread_mem_str, path_mem_str);

  // Edges
  db_graph.edges = calloc(hash_kmers, sizeof(uint8_t));
  if(db_graph.edges == NULL) die("Out of memory");

  // In colour
  size_t words64_per_col = round_bits_to_words64(hash_kmers);
  db_graph.node_in_cols = calloc(words64_per_col*num_of_cols, sizeof(uint64_t));
  if(db_graph.node_in_cols == NULL) die("Out of memory");

  // Paths
  db_graph.kmer_paths = malloc(hash_kmers * sizeof(uint64_t) * 2);
  if(db_graph.kmer_paths == NULL) die("Out of memory");
  memset(db_graph.kmer_paths, 0xff, hash_kmers * sizeof(uint64_t) * 2);

  uint8_t *path_store = malloc(path_mem);
  if(path_store == NULL) die("Out of memory");
  binary_paths_init(&db_graph.pdata, path_store, path_mem, num_of_cols);

  // Load graph
  SeqLoadingStats *stats = seq_loading_stats_create(0);
  SeqLoadingPrefs prefs = {.into_colour = 0, .merge_colours = false,
                           .load_binaries = true,
                           .must_exist_in_colour = -1,
                           .empty_colours = true,
                           .load_as_union = false,
                           .update_ginfo = true,
                           .db_graph = &db_graph};

  binary_load(input_ctx_path, &db_graph, &prefs, stats);
  hash_table_print_stats(&db_graph.ht);

  // Load path file
  paths_format_read(&db_graph, &db_graph.pdata, false, input_paths_file);

  /* initialize random seed: */
  srand(time(NULL));

  //
  // Set up temporary files
  //
  StrBuf *tmppath = strbuf_new();
  char **tmp_paths = malloc(num_of_threads * sizeof(char*));

  int r = rand() & ((1<<20)-1);

  for(i = 0; i < num_of_threads; i++)
  {
    strbuf_set(tmppath, out_path);
    strbuf_sprintf(tmppath, ".%i.%zu", r, i);
    tmp_paths[i] = strbuf_dup(tmppath);
    if(!test_file_writable(tmp_paths[i])) {
      while(i > 0) unlink(tmp_paths[--i]);
      die("Cannot write temporary file: %s", tmp_paths[i]);
    }
  }

  #ifdef DEBUG
    db_graph_dump_paths_by_kmer(&db_graph);
  #endif

  // Now call variants
  invoke_shaded_bubble_caller(&db_graph, out_path, num_of_threads, tmp_paths);

  free(input_paths_file);

  // Clear up threads
  for(i = 0; i < num_of_threads; i++) {
    unlink(tmp_paths[i]);
    free(tmp_paths[i]);
  }
  free(tmp_paths);

  free(db_graph.edges);
  free(db_graph.node_in_cols);
  free(db_graph.kmer_paths);
  free(path_store);

  seq_loading_stats_free(stats);
  db_graph_dealloc(&db_graph);

  message("Done.\n");
  pthread_exit(NULL);
}