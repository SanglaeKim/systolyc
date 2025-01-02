// RamChip.sv --- 
// 
// Filename: RamChip.sv
// Description: 
// Author: Sanglae Kim
// Maintainer: 
// Created: Thu Dec 26 11:11:50 2024 (+0900)
// Version: 
// Package-Requires: ()
// Last-Updated: Thu Dec 26 11:29:55 2024 (+0900)
//           By: Sanglae Kim
//     Update #: 5
// URL: https://www.doulos.com/knowhow/verilog/simple-ram-model/#
// Doc URL: 
// Keywords: 
// Compatibility: 
// 
// 

module RamChip (Address, Data, CSn, WEn, OEn);

parameter AddressSize = 1;
parameter WordSize    = 1;

input [AddressSize-1:0] Address;
inout [WordSize-1:0] Data;
input CSn, WEn, OEn;

reg [WordSize-1:0] Mem [0:(1<<AddressSize)-1];

assign Data = (!CSn && !OEn) ? Mem[Address] : {WordSize{1'bz}};

always @(CSn or WEn)
  if (!CSn && !WEn)
    Mem[Address] = Data;

always @(WEn or OEn)
  if (!WEn && !OEn)
    $display("Operational error in RamChip: OEn and WEn both active");

endmodule

// 
// RamChip.sv ends here
