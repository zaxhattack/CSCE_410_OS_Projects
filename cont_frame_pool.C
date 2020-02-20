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

ContFramePool * ContFramePool::pool_list[1000];

//------------------------------------------------------------------------------------------------------

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no,
                             unsigned long _n_info_frames)
{
    base_frame_number = _base_frame_no;
    number_of_frames = _n_frames;
    info_frame_number = _info_frame_no;
    number_of_info_frames = _n_info_frames;

    for(int i = 0; i < 100000; ++i){
        if(pool_list[i] == NULL){
            pool_list[i] = this;
            break;
        }
    }

    if(info_frame_number == 0) {
        bitmap = (unsigned char *) (base_frame_number * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *) (info_frame_number * FRAME_SIZE);
    }

    for (int i = 0; i < (number_of_frames / 4); ++i){
        bitmap[i] = 0xFF;
    }
}

//------------------------------------------------------------------------------------------------------

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    unsigned long count = 0;
    unsigned long iterate = 20;
    unsigned int start = 0;

    Console::puts("-----------------------------\n");

    while(1){
        Console::puts("Checking bitmap entry for ");
        Console::puti(_n_frames);
        Console::puts(" free frames... Index = ");
        Console::puti(iterate);
        Console::puts("...\n");
        if(check_bitmap_index(iterate) == free){
            Console::puts("Found a free one!! Counting...");
            start = iterate;
            while(1){
                if(check_bitmap_index(iterate) == free){
                    count++;
                }else{
                    break;
                    count = 0;
                }
                if(count == _n_frames){
                    Console::puts("Found enough! We looked for ");
                    Console::puti(_n_frames);
                    Console::puts(" and found ");
                    Console::puti(count);
                    Console::puts("\n          at bitmap index ");
                    Console::puti(start);
                    Console::puts(". The bitmap: ");
                    Console::puti(bitmap[start - (start % 4)]);
                    Console::puts("\n");                    
                    break;
                }
                iterate++;
            }
        }

        if(count == _n_frames){
            break;
        }

    iterate++;

    }

    Console::puts("Reserving the header....");
    mark_bitmap_index(start, hos);

    start++;

    for (int i = 0; i < _n_frames - 1; ++i){
        Console::puts("Reserving subsequent frame at index ");
        Console::puti(start);
        mark_bitmap_index(start, occ);
        Console::puts("\nThe bitmap here is now: ");                    
        Console::puti(bitmap[start - (start % 4)]);
        Console::puts("\n");
        start++;
    }

}

//------------------------------------------------------------------------------------------------------

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    
    assert(false);
}

//------------------------------------------------------------------------------------------------------

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    // TODO: IMPLEMENTATION NEEEDED!
    assert(false);
}

//------------------------------------------------------------------------------------------------------

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    // TODO: IMPLEMENTATION NEEEDED!
    assert(false);
}

//------------------------------------------------------------------------------------------------------

unsigned long ContFramePool::get_bitmap_index(unsigned long frame_number){
    return(frame_number - base_frame_number);
}

//------------------------------------------------------------------------------------------------------

ContFramePool::frame_status ContFramePool::check_bitmap_index(unsigned long bitmap_index){
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
        assert(false);
    }
    return free; //keep compiler happy
}

//------------------------------------------------------------------------------------------------------

void ContFramePool::mark_bitmap_index(unsigned long bitmap_index, frame_status status){
    unsigned char mask;

    if(status == free)
        mask = 0xC0;
    else if (status == occ)
        mask = 0x0;
    else if (status == hos)
        mask = 0x40;

    mask = mask >> ((bitmap_index % 4) * 2);

    unsigned char index_alone = bitmap[bitmap_index - (bitmap_index % 4)] & (0xC0 >> ((bitmap_index % 4) * 2));

    if(mask > index_alone){
        Console::puts("\nMarking bitmap position ");
        Console::puti(bitmap_index);
        Console::puts(" with this ");
        Console::puti(mask);
        Console::puts("\n");
        bitmap[bitmap_index - (bitmap_index % 4)] = bitmap[bitmap_index - (bitmap_index % 4)] + (mask - index_alone);
        Console::puts("It is now this ");
        Console::puti(bitmap[bitmap_index - (bitmap_index % 4)]);
        Console::puts("\n");
    }
    else {
        Console::puts("\nMarking bitmap position ");
        Console::puti(bitmap_index);
        Console::puts(" with this ");
        Console::puti(mask);
        Console::puts("\n");
        bitmap[bitmap_index - (bitmap_index % 4)] = bitmap[bitmap_index - (bitmap_index % 4)] - (index_alone - mask);
        Console::puts("It is now this ");
        Console::puti(bitmap[bitmap_index - (bitmap_index % 4)]);
        Console::puts("\n");
    }
}