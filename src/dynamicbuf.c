#include "dynamicbuf.h"
#include <stdlib.h>
#include <string.h>

const char * dbGetErrorString(DBError s)
{
    switch (s)
    {
        case dbErrorOk:
            return "Success.";
        case dbErrorNullBufferObject:
            return "Dynamic list pointer is NULL.";
        case dbErrorNullBufferData:
            return "Dynamic list's data_buffer buffer pointer is NULL.";
        case dbErrorNullArgument:
            return "Given argument is NULL.";
        case dbErrorOutOfMemory:
            return "Could not allocate additional memory.";
        case dbErrorIndexOutOfBounds:
            return "Index exceeds list's bounds.";
        case dbErrorInvalidResizeFactor:
            return "Resize factor is invalid.";
        case dbErrorInvalidCapacity:
            return "Initial capacity is invalid.";
        default:
            return "Unknown.";
    }
}

DynamicBuf *internal_dbNew(
    const unsigned int starting_cap,
    const unsigned int stride)
{
    if (starting_cap == 0)
        return NULL;

    DynamicBuf *db = malloc(sizeof(DynamicBuf));
    if (!db) return NULL;

    db->capacity = starting_cap;
    db->stride = stride;
    db->data_buffer = malloc(starting_cap * stride);
    if (!db->data_buffer)
    {
        free(db);
        return NULL;
    }

    return db;
}

void dbFree(DynamicBuf *db)
{
    if (db == NULL) return;

    free(db->data_buffer);
    free(db);
}

const void * dbFirst(const DynamicBuf *db)
{
    return db->data_buffer;
}

const void * dbLast(const DynamicBuf *db)
{
    return db->data_buffer + (db->stride * (db->count - 1));
}

// Read
const void * internal_dbGet(
    const DynamicBuf *db,
    const unsigned int  index)
{
    if (!db) return NULL;
    if (!db->data_buffer) return NULL;
    if (index >= db->capacity) return NULL;

    return db->data_buffer + (db->stride * index);
}

// Write
//

DBError dbClear(DynamicBuf *db)
{
    if (!db) return dbErrorNullBufferObject;
    if (!db->data_buffer) return dbErrorNullBufferData;

    memset(db->data_buffer, 0, db->capacity * db->stride);
    db->count = 0;
    return dbErrorOk;
}

DBError dbResize(DynamicBuf *db, const float factor)
{
    if (!db) return dbErrorNullBufferObject;
    if (!db->data_buffer) return dbErrorNullBufferData;
    if (factor <= 0)
        return dbErrorInvalidResizeFactor;

    db->capacity *= factor;
    if (db->capacity == 1) // If lists start with a capacity of 1, it cannot be multiplied by factor
        db->capacity = 2;

    char *tmp = realloc(db->data_buffer, db->capacity * db->stride);
    if (tmp == NULL)
        return dbErrorOutOfMemory;

    db->data_buffer = tmp;
    return dbErrorOk;
}

DBError dbShrinkToFit(DynamicBuf *db)
{
    if (!db) return dbErrorNullBufferObject;
    if (!db->data_buffer) return dbErrorNullBufferData;

    // Already shrunk bail
    if (db->count == db->capacity)
        return dbErrorOk;

    int new_buf_size = db->stride * db->count;
    if (new_buf_size == 0)
        return dbErrorOk;

    void *t = realloc(db->data_buffer, new_buf_size);
    if (!t)
        return -1;

    db->data_buffer = t;
    db->capacity = new_buf_size / db->stride;
    return dbErrorOk;
}

DBError dbSet(
    DynamicBuf *db,
    const unsigned int index,
    const void *element)
{
    if (!db) return dbErrorNullBufferObject;
    if (!db->data_buffer) return dbErrorNullBufferData;
    if (!element) return dbErrorNullArgument;
    if (index >= db->capacity)
        return dbErrorIndexOutOfBounds;

    char *dest = db->data_buffer + (db->stride * index);
    memcpy(dest, element, db->stride);

    if (index == db->count)
        db->count++;
    else if (index > db->count)
        db->count = index + 1;

    return dbErrorOk;
}

DBError dbPush(
    DynamicBuf *db,
    const void      *element)
{
    if (!db) return dbErrorNullBufferObject;
    if (!db->data_buffer) return dbErrorNullBufferData;
    if (!element) return dbErrorNullArgument;

    if (db->count >= db->capacity)
        dbResize(db, 1.6);

    char *dest = db->data_buffer + (db->stride * db->count);

    memcpy(dest, element, db->stride);
    db->count++;
    return dbErrorOk;
}

void shrinkIfOK(DynamicBuf *db)
{
    if ((db->capacity - db->count) >= db-> count * 5)
        dbShrinkToFit(db);
}

DBError dbPop(DynamicBuf *db)
{
    if (!db) return dbErrorNullBufferObject;
    if (!db->data_buffer) return dbErrorNullBufferData;

    if (db->count == 0)
        return dbErrorOk;

    char *dest = db->data_buffer + (db->stride * (db->count - 1));

    memset(dest, 0, db->stride);
    db->count--;

    shrinkIfOK(db);
    return dbErrorOk;
}

DBError dbRemoveOrdered(
    DynamicBuf    *db,
    const unsigned int index)
{
    if (!db) return dbErrorNullBufferObject;
    if (!db->data_buffer) return dbErrorNullBufferData;
    if (index >= db->count)
        return dbErrorIndexOutOfBounds;

    // Pop last element
    if (index == db->count - 1)
    {
        dbPop(db);
        return dbErrorOk;
    }

    // Move all memory after index "left" one space;
    char *dest = dbGet(db, char, index);
    const unsigned int size = (db->count - index - 1) * db->stride;

    memmove(dest, dest + db->stride, size);
    memset((void *)dbLast(db), 0, db->stride);
    db->count--;

    shrinkIfOK(db);
    return dbErrorOk;
}

DBError dbRemoveUnordered(
    DynamicBuf    *db,
    const unsigned int index)
{
    if (!db) return dbErrorNullBufferObject;
    if (!db->data_buffer) return dbErrorNullBufferData;
    if (index >= db->count)
        return dbErrorIndexOutOfBounds;

    // Pop last element
    if (index == db->count - 1)
    {
        dbPop(db);
        return dbErrorOk;
    }

    // Copy last element to given index and remove duplicate
    char *dest = db->data_buffer + (index * db->stride);
    char *last = (char*)dbLast(db);

    memcpy(dest, last, db->stride);
    memset(last, 0, db->stride);
    db->count--;

    shrinkIfOK(db);
    return dbErrorOk;
}

// Traversal
//

DBError dbResetIterator(DynamicBuf *db)
{
    if (!db) return dbErrorNullBufferObject;
    db->iterator = 0;
    return dbErrorOk;
}

int dbHasNext(const DynamicBuf *db)
{
    if (!db) return 0;
    return db->iterator < db->count;
}

const void * internal_dbNext(DynamicBuf *db)
{
    if (!dbHasNext(db)) return NULL;

    const void* element = dbGet(db, void, db->iterator);
    db->iterator++;
    return element;
}
