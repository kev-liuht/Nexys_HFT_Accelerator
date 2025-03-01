open_project ta_parser_hls -reset
set_top ta_parser
add_files ta_parser_src/ta_parser.cpp
add_files ta_parser_src/ta_parser.hpp
add_files -tb ta_parser_src/tb_ta_parser.cpp
open_solution "solution1"
set_part {xc7a200tsbg484-1} -tool vivado
create_clock -period 10 -name default
csynth_design
export_design -format ip_catalog