#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "bf.h"
#include "hp_file.h"
#include "record.h"
#include <errno.h>

#define HP_ERROR -1

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    return HP_ERROR;        \
  }                         \
}

int HP_CreateFile(char *fileName){
  
  
  int fd;
  BF_Block *head_block;
  BF_Block_Init(&head_block);

  //Δημιουργω το αρχειο
  CALL_BF(BF_CreateFile(fileName));

  //Ανοιγω το αρχειο
  CALL_BF(BF_OpenFile(fileName, &fd));
    

  CALL_BF(BF_AllocateBlock(fd, head_block));

  CALL_BF(BF_GetBlock(fd,0,head_block));

  char* data = BF_Block_GetData(head_block);             // Τα περιεχόμενα του block στην ενδιάμεση μνήμη

  //Υπολογιζω τον χωρο που καταλαμβανει η δομη HP_block_info στο τελος καθε block
  int size_blockInfo = sizeof(HP_block_info);

  HP_info hp;

  hp.last_block_id = 0;           //Το id του head block ειναι μηδεν (το 1ο μπλοκ)
  hp.records_per_block = (BF_BLOCK_SIZE - size_blockInfo)/(sizeof(Record));  //Υπολογιζω ποσες εγγραφες χωρανε σε καθε μπλοκ αφαιρωντας
                                                                              //Το χωρο που πιανει το HP_block_info και διαιρωντας με το size καθε εγγραφης

  memset(data,0,BF_BLOCK_SIZE);
  memcpy(data, &hp, sizeof(hp));  //Αντιγραφω τα μεταδεδομενα του αρχειου σωρου στο 1ο block

  //Ενημερωνω οτι υπηρξε αλλαγη στα data
  BF_Block_SetDirty(head_block);


  //Κλεινω το αρχειο
  CALL_BF(BF_CloseFile(fd));
  
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

  //Ενας δεικτης HP_info ο οποιος δειχνει στην αρχη των δεδομενων για να διαβασουμε τα μεταδεδομενα και τον επιστρεφουμε
  HP_info* hp = (HP_info*)data;

  
  BF_Block_SetDirty(head_block);

  return hp;

}


int HP_CloseFile( HP_info* hp_info, int file_desc ){

  BF_Block* head_block;
  BF_Block_Init(&head_block);
  CALL_BF(BF_GetBlock(file_desc,0,head_block));
  
  //Unpin και destroy το 1ο μπλοκ που περιεχει τα μεταδεδομενα του αρχειου
  //Το head block ηταν pinned καθολη τη διαρκεια της εκτελεσης του προγραμματος
  //Και εγινε unpinned οταν κλεισαμε το αρχειο
  BF_UnpinBlock(head_block);
  BF_Block_Destroy(&head_block);

  CALL_BF(BF_CloseFile(file_desc));
}

