open_project dummy_burst_hls -reset
set_top burst_generator
add_files dummy_burst_src/burst_generator.cpp
add_files -tb dummy_burst_src/tb_burst_generator.cpp
open_solution "solution1"
set_part {xc7a200tsbg484-1} -tool vivado
create_clock -period 10 -name default
csynth_design
config_export \
  -version  "1.0.0"
export_design -format ip_catalog