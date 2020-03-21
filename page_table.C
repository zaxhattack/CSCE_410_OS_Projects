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

  unsigned long address = PAGE_SIZE * kernel_mem_pool->get_frames(1); //reserve memory for the page directory
  page_directory = (unsigned long *) address;

  address = PAGE_SIZE * kernel_mem_pool->get_frames(1);
  unsigned long* page_table = (unsigned long*) address;

  address = 0;

  int i;
  for(i = 0; i < (shared_size / PAGE_SIZE); i++){
    page_table[i] = address | 0x3;
    address = address + PAGE_SIZE;
  }

  Console::puti(page_table[0]);

  page_directory[0] = (unsigned long)page_table;
  page_directory[0] = page_directory[0] | 3;

  for(i = 1; i < ENTRIES_PER_PAGE; i++){
    page_directory[i] = 0x2;
  }

  Console::puts("Constructed Page Table object\n");
}

//------------------------------------------------------------------------------------------------

void PageTable::load(){
   write_cr3((unsigned long)page_directory);
   current_page_table = this;
   Console::puts("Loaded page table\n");
}

//------------------------------------------------------------------------------------------------

void PageTable::enable_paging(){
   paging_enabled = 1;
   write_cr0(read_cr0() | 0x80000000);
   Console::puts("Enabled paging\n");
}

//------------------------------------------------------------------------------------------------

void PageTable::handle_fault(REGS * _r){ 

  Machine::enable_interrupts();

 //assert(!(_r->err_code & 1)) //we only want to handle page not found errors

  unsigned long address = read_cr2();
  unsigned long dir_index = address >> 22; //the page directory index in question
  unsigned int table_index  = (address >> 12) & 0x000003FF; //the page table index in question

  Console::puts("\nCR2 = ");
  Console::puti(address);
  Console::puts("\n");

  //check to see if the page directory is missing an entry
  if((current_page_table->page_directory[dir_index] & 0x1) != 0x1){
    Console::puts("Adding a page directory entry\n");
    unsigned long* page_table = (unsigned long*)(PAGE_SIZE * kernel_mem_pool->get_frames(1)); //get a new frame for the page table itself
    int i;
    for(int i = 0; i < 1024; i++){
      page_table[i] = 0x2; //mark each page table entry as not present
    }
    current_page_table->page_directory[dir_index] = (unsigned long) page_table; //update the page dirctory
    current_page_table->page_directory[dir_index] |= 0x3; //mark as present
  }

  Console::puts("Directory index:\n");
  Console::puti(dir_index);
  Console::puts("\n");

  unsigned long* page_table = (unsigned long*)((current_page_table->page_directory[dir_index]) & 0xFFFFF000); //this is the address of the page table pointed to by the given index of the page directory
  
  unsigned long new_address = (process_mem_pool->get_frames(1)) * PAGE_SIZE; //get a new frame for the new page

  page_table[table_index] = new_address | 0x3; //map the new page to the new frame an set as present
  Console::puts("Table index:\n");
  Console::puti(table_index);
  Console::puts("\n");

  Console::puts("handled page fault\n");
}