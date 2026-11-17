// ======================================================================
// \title fprime-samd/Os/Queue/Queue.cpp
// \brief statically allocated FIFO implementation of Os::Queue
// ======================================================================
#include "fprime-samd/Os/Queue/Queue.hpp"
#include <cstring>
#include <limits>
#include "Fw/Types/Assert.hpp"
#include "fprime-samd/Drv/Types/CriticalSection.hpp"
#include "fprime-samd/Svc/StaticMallocator/StaticMallocator.hpp"
#include "samd-config/QueueConfig.hpp"

namespace Os {
namespace Samd21 {

namespace {

using QueueAllocator = ::Samd21::StaticMallocator<::Samd21::QueueConfig::ARENA_SIZE>;

QueueAllocator& queueAllocator() {
    static QueueAllocator s_allocator;
    return s_allocator;
}

}  // namespace

Fw::MemAllocator& Queue::getAllocator() {
    return queueAllocator();
}

FwSizeType Queue::getAllocatorUsed() {
    return queueAllocator().getUsed();
}

Os::QueueInterface::Status Queue::create(FwEnumStoreType id,
                                         const Fw::ConstStringBase& name,
                                         FwSizeType depth,
                                         FwSizeType messageSize) {
    (void)name;
    // Ensure we are created exactly once
    FW_ASSERT(this->m_handle.m_sizes == nullptr);

    // Total size is depth * (sizeof(FwSizeType) + messageSize), reject anything that would overflow it
    constexpr FwSizeType MAX = std::numeric_limits<FwSizeType>::max();
    if ((depth == 0) || (messageSize > (MAX - sizeof(FwSizeType))) ||
        (depth > (MAX / (sizeof(FwSizeType) + messageSize)))) {
        return QueueInterface::Status::ALLOCATION_FAILED;
    }
    const FwSizeType expected = depth * (sizeof(FwSizeType) + messageSize);
    FwSizeType size = expected;
    U8* const storage = static_cast<U8*>(getAllocator().allocate(id, size, alignof(FwSizeType)));
    if (storage == nullptr) {
        return QueueInterface::Status::ALLOCATION_FAILED;
    }
    if (size < expected) {
        getAllocator().deallocate(id, storage);
        return QueueInterface::Status::ALLOCATION_FAILED;
    }

    this->m_handle.m_id = id;
    this->m_handle.m_sizes = reinterpret_cast<FwSizeType*>(storage);
    this->m_handle.m_data = &storage[depth * sizeof(FwSizeType)];
    this->m_handle.m_depth = depth;
    this->m_handle.m_maxSize = messageSize;
    return QueueInterface::Status::OP_OK;
}

void Queue::teardown() {
    if (this->m_handle.m_sizes != nullptr) {
        getAllocator().deallocate(this->m_handle.m_id, this->m_handle.m_sizes);
        this->m_handle = QueueHandle();
    }
}

Os::QueueInterface::Status Queue::send(const U8* buffer,
                                       FwSizeType size,
                                       FwQueuePriorityType priority,
                                       QueueInterface::BlockingType blockType) {
    (void)priority;
    QueueHandle& handle = this->m_handle;
    // Fixed after create, safe to check outside the critical section
    if (size > handle.m_maxSize) {
        return QueueInterface::Status::SIZE_MISMATCH;
    }
    ::Samd21::CriticalSection cs;
    if (handle.m_count == handle.m_depth) {
        // Nothing can run to drain the queue, so blocking here would never return
        return (blockType == BlockingType::BLOCKING) ? QueueInterface::Status::NOT_SUPPORTED
                                                     : QueueInterface::Status::FULL;
    }
    const FwSizeType slot = (handle.m_head + handle.m_count) % handle.m_depth;
    (void)::memcpy(&handle.m_data[slot * handle.m_maxSize], buffer, static_cast<size_t>(size));
    handle.m_sizes[slot] = size;
    handle.m_count++;
    handle.m_highMark = (handle.m_count > handle.m_highMark) ? handle.m_count : handle.m_highMark;
    return QueueInterface::Status::OP_OK;
}

Os::QueueInterface::Status Queue::receive(U8* destination,
                                          FwSizeType capacity,
                                          QueueInterface::BlockingType blockType,
                                          FwSizeType& actualSize,
                                          FwQueuePriorityType& priority) {
    QueueHandle& handle = this->m_handle;
    // Reject undersized buffers before removing anything so the message stays queued
    if (capacity < handle.m_maxSize) {
        return QueueInterface::Status::SIZE_MISMATCH;
    }
    ::Samd21::CriticalSection cs;
    if (handle.m_count == 0) {
        // Nothing can run to fill the queue, so blocking here would never return
        return (blockType == BlockingType::BLOCKING) ? QueueInterface::Status::NOT_SUPPORTED
                                                     : QueueInterface::Status::EMPTY;
    }
    const FwSizeType slot = handle.m_head;
    actualSize = handle.m_sizes[slot];
    priority = 0;
    (void)::memcpy(destination, &handle.m_data[slot * handle.m_maxSize], static_cast<size_t>(actualSize));
    handle.m_head = (slot + 1) % handle.m_depth;
    handle.m_count--;
    return QueueInterface::Status::OP_OK;
}

FwSizeType Queue::getMessagesAvailable() const {
    return this->m_handle.m_count;
}

FwSizeType Queue::getMessageHighWaterMark() const {
    return this->m_handle.m_highMark;
}

Os::QueueHandle* Queue::getHandle() {
    return &this->m_handle;
}

}  // namespace Samd21
}  // namespace Os
