/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool * pool_list[1000]; //the list of frame pools

//------------------------------------------------------------------------------------------------------

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no,
                             unsigned long _n_info_frames)
{
    //set variables:
    base_frame_number = _base_frame_no;
    number_of_frames = _n_frames;
    info_frame_number = _info_frame_no;
    number_of_info_frames = _n_info_frames;
    
    //add this frame pool to the list of frame pools
    for(int i = 0; i < 10000; i++){
        if(pool_list[i] == NULL){
            break;
        }
    }

    
    //set the info frames, just like in simple frame manager
    if(info_frame_number == 0) {
        bitmap = (unsigned char *) (base_frame_number * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *) (info_frame_number * FRAME_SIZE);
    }

    //initialize all frames as free
    for (int i = 0; i < (number_of_frames / 4); ++i){
        bitmap[i] = 0xFF;
    }

    mark_bitmap_index(info_frame_number, occ);
}

//------------------------------------------------------------------------------------------------------

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    //local variables to be used:
    unsigned long count = 0; //keeps track of how long the hole is
    unsigned long iterate = 0; //keeps track of what bitmap index we're on
    unsigned int start = 0; //keeps track of the start of the hole we find

    //run until break
    while(1){
        //first, check the bitmap index we're on to see if it's free
        if(check_bitmap_index(iterate) == free){
            //Console::puts("Found a free one!! Counting...");
            start = iterate; //if it's free, set the start point for the hole here
            while(1){ //now, check to see how long the hole is
                if(check_bitmap_index(iterate) == free){
                    count++; //keep track of the length
                }else{
                    break; //if the hole runs to the end before we determine it's big enough, break
                    count = 0;
                }
                if(count == _n_frames){ //if the hole is big enough, break                  
                    break;
                }
                iterate++;
            }
        }

        if(count == _n_frames){ //if the hole is big enough, break out of the original loop
            break;
        }

    iterate++;

    }

    // Console::puts("Reserving the header....");
    mark_bitmap_index(start, hos); //set the header

    iterate = start;

    iterate++;

    for (int i = 0; i < _n_frames - 1; ++i){ //now set subsequent frames as occupied
        mark_bitmap_index(iterate, occ);
        iterate++;
    }

    free_frame_number = free_frame_number - _n_frames; //update the free frame number


    return start + base_frame_number;
    
}

//------------------------------------------------------------------------------------------------------

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    for (int i = _base_frame_no; i < _n_frames; ++i){ //simply use our function to set the range as occupied so nothing writes over it
        mark_bitmap_index(get_bitmap_index(_base_frame_no), occ);
    }
}

//------------------------------------------------------------------------------------------------------

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    //iterate the list of pools to find the one containing the frame in question
    for(int i = 0; i < 10000; ++i){
        if(pool_list[i] == NULL){
            Console::puts("Error: ran to end of pool_list\n"); //if we can't find it, something went wrong
            assert(false);
        }
        
        if(_first_frame_no >= pool_list[i]->base_frame_number && _first_frame_no <= (pool_list[i]->base_frame_number + pool_list[i]->number_of_frames)){ //check each frame pool for the frame
            Console::puts("Found the frame pool...\n");
            pool_list[i]->release_frames_pvt(_first_frame_no); //if we find it, call that frame pool's release function to release the frame block and break
            break;
        }
    }
}

//------------------------------------------------------------------------------------------------------

void ContFramePool::release_frames_pvt(unsigned long first_frame){
    
    //local variables:
    unsigned long count = 1; //keep track of how many frames we're freeing
    unsigned long bitmap_index = get_bitmap_index(first_frame); //find out the bitmao index of the head of sequence frame

    mark_bitmap_index(bitmap_index, free); //mark the head of sequence frame as free

   bitmap_index++;

    while(check_bitmap_index(bitmap_index) == occ){ //keep setting the occied frames following as free until we get to another head of sequence or a free frame
        mark_bitmap_index(bitmap_index, free);
        count++;
        bitmap_index++;
    }

    free_frame_number = free_frame_number + count; //update the free frame count
}

