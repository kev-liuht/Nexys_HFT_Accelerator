# Software Directory
This directory contains the baseline software implementation of the HFT system, as well as the web market interface.

# Prerequistes
Python 3.8+
Install all python dependencies by running
```
pip install -r requirements.txt
```

## Sample Data

Download ITCH sample data by downloading and unzipping `https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/01302019.NASDAQ_ITCH50.gz` to `data/01302019.NASDAQ_ITCH50.bin`.

The sample data contains a lot of irrelevant messages, so to create a dataset with the relevant messages (Add Order / Cancel Order), run the following:
```
# run the server to send the messages
python itch_server.py
```

And run the following in a separate terminal
```
# this program takes the relevant messages and writes to 01302019.NASDAQ_ITCH50_AFECXDU.bin by default. You can override the output file by using the -o or --output_file
python get_test_data_tcp_client.py --output_file <filename>
``` 

# Resources

- [NASDAQ ITCH 5.0 Protocol](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHspecification.pdf)