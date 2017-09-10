# GRU-examples

This provides a few examples of how to use the SGI GRU ASIC.

# The SGI Global Reference Unit

The SGI GRU is an ASIC that is part of the HARP2 ASIC which is built into certain servers of the SGI UltraViolet family (recently acquired by HPE). It is programmable via a proprietary API and provides operations to copy and process memory as well as atomic memory operations (AMOs).

More information on the GRU: https://www.sgi.com/pdfs/4551.pdf

What the GRU can do for you: http://www.adms-conf.org/2017/presentations/NUMA.pdf

# How this came about

For my master thesis I extensively tested the capabilities of the SGI GRU. Some of my results were used in the paper "Hardware-Accelerated Memory Operations for Large-Scale NUMA Systems", which was presented at the ADMS on September 1st 2017. 
Since getting started with programming the GRU was pretty frustrating due to lack of documentation I wanted to provide a few basic code examples. I took code used for my thesis and trimmed it down.