int HP_InsertEntry(int file_desc, HP_info* hp_info, Record record){
  
  //Ελεγχω εαν βρισκομαστε στο head block
  //Εαν βρισκομαστε στο 1ο block, δλδ id == 0, πρεπει να δεσμευσουμε χωρο για ενα καινουριο μπλοκ
  //Καθως δεν εισαγουμε εγγραφες στο head block
  if (hp_info->last_block_id == 0) {
    printf("mphka\n");

    BF_Block* new_block;
    BF_Block_Init(&new_block);
    CALL_BF(BF_AllocateBlock(file_desc, new_block));

    char* new_data = BF_Block_GetData(new_block);
    //Τοποθετουμε τη δομη HP_block_info στο τελος του block υπολογιζοντας το offset
    HP_block_info* new_block_info = (HP_block_info*)(new_data + (BF_BLOCK_SIZE - sizeof(HP_block_info)));

    //Ο δεικτης rec δειχνει στην αρχη των data, εκει θα αντιγραφει η εγγραφη
    Record* new_rec = (Record*)new_data;
    memcpy(new_rec, &record, sizeof(Record));

    new_block_info->num_records++;    //Αυξανουμε τα records του μπλοκ κατα ενα γτ καναμε insert μια εγγραφη

    BF_Block_SetDirty(new_block);
    
    hp_info->last_block_id++;         //Αυξανουμε κατα 1 καθως φτιαξαμε καινουριο block οποτε βρισκομαστε μια θεση πιο πανω

    //Unpin δεν το χρειαζομαστε αλλο
    CALL_BF(BF_UnpinBlock(new_block));
    BF_Block_Destroy(&new_block);

    return hp_info->last_block_id;        //Επιστρεφουμε το id του block
  }
  


  //Στην περιπτωση που δεν βρισκομαστε στο head block συνεχιζεται η εκτελεση του κωδικα
  //Τωρα διακρινουμε παρακατω τις εξης δυο περιπτωσσεις
  //Η εγγραφη να χωραει στο block που εχουμε στη διαθεση μας οποτε κανουμε insert
  //Η εγγραφη να μην χωραει οποτε κανουμε allocate νεο block

  BF_Block* last_block;
  BF_Block_Init(&last_block);

  CALL_BF(BF_GetBlock(file_desc, hp_info->last_block_id, last_block));
  char* data = BF_Block_GetData(last_block);

  HP_block_info* block_info = (HP_block_info*)(data + (BF_BLOCK_SIZE - sizeof(HP_block_info)));

  printf("ok%d \n", block_info->num_records);

  //Εχω υπολογισει ποσα records χωρανε σε καθε block
  int max_records_per_block = hp_info->records_per_block;

  //Ελεγχω εαν υπαρχει χωρος στο μπλοκ που εχουμε στη διαθεση μας
  if ((block_info->num_records + 1) <= max_records_per_block) {

    //Εαν υπαρχει χωρος σημαινει πως μεσα στο block υπαρχουν εγγραφες/φη
    //Οποτε πρεπει να υπολογισουμε το offset ωστε να δειχνει στη σωστη θεση των data ο δεικτης rec
    int offset = block_info->num_records * sizeof(Record);
    Record* new_rec = (Record*)(data + offset);
    
    //Αντιγραφουμε την εγγραφη
    memcpy(new_rec, &record, sizeof(Record));

    block_info->num_records++;      //Αυξανουμε τα records που περιεχει το block κατα 1

    //Ενημερωνουμε πως υπηρξαν αλλαγες στα data του block
    BF_Block_SetDirty(last_block);

    CALL_BF(BF_UnpinBlock(last_block));
    BF_Block_Destroy(&last_block);

    return hp_info->last_block_id;
  }

  
  CALL_BF(BF_UnpinBlock(last_block));
  BF_Block_Destroy(&last_block);

  //Διαφορετικα φτιαχνουμε νεο μπλοκ αφου ειναι γεματο αυτο που ελεγχουμε και δεν ξεχναμε να κανουμε unpin και destroy το last_block το οποιο δεν χρησιμοποιησαμε
    BF_Block* new_block;
    BF_Block_Init(&new_block);
    CALL_BF(BF_AllocateBlock(file_desc, new_block));

    //Εκτελουμε την ιδια διαδικασια με τη μονη διαφορα πως τωρα ο δεικτης rec δειχνει στην αρχη των data αφου το μπλοκ ειναι κενο
    char* datab = BF_Block_GetData(new_block);
    HP_block_info* new_block_info = (HP_block_info*)(datab + (BF_BLOCK_SIZE - sizeof(HP_block_info)));
    new_block_info->num_records++;

    Record* new_rec = (Record*)datab;
    memcpy(new_rec, &record, sizeof(Record));

    //Αυξανουμε κατα ενα γιατι φτιαξαμε νεο block
    hp_info->last_block_id++;

    BF_Block_SetDirty(new_block);
    CALL_BF(BF_UnpinBlock(new_block));
    BF_Block_Destroy(&new_block);

    return hp_info->last_block_id;
}

int HP_GetAllEntries(int file_desc, HP_info* hp_info, int value){

  //Ελεγχουμε καθε block εκτος του block[0] δλδ για i = 0 καθως το head block δεν περιχει εγγραφες
  for (int i = 1; i <= hp_info->last_block_id; i++) {
    BF_Block* block;
    BF_Block_Init(&block);

    CALL_BF(BF_GetBlock(file_desc, i, block));
    char* data = BF_Block_GetData(block);             // Τα περιεχόμενα του block στην ενδιάμεση μνήμη
    HP_block_info* block_info = (HP_block_info*)(data + BF_BLOCK_SIZE - sizeof(HP_block_info));

    Record* new_rec = (Record*)data;
    //Ελεγχω ολες τις εγγραφες καθε μπλοκ εαν εχουν id = value και εκτυπωνω
    for(int j = 0; j< block_info->num_records; j++){
      printf("Block %d, Record %d: id=%d, name=%s, surname=%s, address=%s\n", i, j, new_rec[j].id, new_rec[j].name, new_rec[j].surname, new_rec[j].city);
      if(new_rec[j].id == value){
      printf("////////////////////////////////////////////////////////////////////////////////////\n");
      printf("//FOUND IT: Block %d, Record %d: id=%d, name=%s, surname=%s, city=%s//\n", i, j, new_rec[j].id, new_rec[j].name, new_rec[j].surname, new_rec[j].city);
      printf("////////////////////////////////////////////////////////////////////////////////////\n");
      }
    }

    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
  }
}

