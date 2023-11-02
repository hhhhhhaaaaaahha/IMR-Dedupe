#include "lba.h"
#include "hash_table.h"

int hashCode(char *hash)
{
    int sum = 0;
    while (*hash != '\0')
    {
        sum = sum + *hash;
        hash++;
    }

    return sum % SIZE;
}

struct DataItem *search(struct disk *d, char *hash)
{
    // get the hash
    int hashIndex = hashCode(hash);

    // move in array until an empty
    while (d->hash_table[hashIndex] != NULL)
    {

        if (d->hash_table[hashIndex]->hash == hash)
        {
            return d->hash_table[hashIndex];
        }

        // go to next cell
        ++hashIndex;

        // wrap around the table
        hashIndex %= SIZE;
    }

    return NULL;
}

void insert(struct disk *d, char *hash, int pba)
{

    struct DataItem *item = (struct DataItem *)malloc(sizeof(struct DataItem));
    item->pba = pba;
    strcpy(item->hash, hash);
    //    item->hash = hash;

    // get the hash
    int hashIndex = hashCode(hash);

    // move in array until an empty or deleted cell
    while (d->hash_table[hashIndex] != NULL && d->hash_table[hashIndex]->hash != -1)
    {
        // go to next cell
        ++hashIndex;

        // wrap around the table
        hashIndex %= SIZE;
    }

    d->hash_table[hashIndex] = item;
}

struct DataItem *deleteItem(struct disk *d, struct DataItem *item)
{
    char *hash = item->hash;

    // get the hash
    int hashIndex = hashCode(hash);

    // move in array until an empty
    while (d->hash_table[hashIndex] != NULL)
    {

        if (d->hash_table[hashIndex]->hash == hash)
        {
            struct DataItem *temp = d->hash_table[hashIndex];

            // assign a dummy item at deleted position
            d->hash_table[hashIndex] = dummyItem;
            return temp;
        }

        // go to next cell
        ++hashIndex;

        // wrap around the table
        hashIndex %= SIZE;
    }

    return NULL;
}