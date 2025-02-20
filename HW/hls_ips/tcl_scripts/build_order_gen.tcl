open_project order_gen_hls -reset
set_top order_gen_axis
add_files order_gen_src/order_gen_axis.cpp
add_files -tb order_gen_src/tb_order_gen_axis.cpp
open_solution "solution1"
set_part {xc7a200tsbg484-1} -tool vivado
create_clock -period 10 -name default
csynth_design
export_design -format ip_catalog