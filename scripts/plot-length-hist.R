# Plot coverage histograms generated by e.g. 'ctx31 clean --covgs out.csv ...'
# input csv should have the columns: 'bp' and 'Count'
#
args <- commandArgs(trailingOnly=TRUE)
if(length(args) != 2) {
  stop("Usage: R --vanilla --file=plot-hist-hist.R --args <lengths.csv> <lengths.pdf>\n")
}

input_csv=args[1]
output_pdf=args[2]

require(ggplot2)

d=read.csv(file=input_csv,sep=',',as.is=T)

p <- ggplot(data=d, aes(x=bp, y=Count)) +
       geom_bar(stat="identity") +
       xlab("Supernode Length (bp)") +
       ylab("Number of Supernodes") +
       ggtitle("Supernode Length Distribution") +
       xlim(0,150)

ggsave(filename=output_pdf, plot=p, width=6, height=6)
