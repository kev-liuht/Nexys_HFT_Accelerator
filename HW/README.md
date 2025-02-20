# Hardware Directory
This directory contains the main Vivado project.

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
