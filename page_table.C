#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"
#include "utils.H"

PageTable * PageTable::current_page_table = NULL;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = NULL;
ContFramePool * PageTable::process_mem_pool = NULL;
unsigned long PageTable::shared_size = 0;


void PageTable::init_paging(ContFramePool * _kernel_mem_pool, ContFramePool * _process_mem_pool, const unsigned long _shared_size){
  kernel_mem_pool = _kernel_mem_pool;
  process_mem_pool = _process_mem_pool;
  shared_size = _shared_size;
  Console::puts("Initialized Paging System\n");
}

//------------------------------------------------------------------------------------------------

PageTable::PageTable(){

  //grab a frame for the page directory array and allocate it
  unsigned long address = PAGE_SIZE * kernel_mem_pool->get_frames(1); //reserve memory for the page directory
  page_directory = (unsigned long *) address;

  //grab a frame for the new first page table we need for direct mapping
  address = PAGE_SIZE * kernel_mem_pool->get_frames(1);
  unsigned long* page_table = (unsigned long*) address;

  address = 0; 

  //direct map the whole page table, and mark each address as present
  int i;
  for(i = 0; i < (shared_size / PAGE_SIZE); i++){
    page_table[i] = address | 0x3;
    address = address + PAGE_SIZE;
  }

  //set the first entry in the page directory as present
  page_directory[0] = (unsigned long)page_table;
  page_directory[0] = page_directory[0] | 3;

  //set the rest of the entries as not present
  for(i = 1; i < ENTRIES_PER_PAGE; i++){
    page_directory[i] = 0x2;
  }

  Console::puts("Constructed Page Table object\n");
}

//------------------------------------------------------------------------------------------------

void PageTable::load(){
  //load the page directory of this PageTable into register 3 for use by the system
  write_cr3((unsigned long)page_directory);

  //set the current PageTable object as this one
  current_page_table = this;

  Console::puts("Loaded page table\n");
}

//------------------------------------------------------------------------------------------------

void PageTable::enable_paging(){
  //turn on paging by setting our variable to 1 and register 0 to 0x80000000 for the system to see
  paging_enabled = 1;
  write_cr0(read_cr0() | 0x80000000);

  Console::puts("Enabled paging\n");
}

//------------------------------------------------------------------------------------------------

void PageTable::handle_fault(REGS * _r){ 
  Machine::enable_interrupts();

 //assert(!(_r->err_code & 1)) //we only want to handle page not found errors

  //grab the address the system tried to access that caused the exception
  unsigned long address = read_cr2();

  //the director index is the upper 10 bits:
  unsigned long dir_index = address >> 22; //the page directory index in question

  //the page table index is the next 10 bits:
  unsigned int table_index  = (address >> 12) & 0x000003FF; //the page table index in question

  Console::puts("\nCR2 = ");
  Console::puti(address);
  Console::puts("\n");

  //check to see if the page directory is missing an entry
  if((current_page_table->page_directory[dir_index] & 0x1) != 0x1){
    Console::puts("Adding a page directory entry\n");

    //get a new frame for the page table itself
    unsigned long* page_table = (unsigned long*)(PAGE_SIZE * kernel_mem_pool->get_frames(1)); 

    //mark the table entries as not present
    int i;
    for(int i = 0; i < 1024; i++){
      page_table[i] = 0x2; //mark each page table entry as not present
    }
    //update the page dirctory and mark as present
    current_page_table->page_directory[dir_index] = (unsigned long) page_table; 
    current_page_table->page_directory[dir_index] |= 0x3;
  }

  Console::puts("Directory index:\n");
  Console::puti(dir_index);
  Console::puts("\n");

  //this is the address of the page table pointed to by the given index of the page directory
  unsigned long* page_table = (unsigned long*)((current_page_table->page_directory[dir_index]) & 0xFFFFF000);

  //get a new frame for the new page
  unsigned long new_address = (process_mem_pool->get_frames(1)) * PAGE_SIZE; 
  
  //map the new page to the new frame an set as present
  page_table[table_index] = new_address | 0x3; 
  Console::puts("Table index:\n");
  Console::puti(table_index);
  Console::puts("\n");

  Console::puts("handled page fault\n");
}