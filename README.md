# WORK IN PROGRESS

# DQCsim OpenQL-mapper operator

This repository contains some glue code to use the OpenQL Mapper as a DQCsim
operator.

## Compile-time mapping (OpenQL) vs runtime mapping (DQCsim + OpenQL)

It is very important to note that DQCsim is a *simulator* framework, and
operators are placed in the purely-quantum gatestream that results from
executing any and all classical and mixed quantum-classical instructions, such
as loops, if statements, and anything based on measurement results. This
results in two major differences between mapping a program with OpenQL and then
simulating versus using DQCsim to do it.

 - As a DQCsim operator, the mapper does not have access to the whole program
   before it is executed; it can only see gates up to the first measurement.
   This is because any subsequent gate may depend on the measurement result
   through the classical instructions in the frontend; trying to receive gates
   past this point may result in a deadlock. This may adversely affect the
   quality of the mapping.

 - As a DQCsim operator, the mapper maps exactly the actually executed gates.
   If you for instance have an if-else statement based on a measurement result,
   the mapper will only see the block that was actually executed based on that
   measurement. This also means that the virtual-to-physical mappings may
   change based on the stochastic measurement results from the quantum
   simulation. Furthermore, if you have a loop, the mapper will be invoked for
   each loop iteration; this may make it significantly slower, but may in fact
   improve the mapping result as the mapper doesn't need to insert swaps at
   the end of the loop body to return to the mapping at the start of the loop.
   The effect is as if all loops (even those with dynamic conditions) are
   unrolled. Ultimately, these things should improve the mapping result as more
   information is available, but may result in longer simulation times.

The computer engineers among us may note that this is exactly the difference
between compile-time scheduling (including predication, loop unrolling, etc)
and runtime scheduling (Tomasulo, speculation, etc.) in classical computer
architecture. Neither is necessarily better than the other, but the results
are different. Please keep this in mind when evaluating the mapper results.

