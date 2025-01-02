
// file name : file.sv
`include "defines.sv"

`define NUM_BYTES_WEIGHT 	(103040)

module top;

//   timeunit 1ns;
//   timeprecision 100ps;

   import "DPI-C" function void read_binary_file(input StSram stSram, output byte v[]);

   localparam real T_100Mhz = 10;
   localparam real T_40Mhz  = 25;
   localparam real T_1Mhz   = 1000;

   localparam numBytes = 103040;

   byte		  buffer[];

   //string	  fileName ="../../data/iData0.bin";
   integer		  addr = 0;
   
   logic [31:0]	lgTemp;

   logic		bram_0_clk		=1'b0	;	
   logic [21:0]	bram_0_addr		= 'h0	;		// Byte Address/clk
   logic		bram_0_en		=1'b0	;	
   logic		bram_0_wr_en	=1'b0	;	
   logic [31:0]	bram_0_wrdata	= 'h0	;	

   task write_to_sram;
	  input        i_clk    ;
	  input [21:0] i_addr   ;
	  input [31:0] i_wrdata ;

	  output	   o_en    ;
	  output	   o_wr_en ;
	  
	  output [21:0]	o_addr  ;
	  output [31:0]	o_wrdata;

	  begin
		 @(negedge bram_0_clk);
		 o_en     = 1'b1;
		 o_wr_en  = 1'b1;
		 o_addr   = i_addr;
		 o_wrdata = i_wrdata;
	  end
   endtask // write_to_sram
  
   StSram wStSram[6]; int wFileSize[6];
   StSram bStSram[6]; int bFileSize[6];
   StSram iStSram[16];

   always #(T_100Mhz/2) bram_0_clk = ~bram_0_clk;

   initial begin

`include "bram_defines.sv"

	  foreach (wStSram[i]) begin
		 buffer = new[wStSram[i].numBytes];
		 read_binary_file(wStSram[i], buffer);
		 addr = wStSram[i].pBase;

		 for(int j=0; j<wStSram[i].numBytes; j+=4) begin
			lgTemp = {buffer[j+3] , buffer[j+2] , buffer[j+1] , buffer[j+0]};

			write_to_sram(bram_0_clk, addr, lgTemp
						  , bram_0_en
						  , bram_0_wr_en
						  , bram_0_addr
						  , bram_0_wrdata
						  );
			addr++;
		 end
	  end // foreach (wStSram[i])

	  foreach (iStSram[i]) begin
		 buffer = new[iStSram[i].numBytes];
		 read_binary_file(iStSram[i], buffer);
		 for(int j=0; j<iStSram[i].numBytes; j+=4) begin
			lgTemp = {buffer[j+3] , buffer[j+2] , buffer[j+1] , buffer[j+0]};
		 end
	  end // foreach (iStSram[i])

	  $finish;
   end

endmodule
