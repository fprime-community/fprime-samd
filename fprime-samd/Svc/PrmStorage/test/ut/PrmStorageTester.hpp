// ======================================================================
// \title  PrmStorageTester.hpp
// \author tumbar
// \brief  hpp file for PrmStorage component test harness implementation class
// ======================================================================

#ifndef Samd21_PrmStorageTester_HPP
#define Samd21_PrmStorageTester_HPP

#include "FramConfig/FramConfig.hpp"
#include "fprime-samd/Svc/PrmStorage/PrmStorage.hpp"
#include "fprime-samd/Svc/PrmStorage/PrmStorageGTestBase.hpp"
#include "fprime-samd/Svc/PrmStorage/PrmStorageLayout.hpp"

namespace Samd21 {

class PrmStorageTester : public PrmStorageGTestBase {
  public:
    // Maximum size for histories
    static constexpr FwSizeType MAX_HISTORY_SIZE = 100;

    // Test instance ID
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Bank layout under test (default PrmStorageConfig.hpp)
    static constexpr U32 PRIME_OFFSET = PrmStorageConfig::PRIME_OFFSET;
    static constexpr U32 BACKUP_OFFSET = PrmStorageConfig::BACKUP_OFFSET;

    //! Database image size: two full F-RAM chunks and a partial one
    static constexpr FwSizeType IMAGE_SIZE = 2 * Fram::FramConfig::MaxTransfer + 21;

    //! Simulated F-RAM, a window over the start of the device holding both banks
    static constexpr FwSizeType FRAM_SIZE = 0x2000;

    static_assert(PrmStorageLayout::banksHold(IMAGE_SIZE), "Test image must fit the default bank layout");
    static_assert((PRIME_OFFSET + IMAGE_SIZE <= FRAM_SIZE) && (BACKUP_OFFSET + IMAGE_SIZE <= FRAM_SIZE),
                  "Simulated F-RAM must hold both banks");

    // Construction and destruction
    PrmStorageTester();
    ~PrmStorageTester();

    //! A save writes the image to the selected bank in MaxTransfer chunks and
    //! returns OP_OK after the last one.
    void testSave();

    //! A load reads the selected bank into the image and returns OP_OK.
    void testLoad();

    //! SELECT_BANK moves the following transfers to the other bank, leaving the
    //! first untouched.
    void testSelectBank();

    //! SELECT_BANK while a transfer is in progress is rejected BUSY and the
    //! transfer stays on its bank.
    void testSelectBankBusy();

    //! A failed chunk ends the transfer with OTHER_ERROR and no more requests.
    void testStorageFailure();

    //! Requests rejected BUSY are retried on schedIn up to BUSY_RETRY_LIMIT.
    void testBusyRetry();

    //! A request still BUSY after BUSY_RETRY_LIMIT retries fails the transfer.
    void testBusyRetryExhausted();

    //! A request rejected other than BUSY fails the transfer without a retry.
    void testRequestRejected();

    //! Save and load work with completions delivered inside the request call.
    void testInCallCompletion();

  protected:
    //! Simulated F-RAM read
    Fram::Status from_framRead_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) override;

    //! Simulated F-RAM write
    Fram::Status from_framWrite_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) override;

  private:
    //! A request accepted by the simulated F-RAM, not yet completed
    struct Pending {
        bool valid;
        Fram::Op op;
        U32 offset;
        Fw::Buffer buffer;
    };

    //! Component under test
    PrmStorage component;

    //! Simulated F-RAM contents
    U8 m_fram[FRAM_SIZE];

    //! The database image handed to the component
    U8 m_image[IMAGE_SIZE];

    //! Outstanding request
    Pending m_pending;

    //! Deliver completions from inside the request call
    bool m_completeInCall;

    //! Status the next m_rejectCount requests are rejected with
    Fram::Status m_rejectStatus;
    U32 m_rejectCount;

    //! Complete the request with this sequence number with SPI_ERROR (0: none)
    U32 m_failRequestNumber;

    //! Requests accepted so far
    U32 m_requests;

    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Fill the first size bytes of the image with a pattern seeded by seed
    void fillImage(U8 seed, FwSizeType size = IMAGE_SIZE);

    //! Assert size bytes at data hold the pattern seeded by seed
    static void assertPattern(const U8* data, U8 seed, FwSizeType size = IMAGE_SIZE);

    //! Clear history and send the image to writeIn
    void sendSave(FwSizeType size = IMAGE_SIZE);

    //! Clear history and send the image to readIn
    void sendLoad(FwSizeType size = IMAGE_SIZE);

    //! Clear history and send SELECT_BANK
    void sendSelectBank(PrmBank bank, U32 cmdSeq);

    //! Deliver outstanding completions until none is left
    void drain();

    //! Assert exactly one write return with the given status
    void assertSaveDone(Drv::ByteStreamStatus status, FwSizeType size = IMAGE_SIZE);

    //! Assert exactly one read return with the given status
    void assertLoadDone(Drv::ByteStreamStatus status, FwSizeType size = IMAGE_SIZE);

    //! Assert the request history covers size image bytes at bankOffset in
    //! MaxTransfer chunks, in order
    void assertChunks(U32 bankOffset, FwSizeType size = IMAGE_SIZE) const;

    //! Execute and record one simulated F-RAM request
    Fram::Status framRequest(Fram::Op op, U32 offset, Fw::Buffer& buffer);

    //! Connect ports (generated by UT_AUTO_HELPERS)
    void connectPorts();

    //! Initialize components (generated by UT_AUTO_HELPERS)
    void initComponents();
};

}  // namespace Samd21

#endif
