// ======================================================================
// \title  FatalHandlerTestMain.cpp
// \author tumbar
// \brief  cpp file for FatalHandler component test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "fprime-samd/Svc/FatalHandler/test/ut/FatalHandlerTester.hpp"

TEST(Nominal, testNoDynamicHookRegistered) {
    Samd21::FatalHandlerTester tester;
    tester.testNoDynamicHookRegistered();
}

TEST(Nominal, testReportSendsAssertionPacket) {
    Samd21::FatalHandlerTester tester;
    tester.testReportSendsAssertionPacket();
}

TEST(Nominal, testReportUsesIdBase) {
    Samd21::FatalHandlerTester tester;
    tester.testReportUsesIdBase();
}

TEST(Nominal, testReportedArgumentCounts) {
    Samd21::FatalHandlerTester tester;
    tester.testReportedArgumentCounts();
}

TEST(Nominal, testTimeReadOnce) {
    Samd21::FatalHandlerTester tester;
    tester.testTimeReadOnce();
}

TEST(Nominal, testLatestInstanceReports) {
    Samd21::FatalHandlerTester tester;
    tester.testLatestInstanceReports();
}

TEST(Nominal, testTerminateResets) {
    Samd21::FatalHandlerTester tester;
    tester.testTerminateResets();
}

TEST(OffNominal, testNoInstanceResetsSilently) {
    Samd21::FatalHandlerTester tester;
    tester.testNoInstanceResetsSilently();
}

TEST(OffNominal, testRecursiveAssertResets) {
    Samd21::FatalHandlerTester tester;
    tester.testRecursiveAssertResets();
}

TEST(OffNominal, testReportWithoutFatalOut) {
    Samd21::FatalHandlerTester tester;
    tester.testReportWithoutFatalOut();
}

TEST(Death, testFwAssertRoutedToHandler) {
    Samd21::FatalHandlerTester tester;
    tester.testFwAssertRoutedToHandler();
}

TEST(Death, testFatalReceiveAsserts) {
    Samd21::FatalHandlerTester tester;
    tester.testFatalReceiveAsserts();
}

int main(int argc, char** argv) {
    // Seed random number generator for STest
    STest::Random::seed();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
