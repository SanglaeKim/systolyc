
// file name: function.c

#include "svdpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_BYTES_WEIGHT 	(103040U)

#define DEBUG

typedef struct{
  int pBase;
  int numBytes;
  char *fileName;
}StSram;

void read_binary_file(StSram *pStSram , svOpenArrayHandle v){

  //printf("file size: %d\n", pStSram->numBytes);
  FILE *file = fopen(pStSram->fileName, "rb");
  if (file == NULL) {
	printf("%s - file open error\n", pStSram->fileName);
	return;
  }

  // check if file size is correct!
  fseek(file, 0, SEEK_END);
  uint32_t fileSize_ = ftell(file);

#ifdef DEBUG
  printf("file name: %s, file size expected: %d, got: %d\n"
		 , pStSram->fileName
		 , pStSram->numBytes
		 , fileSize_);
#endif

  if (fileSize_ != pStSram->numBytes) {
	printf("file name: %s\n", pStSram->numBytes);
	printf("[ERROR]exptected: %ld, fileSize_: %ld \n", pStSram->numBytes, fileSize_);
	return;
  }

  // move to the beggining
  fseek(file, 0, SEEK_SET);
  char *byteArr =(char*)malloc(fileSize_);
  fread(byteArr, 1, fileSize_, file);

  for (int i=0; i<fileSize_; ++i) {
	*((char*)svGetArrElemPtr(v,i)) = byteArr[i];
#ifdef DEBUG_
	if ( i>316 && i<329 ) {
	  printf("[%d]: %2x\n", i, byteArr[i]&0xFF);
	}
#endif

  }
  free(byteArr);
  
  fclose(file);
}
