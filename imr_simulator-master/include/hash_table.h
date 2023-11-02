#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define SIZE 20

struct DataItem
{
    int pba;
    char hash[20];
};

struct DataItem *hashArray[SIZE];
struct DataItem *dummyItem;
struct DataItem *item;

int hashCode(char *hash);
struct DataItem search(struct disk *d, char *hash);
void insert(struct disk *d, char *hash, int pba);
struct DataItem deleteItem(struct disk *d, struct DataItem *item);

#endif