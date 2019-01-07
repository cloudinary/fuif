#pragma once

#define HAS_ENCODER

#define MAX_BIT_DEPTH 15

// MAX_BIT_DEPTH is the maximum bit depth of the absolute values of the numbers that actually get encoded
// Squeeze residuals plus YCoCg can result in 17-bit absolute values on 16-bit input, so 17 is needed to encode 16-bit input with default options
// Higher bit depth is needed when DCT is used on 16-bit input.

//#define MAX_BIT_DEPTH 17

// The above compile-time constant only determines the size of the chance tables in the MANIAC trees,
// and in any case the maximum bit depth is limited by the integer type used in the channel buffers
// (currently int32_t, which means at most 30-bit unsigned input)
//#define MAX_BIT_DEPTH 30


// 2 byte improvement needed before splitting a MANIAC leaf node
#define CONTEXT_TREE_SPLIT_THRESHOLD (5461*8*2)

#define CONTEXT_TREE_COUNT_DIV 1
#define CONTEXT_TREE_COUNT_POWER 0.166666667

//#define CONTEXT_TREE_COUNT_DIV 30
//#define CONTEXT_TREE_COUNT_POWER 1
#define CONTEXT_TREE_MIN_SUBTREE_SIZE 10
#define CONTEXT_TREE_MIN_COUNT_ENCODER 1
//#define CONTEXT_TREE_MIN_COUNT_ENCODER -1




/**************************************************/
/* DANGER ZONE: OPTIONS THAT CHANGE THE BITSTREAM */
/* If you modify these, the bitstream format      */
/* changes, so it is no longer compatible!        */
/**************************************************/


// Default squeeze will ensure that the first 'scan' fits in a 8x8 rectangle
#define MAX_FIRST_PREVIEW_SIZE 8
// Round truncation offsets to a multiples of 1 byte (using less precise offsets requires a more careful implementation of partial decode)
#define TRUNCATION_OFFSET_RESOLUTION 1


#ifdef _MSC_VER
#define ATTRIBUTE_HOT
#else
#define ATTRIBUTE_HOT __attribute__ ((hot))
#endif

#include "maniac/rac.h"

#include "fileio.h"
#include "io.h"
#include "util.h"

template <typename IO> using RacIn = RacInput24<IO>;

#ifdef HAS_ENCODER
template <typename IO> using RacOut = RacOutput24<IO>;
#endif

//#define CONTEXT_TREE_MIN_COUNT -1
//#define CONTEXT_TREE_MAX_COUNT -1


// bounds for node counters
#define CONTEXT_TREE_MIN_COUNT 1
#define CONTEXT_TREE_MAX_COUNT 255
//#define CONTEXT_TREE_MAX_COUNT 512

#include "maniac/compound.h"

typedef SimpleBitChance  FUIFBitChanceMeta;
typedef SimpleBitChance  FUIFBitChancePass1;
typedef SimpleBitChance  FUIFBitChancePass2;
typedef SimpleBitChance  FUIFBitChanceTree;

