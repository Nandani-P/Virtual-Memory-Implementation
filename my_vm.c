#include "my_vm.h"
#include <math.h>

void *physicalMemory;
int numberOfVirtPages;
int numberOfPhysPages;
bool initializePhysicalFlag = false;

int** innerPagetable;       //size of pte_t
bool* physicalCheckFree;
bool* virtualCheckFree;

int pageTableEntriesPerBlock;
//int* outerPageTable[(int)pow(2,ceil((32 - log2(PGSIZE))/2))];      //initialize outerpage table inside SetPhysicalMem() without hardcode
int* outerPageTable;
int innerLength ;
int outerLength ;
int offsetLength;
int tlb_hit = 0;
int tlb_miss = 0;
void* tlb[TLB_SIZE][2];
pthread_mutex_t function_mutex;
/*
Function responsible for allocating and setting your physical memory
*/
void SetPhysicalMem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    pthread_mutex_lock(&function_mutex);
    int i;
   
    innerLength = floor((32 - log2(PGSIZE))/2);
    outerLength = ceil((32 - log2(PGSIZE))/2);
    offsetLength = log2(PGSIZE);
    
    pageTableEntriesPerBlock = (int)pow(2,innerLength);
    outerPageTable = (int*) malloc((int)pow(2,outerLength) * sizeof(int));
    physicalMemory = (void *) malloc(MEMSIZE);
    initializePhysicalFlag = true;
    
    for (i = 0; i < (int)pow(2,outerLength); i++) {
            outerPageTable[i] =  NULL;
    }

    for (i = 0; i < TLB_SIZE; i++) {
            tlb[i][0] = NULL;
            tlb[i][1] = NULL;
    }
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them

    numberOfVirtPages = MAX_MEMSIZE/ PGSIZE;
    numberOfPhysPages = MEMSIZE/ PGSIZE; 

    innerPagetable = malloc(numberOfVirtPages*sizeof(int*));
    for(i = 0; i<numberOfVirtPages; ++i){
        innerPagetable[i] = NULL;
    }

    physicalCheckFree = malloc(numberOfPhysPages*sizeof(bool));
    for(i = 0; i<numberOfPhysPages; ++i){
        physicalCheckFree[i] = true;
    }

    virtualCheckFree = malloc(numberOfVirtPages*sizeof(bool));
    for(i = 0; i<numberOfVirtPages; ++i){
        virtualCheckFree[i] = true;
    }
   pthread_mutex_unlock(&function_mutex);
}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int add_TLB(void *va, void *pa)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    pthread_mutex_lock(&function_mutex);
    unsigned int va_int = va; 
    int va_no = va_int >> offsetLength;

    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i][0] == NULL){
            tlb[i][0] = va_no;
            tlb[i][1] = pa;
            return 1;
        }
    }
    int randToEvict = rand() % (TLB_SIZE);
    tlb[randToEvict][0] = va_no;
    tlb[randToEvict][1] = pa;
    pthread_mutex_unlock(&function_mutex);
    return 1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *check_TLB(void *va) {
    /* Part 2: TLB lookup code here */
    pthread_mutex_lock(&function_mutex);
     unsigned int va_int = va;
    int va_no = va_int >> offsetLength;
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i][0] == va_no) {
           // printf("Tlb match occured");
	    pthread_mutex_unlock(&function_mutex);
            return tlb[i][1];  
        }
  }
    pthread_mutex_unlock(&function_mutex);
    return NULL;
}

/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate()
{
    pthread_mutex_lock(&function_mutex);
    double miss_rate = 0;
    /*Part 2 Code here to calculate and print the TLB miss rate*/
    miss_rate = (double)tlb_miss / (tlb_miss + tlb_hit);
    printf(" TLB miss and hit %d %d ",tlb_miss,tlb_hit);
    printf("TLB miss rate %lf \n", miss_rate);
    pthread_mutex_unlock(&function_mutex);
}


