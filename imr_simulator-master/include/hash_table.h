#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// #define SIZE 20

struct DataItem
{
    char hash[20];
    long long pba;
};

// struct DataItem *hashArray[SIZE];
// struct DataItem dummyItem = {.pba = -1, .hash = ""};
// struct DataItem *item;

int hashCode(char *hash, unsigned long size);
struct DataItem *searchItem(struct disk *d, char *hash);
void insert(struct disk *d, char *hash, int pba);
struct DataItem *deleteItem(struct disk *d, char *hash);

#endif