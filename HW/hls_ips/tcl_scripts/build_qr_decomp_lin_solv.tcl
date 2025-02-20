open_project qr_decomp_lin_solv_hls -reset
set_top qr_decomp_lin_solv_axis
add_files qr_decomp_lin_solv_src/qr_decomp_lin_solv_axis.cpp
add_files -tb qr_decomp_lin_solv_src/tb_qr_decomp_lin_solv_axis.cpp
open_solution "solution1"
set_part {xc7a200tsbg484-1} -tool vivado
create_clock -period 10 -name default
csynth_design
export_design -format ip_catalog