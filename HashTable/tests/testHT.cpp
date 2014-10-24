#include "CppUTest/TestHarness.h"

extern "C"
{
    #include <lru_table.h>
}


TEST_GROUP(HashTable)
{
};

TEST(HashTable, FirstTest)
{
    int size = 20;
    LruTable *t1 = NULL, *t2 = NULL;

    t1 = LruTable_Create(size);

    if ( !t1 )
	FAIL("CAN'T ALLOCATE MEMORY FOR HASH TABLE.");
    if ( t1->maxSize != (2^size) )
	FAIL("Incorrect Size Allocated!");
    if ( t1->keyBits != size )
	FAIL("Incorrect keyBits Initialized!");

    CHECK_EQUAL(0, t1->currentSize);
}

TEST(HashTable, SecondTest)
{
    LruTable *t1 = NULL;

    t1 = LruTable_Create(15);

}
