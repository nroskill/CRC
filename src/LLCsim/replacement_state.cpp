#include "replacement_state.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This file is distributed as part of the Cache Replacement Championship     //
// workshop held in conjunction with ISCA'2010.                               //
//                                                                            //
//                                                                            //
// Everyone is granted permission to copy, modify, and/or re-distribute       //
// this software.                                                             //
//                                                                            //
// Please contact Aamer Jaleel <ajaleel@gmail.com> should you have any        //
// questions                                                                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/*
** This file implements the cache replacement state. Users can enhance the code
** below to develop their cache replacement ideas.
**
*/


////////////////////////////////////////////////////////////////////////////////
// The replacement state constructor:                                         //
// Inputs: number of sets, associativity, and replacement policy to use       //
// Outputs: None                                                              //
//                                                                            //
// DO NOT CHANGE THE CONSTRUCTOR PROTOTYPE                                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
CACHE_REPLACEMENT_STATE::CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol )
{

    numsets    = _sets;
    assoc      = _assoc;
    replPolicy = _pol;

    mytimer    = 0;

    InitReplacementState();
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function initializes the replacement policy hardware by creating      //
// storage for the replacement state on a per-line/per-cache basis.           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::InitReplacementState()
{
    // Create the state for sets, then create the state for the ways
    repl  = new LINE_REPLACEMENT_STATE* [ numsets ];

    // ensure that we were able to create replacement state
    assert(repl);

    // Create the state for the sets
    for(UINT32 setIndex=0; setIndex<numsets; setIndex++) 
    {
        repl[ setIndex ]  = new LINE_REPLACEMENT_STATE[ assoc ];

        for(UINT32 way=0; way<assoc; way++) 
        {
            // initialize stack position (for true LRU)
            repl[ setIndex ][ way ].LRUstackposition = way;
            repl[ setIndex ][ way ].RRPV = 3;
            repl[ setIndex ][ way ].UsedTimes = 0;
            repl[ setIndex ][ way ].AllTimes = 1;
        }
    }

    // Contestants:  ADD INITIALIZATION FOR YOUR HARDWARE HERE

}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache on every cache miss. The input        //
// arguments are the thread id, set index, pointers to ways in current set    //
// and the associativity.  We are also providing the PC, physical address,    //
// and accesstype should you wish to use them at victim selection time.       //
// The return value is the physical way index for the line being replaced.    //
// Return -1 if you wish to bypass LLC.                                       //
//                                                                            //
// vicSet is the current set. You can access the contents of the set by       //
// indexing using the wayID which ranges from 0 to assoc-1 e.g. vicSet[0]     //
// is the first way and vicSet[4] is the 4th physical way of the cache.       //
// Elements of LINE_STATE are defined in crc_cache_defs.h                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc,
                                               Addr_t PC, Addr_t paddr, UINT32 accessType )
{
    switch(replPolicy)
    {
        case CRC_REPL_LRU:
            return Get_LRU_Victim( setIndex );
        case CRC_REPL_RANDOM:
            return Get_Random_Victim( setIndex );
        case CRC_REPL_SRRIP:
        case CRC_REPL_BRRIP:
            return Get_SRRIP_Victim( setIndex );
        case CRC_REPL_SFRRIP1:
        case CRC_REPL_BFRRIP1:
            return Get_SFRRIP1_Victim( setIndex );
        case CRC_REPL_BFRRIP2:
        case CRC_REPL_SFRRIP2:
            return Get_SFRRIP2_Victim( setIndex );
        default:
            break;
    }

    assert(0);

    return -1; // Returning -1 bypasses the LLC
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache after every cache hit/miss            //
// The arguments are: the set index, the physical way of the cache,           //
// the pointer to the physical line (should contestants need access           //
// to information of the line filled or hit upon), the thread id              //
// of the request, the PC of the request, the accesstype, and finall          //
// whether the line was a cachehit or not (cacheHit=true implies hit)         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateReplacementState( 
    UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine, 
    UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit )
{    
    switch(replPolicy)
    {
        case CRC_REPL_LRU:
            UpdateLRU( setIndex, updateWayID );
            break;
        case CRC_REPL_RANDOM:
            break;
        case CRC_REPL_BRRIP:
        case CRC_REPL_BFRRIP1:
        case CRC_REPL_BFRRIP2:
            UpdateBRRIP( setIndex, updateWayID );
            break;
        case CRC_REPL_SRRIP:
        case CRC_REPL_SFRRIP1:
        case CRC_REPL_SFRRIP2:
            UpdateSFRRIP( setIndex, updateWayID );
            break;
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//////// HELPER FUNCTIONS FOR REPLACEMENT UPDATE AND VICTIM SELECTION //////////
//                                                                            //
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds the LRU victim in the cache set by returning the       //
// cache block at the bottom of the LRU stack. Top of LRU stack is '0'        //
// while bottom of LRU stack is 'assoc-1'                                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_LRU_Victim( UINT32 setIndex )
{
    // Get pointer to replacement state of current set
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];

    INT32   lruWay   = 0;

    // Search for victim whose stack position is assoc-1
    for(UINT32 way=0; way<assoc; way++) 
    {
        if( replSet[way].LRUstackposition == (assoc-1) ) 
        {
            lruWay = way;
            break;
        }
    }

    // return lru way
    return lruWay;
}

INT32 CACHE_REPLACEMENT_STATE::Get_SRRIP_Victim( UINT32 setIndex )
{
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
    INT32 victim = -1;

    for(UINT32 way=0; way<assoc; way++)
        if( replSet[way].RRPV == 3 )
            victim = way;

    for(UINT32 way=0; victim<0 || way%assoc; way++)
        if( ++(replSet[way%assoc].RRPV) == 3 )
            victim = way%assoc;

    replSet[victim].RRPV = 4;
    return victim;
}

INT32 CACHE_REPLACEMENT_STATE::Get_SFRRIP1_Victim( UINT32 setIndex )
{
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
    INT32 victim = -1;
    UINT32 MinTimes = 0xffffffff;

    for(UINT32 way=0; way<assoc; way++)
        if( replSet[way].RRPV == 3 && replSet[way].UsedTimes <= MinTimes)
        {
            victim = way;
            MinTimes = replSet[way].UsedTimes;
        }

    for(UINT32 way=0; victim<0 || way%assoc; way++)
    {
        replSet[way%assoc].RRPV += 1;
        if( replSet[way%assoc].RRPV == 3 && replSet[way%assoc].UsedTimes <= MinTimes)
        {
            victim = way%assoc;
            MinTimes = replSet[way%assoc].UsedTimes;
        }
    }

    replSet[victim].RRPV = 4;
    return victim;
}

INT32 CACHE_REPLACEMENT_STATE::Get_SFRRIP2_Victim( UINT32 setIndex )
{
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
    INT32 victim = -1;
    UINT32 MinUsedTimes = 1;
    UINT32 MinAllTimes = 1;

    for(UINT32 way=0; way<assoc; way++)
        if( replSet[way].RRPV == 3 && 1.0*replSet[way].UsedTimes/replSet[way].AllTimes <= 1.0*MinUsedTimes/MinAllTimes)
        {
            victim = way;
            MinUsedTimes = replSet[way].UsedTimes;
            MinAllTimes = replSet[way].AllTimes;
        }
    for(UINT32 way=0; victim<0 || way%assoc; way++)
    {
        replSet[way%assoc].RRPV += 1;
        if( replSet[way%assoc].RRPV == 3 && 1.0*replSet[way%assoc].UsedTimes/replSet[way%assoc].AllTimes <= 1.0*MinUsedTimes/MinAllTimes)
        {
            victim = way%assoc;
            MinUsedTimes = replSet[way%assoc].UsedTimes;
            MinAllTimes = replSet[way%assoc].AllTimes;
        }
    }

    replSet[victim].RRPV = 4;
    return victim;
}
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds a random victim in the cache set                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_Random_Victim( UINT32 setIndex )
{
    INT32 way = (rand() % assoc);
    
    return way;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function implements the LRU update routine for the traditional        //
// LRU replacement policy. The arguments to the function are the physical     //
// way and set index.                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateSFRRIP( UINT32 setIndex, INT32 updateWayID )
{
    if(repl[ setIndex ][ updateWayID ].RRPV == 4)
    {
        repl[ setIndex ][ updateWayID ].RRPV = 2;
        repl[ setIndex ][ updateWayID ].UsedTimes = 0;
        repl[ setIndex ][ updateWayID ].AllTimes = 0;
    }
    else
        repl[ setIndex ][ updateWayID ].RRPV = 0;
    repl[ setIndex ][ updateWayID ].UsedTimes++;

    for(UINT32 way=0; way<assoc; way++)
        repl[ setIndex ][ way ].AllTimes++;
}

void CACHE_REPLACEMENT_STATE::UpdateBRRIP( UINT32 setIndex, INT32 updateWayID )
{
    if(repl[ setIndex ][ updateWayID ].RRPV == 4)
    {
        if(rand()%32 == 0)
            repl[ setIndex ][ updateWayID ].RRPV = 2;
        else
            repl[ setIndex ][ updateWayID ].RRPV = 3;
        repl[ setIndex ][ updateWayID ].UsedTimes = 0;
        repl[ setIndex ][ updateWayID ].AllTimes = 0;
    }
    else
        repl[ setIndex ][ updateWayID ].RRPV = 0;
    repl[ setIndex ][ updateWayID ].UsedTimes++;

    for(UINT32 way=0; way<assoc; way++)
        repl[ setIndex ][ way ].AllTimes++;
}

void CACHE_REPLACEMENT_STATE::UpdateLRU( UINT32 setIndex, INT32 updateWayID )
{
    UINT32 currLRUstackposition = repl[ setIndex ][ updateWayID ].LRUstackposition;

    for(UINT32 way=0; way<assoc; way++) 
        if( repl[setIndex][way].LRUstackposition < currLRUstackposition ) 
            repl[setIndex][way].LRUstackposition++;

    repl[ setIndex ][ updateWayID ].LRUstackposition = 0;
}
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// The function prints the statistics for the cache                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
ostream & CACHE_REPLACEMENT_STATE::PrintStats(ostream &out)
{

    out<<"=========================================================="<<endl;
    out<<"=========== Replacement Policy Statistics ================"<<endl;
    out<<"=========================================================="<<endl;

    // CONTESTANTS:  Insert your statistics printing here

    return out;
    
}

