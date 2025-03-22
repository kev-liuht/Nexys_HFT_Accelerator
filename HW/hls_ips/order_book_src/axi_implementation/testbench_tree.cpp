//#include <iostream>
//#include <hls_stream.h>
//#include <ap_int.h>
//#include "orderbook_tree_optimized.h"  // Must have declaration of Orderbook_wrapper(...)
//
//// For clarity in printing errors
//#include <iomanip>
//
//using namespace std;
//
///**
// * Helper function to print top-5 ask/bid arrays.
// */
//static void print_top5(const char* side,
//                       const ap_uint<32> prices[5],
//                       const ap_uint<32> quantities[5])
//{
//    cout << side << " Prices: ";
//    for (int i = 0; i < 5; i++) {
//        if (prices[i] == 0xFFFFFFFF)
//            cout << "N/A ";
//        else
//            cout << prices[i] << " ";
//    }
//    cout << endl << side << " Quantities: ";
//    for (int i = 0; i < 5; i++) {
//        cout << quantities[i] << " ";
//    }
//    cout << endl << "-------------------" << endl;
//}
//
///**
// * A class to encapsulate testing logic for an HLS-based order book.
// * It manages the streams and provides helper functions to send commands
// * and read outputs.
// */
//class OrderBookTestRunner {
//public:
//    // Streams for interacting with the order book
//    hls::stream<ap_uint<136>> inStream_pars;
//    hls::stream<ap_uint<1>>   inStream_algo;
//    hls::stream<ap_uint<32>>  outStream_algo;
//
//    // We'll assume we have exactly 2 stocks, ID 0 and 1.
//    int stock_ids[2];
//
//    // Constructor (initialize things if needed)
//    OrderBookTestRunner()
//    : initialized(false), stock_ids{0, 1} {
//        // If you need any special init code, put it here.
//    }
//
//    // Main public method to run all tests for a particular stock_id
//    // Returns true if all tests pass, false otherwise.
//    bool runTestsForStock(ap_uint<32> stock_id) {
//        cout << "----------------------------------------" << endl;
//        cout << "Running test suite for stock_id = " << stock_id << endl;
//        cout << "----------------------------------------" << endl;
//
//        bool ok = true;
//        ok &= testCase1_AddOrders(stock_id);
//        ok &= testCase2_AddMoreBids(stock_id);
//        ok &= testCase3_CancelOrder(stock_id);
//
//        if (ok) {
//            cout << "All test cases for stock_id " << stock_id
//                 << " passed!" << endl;
//        } else {
//            cout << "One or more test cases for stock_id " << stock_id
//                 << " FAILED!" << endl;
//        }
//        return ok;
//    }
//
//private:
//    bool initialized;
//
//    //------------------------------------------------------
//    // New Function: checkEmpty(...)
//    //------------------------------------------------------
//    // Checks that the given stock_id has no orders (top-5 are all empty).
//    // We'll define "empty" as price=0xFFFFFFFF and qty=0 for all 5 levels
//    // of both ask and bid.
//    bool checkEmpty(ap_uint<32> stock_id) {
//        bool pass = true;
//        ap_uint<32> askPrices[5], askQty[5];
//        ap_uint<32> bidPrices[5], bidQty[5];
//
//        // Get the top-5 for this stock
//        getTop5Levels(stock_id, askPrices, askQty, bidPrices, bidQty);
//
//        // Check each of the 5 levels
//        for (int i = 0; i < 5; i++) {
//            if (askPrices[i] != 0xFFFFFFFF) {
//                cerr << "ERROR: stock " << stock_id
//                     << " askPrices[" << i << "] expected 0xFFFFFFFF but got "
//                     << askPrices[i] << endl;
//                pass = false;
//            }
//            if (askQty[i] != 0) {
//                cerr << "ERROR: stock " << stock_id
//                     << " askQty[" << i << "] expected 0 but got "
//                     << askQty[i] << endl;
//                pass = false;
//            }
//            if (bidPrices[i] != 0xFFFFFFFF) {
//                cerr << "ERROR: stock " << stock_id
//                     << " bidPrices[" << i << "] expected 0xFFFFFFFF but got "
//                     << bidPrices[i] << endl;
//                pass = false;
//            }
//            if (bidQty[i] != 0) {
//                cerr << "ERROR: stock " << stock_id
//                     << " bidQty[" << i << "] expected 0 but got "
//                     << bidQty[i] << endl;
//                pass = false;
//            }
//        }
//        return pass;
//    }
//
//    //------------------------------------------------------
//    // Test case 1: Add one Ask (Sell) & one Bid (Buy), then verify
//    //------------------------------------------------------
//    bool testCase1_AddOrders(ap_uint<32> stock_id) {
//        cout << "[Test Case 1] Adding Orders to Stock " << stock_id << endl;
//
//        // Before we do anything with this stock, let's ensure all OTHER stocks are empty.
//        bool pass = true;
//
//
//        // Add Ask Order: stock_id, order_id=2, price=104, qty=300
//        sendAddOrder(stock_id, /*order_id=*/2, /*price=*/104, /*qty=*/300, /*isAsk=*/true);
//
//        // Add Bid Order: stock_id, order_id=1, price=108, qty=500
//        sendAddOrder(stock_id, /*order_id=*/1, /*price=*/108, /*qty=*/500, /*isAsk=*/false);
//
//        // Now get top-5 from hardware (just for local checks)
//        ap_uint<32> askPrices[5], askQty[5], bidPrices[5], bidQty[5];
//        getTop5Levels(stock_id, askPrices, askQty, bidPrices, bidQty);
//
//        cout << "Stock " << stock_id
//             << " Results after adding one Ask & one Bid:" << endl;
//        print_top5("Ask", askPrices, askQty);
//        print_top5("Bid", bidPrices, bidQty);
//
//        // Verification logic for stock under test
//        if (askPrices[0] != 104) {
//            cerr << "ERROR: TestCase1 - Ask Price[0] expected 104 but got "
//                 << askPrices[0] << endl;
//            pass = false;
//        }
//        if (askQty[0] != 300) {
//            cerr << "ERROR: TestCase1 - Ask Qty[0] expected 300 but got "
//                 << askQty[0] << endl;
//            pass = false;
//        }
//        if (bidPrices[0] != 108) {
//            cerr << "ERROR: TestCase1 - Bid Price[0] expected 108 but got "
//                 << bidPrices[0] << endl;
//            pass = false;
//        }
//        if (bidQty[0] != 500) {
//            cerr << "ERROR: TestCase1 - Bid Qty[0] expected 500 but got "
//                 << bidQty[0] << endl;
//            pass = false;
//        }
//
//        // After completing this test case, print the top-5 for *all* stocks
//        cout << "\n>>> End of TestCase1: Printing top-5 for all known stock IDs...\n";
//        printAllStocksTop5();
//
//        for (int i = 0; i < 2; i++) {
//                    if (stock_ids[i] != (int)stock_id) {
//                        pass &= checkEmpty(stock_ids[i]);
//                    }
//                }
//
//        return pass;
//    }
//
//    //------------------------------------------------------
//    // Test case 2: Add multiple Bid orders, then verify top-5
//    //------------------------------------------------------
//    bool testCase2_AddMoreBids(ap_uint<32> stock_id) {
//        cout << "[Test Case 2] Adding More Bids to Stock " << stock_id << endl;
//
//        // Check other stocks are empty first
//        bool pass = true;
//
//        // Example set of (price, qty) for new Bid orders
//        ap_uint<32> prices[] = {110, 106, 110, 104, 114, 124, 118, 114, 122, 120};
//        ap_uint<32> qtys[]   = {600, 400, 700, 300, 800, 300, 2100, 3100, 3500, 3600};
//
//        // Add them in a loop
//        for (int i = 0; i < 10; i++) {
//            ap_uint<32> order_id = 3 + i;  // arbitrary
//            sendAddOrder(stock_id, order_id, prices[i], qtys[i], /*isAsk=*/false);
//        }
//
//        // Verify top-5
//        ap_uint<32> askPrices[5], askQty[5], bidPrices[5], bidQty[5];
//        getTop5Levels(stock_id, askPrices, askQty, bidPrices, bidQty);
//
//        cout << "Stock " << stock_id
//             << " Updated Bids (top-5) after adding multiple bids:" << endl;
//        print_top5("Bid", bidPrices, bidQty);
//
//        // Example verification checks for the stock under test
//        if (bidPrices[0] != 124) {
//            cerr << "ERROR: TestCase2 - Bid Price[0] expected 124 but got "
//                 << bidPrices[0] << endl;
//            pass = false;
//        }
//        if (bidQty[0] != 300) {
//            cerr << "ERROR: TestCase2 - Bid Qty[0] expected 300 but got "
//                 << bidQty[0] << endl;
//            pass = false;
//        }
//        if (bidPrices[1] != 122) {
//            cerr << "ERROR: TestCase2 - Bid Price[1] expected 122 but got "
//                 << bidPrices[1] << endl;
//            pass = false;
//        }
//        if (bidQty[1] != 3500) {
//            cerr << "ERROR: TestCase2 - Bid Qty[1] expected 3500 but got "
//                 << bidQty[1] << endl;
//            pass = false;
//        }
//        if (bidPrices[2] != 120) {
//            cerr << "ERROR: TestCase2 - Bid Price[2] expected 120 but got "
//                 << bidPrices[2] << endl;
//            pass = false;
//        }
//        if (bidQty[2] != 3600) {
//            cerr << "ERROR: TestCase2 - Bid Qty[2] expected 3600 but got "
//                 << bidQty[2] << endl;
//            pass = false;
//        }
//        if (bidPrices[3] != 118) {
//            cerr << "ERROR: TestCase2 - Bid Price[3] expected 118 but got "
//                 << bidPrices[3] << endl;
//            pass = false;
//        }
//        if (bidQty[3] != 2100) {
//            cerr << "ERROR: TestCase2 - Bid Qty[3] expected 2100 but got "
//                 << bidQty[3] << endl;
//            pass = false;
//        }
//        if (bidPrices[4] != 114) {
//            cerr << "ERROR: TestCase2 - Bid Price[4] expected 114 but got "
//                 << bidPrices[4] << endl;
//            pass = false;
//        }
//        if (bidQty[4] != 3900) {
//            cerr << "ERROR: TestCase2 - Bid Qty[4] expected 3900 but got "
//                 << bidQty[4] << endl;
//            pass = false;
//        }
//
//        // After completing this test case, print the top-5 for *all* stocks
//        cout << "\n>>> End of TestCase2: Printing top-5 for all known stock IDs...\n";
//        printAllStocksTop5();
//
//        for (int i = 0; i < 2; i++) {
//                    if (stock_ids[i] != (int)stock_id) {
//                        pass &= checkEmpty(stock_ids[i]);
//                    }
//                }
//
//
//
//        return pass;
//    }
//
//    //------------------------------------------------------
//    // Test case 3: Cancel (part of) an order, then verify
//    //------------------------------------------------------
//    bool testCase3_CancelOrder(ap_uint<32> stock_id) {
//        cout << "[Test Case 3] Canceling an Order in Stock " << stock_id << endl;
//
//        // Check other stocks are empty first
//        bool pass = true;
//
//
//        // Example: Cancel some quantity from order_ref_num=8
//        cancelOrder(stock_id, /*order_id=*/8, /*cancel_qty=*/300);
//
//        // Now get top-5 after cancel
//        ap_uint<32> askPrices[5], askQty[5], bidPrices[5], bidQty[5];
//        getTop5Levels(stock_id, askPrices, askQty, bidPrices, bidQty);
//
//        cout << "Stock " << stock_id << " Bids After Cancel:" << endl;
//        print_top5("Bid", bidPrices, bidQty);
//
//        // Example checks
//        if (bidPrices[0] != 122) {
//            cerr << "ERROR: TestCase3 - Bid Price[0] expected 122 but got "
//                 << bidPrices[0] << endl;
//            pass = false;
//        }
//        if (bidPrices[1] != 120) {
//            cerr << "ERROR: TestCase3 - Bid Price[1] expected 120 but got "
//                 << bidPrices[1] << endl;
//            pass = false;
//        }
//        if (bidPrices[2] != 118) {
//            cerr << "ERROR: TestCase3 - Bid Price[2] expected 118 but got "
//                 << bidPrices[2] << endl;
//            pass = false;
//        }
//        if (bidPrices[3] != 114) {
//            cerr << "ERROR: TestCase3 - Bid Price[3] expected 114 but got "
//                 << bidPrices[3] << endl;
//            pass = false;
//        }
//        // etc...
//        for (int i = 0; i < 2; i++) {
//                    if (stock_ids[i] != (int)stock_id) {
//                        pass &= checkEmpty(stock_ids[i]);
//                    }
//                }
//        // After completing this test case, print the top-5 for *all* stocks
//        cout << "\n>>> End of TestCase3: Printing top-5 for all known stock IDs...\n";
//        printAllStocksTop5();
//
//        return pass;
//    }
//
//    //------------------------------------------------------
//    // HELPER: Send an "Add" order to the order book
//    //------------------------------------------------------
//    void sendAddOrder(ap_uint<32> stock_id,
//                      ap_uint<32> order_id,
//                      ap_uint<32> price,
//                      ap_uint<32> quantity,
//                      bool        isAsk)
//    {
//        ap_uint<136> cmd;
//        // Format:
//        //   [31..0]   = stock_id
//        //   [63..32]  = order_ref_num
//        //   [95..64]  = num_shares
//        //   [127..96] = price
//        //   [131..128]= order_type (0x1 for Add)
//        //   [132]     = buy_sell (1=Ask, 0=Bid)
//        cmd.range(31, 0)    = stock_id;
//        cmd.range(63, 32)   = order_id;
//        cmd.range(95, 64)   = quantity;
//        cmd.range(127, 96)  = price;
//        cmd.range(131, 128) = 0x1;    // Add
//        cmd[132]            = isAsk ? 1 : 0;
//        cmd.range(135, 133) = 0;      // Unused
//
//        inStream_pars.write(cmd);
//        callOrderbookWrapper();
//    }
//
//    //------------------------------------------------------
//    // HELPER: Send a "Cancel" command to the order book
//    //------------------------------------------------------
//    void cancelOrder(ap_uint<32> stock_id,
//                     ap_uint<32> order_id,
//                     ap_uint<32> cancel_qty)
//    {
//        ap_uint<136> cmd;
//        // For Cancel:
//        //   [31..0]   = stock_id
//        //   [63..32]  = order_id
//        //   [95..64]  = cancel_qty
//        //   [131..128]= 0x2 (Cancel)
//        cmd.range(31, 0)    = stock_id;
//        cmd.range(63, 32)   = order_id;
//        cmd.range(95, 64)   = cancel_qty;
//        cmd.range(127, 96)  = 0;      // Unused
//        cmd.range(131, 128) = 0x2;    // Cancel
//        cmd[132]            = 0;      // Unused
//        cmd.range(135, 133) = 0;      // Unused
//
//        inStream_pars.write(cmd);
//        callOrderbookWrapper();
//    }
//
//    //------------------------------------------------------
//    // HELPER: Request top-5 from the HLS order book, then read results
//    //------------------------------------------------------
//    void getTop5Levels(ap_uint<32> stock_id,
//                       ap_uint<32> askPrices[5],
//                       ap_uint<32> askQty[5],
//                       ap_uint<32> bidPrices[5],
//                       ap_uint<32> bidQty[5])
//    {
//        // Write "1" to inStream_algo to trigger top-5 extraction
//        inStream_algo.write(1);
//        callOrderbookWrapper();
//
//        // The hardware returns 40 words (32 bits each) total,
//        // supporting up to 2 stocks (each stock = 20 words).
//        // Layout:
//        //    For stock 0: [0..4] = AskPrice,  [5..9]  = AskQty,
//        //                 [10..14]= BidPrice, [15..19]= BidQty
//        //    For stock 1: [20..24]= AskPrice, [25..29]= AskQty,
//        //                 [30..34]= BidPrice, [35..39]= BidQty
//        ap_uint<32> out_data[40];
//        for (int i = 0; i < 40; i++) {
//            out_data[i] = outStream_algo.read();
//        }
//
//        int start_index = stock_id * 20;
//
//        for (int i = 0; i < 5; i++) {
//            askPrices[i] = out_data[start_index + i];       // 0..4 or 20..24
//            askQty[i]    = out_data[start_index + i + 5];   // 5..9 or 25..29
//            bidPrices[i] = out_data[start_index + i + 10];  // 10..14 or 30..34
//            bidQty[i]    = out_data[start_index + i + 15];  // 15..19 or 35..39
//        }
//    }
//
//    //------------------------------------------------------
//    // Utility: Print the top-5 for *all* stocks
//    //------------------------------------------------------
//    void printAllStocksTop5() {
//        cout << "=== Printing top-5 for all known stock IDs ===" << endl;
//        for (int i = 0; i < 2; i++) {
//            ap_uint<32> askPrices[5], askQty[5], bidPrices[5], bidQty[5];
//            getTop5Levels(stock_ids[i], askPrices, askQty, bidPrices, bidQty);
//
//            cout << "Stock ID: " << stock_ids[i] << endl;
//            print_top5("Ask", askPrices, askQty);
//            print_top5("Bid", bidPrices, bidQty);
//            cout << endl;
//        }
//    }
//
//    //------------------------------------------------------
//    // Utility: Call the HLS order book
//    //------------------------------------------------------
//    void callOrderbookWrapper() {
//        // If there is any one-time initialization needed, do it only once.
//        if (!initialized) {
//            // e.g. an "init" command if your design supports it.
//            initialized = true;
//        }
//        // The HLS function (defined in "orderbook_tree_optimized.h").
//        Orderbook_wrapper(inStream_pars, inStream_algo, outStream_algo);
//    }
//};
//
///**
// * Main function:
// * - Creates a test runner instance
// * - Runs tests for the desired stock ID (or both)
// * - Returns 0 if all tests pass, 1 otherwise
// */
//int main() {
//    OrderBookTestRunner tester;
//
//    // Run tests for stock_id = 0, for example
//    bool result = tester.runTestsForStock(/*stock_id=*/0);
//
//    cout << "----------------------------------------" << endl;
//    if (result) {
//        cout << "All test cases for stock_id=0 completed successfully!" << endl;
//        cout << "----------------------------------------" << endl;
//        return 0;  // success
//    } else {
//        cout << "Some tests failed for stock_id=0!" << endl;
//        cout << "----------------------------------------" << endl;
//        return 1;  // failure
//    }
//}
