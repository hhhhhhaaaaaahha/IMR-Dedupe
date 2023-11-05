#include "lba.h"
#include "hash_table.h"

int hashCode(char *hash, unsigned long size)
{
    int sum = 0;
    while (*hash != '\0')
    {
        sum = sum + *hash;
        hash++;
    }

    return sum % size;
}

struct DataItem *searchItem(struct disk *d, char *hash)
{
    // get the hash
    uint64_t size = d->report.max_track_num;
    int hashIndex = hashCode(hash, size);

    // move in array until an empty
    while (d->hash_table[hashIndex] != NULL)
    {
        if (strcmp(d->hash_table[hashIndex]->hash, hash) == 0)
        {
            return d->hash_table[hashIndex];
        }

        // go to next cell
        ++hashIndex;

        // wrap around the table
        hashIndex %= size;
    }

    return NULL;
}

void insert(struct disk *d, char *hash, int pba)
{
    struct DataItem *item = (struct DataItem *)malloc(sizeof(struct DataItem));
    uint64_t size = d->report.max_track_num;
    strcpy(item->hash, hash);
    item->pba = pba;

    // get the hash
    int hashIndex = hashCode(hash, size);

    // move in array until an empty or deleted cell
    while (d->hash_table[hashIndex] != NULL && strcmp(d->hash_table[hashIndex]->hash, "") != 0) // && d->hash_table[hashIndex]->hash != -1)
    {
        // go to next cell
        ++hashIndex;

        // wrap around the table
        hashIndex %= size;
    }

    d->hash_table[hashIndex] = item;
}

struct DataItem *deleteItem(struct disk *d, char *hash)
{
    // char *hash = item->hash;
    uint64_t size = d->report.max_track_num;

    // get the hash
    int hashIndex = hashCode(hash, size);

    // move in array until an empty
    while (d->hash_table[hashIndex] != NULL)
    {

        if (strcmp(d->hash_table[hashIndex]->hash, hash) == 0)
        {
            struct DataItem *temp = malloc(sizeof(struct DataItem));
            strcpy(temp->hash, d->hash_table[hashIndex]->hash);
            temp->pba = d->hash_table[hashIndex]->pba;

            // assign a dummy item at deleted position
            strcpy(d->hash_table[hashIndex]->hash, "");
            d->hash_table[hashIndex]->pba = -1;
            return temp;
        }

        // go to next cell
        ++hashIndex;

        // wrap around the table
        hashIndex %= size;
    }

    return NULL;
}