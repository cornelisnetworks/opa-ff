The tools in this directory are intended to stress a fabric in different ways 
in order to provide information about how a fabric is running.

mpi_groupstress:

USAGE:
  -v/--verbose            Verbose.
  -g/--group    <arg>     Group size. Should be an even number between 2 and 128
  -l/--min      <arg>     Minimum Message Size. Should be between 16384 and
  					 	  (1<<22)
  -u/--max      <arg>     Maximum Message Size. Should be between 16384 and
 						  (1<<22)
  -n/--num      <arg>     Number of times to repeat the test. Enter -1 to run
  						  forever.
  -h/--help               Provides this help text.

The first tool, mpi_groupstress breaks the nodes into groups and then runs the 
osu bandwidth benchmark on pairs of nodes within each group.

This is useful for stressing the fabric in specific ways. For example, consider
a fabric where all the nodes are connected to the core switch via leaf switches,
with 18 nodes per leaf switch. If you list the nodes in the mpi_hosts file in
topological order and run mpi_groupstress with a group size of 18, you can
stress all the leaf-to-node connections without sending traffic over the core
switch. If you want to test leaf-to-leaf connections, doubling the group size
to 36 will ensure that every single test will pass through an inter-switch link.

Note that, as mentioned above, adding nodes to the hosts file is very important.
mpi_groupstress has no knowledge of the fabric topology, so that knowledge
must be embedded in the hosts file.

A third use case might be to stress a single link as hard as possible. For 
example, if each node has 16 cores,  and you want to stress the path between
two nodes, list each node 16 times in the hostfile, then run mpi_groupstress
with a group size of 32.



mpi_latencystress:

USAGE:
  -v/--verbose  			 Verbose. Outputs some debugging information. 
  							 Use multiple times for more detailed information.
  -s/--size     			 Message Size. Should be between 0 and (1<<22)
  -n/--num      <arg>	     Number of times to repeat the test. Enter -1 to 
  							 run forever.
  -h/--help     			 Provides this help text.

mpi_latencystress iterates through every possible pair of nodes in the fabric,
looking for slow links. Unlike similar tools, it will do as many pair-wise
tests in parallel as it can, to reduce the total run time of the test.