#ifndef OBTS_TRANSCEIVER52M_LINKEDLISTS_H
#define OBTS_TRANSCEIVER52M_LINKEDLISTS_H

#include <stdlib.h>
#include <assert.h>


/** This node class is used to build singly-linked lists. */
class ListNode {
public:
    ListNode * next() { return mNext; }

    void next(ListNode * wNext) { mNext = wNext; }

    void * data() { return mData; }

    void data(void * wData) { mData = wData; }

private:
    ListNode * mNext;
    void * mData;
};


/** A fast FIFO for pointer-based storage. */
class PointerFIFO {
public:
    PointerFIFO() : mHead(NULL), mTail(NULL), mFreeList(NULL), mSize(0) {}

    unsigned size() const { return mSize; }

    unsigned totalSize() const { return 0; }    // Not used in this version.

    /** Put an item into the FIFO at the back of the queue. aka push_back */
    void put(void * val);

    /** Push an item on the front of the FIFO. */
    void push_front(void * val);            // pat added.

    /**
        Take an item from the FIFO. aka pop_front, but returns NULL
        Returns NULL for empty list.
    */
    void * get();

    /** Peek at front item without removal. */
    void * front() { return mHead ? mHead->data() : 0; }    // pat added

protected:
    ListNode * mHead;        ///< points to next item out
    ListNode * mTail;        ///< points to last item in
    ListNode * mFreeList;    ///< pool of previously-allocated nodes
    unsigned mSize;            ///< number of items in the FIFO


private:
    /** Allocate a new node to extend the FIFO. */
    ListNode * allocate();

    /** Release a node to the free pool after removal from the FIFO. */
    void release(ListNode * wNode);

};

// This is the default type for SingleLinkList Node element;
// You can derive your class directly from this, but then you must add type casts
// all over the place.
class SingleLinkListNode {
public:
    SingleLinkListNode * mNext;

    SingleLinkListNode * next() { return mNext; }

    void setNext(SingleLinkListNode * item) { mNext = item; }

    SingleLinkListNode() : mNext(0) {}

    virtual unsigned size() { return 0; }
};

// A single-linked lists of elements with internal pointers.
// The methods must match those from SingleLinkListNode.
// This class also assumes the Node has a size() method, and accumulates
// the total size of elements in the list in totalSize().
template<class Node=SingleLinkListNode>
class SingleLinkList {
public:
    typedef void iterator;    // Does not exist for this class, but needs to be defined.
    typedef void const_iterator;    // Does not exist for this class, but needs to be defined.
    SingleLinkList() : mHead(0), mTail(0), mSize(0), mTotalSize(0) {}

    unsigned size() const { return mSize; }

    unsigned totalSize() const { return mTotalSize; }

    Node * pop_back() { assert(0); } // Not efficient with this type of list.

    Node * pop_front() {
        if (!mHead) return NULL;
        Node * result = mHead;
        mHead = mHead->next();
        if (mTail == result) {
            mTail = NULL;
            assert(mHead == NULL);
        }
        result->setNext(NULL);    // be neat
        mSize--;
        mTotalSize -= result->size();
        return result;
    }

    void push_front(Node * item) {
        item->setNext(mHead);
        mHead = item;
        if (!mTail) { mTail = item; }
        mSize++;
        mTotalSize += item->size();
    }

    void push_back(Node * item) {
        item->setNext(NULL);
        if (mTail) { mTail->setNext(item); }
        mTail = item;
        if (!mHead) mHead = item;
        mSize++;
        mTotalSize += item->size();
    }

    Node * front() const { return mHead; }

    Node * back() const { return mTail; }

    // Interface to InterthreadQueue so it can used SingleLinkList as the Fifo.
    void put(void * val) { push_back((Node *) val); }

    void * get() { return pop_front(); }

private:
    Node * mHead, * mTail;
    unsigned mSize;            // Number of elements in list.
    unsigned mTotalSize;    // Total of size() method of elements in list.
};


#endif //OBTS_TRANSCEIVER52M_LINKEDLISTS_H
