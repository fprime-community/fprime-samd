// ======================================================================
// \title  StaticPrmDbTester.hpp
// \author tumbar
// \brief  hpp file for StaticPrmDb component test harness implementation class
// ======================================================================

#ifndef Samd21_StaticPrmDbTester_HPP
#define Samd21_StaticPrmDbTester_HPP

#include "fprime-samd/Svc/StaticPrmDb/StaticPrmDb.hpp"
#include "fprime-samd/Svc/StaticPrmDb/StaticPrmDbGTestBase.hpp"
#include "fprime-samd/Svc/Types/PrmRecord.hpp"

namespace Samd21 {

//! Drives StaticPrmDb against the test layout (StaticPrmDbTester.cpp) in place of an
//! autocoded deployment, with a simulated persistent store.
class StaticPrmDbTester : public StaticPrmDbGTestBase {
  public:
    // Maximum size for histories
    static constexpr FwSizeType MAX_HISTORY_SIZE = 100;

    // Test instance ID
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Test parameters: U32, a 12-byte value split over two records, and a U8
    static constexpr FwPrmIdType PRM_A = 0x10;
    static constexpr FwPrmIdType PRM_B = 0x20;
    static constexpr FwPrmIdType PRM_C = 0x30;
    static constexpr FwSizeType SIZE_A = 4;
    static constexpr FwSizeType SIZE_B = 12;
    static constexpr FwSizeType SIZE_C = 1;

    //! Record IDs of the test layout
    static constexpr U16 RECORD_A = 0x1111;
    static constexpr U16 RECORD_B0 = 0x2220;
    static constexpr U16 RECORD_B1 = 0x2221;
    static constexpr U16 RECORD_C = 0x0333;

    //! Command sequence number of a SAVE sent from a command response
    static constexpr U32 CHAINED_CMD_SEQ = 0xC4A1;

    //! Record slots of the simulated store (the default StaticPrmDbConfig)
    static constexpr FwSizeType STORE_RECORDS = StaticPrmDbConfig::STORE_RECORDS;
    static constexpr FwSizeType STORE_SIZE = StaticPrmDb::STORE_SIZE;

    // Not a divisor, so the last transfer is short
    static_assert(STORE_RECORDS % StaticPrmDbConfig::SCRATCH_RECORDS != 0,
                  "The tests expect a short last transfer with the default StaticPrmDbConfig");

    //! Calls of the deployment loadParameters()
    static U32 s_loadParametersCalls;

    // Construction and destruction
    StaticPrmDbTester();
    ~StaticPrmDbTester();

    //! Parameters set through setPrm read back VALID; parameters never set read INVALID.
    void testSetGet();

    //! A save followed by a REPLACE load restores every value.
    void testSaveLoad();

    //! A flipped bit invalidates only the parameter owning the record.
    void testBitFlip();

    //! Records of an older dictionary (unknown IDs) and records in any slot order load.
    void testDictionaryChange();

    //! A MERGE load fills in parameters left invalid without overwriting valid ones.
    void testMerge();

    //! A save interrupted at any record loads every parameter whole from one of the two saves,
    //! or invalid, never a mix of the two.
    void testInterruptedSave();

    //! A MERGE never completes a parameter with segments of the other store's save.
    void testMergeMixedSaves();

    //! A stale record left after an interrupted save loses to the newer record before it.
    void testStaleDuplicate();

    //! A save leaves out invalid parameters and clears records of an earlier save.
    void testSaveSkipsInvalid();

    //! A blank (erased) store loads with every parameter invalid and nothing corrupt.
    void testBlankStore();

    //! A failed read keeps the records already loaded and the values in memory it did not reach,
    //! and fails LOAD.
    void testReadFailure();

    //! A failed read invalidates only a parameter whose records it loaded part of.
    void testReadFailureSplitParam();

    //! A failed write fails SAVE with a throttled event; a successful save emits none and re-arms it.
    void testWriteFailure();

    //! Commands are rejected BUSY, setPrm dropped and getPrm INVALID during a transfer.
    void testBusy();

    //! Save and load work with storage completing inside the transfer call.
    void testInCallCompletion();

    //! activeIn issues at most one transfer per call and acts on its completion in the next call.
    void testOneTransferPerCall();

    //! A command sent from the response of the last one starts after it, one transfer at a time.
    void testChainedCommand();

    //! Without storage connected, SAVE and LOAD fail with an event instead of a port assert.
    void testNoStorage();

  protected:
    //! Record a command response; with m_chainSave, answer it with another SAVE
    void cmdResponseIn(FwOpcodeType opCode, U32 cmdSeq, Fw::CmdResponse response) override;

    //! Simulated store write
    void from_writeOut_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) override;

    //! Simulated store read
    void from_readOut_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) override;

  private:
    //! A transfer accepted by the simulated store, not yet completed
    struct Pending {
        bool valid;
        bool write;
        Fw::Buffer buffer;
    };

    //! Component under test
    StaticPrmDb component;

    //! Simulated persistent store
    U8 m_store[STORE_SIZE];

    //! Outstanding transfer
    Pending m_pending;

    //! Deliver completions from inside the transfer call
    bool m_completeInCall;

    //! Complete the transfer with this sequence number with OTHER_ERROR (0: none)
    U32 m_failTransferNumber;

    //! Transfers so far
    U32 m_transfers;

    //! Answer the next command response with a SAVE (CHAINED_CMD_SEQ)
    bool m_chainSave;

    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Set a parameter to size bytes of a pattern seeded by seed
    void setParam(FwPrmIdType id, FwSizeType size, U8 seed);

    //! Assert the parameter reads back with the given validity (and, when VALID, the pattern)
    void assertParam(FwPrmIdType id, FwSizeType size, Fw::ParamValid valid, U8 seed = 0);

    //! Send SAVE and deliver the completions
    void save(Fw::CmdResponse response = Fw::CmdResponse::OK);

    //! Send LOAD and deliver the completions
    void load(StaticPrmDb_LoadMode mode, Fw::CmdResponse response = Fw::CmdResponse::OK);

    //! Deliver the outstanding transfer's completion
    void complete();

    //! Run the main context (activeIn) and storage until no work is left
    void run();

    //! Slot of the store holding a record ID, STORE_RECORDS when none does
    FwSizeType findSlot(U16 recordId) const;

    //! Leave the store as a save interrupted after writing its first slots: slots from slots on
    //! still hold the earlier store
    void interruptSave(const U8* earlier, FwSizeType slots);

    //! Value of a parameter after a load that must give it whole from one of two seeds, or invalid.
    //! The seed it came from, 0 when invalid.
    U8 paramSeed(FwPrmIdType id, FwSizeType size, U8 seedA, U8 seedB);

    //! Assert the store holds exactly the given records (any order) and empty slots
    void assertStoreRecords(const U16* ids, FwSizeType count) const;

    //! Assert the transfers of the last command walked the store in StaticPrmDbConfig::SCRATCH_RECORDS steps
    void assertTransfers(bool write) const;

    //! Connect ports (generated by UT_AUTO_HELPERS)
    void connectPorts();

    //! Initialize components (generated by UT_AUTO_HELPERS)
    void initComponents();
};

}  // namespace Samd21

#endif
