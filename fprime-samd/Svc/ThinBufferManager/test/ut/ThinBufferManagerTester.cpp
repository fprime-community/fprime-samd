// ======================================================================
// \title  ThinBufferManagerTester.cpp
// \brief  cpp file for ThinBufferManager test harness implementation class
//
// Ported from lib/fprime/Svc/BufferManager/test/ut/BufferManagerTester.cpp,
// adapted for Samd21::ThinBufferManagerComponentImpl. Two behavioral
// differences from the original:
//
// 1. ThinBuffer has no getContext() accessor of its own (only Fw::Buffer
//    does), so context checks go through .buff.getBuffer().getContext()
//    instead of .buff.getContext() directly.
//
// 2. TestAllocator backs onto Samd21::StaticMallocator instead of
//    Fw::MallocAllocator, matching the allocator this component is actually
//    paired with in the real deployment (CastleTester's topology wires
//    ThinBufferManager to a StaticMallocator, never a heap allocator). The
//    bucket (TEST_ALLOCATOR_BUCKET_SIZE) is sized to comfortably cover every
//    test method's total request on this native build's ABI; StaticMallocator
//    only asserts size <= BUCKET_SIZE, not equality, so a single shared,
//    generously-sized bucket is safe to reuse across all three tests.
//
// This UT relies on config-castle/CMakeLists.txt skipping
// ThinBufferManagerConfig.hpp's override when BUILD_TESTING is set (same
// pattern already used there for PlatformCfg.fpp), so this UT always sees
// fprime-samd's own default (THINBUFFERMGR_MAX_NUM_BINS == 3) regardless of
// which deployment's config-castle happens to be in the enclosing CMake
// configure.
//
// ======================================================================

#include "ThinBufferManagerTester.hpp"
#include <Fw/Test/UnitTest.hpp>
#include <fprime-samd/Svc/StaticMallocator/StaticMallocator.hpp>
#include <cstdlib>

// Bin buffer sizes/numbers
static const FwSizeType BIN0_BUFFER_SIZE = 10;
static const U16 BIN0_NUM_BUFFERS = 2;
static const FwSizeType BIN1_BUFFER_SIZE = 12;
static const U16 BIN1_NUM_BUFFERS = 4;
static const FwSizeType BIN2_BUFFER_SIZE = 100;
static const U16 BIN2_NUM_BUFFERS = 3;

// Other constants
static const FwEnumStoreType MEM_ID = 49;
static const U16 MGR_ID = 32;

// Largest total any single test method requests: BIN0_NUM_BUFFERS * BIN0_BUFFER_SIZE +
// BIN1_NUM_BUFFERS * BIN1_BUFFER_SIZE + BIN2_NUM_BUFFERS * BIN2_BUFFER_SIZE, plus
// (BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS) * sizeof(AllocatedBuffer). This UT runs
// on the native (Darwin/x86_64 or arm64) toolchain, not the SAMD21 cross-compiler, so
// sizeof(AllocatedBuffer) here (measured: 48 bytes/slot, due to wider pointers/FwSizeType on this
// ABI) is larger than the SAMD21-target figure (24 bytes/slot) used elsewhere in this project.
// Rounded up generously above the measured 800-byte requirement, to a multiple of 8
// (StaticMallocator's alignment requirement).
static const size_t TEST_ALLOCATOR_BUCKET_SIZE = 840;

// Define our own instrumented allocator for testing
class TestAllocator : public Fw::MemAllocator {
  public:
    TestAllocator() {};
    virtual ~TestAllocator() {};
    //! Allocate memory
    /*!
     * \param identifier the memory segment identifier (must equal MEM_ID -- StaticMallocator asserts
     *        this, unlike the original F´ test's MallocAllocator-backed version, which ignored it)
     * \param size the requested size (not changed)
     * \param recoverable - flag to indicate the memory could be recoverable (always set to false)
     * \return the pointer to memory. Zero if unable to allocate.
     */
    void* allocate(const FwEnumStoreType identifier, FwSizeType& size, bool& recoverable, FwSizeType alignment) {
        this->m_reqId = identifier;
        this->m_reqSize = size;
        this->m_mem = this->m_alloc.allocate(identifier, size, recoverable);
        return this->m_mem;
    }
    //! Deallocate memory
    /*!
     * \param identifier the memory segment identifier (must equal MEM_ID, see allocate())
     * \ptr the pointer to memory returned by allocate()
     */
    void deallocate(const FwEnumStoreType identifier, void* ptr) { this->m_alloc.deallocate(identifier, ptr); }

