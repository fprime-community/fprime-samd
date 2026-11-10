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

    //! A save leaves out invalid parameters and clears records of an earlier save.
    void testSaveSkipsInvalid();

    //! A blank (erased) store loads with every parameter invalid and nothing corrupt.
    void testBlankStore();

    //! A failed read keeps the records already loaded and fails LOAD.
    void testReadFailure();

    //! A failed write fails SAVE.
    void testWriteFailure();

    //! Commands are rejected BUSY, setPrm dropped and getPrm INVALID during a transfer.
    void testBusy();

    //! Save and load work with storage completing inside the transfer call.
    void testInCallCompletion();

  protected:
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

    //! Deliver outstanding completions until none is left
    void drain();

    //! Slot of the store holding a record ID, STORE_RECORDS when none does
    FwSizeType findSlot(U16 recordId) const;

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
