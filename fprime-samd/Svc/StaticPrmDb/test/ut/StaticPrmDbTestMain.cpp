// ======================================================================
// \title  StaticPrmDbTestMain.cpp
// \author tumbar
// \brief  cpp file for StaticPrmDb component test main function
// ======================================================================

#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"
#include "fprime-samd/Svc/StaticPrmDb/test/ut/StaticPrmDbTester.hpp"

TEST(Params, SetGet) {
    REQUIREMENT("PRM-DB-001");
    COMMENT("parameters set through setPrm read back VALID, others INVALID");
    Samd21::StaticPrmDbTester tester;
    tester.testSetGet();
}

TEST(Store, SaveLoad) {
    REQUIREMENT("PRM-DB-002");
    COMMENT("a save followed by a REPLACE load restores every value");
    Samd21::StaticPrmDbTester tester;
    tester.testSaveLoad();
}

TEST(Store, BitFlip) {
    REQUIREMENT("PRM-DB-003");
    REQUIREMENT("PRM-DB-001");
    COMMENT("a flipped bit invalidates only the parameter owning the record");
    Samd21::StaticPrmDbTester tester;
    tester.testBitFlip();
}

TEST(Store, DictionaryChange) {
    REQUIREMENT("PRM-DB-004");
    REQUIREMENT("PRM-DB-006");
    COMMENT("records of an older dictionary load in any slot order, unknown records are skipped");
    Samd21::StaticPrmDbTester tester;
    tester.testDictionaryChange();
}

TEST(Store, Merge) {
    REQUIREMENT("PRM-DB-005");
    REQUIREMENT("PRM-DB-003");
    COMMENT("a MERGE load fills in invalid parameters without overwriting valid ones");
    Samd21::StaticPrmDbTester tester;
    tester.testMerge();
}

TEST(Store, SaveSkipsInvalid) {
    REQUIREMENT("PRM-DB-002");
    COMMENT("a save leaves out invalid parameters and clears records of an earlier save");
    Samd21::StaticPrmDbTester tester;
    tester.testSaveSkipsInvalid();
}

TEST(Store, BlankStore) {
    REQUIREMENT("PRM-DB-006");
    COMMENT("an erased store loads with every parameter invalid");
    Samd21::StaticPrmDbTester tester;
    tester.testBlankStore();
}

TEST(Storage, ReadFailure) {
    REQUIREMENT("PRM-DB-007");
    COMMENT("a failed read keeps the records already loaded and fails LOAD");
    Samd21::StaticPrmDbTester tester;
    tester.testReadFailure();
}

TEST(Storage, WriteFailure) {
    REQUIREMENT("PRM-DB-007");
    COMMENT("a failed write fails SAVE");
    Samd21::StaticPrmDbTester tester;
    tester.testWriteFailure();
}

TEST(Storage, Busy) {
    REQUIREMENT("PRM-DB-008");
    COMMENT("commands are rejected BUSY, setPrm dropped and getPrm INVALID during a transfer");
    Samd21::StaticPrmDbTester tester;
    tester.testBusy();
}

TEST(Storage, InCallCompletion) {
    REQUIREMENT("PRM-DB-009");
    COMMENT("save and load work with storage completing inside the transfer call");
    Samd21::StaticPrmDbTester tester;
    tester.testInCallCompletion();
}

int main(int argc, char** argv) {
    STest::Random::seed();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
