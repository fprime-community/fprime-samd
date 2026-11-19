// ======================================================================
// \title fprime-samd/Os/Queue/Queue.hpp
// \brief statically allocated FIFO implementation of Os::Queue
// ======================================================================
#include "Fw/Types/MemAllocator.hpp"
#include "Os/Queue.hpp"

#ifndef Samd21Os_Os_Queue_HPP
#define Samd21Os_Os_Queue_HPP

namespace Os {
namespace Samd21 {

//! \brief ring buffer state for the queue
//!
//! Storage holds the sizes[depth] of each slot followed by the message data[depth][messageSize].
struct QueueHandle : public Os::QueueHandle {
    FwSizeType* m_sizes = nullptr;  //!< Message size of each slot
    U8* m_data = nullptr;           //!< Message data of each slot
    FwSizeType m_depth = 0;         //!< Depth of the queue, 0 when not created
    FwSizeType m_maxSize = 0;       //!< Maximum size allowed of a message
    FwSizeType m_head = 0;          //!< Slot of the oldest message
    FwSizeType m_count = 0;         //!< Number of messages queued
    FwSizeType m_highMark = 0;      //!< Message count high water mark
    FwEnumStoreType m_id = 0;       //!< Identifier for the queue, used for memory allocation
};

//! \brief FIFO queue that does not rely on mutexes, condition variables or the heap
//!
//! Messages are received in the order they were sent: priority is ignored and reported back as 0. Queue storage is
//! bump allocated at create time from a static pool of QueueConfig::ARENA_SIZE bytes shared by all queues.
//! `create` returns ALLOCATION_FAILED once the pool is exhausted. Send and receive run in a critical section so either
//! side may be called from an ISR.
//!
//! \warning BLOCKING calls that would need to wait return NOT_SUPPORTED, nothing can run to unblock them
class Queue : public Os::QueueInterface {
  public:
    //! \brief default queue interface constructor
    Queue() = default;

    //! \brief default queue destructor
    ~Queue() override = default;

    //! \brief copy constructor is forbidden
    Queue(const Os::QueueInterface& other) = delete;

    //! \brief copy constructor is forbidden
    Queue(const Os::QueueInterface* other) = delete;

    //! \brief assignment operator is forbidden
    Queue& operator=(const Os::QueueInterface& other) override = delete;

    //! \brief create queue storage
    //!
    //! Allocates storage for `depth` messages of `messageSize` size each from the queue allocator.
    //!
    //! \param id: identifier for the queue, used for memory allocation
    //! \param name: name of queue (unused)
    //! \param depth: depth of queue in number of messages
    //! \param messageSize: size of an individual message
    //! \return: OP_OK on success, ALLOCATION_FAILED when the queue allocator cannot fit the queue
    Status create(FwEnumStoreType id,
                  const Fw::ConstStringBase& name,
                  FwSizeType depth,
                  FwSizeType messageSize) override;

    //! \brief teardown the queue, returning its storage to the queue allocator
    void teardown() override;

    //! \brief send a message to the back of the queue
    //!
    //! \param buffer: message data
    //! \param size: size of message data
    //! \param priority: ignored
    //! \param blockType: BLOCKING returns NOT_SUPPORTED when full, NONBLOCKING returns FULL
    //! \return: status of the send
    Status send(const U8* buffer, FwSizeType size, FwQueuePriorityType priority, BlockingType blockType) override;

    //! \brief receive the oldest message from the queue
    //!
    //! \param destination: destination for message data
    //! \param capacity: maximum size of message data
    //! \param blockType: BLOCKING returns NOT_SUPPORTED when empty, NONBLOCKING returns EMPTY
    //! \param actualSize: (output) actual size of message read
    //! \param priority: (output) always 0
    //! \return: status of the receive
    Status receive(U8* destination,
                   FwSizeType capacity,
                   BlockingType blockType,
                   FwSizeType& actualSize,
                   FwQueuePriorityType& priority) override;

    //! \brief get number of messages available
    FwSizeType getMessagesAvailable() const override;

    //! \brief get maximum messages stored at any given time
    FwSizeType getMessageHighWaterMark() const override;

    //! \brief return the underlying queue handle
    Os::QueueHandle* getHandle() override;

    //! \brief allocator all queues draw their storage from
    static Fw::MemAllocator& getAllocator();

    //! \brief bytes of the queue allocator consumed by created queues
    static FwSizeType getAllocatorUsed();

  private:
    QueueHandle m_handle;
};

}  // namespace Samd21
}  // namespace Os

#endif  // Samd21Os_Os_Queue_HPP