/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t * Translate(pde_t *pgdir, void *va) {
    //HINT: Get the Page directory index (1st level) Then get the
    //2nd-level-page table index using the virtual address.  Using the page
    //directory index and page table index get the physical address

    pthread_mutex_lock(&function_mutex);
    unsigned int va_int = va; 

    int firstTenbitsVA = va_int >> (offsetLength + innerLength);
    int nextTenbitsVA = ((1 << innerLength) - 1)  &  (va_int >> (offsetLength));
    int offset = ((1 << offsetLength) - 1)  &  (va_int);
    
    int pgdirVal = outerPageTable[firstTenbitsVA];
    pte_t *pa;
   
    pte_t* p = check_TLB(va);
    if(p != NULL){
        tlb_hit = tlb_hit + 1;
	pthread_mutex_unlock(&function_mutex);
	return ((char*) p + offset);
    }
    else
        tlb_miss = tlb_miss + 1;

    int addressInnerPgTable = pgdirVal *pageTableEntriesPerBlock + nextTenbitsVA;

    if (innerPagetable[addressInnerPgTable] != NULL){
        pa = innerPagetable[addressInnerPgTable];
        add_TLB(va,pa);
        // adding offset to PA
        pa = (char*) pa + offset;
        pthread_mutex_unlock(&function_mutex);
	return pa;
    }
        
   //If translation not successfull
    printf("Translation failed");
    pthread_mutex_unlock(&function_mutex);
    return NULL;
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
PageMap(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to Translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
    pthread_mutex_lock(&function_mutex);
    printf("Inside PageMap \n");
    unsigned int va_int = va; 

    int firstTenbitsVA = va_int >> (offsetLength + innerLength);
    int nextTenbitsVA = ((1 << innerLength) - 1)  &  (va_int >> (offsetLength)); 
    
    if (outerPageTable[firstTenbitsVA] == NULL){
        outerPageTable[firstTenbitsVA] = pgdir;
    }

    unsigned int pgdir_int = pgdir;
    
    int addressInnerPgTable = (pgdir_int * pageTableEntriesPerBlock) + nextTenbitsVA;

    if (innerPagetable[addressInnerPgTable] == NULL){
        innerPagetable[addressInnerPgTable] = pa;

        tlb_miss = tlb_miss + 1;
        add_TLB(va,pa);
    }
    pthread_mutex_unlock(&function_mutex);
    return -1;
}

bool check_require_avail_pa(int num_pages){
    pthread_mutex_lock(&function_mutex);
    int havePageCounter = 0;
    for (int i = 0; i < numberOfPhysPages; i++) {  
        if (physicalCheckFree[i] == true){
            havePageCounter++; 

            if (havePageCounter == num_pages){
		pthread_mutex_unlock(&function_mutex);
                return true; 
            }
        }
    }
    pthread_mutex_unlock(&function_mutex);
    return false;
}


/*Function that gets the next available page
*/
void *get_next_avail_pa(int num_pages) {

    //check for free space using physicalCheckFree array of size is equal to physical memory

    //Use virtual address bitmap to find the next free page
     pthread_mutex_lock(&function_mutex);
     for (int i = 0; i < numberOfPhysPages; i++) {     
        if (physicalCheckFree[i] == true){
            physicalCheckFree[i] = false;
            pthread_mutex_unlock(&function_mutex);
            return physicalMemory + i*PGSIZE;     
        }
    }
    pthread_mutex_unlock(&function_mutex);
    return NULL;
}

int get_next_avail_va(int num_pages) {

    //Use virtual address bitmap to find the next free page
     pthread_mutex_lock(&function_mutex);
     for (int i = 0; i < (numberOfVirtPages - num_pages); i++) {     
        if (virtualCheckFree[i] == true){
            printf("Inside get_next_avail_va if condition\n");
            bool haveContinuousPages = true;
            for (int j = 0; j < num_pages; j++){
                if (virtualCheckFree[i+j] != true){
                    haveContinuousPages = false;
                    break;
                }
            }

            if (!haveContinuousPages){
                continue;
            }

            for (int j = 0; j < num_pages; j++){  
                virtualCheckFree[i+j] = false;
                    
            }
            printf("Virtual address inside get_next_avail_va: %d\n", i);
	    pthread_mutex_unlock(&function_mutex);
            return i;      
        }
    }
    pthread_mutex_unlock(&function_mutex);
    return -1;
}
/* Function responsible for allocating pages
and used by the benchmark
*/
void *myalloc(unsigned int num_bytes) {

    //HINT: If the physical memory is not yet initialized, then allocate and initialize.
    pthread_mutex_lock(&function_mutex);

    if (initializePhysicalFlag == false)
    {
        SetPhysicalMem();
    }
    //printf("Outer page table: %d\n", outerPageTable[1]);

   /* HINT: If the page directory is not initialized, then initialize the
   page directory. Next, using get_next_avail(), check if there are free pages. If
   free pages are available, set the bitmaps and map a new page. Note, you will
   have to mark which physical pages are used. */

    //numner of pages required when myalloc is called 
    int num_pages = num_bytes/PGSIZE;
    if (num_bytes % PGSIZE != 0){
        num_pages = num_pages + 1; 
    }
    
    printf("Searching for physical memory\n");
    
    if (check_require_avail_pa(num_pages) == false){
        printf("Physical memory not found\n");
	pthread_mutex_unlock(&function_mutex);
        return NULL;
    }

    printf("Searching for virtual memory\n");

    int va_EntryNumber;
    va_EntryNumber = get_next_avail_va(num_pages);  
    if (va_EntryNumber == -1){
        printf("virtual memory is not available\n");
	pthread_mutex_unlock(&function_mutex);
        return NULL;
    }
    
    int pgDirEntryNumber;
    pgDirEntryNumber = va_EntryNumber / pageTableEntriesPerBlock; 

    int pgTableEntryNumberInBlock = va_EntryNumber % pageTableEntriesPerBlock; 

    // calculate 32- bit VA
    //void * innerPageTableEntryAddr = innerPagetable + va_EntryNumber*sizeof(int);
    unsigned int va_int = pgDirEntryNumber;
  
    va_int = va_int <<  innerLength;
    va_int = va_int | pgTableEntryNumberInBlock;
    va_int = va_int << offsetLength;  
    
    void *va = va_int;

    pde_t *pgDir = pgDirEntryNumber; // Assuming page directory entry number is same as inner page block number
    void *pa;

    for (int i = 0; i < num_pages; i++) {

        //checking for next free pages and getting the physical address of that page.
        pa = get_next_avail_pa(num_pages);   
        if (pa == NULL){
           printf("This should never happen\n");
        }
        PageMap(pgDir, va, pa);
	    va = va + PGSIZE;
    }
    printf("va is %d \n",va_int);
    pthread_mutex_unlock(&function_mutex);
    return va_int;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void myfree(void *va, int size) {

    //Free the page table entries starting from this virtual address (va)
    // Also mark the pages free in the bitmap
    //Only free if the memory from "va" to va+size is valid
    //free(va);
    pthread_mutex_lock(&function_mutex);
    printf("My Free started\n");
    int num_pages = size/PGSIZE;
    if (size % PGSIZE != 0){
        num_pages = num_pages + 1; 
    }
    unsigned int va_int = va; 

    int firstTenbitsVA = va_int >> (offsetLength + innerLength);
    int nextTenbitsVA = ((1 << innerLength) - 1)  &  (va_int >> (offsetLength));
   
    int pgdirVal = outerPageTable[firstTenbitsVA];
    
    int addressInnerPgTable = pgdirVal *pageTableEntriesPerBlock + nextTenbitsVA;

    for (int i = 0; i < num_pages; i++) {
        void *pa = innerPagetable[addressInnerPgTable + i];
        innerPagetable[addressInnerPgTable + i] = NULL;
        physicalCheckFree[(pa - physicalMemory)/PGSIZE] = true;
        virtualCheckFree[addressInnerPgTable + i] = true;  
    }
    pthread_mutex_unlock(&function_mutex);
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void PutVal(void *va, void *val, int size) {

    /* HINT: Using the virtual address and Translate(), find the physical page. Copy
       the contents of "val" to a physical page. NOTE: The "size" value can be larger
       than one page. Therefore, you may have to find multiple pages using Translate()
       function.*/

 //   printf("Put value\n");
    pthread_mutex_lock(&function_mutex);
    pte_t * physicalAddress;
    for (int i = 0; i < size; i++) {
        physicalAddress = Translate(NULL, (char*) va + i );  
        //setting value to a address(physicalAddress) 
        memcpy(physicalAddress, (char*)val+i, 1);
    }
    pthread_mutex_unlock(&function_mutex);
}


/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    "val" address. Assume you can access "val" directly by derefencing them.
    If you are implementing TLB,  always check first the presence of translation
    in TLB before proceeding forward */
//    printf("Get value\n");
    pthread_mutex_lock(&function_mutex);
    pte_t * physicalAddress;
    for (int i = 0; i < size; i++) {
        physicalAddress = Translate(NULL, (char*) va + i);        
        //setting value located at physicalAddress to val
        memcpy((char*)val+i, physicalAddress, 1);
    }
     pthread_mutex_unlock(&function_mutex);
}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void MatMult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
    matrix accessed. Similar to the code in test.c, you will use GetVal() to
    load each element and perform multiplication. Take a look at test.c! In addition to
    getting the values from two matrices, you will perform multiplication and
    store the result to the "answer array"*/   
      pthread_mutex_lock(&function_mutex);
      int x;
      int y;
      int sum;
      unsigned int address_mat1;
      unsigned int address_mat2;
      unsigned int address_ans;
      for(int i=0;i<size;i++)
        {
            for(int j=0;j<size;j++)
            {
               sum=0;
               for(int k=0;k<size;k++)
               {
           
                 address_mat1 = (unsigned int)mat1 + (i*size * sizeof(int))+(k* sizeof(int));
                 address_mat2 = (unsigned int)mat2 + (k*size * sizeof(int))+(j* sizeof(int));
                 GetVal((void *)address_mat1, &x, sizeof(int));
                 GetVal((void *)address_mat2, &y, sizeof(int));
		 sum+= (x * y);
               }
            address_ans = (unsigned int) answer+(i*size * sizeof(int))+(j* sizeof(int));
            PutVal((void *)address_ans, &sum, sizeof(int)); 
            }
        }
       pthread_mutex_unlock(&function_mutex);
}
