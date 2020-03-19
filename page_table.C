#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

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

PageTable::PageTable(){

  unsigned long address = 4096 * kernel_mem_pool->get_frames(1); //reserve memory for the page directory
  page_directory = (unsigned long *) address;

  address = 4096 * kernel_mem_pool->get_frames(1);
  unsigned long* page_table = (unsigned long*) address;

  address = 0;

  int i;
  for(i = 0; i < 1024; i++){
    page_table[i] = address | 3;
    address = address + 4096;
  }

  Console::puti(page_table[0]);

  page_directory[0] = (unsigned long)page_table;
  page_directory[0] = page_directory[0] | 3;

  for(i = 1; i < 1024; i++){
    page_directory[i] = 0 | 2;
  }

  current_page_table = this;

  Console::puts("Constructed Page Table object\n");
}


void PageTable::load(){
   write_cr3((unsigned long)page_directory);
   Console::puts("Loaded page table\n");
}

void PageTable::enable_paging(){
   paging_enabled = 1;
   write_cr0(read_cr0() | 0x80000000);
   Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS * _r){
  assert(false);
  Console::puts("handled page fault\n");
}

