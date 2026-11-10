// ======================================================================
// \title  PrmStorageTestMain.cpp
// \author tumbar
// \brief  cpp file for PrmStorage component test main function
// ======================================================================

#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"
#include "fprime-samd/Svc/PrmStorage/test/ut/PrmStorageTester.hpp"

TEST(Transfer, Save) {
    REQUIREMENT("PRM-STORE-001");
    COMMENT("a save writes the image to the selected bank in MaxTransfer chunks");
    Samd21::PrmStorageTester tester;
    tester.testSave();
}

TEST(Transfer, Load) {
    REQUIREMENT("PRM-STORE-001");
    COMMENT("a load reads the selected bank into the image");
    Samd21::PrmStorageTester tester;
    tester.testLoad();
}

TEST(Transfer, Offset) {
    REQUIREMENT("PRM-STORE-001");
    COMMENT("a transfer lands at its offset within the selected bank");
    Samd21::PrmStorageTester tester;
    tester.testOffset();
}

TEST(Transfer, StorageFailure) {
    REQUIREMENT("PRM-STORE-002");
    COMMENT("a failed chunk ends the transfer with OTHER_ERROR");
    Samd21::PrmStorageTester tester;
    tester.testStorageFailure();
}

TEST(Bank, Select) {
    REQUIREMENT("PRM-STORE-003");
    COMMENT("SELECT_BANK moves the following transfers to the other bank");
    Samd21::PrmStorageTester tester;
    tester.testSelectBank();
}

TEST(Bank, SelectBusy) {
    REQUIREMENT("PRM-STORE-003");
    COMMENT("SELECT_BANK during a transfer is rejected BUSY and the transfer keeps its bank");
    Samd21::PrmStorageTester tester;
    tester.testSelectBankBusy();
}

TEST(Fram, BusyRetry) {
    REQUIREMENT("PRM-STORE-004");
    COMMENT("requests rejected BUSY are retried on schedIn");
    Samd21::PrmStorageTester tester;
    tester.testBusyRetry();
}

TEST(Fram, BusyRetryExhausted) {
    REQUIREMENT("PRM-STORE-004");
    COMMENT("a request still BUSY after BUSY_RETRY_LIMIT retries fails the transfer");
    Samd21::PrmStorageTester tester;
    tester.testBusyRetryExhausted();
}

TEST(Fram, RequestRejected) {
    REQUIREMENT("PRM-STORE-004");
    COMMENT("a request rejected other than BUSY fails the transfer without a retry");
    Samd21::PrmStorageTester tester;
    tester.testRequestRejected();
}

TEST(Fram, InCallCompletion) {
    REQUIREMENT("PRM-STORE-005");
    COMMENT("save and load complete with F-RAM completions delivered inside the request call");
    Samd21::PrmStorageTester tester;
    tester.testInCallCompletion();
}

int main(int argc, char** argv) {
    STest::Random::seed();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
