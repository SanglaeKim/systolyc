// spROM.sv --- 
// 
// Filename: spROM.sv
// Description: 
// Author: Sanglae Kim
// Maintainer: 
// Created: Thu Dec 26 11:30:35 2024 (+0900)
// Version: 
// Package-Requires: ()
// Last-Updated: Thu Dec 26 11:31:13 2024 (+0900)
//           By: Sanglae Kim
//     Update #: 1
// URL: http://www.markharvey.info/rtl/mem_init_20.02.2017/mem_init_20.02.2017.html
// Doc URL: 
// Keywords: 
// Compatibility: 
// 
// 
`include "defines.sv"

module spROM
  #(parameter int unsigned	width = 8,
	parameter int unsigned	depth = 8,
	parameter				intFile = "dummy.mif",
	localparam int unsigned	addrBits = $clog2(depth)
	)
   (
	input logic				   CLK,
	input logic [addrBits-1:0] ADDRESS,
	output logic [width-1:0]   DATAOUT
	);

   import "DPI-C" function void read_binary_file(input StSram_t StSram, output byte v[]);

   
   logic [width-1:0] rom [0:depth-1];
   // initialise ROM contents
   initial begin
	  //$readmemh(intFile,rom);
	  read_binary_file(intFile,rom);
   end
   always_ff @ (posedge CLK)
	 begin
		DATAOUT <= rom[ADDRESS];
	 end
endmodule : spROM


// 
// spROM.sv ends here
