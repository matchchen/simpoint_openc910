# simpoint_openc910
modified openc910 for the simpoint purpose, some changess to the original openc910 git repo

# Main changes
(1) added the dpi interface to the mem_ctrl.v for loading the check point memory snapshot into the RAM (Distributed to the 16 RAM banks)

(2) added a dpi interface to the ram.v to load and set the content of the RAM

(3) supported to load bin file into memory

(4) supported to load vmem file into memory

# v0.1
1. modify putc address
2. add "--i \<n\>" command argument to statistic cycle/instnum and host time
