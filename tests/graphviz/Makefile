SHELL:=/bin/bash -euo pipefail

CTXDIR=../..
CTX=$(CTXDIR)/bin/mccortex31
CTX2DOT=$(CTXDIR)/scripts/cortex_to_graphviz.pl

all: seq.k15.ctx seq.k15.supernodes.dot seq.k15.kmers.dot

plots: seq.k15.supernodes.pdf seq.k15.kmers.pdf

seq.fa:
	echo TACATTCCCCCATAGTCGTAGGCGTTAAATACA     > seq.fa
	echo TACATTCCCCCATAGTAGTAGGCGTTAAATACA    >> seq.fa
	echo GGCGTTCGCTTATCCGGATAAGCGAACGCC       >> seq.fa
	echo ATATATATATATATATATATATATATATATATATAT >> seq.fa

seq.k15.ctx: seq.fa
	$(CTX) build -k 15 --sample MasterSeq --seq seq.fa seq.k15.ctx

seq.k15.supernodes.dot: seq.k15.ctx
	$(CTX) supernodes --dot $< > $@

seq.k15.kmers.dot: seq.k15.ctx
	$(CTX2DOT) $< > $@

%.pdf: %.dot
	cat $< | dot -Tpdf > $@

clean:
	rm -rf seq.fa seq.k15.ctx
	rm -rf seq.k15.supernodes.dot seq.k15.kmers.dot
	rm -rf seq.k15.supernodes.pdf seq.k15.kmers.pdf

.PHONY: all clean plots
