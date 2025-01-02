
// file name: function.c

#include "svdpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_BYTES_WEIGHT 	(103040U)

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

  if (fileSize_ != pStSram->numBytes) {
	printf("file name: %s\n", pStSram->numBytes);
	printf("[ERROR]exptected: %ld, fileSize_: %ld \n", pStSram->numBytes, fileSize_);
	return;
  }

  // move to the beggining
  fseek(file, 0, 0);
  char *byteArr =(char*)malloc(fileSize_);
  fread(byteArr, 1, fileSize_, file);

  for (int i=0; i<fileSize_; ++i) {
	*((char*)svGetArrElemPtr(v,i)) = byteArr[i];
#ifdef DEBUG
	if ( i>316 && i<329 ) {
	  printf("[%d]: %2x\n", i, byteArr[i]&0xFF);
	}
#endif

  }
  free(byteArr);
  
  fclose(file);
}

// Define the struct in C
typedef struct {
  int id;
  const char* name;
} my_struct_t;

// Function to set the string in the struct
void set_string(my_struct_t* s) {
  static  char msg[32];
  sprintf(msg, "iData%d.bin", s->id);
  //printf("%s\n", msg);
  //s->name = "Hello from C!";
  //strcpy(s->name, msg);
  s->name = msg;
}
