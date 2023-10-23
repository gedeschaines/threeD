/**********************************************************************/
/* FILE:  pquelib,c
 * DATE:  24 AUG 2000
 * AUTH:  G. E. Deschaines
 * DESC:  Data structures and methods for a priority queue implemented
 *        with an ordered binary heap maintained in an array.  A decent
 *        explanation of priority queues can be found at this link:
 *
 *          http://algs4.cs.princeton.edu/24pq/
 *
 *        Methods presented herein were derived from those presented in
 *        a textbook on programming and data structures using Pascal.
*/
/**********************************************************************/

#define MaxElements  1024 
#define FALSE           0
#define TRUE            1
 
typedef struct
{
  Longint  Key;
  Integer  Info;
} HeapElement;

typedef HeapElement  HeapArray[MaxElements];

typedef struct PQ_type  *PQtypePtr;
typedef struct PQ_type
{
  HeapElement  Elements[MaxElements];
  Integer      Bottom;
} PQtype;

void ClearPQ( PQtypePtr pPQueue )
{
   pPQueue->Bottom = 0;
}

Boolean EmptyPQ( PQtype aPQueue )
{
   return (Boolean)( aPQueue.Bottom == 0 );
}

Boolean FullPQ( PQtype aPQueue )
{
   return (Boolean)( aPQueue.Bottom == MaxElements );
}

void ReHeapUp( HeapElement *HeapElements, Integer Bottom )
{
   Integer      CurrentIndex;
   Integer      ParentIndex;
   Boolean      HeapOk;
   HeapElement  TempElement;

   HeapOk       = FALSE;
   CurrentIndex = Bottom;
   ParentIndex  = CurrentIndex / 2;
   while ( ( CurrentIndex > 1 ) && ( ! HeapOk ) )
   {
      if ( HeapElements[ParentIndex].Key >= 
           HeapElements[CurrentIndex].Key    )
      {
         HeapOk = TRUE;
      }
      else
      {
         TempElement                = HeapElements[ParentIndex];
         HeapElements[ParentIndex]  = HeapElements[CurrentIndex];
         HeapElements[CurrentIndex] = TempElement;
         CurrentIndex               = ParentIndex;
         ParentIndex                = ParentIndex / 2;
      }
   }
}

void ReHeapDown( HeapElement *HeapElements, Integer Root, Integer Bottom )
{
   Boolean      HeapOk;
   Integer      MaxChild;
   Integer      Root2;
   Integer      Root2p1;
   HeapElement  TempElement;

   Root2   = Root*2;
   Root2p1 = Root2 + 1;
   HeapOk  = FALSE;
   while ( ( Root2 <= Bottom ) && ( ! HeapOk ) )
   {
      if ( Root2 == Bottom )
      {
         MaxChild = Root2;
      }
      else
      {
         if ( HeapElements[Root2].Key > HeapElements[Root2p1].Key )
         {
            MaxChild = Root2;
         }
         else
         {
            MaxChild = Root2p1;
         }
      }
      if ( HeapElements[Root].Key < HeapElements[MaxChild].Key )
      {
         TempElement            = HeapElements[Root];
         HeapElements[Root]     = HeapElements[MaxChild];
         HeapElements[MaxChild] = TempElement;
         Root                   = MaxChild;
         Root2                  = Root*2;
         Root2p1                = Root2 + 1;
      }
      else
      {
         HeapOk = TRUE;
      }
   }   
}

void PriorityEnq( PQtypePtr pPQueue, HeapElement NewElement )
{
   pPQueue->Bottom                    = pPQueue->Bottom + 1;
   pPQueue->Elements[pPQueue->Bottom] = NewElement;
   ReHeapUp(pPQueue->Elements, pPQueue->Bottom);
}

void PriorityDeq( PQtypePtr pPQueue, HeapElement *FirstElement )
{
   *FirstElement        = pPQueue->Elements[1];
   pPQueue->Elements[1] = pPQueue->Elements[pPQueue->Bottom];
   pPQueue->Bottom      = pPQueue->Bottom - 1;
   ReHeapDown(pPQueue->Elements, 1, pPQueue->Bottom);
}

/**********************************************************************/
/**********************************************************************/

