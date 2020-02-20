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

/* -- (none) -- */

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

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no,
                             unsigned long _n_info_frames)
{
    assert(_n_frames <= FRAME_SIZE * 8); //ensure the number of frames doesn't exceed what's available

    for(int i = 0; i < 10000; ++i){
        if(pool_list[i] == NULL){
            pool_list[i] = this;
            break;
        }
    }

    //set variables
    base_frame_no = _base_frame_no;
    n_frames = _n_frames;
    nFreeFrames = _n_frames;
    info_frame_no = _info_frame_no;
    n_info_frames = _n_info_frames;

    if(info_frame_no == 0)
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
    else
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);

    set_bitmap(info_frame_no, n_info_frames); //set the info frames

    Console::puts("Frame Pool initialized\n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames) //for this I'm using FIRST FIT implementation
{
    Console::puts("Allocating...\n");
    assert(nFreeFrames >= _n_frames);

    unsigned long position = 0;
    unsigned long frame_start = 0;
    unsigned long i = 0;
    unsigned long length = 0;

    unsigned char mask = 0xC0;
    while(1){
        //The first while loop looks for a bitmap entry of 11, indicating a free frame
        while ((mask & bitmap[i]) != 0xC0 && (mask & bitmap[i]) != 0x30 && (mask & bitmap[i]) != 0xC && (mask & bitmap[i]) != 0x03) {
            mask = mask >> 2;
            position++;
            if(position % 4 == 0)
                i++;
        }
        //once a free frame is found, record this as the start position of the sequence and count how many frames are free afterwards
        frame_start = position;
        while ((mask & bitmap[i]) == 0xC0 || (mask & bitmap[i]) == 0x30 || (mask & bitmap[i]) == 0xC || (mask & bitmap[i]) == 0x03) {
            mask = mask >> 2;
            position++;
            length++;
            if(position % 4 == 0)
                i++;

            //if we run out reach the end of our frames, we have no more free frames to allocate for this request, and we crash
            /*if(position = n_frames){
                Console::puts("Error: No more free holes for allocation\n");
                assert(false);
            }*/

            //if we have enough frames in the sequence, break out of this loop
            if(length == _n_frames)
                break;
        }

        ///if we have enough frames in the sequence, break out of this loop as well and record how many free frames we have left total
        if(length = _n_frames){
            nFreeFrames = nFreeFrames - _n_frames;
            break;
        }
    }
    
    //if we got out of the loop above successfully, we have a frame number recorded for us to use as the start of the sequence. 
    //Send it and the length of the sequence to the set_bitmap function, and return the position of the frame sequence
    set_bitmap(frame_start, _n_frames); //Reserve the frames on the bitmap

    return(frame_start + base_frame_no);
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    Console::puts("Mark inaccessible...\n");
    assert(false);
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    Console::puts("release frames...\n");

    ContFramePool * ptr;

    for(int i = 0; i < 10000; ++i){
        if(pool_list[i] == NULL){
            Console::puts("Error: ran to end of pool_list\n");
            assert(false);
        }

        if(_first_frame_no >= pool_list[i]->base_frame_no && _first_frame_no <= (pool_list[i]->base_frame_no + pool_list[i]->n_frames)){
            ptr = pool_list[i];
            break;
        }
    }

    ptr->release_frames_pvt(_first_frame_no);
}

void ContFramePool::release_frames_pvt(unsigned long _first_frame_no){
    
    unsigned int bitmap_index = _first_frame_no - base_frame_no;

    unsigned long count = 1;

    if((bitmap[bitmap_index] & mask) != 0) {
        Console::puts("Error, Frame being released is not being used\n");
        assert(false);
    }
    //set the first frame to 11
    unsigned char mask = 0xC0 >> ((bitmap_index % 4) * 2);
    
    //keep going, setting each 01 to 11, until we got to either a 11 or a 00, indication we have traversed the entire sequence
    unsigned long position = bitmap_index % 4;
    unsigned long i = bitmap_index - position;
    while ((mask & bitmap[i]) == 0x40 || (mask & bitmap[i]) == 0x10 || (mask & bitmap[i]) == 0x04 || (mask & bitmap[i]) == 0x01) {

        bitmap[position - (position % 4)] = bitmap[position - (position % 4)] ^ mask;

        mask = mask >> 2;

        count++;

        position++;
        if(position % 4 == 0)
            i++;
    }

    nFreeFrames = nFreeFrames + count;
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    Console::puts("Needed info...\n");
    return(_n_frames / 16384 + (_n_frames % 16384 > 0 ? 1 : 0));
}

void ContFramePool::set_bitmap(unsigned long start, unsigned long length)
{
    Console::puts("Set Bitmap...\n");

    //set first position of the frame to be occupied and head of sequence (00)
    if(start % 4 == 0)
        bitmap[start - (start % 4)] = bitmap[start - (start % 4)] & 0x7F;
    else if(start % 4 == 1)
        bitmap[start - (start % 4)] = bitmap[start - (start % 4)] & 0xFF;
    else if(start % 4 == 2)
        bitmap[start - (start % 4)] = bitmap[start - (start % 4)] & 0xF7;
    else if(start % 4 == 3)
        bitmap[start - (start % 4)] = bitmap[start - (start % 4)] & 0xFD;

    bitmap[start - (start % 4)] = 0x3F; 

    unsigned long index = start + 1;

    for(unsigned long i = 0; i < length; i++){
        //set the following frames in the sequence to be 01, or occurpied but not head of squence. I'm using bitwise AND for this
        if(index % 4 == 0)
            bitmap[index - (index % 4)] = bitmap[index - (index % 4)] & 0x7F;
        else if(index % 4 == 1)
            bitmap[index - (index % 4)] = bitmap[index - (index % 4)] & 0xFF;
        else if(index % 4 == 2)
            bitmap[index - (index % 4)] = bitmap[index - (index % 4)] & 0xF7;
        else if(index % 4 == 3)
            bitmap[index - (index % 4)] = bitmap[index - (index % 4)] & 0xFD;

        index++;
    }

    nFreeFrames = nFreeFrames - length;
}