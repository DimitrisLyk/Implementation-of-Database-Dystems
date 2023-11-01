#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "bf.h"
#include "hp_file.h"
#include "record.h"
#include <errno.h>

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }

int HP_CreateFile(char *fileName){
  
  
  int fd;
  BF_Block *head_block;
  BF_Block_Init(&head_block);

   BF_ErrorCode e = BF_CreateFile(fileName);
    if (e != BF_OK) {
        BF_PrintError(e);
        return -1;
    }

    // Open the file
    e = BF_OpenFile(fileName, &fd);
    if (e != BF_OK) {
        BF_PrintError(e);
        return -1;
    }

  


  // Allocate a block for the header and initialize metadata
    e = BF_AllocateBlock(fd, head_block);
    if (e != BF_OK) {
        BF_PrintError(e);
        BF_CloseFile(fd);
        return -1;
    }
  char* data = BF_Block_GetData(head_block);             // Τα περιεχόμενα του block στην ενδιάμεση μνήμη
  //Record* rec = (Record*) data;                         // Ο δείκτης rec δείχνει στην αρχή της περιοχής μνήμης data

  HP_info hp;
  //int *ptr = &hp;
  hp.last_block_id = 0;
  hp.records_per_block = BF_BLOCK_SIZE / (int)(sizeof(Record)); 
  memset(data,0,BF_BLOCK_SIZE);
  memcpy(data, &hp, sizeof(hp));

  BF_Block_SetDirty(head_block);
  BF_GetBlock(fd,0,head_block);

  //BF_UnpinBlock(head_block);

  BF_Block_Destroy(&head_block);
  
  // Close the file
  e = BF_CloseFile(fd);
  if (e != BF_OK) {
      BF_PrintError(e);
      return -1;
  }
  
  return 0;
}

HP_info* HP_OpenFile(char *fileName, int *file_desc){
  
  BF_ErrorCode e = BF_OpenFile(fileName, file_desc);
  if (e != BF_OK) {
      return NULL;
  }

  BF_Block* head_block;

  BF_Block_Init(&head_block);

  e = BF_GetBlock(*file_desc,0,head_block);
  if(e != BF_OK){
    return NULL;
  }

  char* data = BF_Block_GetData(head_block);             // Τα περιεχόμενα του block στην ενδιάμεση μνήμη

  HP_info* hp;
  memcpy(hp, &data, sizeof(data));

  BF_Block_Destroy(&head_block);
  //BF_UnpinBlock(head_block);

  return hp;

}


int HP_CloseFile( HP_info* hp_info, int file_desc ){

  BF_Block* head_block;
  
  BF_ErrorCode e = BF_GetBlock(file_desc,0,head_block);
  if(e != BF_OK){
    return -1;
  }

  char* data = BF_Block_GetData(head_block);             // Τα περιεχόμενα του block στην ενδιάμεση μνήμη

  memcpy(hp_info, &data, sizeof(data));

  BF_UnpinBlock(head_block);

  // Close the file
  e = BF_CloseFile(file_desc);
  if (e != BF_OK) {
      BF_PrintError(e);
      return -1;
  }

  return 0;

}

int HP_InsertEntry(HP_info* hp_info, Record record, int file_desc){

  int record_size = sizeof(Record);
  
  // Υπολογιζω ποσα max records χωρανε σε ενα block
  int max_records_per_block = hp_info->records_per_block;

  int block_size = BF_BLOCK_SIZE;

  int block_num = 0;
  BF_Block* block;
  BF_Block_Init(&block);
  int *blocks_num;

  

  BF_GetBlockCounter(file_desc, *blocks_num);



  if(block_num == 1 ){
    //create first block

    BF_AllocateBlock(file_desc,block);
    
    char* data = BF_Block_GetData(block);
    memset(data,(int)0,BF_BLOCK_SIZE);
    int one = 1;
    //number of records in the block is one
    memcpy(data,&one,sizeof(int));
    data += sizeof(int);

    SR_InsertData(data,record);

  }





  return 0;
}

int HP_GetAllEntries(HP_info* hp_info, int value){
   return 0;
}

