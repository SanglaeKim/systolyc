// bram_defines.sv --- 
// 
// Filename: bram_defines.sv
// Description: 
// Author: Sanglae Kim
// Maintainer: 
// Created: Fri Dec 27 07:03:31 2024 (+0900)
// Version: 
// Package-Requires: ()
// Last-Updated: 
//           By: 
//     Update #: 0
// URL: 
// Doc URL: 
// Keywords: 
// Compatibility: 
// 
// 

// Code:

	  wFileSize = {432 , 4608 , 1024 , 2304 , 2304 , 1536};
	  bFileSize = {256 , 512 , 512 , 256 , 256 , 512};
	  
	  wStSram = '{
				  '{32'hA3000000, wFileSize[0], "../../data/wData0.bin"},
				  '{32'hA30004B0, wFileSize[1], "../../data/wData1.bin"},
				  '{32'hA3002A30, wFileSize[2], "../../data/wData2.bin"},
				  '{32'hA3003390, wFileSize[3], "../../data/wData3.bin"},
				  '{32'hA3004650, wFileSize[4], "../../data/wData4.bin"},
				  '{32'hA3005910, wFileSize[5], "../../data/wData5.bin"}};
	  
	  bStSram = '{
				  '{ 32'hA3006590 , bFileSize[0], "../../data/bData0.bin"},
				  '{ 32'hA3006690 , bFileSize[1], "../../data/bData1.bin"},
				  '{ 32'hA3006790 , bFileSize[2], "../../data/bData2.bin"},
				  '{ 32'hA3006890 , bFileSize[3], "../../data/bData3.bin"},
				  '{ 32'hA3006990 , bFileSize[4], "../../data/bData4.bin"},
				  '{ 32'hA3006A90 , bFileSize[5], "../../data/bData5.bin"}};

	  iStSram = '{
				  '{ 32'hA0000000 , `NUM_BYTES_WEIGHT, "../../data/iData0.bin"},
				  '{ 32'hA0020000 , `NUM_BYTES_WEIGHT, "../../data/iData1.bin"},
				  '{ 32'hA0040000 , `NUM_BYTES_WEIGHT, "../../data/iData2.bin"},
				  '{ 32'hA0060000 , `NUM_BYTES_WEIGHT, "../../data/iData3.bin"},
				  '{ 32'hA0080000 , `NUM_BYTES_WEIGHT, "../../data/iData4.bin"},
				  '{ 32'hA00A0000 , `NUM_BYTES_WEIGHT, "../../data/iData5.bin"},
				  '{ 32'hA00C0000 , `NUM_BYTES_WEIGHT, "../../data/iData6.bin"},
				  '{ 32'hA00E0000 , `NUM_BYTES_WEIGHT, "../../data/iData7.bin"},
				  '{ 32'hA0100000 , `NUM_BYTES_WEIGHT, "../../data/iData8.bin"},
				  '{ 32'hA0120000 , `NUM_BYTES_WEIGHT, "../../data/iData9.bin"},
				  '{ 32'hA0140000 , `NUM_BYTES_WEIGHT, "../../data/iData10.bin"},
				  '{ 32'hA0160000 , `NUM_BYTES_WEIGHT, "../../data/iData11.bin"},
				  '{ 32'hA0180000 , `NUM_BYTES_WEIGHT, "../../data/iData12.bin"},
				  '{ 32'hA01A0000 , `NUM_BYTES_WEIGHT, "../../data/iData13.bin"},
				  '{ 32'hA01C0000 , `NUM_BYTES_WEIGHT, "../../data/iData14.bin"},
				  '{ 32'hA01E0000 , `NUM_BYTES_WEIGHT, "../../data/iData15.bin"}};

// 
// bram_defines.sv ends here
