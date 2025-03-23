open_project cov_update_hls -reset
set_top cov_update
add_files cov_update_src/cov_update.cpp
# add_files cov_update_src/cov_update.hpp
add_files -tb cov_update_src/cov_update_tb.cpp
open_solution "solution1"
set_part {xc7a200tsbg484-1} -tool vivado
create_clock -period 10 -name default
csynth_design
config_export \
  -version  "1.0.0"
export_design -format ip_catalog