    FwEnumStoreType getId() { return this->m_reqId; }

    FwSizeType getSize() { return this->m_reqSize; }

    void* getMem() { return this->m_mem; }

  private:
    Samd21::StaticMallocator<TEST_ALLOCATOR_BUCKET_SIZE, MEM_ID> m_alloc;
    FwEnumStoreType m_reqId;
    FwSizeType m_reqSize;
    void* m_mem;
};

namespace Samd21 {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

ThinBufferManagerTester ::ThinBufferManagerTester()
    : ThinBufferManagerGTestBase("Tester", ThinBufferManagerTester::MAX_HISTORY_SIZE),
      component("ThinBufferManager") {
    this->initComponents();
    this->connectPorts();
}

ThinBufferManagerTester ::~ThinBufferManagerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void ThinBufferManagerTester ::testSetup() {
    ThinBufferManagerComponentImpl::BufferBins bins;
    memset(&bins, 0, sizeof(bins));
    bins.bins[0].bufferSize = BIN0_BUFFER_SIZE;
    bins.bins[0].numBuffers = BIN0_NUM_BUFFERS;
    bins.bins[1].bufferSize = BIN1_BUFFER_SIZE;
    bins.bins[1].numBuffers = BIN1_NUM_BUFFERS;
    bins.bins[2].bufferSize = BIN2_BUFFER_SIZE;
    bins.bins[2].numBuffers = BIN2_NUM_BUFFERS;

    TestAllocator alloc;

    this->component.setup(MGR_ID, MEM_ID, alloc, bins);
    ASSERT_EQ(MEM_ID, this->component.m_memId);
    ASSERT_EQ(MGR_ID, this->component.m_mgrId);
    // check allocation state

    // Check that enough buffers were created
    ASSERT_EQ(BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS, this->component.m_numStructs);

    // check that enough memory was requested
    FwSizeType memSize = (BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS) *
                             sizeof(Samd21::ThinBufferManagerComponentImpl::AllocatedBuffer) +
                         (BIN0_NUM_BUFFERS * BIN0_BUFFER_SIZE + BIN1_NUM_BUFFERS * BIN1_BUFFER_SIZE +
                          BIN2_NUM_BUFFERS * BIN2_BUFFER_SIZE);
    ASSERT_EQ(memSize, alloc.getSize());

    // check that correct ID was requested
    ASSERT_EQ(MEM_ID, alloc.getId());

    // first buffer should point at location just past buffer structs
    U8* mem = reinterpret_cast<U8*>(alloc.getMem()) +
              this->component.m_numStructs * sizeof(Samd21::ThinBufferManagerComponentImpl::AllocatedBuffer);
    ;

    // check the buffer properties
    for (U16 entry = 0; entry < this->component.m_numStructs; entry++) {
        // check context ID -- ThinBuffer has no getContext() of its own; round-trip through
        // getBuffer() (which the original BufferManager test doesn't need, since it stores a
        // real Fw::Buffer directly).
        ASSERT_EQ(this->component.m_buffers[entry].buff.getBuffer().getContext(), ((MGR_ID << 16) | entry));
        // check allocation state
        ASSERT_FALSE(this->component.m_buffers[entry].allocated);
        // check buffer sizes
        if (entry < BIN0_NUM_BUFFERS) {
            ASSERT_EQ(BIN0_BUFFER_SIZE, this->component.m_buffers[entry].size);
            ASSERT_EQ(mem, this->component.m_buffers[entry].memory);
            mem += BIN0_BUFFER_SIZE;
        } else if (entry < BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS) {
            ASSERT_EQ(BIN1_BUFFER_SIZE, this->component.m_buffers[entry].size);
            ASSERT_EQ(mem, this->component.m_buffers[entry].memory);
            mem += BIN1_BUFFER_SIZE;
        } else if (entry < BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS) {
            ASSERT_EQ(BIN2_BUFFER_SIZE, this->component.m_buffers[entry].size);
            ASSERT_EQ(mem, this->component.m_buffers[entry].memory);
            mem += BIN2_BUFFER_SIZE;
        } else {
            // just in case the logic is wrong
            ASSERT_TRUE(false);
        }
    }

    // memory location should be at end of allocated memory
    ASSERT_EQ(mem, reinterpret_cast<U8*>(alloc.getMem()) + alloc.getSize());

    this->component.cleanup();
    ASSERT_TRUE(this->component.m_cleaned);
    ASSERT_FALSE(this->component.m_setup);
}

void ThinBufferManagerTester::oneBufferSize() {
    ThinBufferManagerComponentImpl::BufferBins bins;
    memset(&bins, 0, sizeof(bins));
    bins.bins[0].bufferSize = BIN1_BUFFER_SIZE;
    bins.bins[0].numBuffers = BIN1_NUM_BUFFERS;

    TestAllocator alloc;

    this->component.setup(MGR_ID, MEM_ID, alloc, bins);

    Fw::Buffer buffs[BIN1_NUM_BUFFERS];

    for (U16 b = 0; b < BIN1_NUM_BUFFERS; b++) {
        // Get the buffers
        buffs[b] = this->invoke_to_bufferGetCallee(0, BIN1_BUFFER_SIZE);
        // check allocation state
        ASSERT_TRUE(this->component.m_buffers[b].allocated);
        // check stats
        ASSERT_EQ(b + 1, this->component.m_currBuffs);
        // check stats
        ASSERT_EQ(b + 1, this->component.m_highWater);
    }

    // should send back empty buffer
    Fw::Buffer noBuff = this->invoke_to_bufferGetCallee(0, BIN1_BUFFER_SIZE);
    ASSERT_EQ(1, this->component.m_noBuffs);

    // check telemetry
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_SIZE(5);
    ASSERT_TLM_TotalBuffs_SIZE(1);
    ASSERT_TLM_TotalBuffs(0, BIN1_NUM_BUFFERS);
    ASSERT_TLM_CurrBuffs_SIZE(1);
    ASSERT_TLM_CurrBuffs(0, BIN1_NUM_BUFFERS);
    ASSERT_TLM_HiBuffs_SIZE(1);
    ASSERT_TLM_HiBuffs(0, BIN1_NUM_BUFFERS);
    ASSERT_TLM_NoBuffs_SIZE(1);
    ASSERT_TLM_NoBuffs(0, 1);
    ASSERT_TLM_EmptyBuffs_SIZE(1);
    ASSERT_TLM_EmptyBuffs(0, 0);

    // clear histories
    this->clearHistory();

    // randomly return buffers
    bool returned[BIN1_NUM_BUFFERS] = {false};

    for (U16 b = 0; b < BIN1_NUM_BUFFERS; b++) {
        U16 entry;
        while (true) {
            entry = STest::Pick::lowerUpper(0, BIN1_NUM_BUFFERS - 1);
            if (not returned[entry]) {
                returned[entry] = true;
                break;
            }
        }
        // return the buffer
        printf("Returning buffer %d\n", entry);
        this->invoke_to_bufferSendIn(0, buffs[entry]);
        // check allocation state
        ASSERT_FALSE(this->component.m_buffers[entry].allocated);
        ASSERT_EQ(BIN1_NUM_BUFFERS - b - 1, this->component.m_currBuffs);
        ASSERT_EQ(BIN1_NUM_BUFFERS, this->component.m_highWater);
    }

    // should reject empty buffer
    this->invoke_to_bufferSendIn(0, noBuff);
    ASSERT_EQ(1, this->component.m_emptyBuffs);

    // check telemetry
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_SIZE(2);
    // No total buffs or no buffs since only on update
    ASSERT_TLM_CurrBuffs_SIZE(1);
    ASSERT_TLM_CurrBuffs(0, 0);
    ASSERT_TLM_NoBuffs_SIZE(0);
    ASSERT_TLM_EmptyBuffs_SIZE(1);
    ASSERT_TLM_EmptyBuffs(0, 1);

    // all buffers should be deallocated
    for (U16 b = 0; b < this->component.m_numStructs; b++) {
        ASSERT_FALSE(this->component.m_buffers[b].allocated);
    }

    // cleanup ThinBufferManager memory
    this->component.cleanup();
}

void ThinBufferManagerTester::multBuffSize() {
    ThinBufferManagerComponentImpl::BufferBins bins;
    memset(&bins, 0, sizeof(bins));
    bins.bins[0].bufferSize = BIN0_BUFFER_SIZE;
    bins.bins[0].numBuffers = BIN0_NUM_BUFFERS;
    bins.bins[1].bufferSize = BIN1_BUFFER_SIZE;
    bins.bins[1].numBuffers = BIN1_NUM_BUFFERS;
    bins.bins[2].bufferSize = BIN2_BUFFER_SIZE;
    bins.bins[2].numBuffers = BIN2_NUM_BUFFERS;

    TestAllocator alloc;

    this->component.setup(MGR_ID, MEM_ID, alloc, bins);

    Fw::Buffer buffs[BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS];

    // ThinBufferManager should be able to provide the whole pool worth of buffers
    // for a requested size smaller than the smallest bin.

    for (U16 b = 0; b < BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS; b++) {
        // Get the buffers
        buffs[b] = this->invoke_to_bufferGetCallee(0, BIN0_BUFFER_SIZE);
        // check allocation state
        ASSERT_TRUE(this->component.m_buffers[b].allocated);
        // check stats
        ASSERT_EQ(b + 1, this->component.m_currBuffs);
        // check stats
        ASSERT_EQ(b + 1, this->component.m_highWater);
    }

    // should send back empty buffer
    Fw::Buffer noBuff = this->invoke_to_bufferGetCallee(0, BIN1_BUFFER_SIZE);
    ASSERT_EQ(1, this->component.m_noBuffs);

    // check telemetry
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_SIZE(5);
    ASSERT_TLM_TotalBuffs_SIZE(1);
    ASSERT_TLM_TotalBuffs(0, BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS);
    ASSERT_TLM_CurrBuffs_SIZE(1);
    ASSERT_TLM_CurrBuffs(0, BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS);
    ASSERT_TLM_HiBuffs_SIZE(1);
    ASSERT_TLM_HiBuffs(0, BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS);
    ASSERT_TLM_NoBuffs_SIZE(1);
    ASSERT_TLM_NoBuffs(0, 1);
    ASSERT_TLM_EmptyBuffs_SIZE(1);
    ASSERT_TLM_EmptyBuffs(0, 0);

    // clear histories
    this->clearHistory();

    for (U16 b = 0; b < BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS; b++) {
        // return the buffer
        this->invoke_to_bufferSendIn(0, buffs[b]);
        // check allocation state
        ASSERT_FALSE(this->component.m_buffers[b].allocated);
        ASSERT_EQ(BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS - b - 1, this->component.m_currBuffs);
        ASSERT_EQ(BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS, this->component.m_highWater);
    }

    // should reject empty buffer
    this->invoke_to_bufferSendIn(0, noBuff);
    ASSERT_EQ(1, this->component.m_emptyBuffs);

    // check telemetry
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_SIZE(2);
    // No total buffs or no buffs since only on update
    ASSERT_TLM_CurrBuffs_SIZE(1);
    ASSERT_TLM_CurrBuffs(0, 0);
    ASSERT_TLM_NoBuffs_SIZE(0);
    ASSERT_TLM_EmptyBuffs_SIZE(1);
    ASSERT_TLM_EmptyBuffs(0, 1);

    // clear histories
    this->clearHistory();

    // all buffers should be deallocated
    for (U16 b = 0; b < this->component.m_numStructs; b++) {
        ASSERT_FALSE(this->component.m_buffers[b].allocated);
    }

    // clear histories
    this->clearEvents();

    // ThinBufferManager should be able to provide the BIN1 and BIN2 worth of buffers
    // for a requested size just smaller than the BIN1 size

    for (U16 b = 0; b < BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS; b++) {
        // Get the buffers
        buffs[b] = this->invoke_to_bufferGetCallee(0, BIN1_BUFFER_SIZE);
        // check allocation state - should be allocating from bin 1
        ASSERT_TRUE(this->component.m_buffers[b + BIN0_NUM_BUFFERS].allocated);
        // check stats
        ASSERT_EQ(b + 1, this->component.m_currBuffs);
    }

    // should send back empty buffer
    noBuff = this->invoke_to_bufferGetCallee(0, BIN1_BUFFER_SIZE);
    ASSERT_EQ(2, this->component.m_noBuffs);

    // check telemetry
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_SIZE(2);
    ASSERT_TLM_TotalBuffs_SIZE(0);
    ASSERT_TLM_CurrBuffs_SIZE(1);
    ASSERT_TLM_CurrBuffs(0, BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS);
    ASSERT_TLM_NoBuffs_SIZE(1);
    ASSERT_TLM_NoBuffs(0, 2);
    ASSERT_TLM_EmptyBuffs_SIZE(0);

    // clear histories
    this->clearHistory();

    for (U16 b = 0; b < BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS; b++) {
        // return the buffer
        this->invoke_to_bufferSendIn(0, buffs[b]);
        // check allocation state - should be freeing from bin 1
        ASSERT_FALSE(this->component.m_buffers[b + BIN0_NUM_BUFFERS].allocated);
        ASSERT_EQ(BIN1_NUM_BUFFERS + BIN2_NUM_BUFFERS - b - 1, this->component.m_currBuffs);
    }

    // should reject empty buffer
    this->invoke_to_bufferSendIn(0, noBuff);
    ASSERT_EQ(2, this->component.m_emptyBuffs);

    // check telemetry
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_SIZE(2);
    // No total buffs or no buffs since only on update
    ASSERT_TLM_CurrBuffs_SIZE(1);
    ASSERT_TLM_CurrBuffs(0, 0);
    ASSERT_TLM_NoBuffs_SIZE(0);
    ASSERT_TLM_EmptyBuffs_SIZE(1);
    ASSERT_TLM_EmptyBuffs(0, 2);

    // clear histories
    this->clearHistory();

    // all buffers should be deallocated
    for (U16 b = 0; b < this->component.m_numStructs; b++) {
        ASSERT_FALSE(this->component.m_buffers[b].allocated);
    }

    // ThinBufferManager should be able to provide the BIN2 worth of buffers
    // for a requested size just smaller than the BIN2 size

    for (U16 b = 0; b < BIN2_NUM_BUFFERS; b++) {
        // Get the buffers
        buffs[b] = this->invoke_to_bufferGetCallee(0, BIN2_BUFFER_SIZE);
        // check allocation state - should be allocating from bin 1
        ASSERT_TRUE(this->component.m_buffers[b + BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS].allocated);
        // check stats
        ASSERT_EQ(b + 1, this->component.m_currBuffs);
    }

    // should send back empty buffer
    noBuff = this->invoke_to_bufferGetCallee(0, BIN2_BUFFER_SIZE);
    ASSERT_EQ(3, this->component.m_noBuffs);

    // check telemetry
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_SIZE(2);
    ASSERT_TLM_TotalBuffs_SIZE(0);
    ASSERT_TLM_CurrBuffs_SIZE(1);
    ASSERT_TLM_CurrBuffs(0, BIN2_NUM_BUFFERS);
    ASSERT_TLM_NoBuffs_SIZE(1);
    ASSERT_TLM_NoBuffs(0, 3);
    ASSERT_TLM_EmptyBuffs_SIZE(0);

    // clear histories
    this->clearHistory();

    for (U16 b = 0; b < BIN2_NUM_BUFFERS; b++) {
        // return the buffer
        this->invoke_to_bufferSendIn(0, buffs[b]);
        // check allocation state - should be freeing from bin 1
        ASSERT_FALSE(this->component.m_buffers[b + BIN0_NUM_BUFFERS + BIN1_NUM_BUFFERS].allocated);
        ASSERT_EQ(BIN2_NUM_BUFFERS - b - 1, this->component.m_currBuffs);
    }

    // should reject empty buffer
    this->invoke_to_bufferSendIn(0, noBuff);
    ASSERT_EQ(3, this->component.m_emptyBuffs);

    // check telemetry
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_SIZE(2);
    // No total buffs or no buffs since only on update
    ASSERT_TLM_CurrBuffs_SIZE(1);
    ASSERT_TLM_CurrBuffs(0, 0);
    ASSERT_TLM_NoBuffs_SIZE(0);
    ASSERT_TLM_EmptyBuffs_SIZE(1);
    ASSERT_TLM_EmptyBuffs(0, 3);

    // all buffers should be deallocated
    for (U16 b = 0; b < this->component.m_numStructs; b++) {
        ASSERT_FALSE(this->component.m_buffers[b].allocated);
    }

    // cleanup ThinBufferManager memory
    this->component.cleanup();
}

// connectPorts() and initComponents() are provided by the generated
// ThinBufferManagerTesterHelpers.cpp (UT_AUTO_HELPERS in CMakeLists.txt).

}  // end namespace Samd21
