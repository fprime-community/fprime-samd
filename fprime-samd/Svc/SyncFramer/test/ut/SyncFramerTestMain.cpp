// ======================================================================
// \title  SyncFramerTestMain.cpp
// \author tumbar
// \brief  cpp file for SyncFramer component test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "fprime-samd/Svc/SyncFramer/test/ut/SyncFramerTester.hpp"

TEST(Nominal, testFrameLayout) {
    Samd21::SyncFramerTester tester;
    tester.testFrameLayout();
}

TEST(Nominal, testKnownCrc) {
    Samd21::SyncFramerTester tester;
    tester.testKnownCrc();
}

TEST(Nominal, testGroundDeframes) {
    Samd21::SyncFramerTester tester;
    tester.testGroundDeframes();
}

TEST(Nominal, testPacketSizes) {
    Samd21::SyncFramerTester tester;
    tester.testPacketSizes();
}

TEST(Nominal, testBackToBackFrames) {
    Samd21::SyncFramerTester tester;
    tester.testBackToBackFrames();
}

TEST(Nominal, testDrvConnectedNoOp) {
    Samd21::SyncFramerTester tester;
    tester.testDrvConnectedNoOp();
}

TEST(OffNominal, testDriverErrorIgnored) {
    Samd21::SyncFramerTester tester;
    tester.testDriverErrorIgnored();
}

int main(int argc, char** argv) {
    // Seed random number generator for STest
    STest::Random::seed();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
