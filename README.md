# README

## Overview

This repository contains a hardware accelerator design and a corresponding software framework to generate and process ITCH messages. The design is split into two main parts:

1. **Software (SW) Implementation**  
   - Written in Python  
   - Includes an **Order Generator**, an **ITCH Server**, and a **software benchmark** that mimics hardware functionality for verification  

2. **Hardware (HW) Implementation**  
   - Implemented in Xilinx Vivado  
   - Composed of custom HLS IP cores, a parser IP, and supporting MicroBlaze c files 

## Repository Structure

```
├── SW/
│   ├── config.csv
│   ├── order_generator.py
│   ├── itch_server.py
│   ├── itch_encoder.py
│   ├── test_tcp_client.py
│   ├── ...
└── HW/
    ├── build.tcl
    ├── hls_ips/
    │   ├── <IP_NAME>/
    │   │   ├── src/
    │   │   └── build_<IP_NAME>.tcl
    │   └── build_all.tcl
    ├── ip_repo/
    │   └── parser/
    └── SDK/
        └── <MicroBlaze_C_Projects>/

```

### Key Directories & Files

- **hls_ips/**  
  Contains custom HLS IPs and Tcl scripts. Each IP has its own folder with source files and a dedicated Tcl script for building.

- **ip_repo/**  
  Includes the parser IP (and potentially other IPs) that integrate into the Vivado project.

- **SDK/**  
  Holds MicroBlaze C application projects. Once the hardware is built and the bitstream is generated, these projects can be imported into Xilinx SDK or a similar toolchain to run on the FPGA.

- **software/**  
  - **config.csv** – Configuration parameters for the Order Generator (e.g., starting/ending price, number of stocks).  
  - **order_generator.py** – Creates a `test.csv` dataset in csv format using `config.csv`.  
  - **itch_encoder.py** – Creates a `.bin` dataset using `test.csv`.  
  - **itch_server.py** – Publishes ITCH messages from the generated `.bin` file whenever a designated port is available. Will  receive and parse ouch message from either HW/SW client and instanitate GUI with --monitor parameter,  
  - **ouch_parser**  - parse ouch message from Ordergen
  - **Gui.py**  - display parsed ouch message with current stocks in holding
  - **test_tcp_client.py** – Instantiates all software modules, opens a port, and communicates with the ITCH server, effectively benchmarking the system in software.  All script below are modules instantiated in test_tcp_client.
  - **itch_parser.py**  - Parser module
  - **Orderbook.py**  - Orderbook module
  - **CovUpdate.py** , **QrDecompLinSolver.py** ,  **TaParser.py** , **OrderGen.py** - All modules represent algo + ordergen



- **build.tcl**  
  The top-level Vivado build script that integrates all compiled HLS IPs and the parser IP into a single Vivado project, then runs synthesis, implementation, and bitstream generation.

## Software Workflow

1. **Configure Parameters**  
   - Edit `config.csv` to specify the parameters for message generation (e.g., number of orders, rates, etc.).

2. **Generate `.bin` Dataset**  
   - Run `order_generator.py`:
     ```bash
     python order_generator.py
     ```
   - A `.bin` file will be produced based on the configuration.

3. **Start the ITCH Server**  
   - Run `itch_server.py --monitor` to publish ITCH messages from the `.bin` file:
     ```bash
     python itch_server.py
     ```
   - The server will listen on a specified port (defined in the script or via command-line arguments).

4. **Test / Benchmark in Software**  
   - Execute `test_tcp_client.py`:
     ```bash
     python test_tcp_client.py
     ```
   - This script opens a port, connects to the ITCH server, and exchanges data, emulating the hardware accelerator's logic for verification.

## Hardware Workflow

1. **Build HLS IPs**  
   - Inside the `hls_ips/` directory, create a cmd terminal, run:
    ```tcl
    setting64.bat of vivado
     vivado_hls -f build_all.tcl
     ```
   - This command invokes Vivado HLS to build all custom IPs in sequence (all the dir ending with _hls).

2. **Create the Top-Level Hardware Project**  
   - From the root of the repository (or within Vivado Tcl console), run:
     ```tcl
     setting64.bat of vivado
     vivado -mode tcl -source build.tcl
     ```
   - This integrates all HLS IPs (under `hls_ips/`), the parser IP (under `ip_repo/`), and wires them together in a single Vivado project.
   - The process will produce the FPGA bitstream file once synthesis and implementation complete.
   - To run synethisis and implementation correctly, ensure you have the liscense setup for Ethernet IP

3. **Export Hardware to Xilinx SDK (or Vitis)**  
   - After generating the bitstream, export the hardware design to the Xilinx SDK environment to produce the SDK directories
   - Use the **File** → **Export** → **Export Hardware** option in Vivado, making sure to include the bitstream.

4. **Load the MicroBlaze Application**  
   - In the **SDK/** folder, open or import the tcp_client and tcp_client_bsp MicroBlaze C projects into Xilinx SDK
   - Configure the application to run on the generated hardware platform.
   - Program the FPGA with the bitstream, then run the C application on the MicroBlaze to open a port and communicate with the ITCH server.

