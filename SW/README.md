1. **Software (SW) Implementation**  
   - Written in Python  
   - Includes an **Order Generator**, an **ITCH Server**, and a **software benchmark** that mimics hardware functionality for verification  


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
   - Run `itch_server.py` to publish ITCH messages from the `.bin` file:
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
