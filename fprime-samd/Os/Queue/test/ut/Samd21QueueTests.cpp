// ======================================================================
// \title fprime-samd/Os/Queue/test/ut/Samd21QueueTests.cpp
// \brief tests for the statically allocated FIFO Os::Samd21::Queue
// ======================================================================
#include <gtest/gtest.h>
#include <limits>
#include "Fw/Types/String.hpp"
#include "fprime-samd/Os/Queue/Queue.hpp"
#include "samd-config/QueueConfig.hpp"

using Os::QueueInterface;

namespace {
const U8 MESSAGE[] = {1, 2, 3, 4};
//! Pool bytes a queue of the given shape consumes, before alignment padding
constexpr FwSizeType queueBytes(FwSizeType depth, FwSizeType messageSize) {
    return depth * (messageSize + sizeof(FwSizeType));
}
}  // namespace

TEST(Samd21Queue, CreateConsumesAllocator) {
    Os::Samd21::Queue queue;
    Fw::String name("consume");
    const FwSizeType before = Os::Samd21::Queue::getAllocatorUsed();
    ASSERT_EQ(queue.create(0, name, 3, sizeof(MESSAGE)), QueueInterface::Status::OP_OK);
    EXPECT_GE(Os::Samd21::Queue::getAllocatorUsed(), before + queueBytes(3, sizeof(MESSAGE)));
    queue.teardown();
    EXPECT_EQ(Os::Samd21::Queue::getAllocatorUsed(), before);
}

TEST(Samd21Queue, QueuesShareOneArena) {
    // Differently shaped queues are carved back to back from the same arena, padded only to FwSizeType alignment
    Os::Samd21::Queue small;
    Os::Samd21::Queue large;
    Fw::String name("shared");
    ASSERT_EQ(Os::Samd21::Queue::getAllocatorUsed(), 0u);
    ASSERT_EQ(small.create(0, name, 2, 4), QueueInterface::Status::OP_OK);
    EXPECT_EQ(Os::Samd21::Queue::getAllocatorUsed(), queueBytes(2, 4));
    ASSERT_EQ(large.create(0, name, 1, 2 * sizeof(FwSizeType)), QueueInterface::Status::OP_OK);
    EXPECT_EQ(Os::Samd21::Queue::getAllocatorUsed(), queueBytes(2, 4) + queueBytes(1, 2 * sizeof(FwSizeType)));
    large.teardown();
    small.teardown();
}

TEST(Samd21Queue, CreateFillsAllocator) {
    Os::Samd21::Queue queue;
    Fw::String name("fill");
    ASSERT_EQ(Os::Samd21::Queue::getAllocatorUsed(), 0u);
    // Largest single-slot queue that fits the whole pool
    const FwSizeType messageSize = Samd21::QueueConfig::ARENA_SIZE - sizeof(FwSizeType);
    ASSERT_EQ(queue.create(0, name, 1, messageSize), QueueInterface::Status::OP_OK);
    EXPECT_EQ(Os::Samd21::Queue::getAllocatorUsed(), Samd21::QueueConfig::ARENA_SIZE);

    // The pool is exhausted, a second queue does not fit
    Os::Samd21::Queue second;
    EXPECT_EQ(second.create(0, name, 1, 1), QueueInterface::Status::ALLOCATION_FAILED);
    queue.teardown();
    EXPECT_EQ(second.create(0, name, 1, 1), QueueInterface::Status::OP_OK);
    second.teardown();
}

TEST(Samd21Queue, CreateTooLargeFails) {
    Os::Samd21::Queue queue;
    Fw::String name("too-large");
    EXPECT_EQ(queue.create(0, name, 1, Samd21::QueueConfig::ARENA_SIZE - sizeof(FwSizeType) + 1),
              QueueInterface::Status::ALLOCATION_FAILED);
    EXPECT_EQ(queue.create(0, name, std::numeric_limits<FwSizeType>::max(), 1),
              QueueInterface::Status::ALLOCATION_FAILED);
    EXPECT_EQ(queue.create(0, name, 1, std::numeric_limits<FwSizeType>::max()),
              QueueInterface::Status::ALLOCATION_FAILED);
    EXPECT_EQ(queue.create(0, name, 2, std::numeric_limits<FwSizeType>::max() / 2),
              QueueInterface::Status::ALLOCATION_FAILED);
    EXPECT_EQ(Os::Samd21::Queue::getAllocatorUsed(), 0u);
    // Failed creates leave the queue uncreated
    EXPECT_EQ(queue.create(0, name, 1, sizeof(MESSAGE)), QueueInterface::Status::OP_OK);
    queue.teardown();
}

