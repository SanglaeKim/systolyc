
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

   task write_to_sram(input[21:0] addr, input [32:0]data);
	  begin
		 @(negedge bram_0_clk);
		 bram_0_en    = 1'b1;
		 bram_0_wr_en = 1'b1;
		 bram_0_addr  = addr;
		 bram_0_wrdata = data;
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
			write_to_sram(addr, lgTemp);
			addr++;
		 end

		 //for(int j='h120; j<'h160; j+=4) begin
		 //	lgTemp = {buffer[j+3] , buffer[j+2] , buffer[j+1] , buffer[j+0]};
		 //	$display("%m, %h", lgTemp);
		 //end

	  end // foreach (wStSram[i])

	  foreach (bStSram[i]) begin
		 buffer = new[bStSram[i].numBytes];
		 read_binary_file(bStSram[i], buffer);
		 for(int j=0; j<wStSram[i].numBytes; j+=4) begin
			lgTemp = {buffer[j+3] , buffer[j+2] , buffer[j+1] , buffer[j+0]};
		 end
	  end // foreach (bStSram[i])

	  $finish;
   end

// Define the struct in SystemVerilog
  typedef struct {
    int id;
    string name;
  } my_struct_t;

  // Import the C function
  import "DPI-C" function void set_string(output my_struct_t s);

  // Declare a variable of the struct type
  my_struct_t s;

  initial begin
    // Initialize the struct
    s.id = 1;
    s.name = "";

    // Call the C function to set the string
    set_string(s);

    // Display the struct values
    $display("id: %0d, name: %s", s.id, s.name);
  end




endmodule
