#include "lva.hpp"

#include<cstring>
#include<ctime>
#include<iostream>
#include<cstdlib>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

const uint64_t LVA::max_rand = -1;
LVA::lva_entry LVA::approximator[512];
LVA::bitmess   LVA::GHB[4];
int      LVA::GHB_head = 0;
bool     LVA::init_done = false;
uint64_t LVA::stats_accesses = 0;
uint64_t LVA::stats_cache_misses = 0;
uint64_t LVA::stats_predictions = 0;

float  LVA::threshold = 0.10; // error threshold - determines if prediction was accurate
float  LVA::pHitRate = 0.9;   // cache hit rate
int    LVA::degree = 1;       // how often is prediction value used

#define fuzzy_mantissa_sft 10

namespace {

  inline uint64_t getRandom() {
    static uint64_t x = 12345;
    x ^= (x >> 21);
    x ^= (x << 35);
    x ^= (x >> 4);
    return x;
  }
}

bool LVA::isCacheHit(uint64_t addr) {
    const double rand_number = static_cast<double>(getRandom()) /
        static_cast<double>(max_rand);

    return (rand_number <= pHitRate);
}

uint64_t LVA::getHash(uint64_t pc) {
    return pc;// ^
    //       (GHB[0].b >> fuzzy_mantissa_sft) ^
    //       (GHB[1].b >> fuzzy_mantissa_sft) ^
    //       (GHB[2].b >> fuzzy_mantissa_sft) ^
    //       (GHB[3].b >> fuzzy_mantissa_sft);
}


uint64_t LVA::lvaLoad(uint64_t ld_address, uint64_t ret, const char* type, uint64_t pc) {
    if(init_done == false)
	init();
    stats_accesses++;

    if (isCacheHit(ld_address)) {
	//printf("Cache hit\n");
	return ret;
    }
    stats_cache_misses++;

    bitmess precise;
    precise.b = ret;
    //printf("pc: %ld, ret: %lx, type: %s\n", pc, ret, type);
    //printf("float: %f, double: %f, int8: %d, int16: %d, int32: %d\n",
    //        precise.f, precise.d, precise.i8, precise.i16, precise.i32);

    bitmess retval;
    retval.b = ret;

    // miss in the cache

    uint64_t tag = getHash(pc);
    uint64_t idx = tag & 0x1FF;
    //printf("tag: %llx, idx: %lld\n", tag, idx);
    if(approximator[idx].tag != tag) {
	//printf("Tag miss\n");
	approximator[idx].confidence = -8;
	approximator[idx].degree = 0;
	approximator[idx].tag = tag;
    }

    if(approximator[idx].confidence >= 0) {
	//enough confidence to use predictor
	//printf("Using LVA\n");
	stats_predictions++;
	bitmess r;
	double p = (approximator[idx].LHB[0] +
	            approximator[idx].LHB[1] +
	            approximator[idx].LHB[2] +
	            approximator[idx].LHB[3]) / 4.0;

	if(strcmp(type, "Float") == 0)
	    retval.f = p;
	else if(strcmp(type, "Double") == 0)
	    retval.d = p;
	else if(strcmp(type, "Int8") == 0)
	    retval.i8 = p;
	else if(strcmp(type, "Int16") == 0)
	    retval.i16 = p;
	else if(strcmp(type, "Int32") == 0)
	    retval.i32 = p;
	else {
	    printf("Unsupported prediction type! [%s]\n", type);
	    abort();
	}
    }

    // update degree
    approximator[idx].degree--;
    if(approximator[idx].degree > 0)
	return retval.b;

    // train predictor
    approximator[idx].LHB_head = (approximator[idx].LHB_head + 1) & 0x3;
    if(strcmp(type, "Float") == 0)
        approximator[idx].LHB[approximator[idx].LHB_head] = precise.f;
    else if(strcmp(type, "Double") == 0)
        approximator[idx].LHB[approximator[idx].LHB_head] = precise.d;
    else if(strcmp(type, "Int32") == 0)
        approximator[idx].LHB[approximator[idx].LHB_head] = precise.i32;
    else if(strcmp(type, "Int16") == 0)
        approximator[idx].LHB[approximator[idx].LHB_head] = precise.i16;
    else if(strcmp(type, "Int8") == 0)
        approximator[idx].LHB[approximator[idx].LHB_head] = precise.i8;
    else {
	    printf("Unsupported prediction type! [%s]\n", type);
	    abort();
    }

    // update GHB
    GHB_head = (GHB_head + 1) & 0x3;
    if(strcmp(type, "Float") == 0)
	GHB[GHB_head].f = precise.f;
    else
	GHB[GHB_head].f = precise.d;

    approximator[idx].degree = degree;

    // compute confidence
    double error;
    if(strcmp(type, "Float") == 0)
	error = fabsf(fabsf(precise.f - retval.f)/precise.f);
    else if(strcmp(type, "Double") == 0)
	error = fabs(fabs(precise.d - retval.d)/precise.d);
    else if(strcmp(type, "Int32") == 0)
	error = fabs(fabs(precise.i32 - retval.i32)/precise.i32);
    else if(strcmp(type, "Int16") == 0)
	error = fabs(fabs(precise.i16 - retval.i16)/precise.i16);
    else if(strcmp(type, "Int8") == 0)
	error = fabs(fabs(precise.i8 - retval.i8)/precise.i8);
    else
	abort(); // should never get here

    if(error < threshold) {
	//printf("[%lld]Increasing confidence! Error: %f\n", idx, error);
	approximator[idx].confidence++;
	if(approximator[idx].confidence == 8)
	    approximator[idx].confidence = 7;
    } else {
	//printf("[%lld]Decreasing confidence! Error: %f\n", idx, error);
	approximator[idx].confidence--;
	if(approximator[idx].confidence == -9)
	    approximator[idx].confidence = -8;
    }

    return retval.b;
}

void LVA::init() {
    for(int i = 0; i < 512; i++) {
	approximator[i].degree = 0;
	approximator[i].confidence = -8; // this ensures that the predictor won't be used for the first 8 times
	approximator[i].LHB_head = 0;
	approximator[i].tag = 0;
    }

    GHB[0].b = 0;
    GHB[1].b = 0;
    GHB[2].b = 0;
    GHB[3].b = 0;

    atexit(print_summary);

    init_done = true;
}

void LVA::print_summary() {
    printf("LVA accesses:\t\t%ld\n", stats_accesses);
    printf("LVA cache misses:\t%ld\n", stats_cache_misses);
    printf("LVA predictions:\t%ld\n", stats_predictions);
}