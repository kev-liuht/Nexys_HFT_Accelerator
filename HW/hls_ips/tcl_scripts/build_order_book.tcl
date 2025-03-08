open_project order_book_hls -reset
set_top Orderbook_wrapper
add_files order_book_src/Orderbook_tree_optimized.cpp
add_files order_book_src/Orderbook_tree_optimized.h
open_solution "solution1"
set_part {xc7a200tsbg484-1} -tool vivado
create_clock -period 10 -name default
csynth_design
export_design -format ip_catalog