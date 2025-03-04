# Hardware Directory
This directory contains the main Vivado project.

## Building Project

Took inspiration from [https://github.com/tobiasrj20/Vivado-Version-Control-Example/tree/master ](https://github.com/tobiasrj20/Vivado-Version-Control-Example/tree/master).

IF you use windows cmd, run the following line:

```
C:\Xilinx\Vivado\2018.3\bin\vivado.bat -mode batch -source build.tcl
```

Otherwise if you open Vivado:

1. Open Tcl Console (window -> Tcl Console)
2. cd into the working directory
3. execute `source build.tcl`

### Importing version controlled project to Xilinx SDK

SDK project files are located under `Nexys_HFT_Accelerator/HW/SDK`. Make sure to change the SDK Workspace to that directory (instead of <Local to Project>)

1. Generate bitstream
2. File -> Export -> Export Hardware (and check bitstream) to the SDK source file path under both "Exported location" and "Workspace"
3. Launch SDK: File->Launch SDK Browse to the SDK source file path under both "Exported location" and "Workspace"
4. Import Application project and BSP: File->Import->General->Existing Projects into Workspace browse to BSP folder, repeat and do the same but select the project folder (the one with your .c and .h files)
5. Build project: Project->Build all
6. Program FPGA: Xilinx Tools->Program FPGA
7. Running the application: Run Configuration...

### Version Control Project with TCL

Run the following instructions every time changes have been made to the block-design, which requires a new `build.tcl` file.

1. File -> Project -> Write TCL
2. Check `Copy Sources to New Project` and `Recreate Block Diagrams using TCL`
3. Set Output File to `<path to repo>/Nexys_HFT_Accelerator/HW/build.tcl`
4. Add the following lines to build.tcl
  - Add `catch { apply_bd_automation -rule xilinx.com:bd_rule:mig_7series -config {Board_Interface "ddr3_sdram" }  [get_bd_cells mig_7series_0] }` after `# Create instance: mig_7series_0, and set properties ... ] $mig_7series_0`
  - Add `make_wrapper -files [get_files $design_name.bd] -top -import` after `save_bd_design`

## Compiling and Building HLS IPs
The `hls_ips` directory is configured for automated compilation and building of custom HFT IP cores using Vivado_HLS 2018.3. Follow the instructions below to organize your project files and build your IPs efficiently.

### HLS IPs Folder Structure

- **hls_ips/**  
  - **build_all.tcl**: Master Tcl script to build all IPs.
  - **tcl_scripts/**: Directory where individual IP Tcl scripts are stored.
  - **\<ip_name_src\>/**: Dedicated folder containing individual IP's design source files.

 ### How to Build

1. **Navigate to the `hls_ips` Folder:**

   Open a terminal and change to the `hls_ips` directory:
   `cd path/to/hls_ips`

2.  **Run the Build Script:**
    
    Execute the master build script using Vivado HLS:
  `vivado_hls -f build_all.tcl` 
    
    This script will sequentially call the individual TCL scripts (located in `tcl_scripts/`) required to build your IPs.
    

### Development Guidelines

-   **Individual IP TCL Scripts:**  
    Store each IP’s TCL script in the `tcl_scripts` folder. Update `build_all.tcl` as needed to source or call these scripts.
    
-   **Design Files:**  
    Place the design files for each IP directly into a dedicated folder within `hls_ips/`. For example, for an IP named "ip1", create a folder named `ip1_src` and store the `.c`, `.cpp`,  `.h` (and other design files) there: `hls_ips/ip1_src/file.cpp` 
    

### Additional Notes

-   Ensure that the folder structure is maintained for seamless integration with the build scripts.
-   Verify that Vivado HLS is correctly installed and configured in your environment (by running settings64.bat for Windows or settings64.sh for Linux in Terminal).
-   For any build issues, review the Vivado HLS log output to identify and resolve errors.