TEST(Samd21Queue, FifoIgnoresPriority) {
    Os::Samd21::Queue queue;
    Fw::String name("fifo");
    U8 destination[2] = {};
    FwSizeType actualSize = 0;
    FwQueuePriorityType priority = 0;
    const FwSizeType depth = 3;

    ASSERT_EQ(queue.create(0, name, depth, sizeof(destination)), QueueInterface::Status::OP_OK);
    // Several passes so the ring buffer wraps, with sizes varying per message
    U8 sent = 0;
    U8 received = 0;
    for (FwSizeType pass = 0; pass < 4; pass++) {
        for (FwSizeType i = 0; i < depth; i++) {
            const U8 message[2] = {sent, static_cast<U8>(sent + 1)};
            const FwSizeType size = (sent % 2) + 1;
            ASSERT_EQ(queue.send(message, size, static_cast<FwQueuePriorityType>(sent % 3),
                                 QueueInterface::BlockingType::NONBLOCKING),
                      QueueInterface::Status::OP_OK);
            sent++;
        }
        EXPECT_EQ(queue.send(MESSAGE, 1, 0, QueueInterface::BlockingType::NONBLOCKING), QueueInterface::Status::FULL);
        EXPECT_EQ(queue.getMessagesAvailable(), depth);
        for (FwSizeType i = 0; i < depth; i++) {
            priority = 1;
            ASSERT_EQ(queue.receive(destination, sizeof(destination), QueueInterface::BlockingType::NONBLOCKING,
                                    actualSize, priority),
                      QueueInterface::Status::OP_OK);
            EXPECT_EQ(destination[0], received);
            EXPECT_EQ(actualSize, static_cast<FwSizeType>((received % 2) + 1));
            EXPECT_EQ(priority, 0);
            received++;
        }
        EXPECT_EQ(queue.receive(destination, sizeof(destination), QueueInterface::BlockingType::NONBLOCKING, actualSize,
                                priority),
                  QueueInterface::Status::EMPTY);
    }
    EXPECT_EQ(queue.getMessageHighWaterMark(), depth);
    queue.teardown();
}

TEST(Samd21Queue, BlockingNotSupported) {
    Os::Samd21::Queue queue;
    Fw::String name("blocking");
    U8 destination[sizeof(MESSAGE)] = {};
    FwSizeType actualSize = 0;
    FwQueuePriorityType priority = 0;

    ASSERT_EQ(queue.create(0, name, 1, sizeof(MESSAGE)), QueueInterface::Status::OP_OK);
    EXPECT_EQ(
        queue.receive(destination, sizeof(destination), QueueInterface::BlockingType::BLOCKING, actualSize, priority),
        QueueInterface::Status::NOT_SUPPORTED);
    ASSERT_EQ(queue.send(MESSAGE, sizeof(MESSAGE), 0, QueueInterface::BlockingType::BLOCKING),
              QueueInterface::Status::OP_OK);
    EXPECT_EQ(queue.send(MESSAGE, sizeof(MESSAGE), 0, QueueInterface::BlockingType::BLOCKING),
              QueueInterface::Status::NOT_SUPPORTED);
    EXPECT_EQ(queue.getMessagesAvailable(), 1u);
    queue.teardown();
}

TEST(Samd21Queue, SizeMismatch) {
    Os::Samd21::Queue queue;
    Fw::String name("size-mismatch");
    const U8 tooLarge[sizeof(MESSAGE) + 1] = {};
    U8 destination[sizeof(MESSAGE) - 1] = {};
    FwSizeType actualSize = 0;
    FwQueuePriorityType priority = 0;

    ASSERT_EQ(queue.create(0, name, 1, sizeof(MESSAGE)), QueueInterface::Status::OP_OK);
    EXPECT_EQ(queue.send(tooLarge, sizeof(tooLarge), 0, QueueInterface::BlockingType::NONBLOCKING),
              QueueInterface::Status::SIZE_MISMATCH);
    ASSERT_EQ(queue.send(MESSAGE, sizeof(MESSAGE), 0, QueueInterface::BlockingType::NONBLOCKING),
              QueueInterface::Status::OP_OK);
    EXPECT_EQ(queue.receive(destination, sizeof(destination), QueueInterface::BlockingType::NONBLOCKING, actualSize,
                            priority),
              QueueInterface::Status::SIZE_MISMATCH);
    EXPECT_EQ(queue.getMessagesAvailable(), 1u);
    queue.teardown();
}

TEST(Samd21Queue, ThroughOsQueue) {
    // Os_Queue_Samd21 is the delegate behind Os::Queue
    Os::Queue queue;
    Fw::String name("os-queue");
    U8 destination[sizeof(MESSAGE)] = {};
    FwSizeType actualSize = 0;
    FwQueuePriorityType priority = 0;

    ASSERT_EQ(queue.create(0, name, 2, sizeof(MESSAGE)), QueueInterface::Status::OP_OK);
    ASSERT_EQ(queue.send(MESSAGE, sizeof(MESSAGE), 0, QueueInterface::BlockingType::NONBLOCKING),
              QueueInterface::Status::OP_OK);
    ASSERT_EQ(queue.receive(destination, sizeof(destination), QueueInterface::BlockingType::NONBLOCKING, actualSize,
                            priority),
              QueueInterface::Status::OP_OK);
    EXPECT_EQ(actualSize, sizeof(MESSAGE));
    EXPECT_EQ(destination[3], MESSAGE[3]);
    queue.teardown();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
