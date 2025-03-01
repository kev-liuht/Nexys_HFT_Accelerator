open_project cov_update_hls -reset
set_top cov_update
add_files cov_update_src/cov_update.cpp
add_files cov_update_src/cov_update.hpp
add_files -tb cov_update_src/tb_cov_update.cpp
open_solution "solution1"
set_part {xc7a200tsbg484-1} -tool vivado
create_clock -period 10 -name default
csynth_design
export_design -format ip_catalog