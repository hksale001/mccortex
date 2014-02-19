#include "global.h"
#include "all_tests.h"

#include "db_graph.h"
#include "build_graph.h"
#include "clean_graph.h"

void test_cleaning()
{
  test_status("[cleaning] Testing graph cleaning...");

  // Construct 1 colour graph with kmer-size=11
  dBGraph graph;
  const size_t kmer_size = 19, ncols = 1;
  size_t i;

  db_graph_alloc(&graph, kmer_size, ncols, ncols, 2000);
  // Graph data
  graph.bktlocks = calloc2(roundup_bits2bytes(graph.ht.num_of_buckets), 1);
  graph.col_edges = calloc2(graph.ht.capacity * ncols, sizeof(Edges));
  graph.col_covgs = calloc2(graph.ht.capacity * ncols, sizeof(Covg));

  size_t num_visited_words = roundup_bits2words64(graph.ht.capacity);
  uint64_t *visited = calloc2(num_visited_words, sizeof(uint64_t));

  // Simple graph - 1000 bases, should all be cleaned off
  char graphseq[] =
"GGCTACCTAACCAGATATCTCTGTATACAGCTGCATTGTGTTTAGTCTACAACGACAGAAATCCCCTTCGACGCCCGC"
"GACCTCTCTTAACGGACGACGCCTTCCGGTTGCGATATCGATGGATCGACAGAACAAGCCGCTTCCCTAACAACTGCG"
"CATGAAATCCAAAGTGCGCCGATGCTTGCTTGACGATTCCAAATCCCCATGTGACCTGTGAAGACGACTACCGTAAGA"
"TGTGTCACGGGTCAGTCGCTTTTACCACCTACGGAAGGTAGACGGTTATACTCAATTATTGGCACTTTAGCTGGGCAG"
"GTCAAAGGGAACAAGTCTGAAGTAGATATAACCTCAGTCCTTTATACGCACGTGACCCGCGTATAATCTTGCCGGTGC"
"GCAACGAGGGGCTTGGATAAAACAGCTTGGGACTTATACGTTCACCCACGACCCGCCTTAGCTCAACGCTCGTAACGA"
"CTGAATATGAGTAACGTACCTGAGGTGGGTCCGCCTTGCGGAGGTGGTGGTTCTTACTTCTATCCTCTTGTAGAGAAA"
"AGAATAGGTCGTCACTAACACTCTTGTGGGGACAAACGTGTATCGATTCCCAAACGTCCGTTAGTGAATATCCTACGT"
"GTTCCATTCGATCACACTGGAATATGGCCTTAGTTGGCCCATCTTAGTGCGCCAAGTGTTCGCAGTGGTCGTAGGCAA"
"CAGGCATCGGCGGTCTAGAGTTCACGCCAAGTCGGCCGTGTGAAGTTAAGCGTAAGTGCGGGACAACAAACCGAATGT"
"TCCGTGGCACACATGTTCGCTTATTATCAGGTAACCCTCATCTCCAGGGAGAACGCCTCAGCAGGCTTGCACCGCTTG"
"TAATCCCTCCTTATCAGAAGTAATCGTCGTTGCCGAGTTAGATCATGTCGGGACGTTGCCCTCAAGACGCCCAACGGA"
"AAAATTCACGATAGTGGCGCTCGGGAGGAGTACGCAACTCAGCACCCCGGTGAGTAGCTCCCTT";

  build_graph_from_str_mt(&graph, 0, graphseq, strlen(graphseq));
  TASSERT2(graph.ht.num_kmers == 1000-19+1,
           "%"PRIu64" kmers", graph.ht.num_kmers);

  // No change (min_tip_len must be > 1)
  cleaning_remove_tips(2, visited, &graph);
  memset(visited, 0, num_visited_words * sizeof(uint64_t));
  TASSERT(graph.ht.num_kmers == 1000-19+1);
  TASSERT(graph.ht.num_kmers == hash_table_count_kmers(&graph.ht));

  // No change (min_tip_len must be > 1)
  cleaning_remove_tips(1000-19+1, visited, &graph);
  memset(visited, 0, num_visited_words * sizeof(uint64_t));
  TASSERT(graph.ht.num_kmers == 1000-19+1);
  TASSERT(graph.ht.num_kmers == hash_table_count_kmers(&graph.ht));

  // All removed
  cleaning_remove_tips(1000-19+2, visited, &graph);
  memset(visited, 0, num_visited_words * sizeof(uint64_t));
  TASSERT(graph.ht.num_kmers == 0);
  TASSERT(graph.ht.num_kmers == hash_table_count_kmers(&graph.ht));

  // Reload first 200 bases of graph 3 times
  for(i = 0; i < 3; i++)
    build_graph_from_str_mt(&graph, 0, graphseq, 200);
  TASSERT2(graph.ht.num_kmers == 200-19+1,
           "%"PRIu64" kmers", graph.ht.num_kmers);

  // First 100 bp with two SNPs
  char tmp[] =
"GGCTACCTAACCAGATATCTCTGTATcCAGCTGCATTGTGTTTAGTCTACAACGACAGAtATCCCCTTCGACGCCCGC"
"GACCTCTCTTAACGGACGACGC";

  build_graph_from_str_mt(&graph, 0, tmp, strlen(tmp));

  size_t thresh = cleaning_remove_supernodes(true, 0, 4, NULL, visited, &graph);
  memset(visited, 0, num_visited_words * sizeof(uint64_t));
  TASSERT2(thresh > 1, "threshold: %zu", thresh);

  TASSERT(graph.ht.num_kmers == 200-19+1);
  TASSERT(graph.ht.num_kmers == hash_table_count_kmers(&graph.ht));

  // First 78 bp with a single SNP creating a tip 23bp -> 5kmers long
  char tmp2[] =
"GGCTACCTAACCAGATATCTCTGTATACAGCTGCATTGTGTTTAGTCTACAACGACAGAAATCCCCTTCGACGgCCGC";

  build_graph_from_str_mt(&graph, 0, tmp2, strlen(tmp2));
  TASSERT(graph.ht.num_kmers == 200-19+1 + 23-19+1);
  TASSERT(graph.ht.num_kmers == hash_table_count_kmers(&graph.ht));

  cleaning_remove_tips(2*19-1, visited, &graph);
  memset(visited, 0, num_visited_words * sizeof(uint64_t));
  TASSERT(graph.ht.num_kmers == 200-19+1);
  TASSERT(graph.ht.num_kmers == hash_table_count_kmers(&graph.ht));

  free(visited);
  free(graph.bktlocks);
  free(graph.col_edges);
  free(graph.col_covgs);
  db_graph_dealloc(&graph);
}