//------------------------------------------------------------------------------------------------------

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    Console::puts("Needed info...\n");
    return(_n_frames / 16384 + (_n_frames % 16384 > 0 ? 1 : 0)); //speaks for itself. Computes the number of frames needed for info for this frame pool
}

//------------------------------------------------------------------------------------------------------

unsigned long ContFramePool::get_bitmap_index(unsigned long frame_number){
    return(frame_number - base_frame_number); //simple function to find the bitmap index of the frame
}

//------------------------------------------------------------------------------------------------------

ContFramePool::frame_status ContFramePool::check_bitmap_index(unsigned long bitmap_index){
    //this function checks the bitmap index given to it to tell the caller if it is free, occupied, or head of sequence by using lots of binary, bitwise operations
    if(bitmap_index % 4 == 0){
        if((bitmap[bitmap_index - (bitmap_index % 4)] & 0xC0) == 0xC0)
            return free;
        else if((bitmap[bitmap_index - (bitmap_index % 4)] & 0xC0) == 0x40)
            return hos;
        else if((bitmap[bitmap_index - (bitmap_index % 4)] & 0xC0) == 0x0)
            return occ;
    }else if(bitmap_index % 4 == 1){
        if((bitmap[bitmap_index - (bitmap_index % 4)] & 0x30) == 0x30)
            return free;
        else if((bitmap[bitmap_index - (bitmap_index % 4)] & 0x30) == 0x10)
            return hos;
        else if((bitmap[bitmap_index - (bitmap_index % 4)] & 0x30) == 0x0)
            return occ;
    }else if(bitmap_index % 4 == 2){
        if((bitmap[bitmap_index - (bitmap_index % 4)] & 0xC) == 0xC)
            return free;
        else if((bitmap[bitmap_index - (bitmap_index % 4)] & 0xC) == 0x4)
            return hos;
        else if((bitmap[bitmap_index - (bitmap_index % 4)] & 0xC) == 0x0)
            return occ;
    }else if(bitmap_index % 4 == 3){
        if((bitmap[bitmap_index - (bitmap_index % 4)] & 0x3) == 0x3)
            return free;
        else if((bitmap[bitmap_index - (bitmap_index % 4)] & 0x3) == 0x1)
            return hos;
        else if((bitmap[bitmap_index - (bitmap_index % 4)] & 0x3) == 0x0)
            return occ;
    }else{
        assert(false); //something went wrong
    }
    return free; //keep compiler happy
}

//------------------------------------------------------------------------------------------------------

void ContFramePool::mark_bitmap_index(unsigned long bitmap_index, frame_status status){

    //this function actuall sets individual bitmap entries to whatever we need
    unsigned char mask;

    //set the mask to the appropriate mask for what we're setting it to
    if(status == free)
        mask = 0xC0;
    else if (status == occ)
        mask = 0x0;
    else if (status == hos)
        mask = 0x40;

    mask = mask >> ((bitmap_index % 4) * 2); //set the mask to the correct position based on where the 2 bit index is with repsect to the char

    unsigned char index_alone = bitmap[bitmap_index - (bitmap_index % 4)] & (0xC0 >> ((bitmap_index % 4) * 2)); //this isolates the index we're looking at by making the other bits of the char 0

    //if the mask value (in hex) is larger or smaller than the isolated index value (in hex)...
    if(mask > index_alone){
        bitmap[bitmap_index - (bitmap_index % 4)] = bitmap[bitmap_index - (bitmap_index % 4)] + (mask - index_alone); //...do this math operation to set the bit index to what we need without interfering with the other bits of the char
    }
    else {
        bitmap[bitmap_index - (bitmap_index % 4)] = bitmap[bitmap_index - (bitmap_index % 4)] - (index_alone - mask); //...do this math operation to set the bit index to what we need without interfering with the other bits of the char
    }
}