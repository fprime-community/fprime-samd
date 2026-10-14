// ======================================================================
// \title  SpiDriverTestMain.cpp
// \brief  cpp file for SpiDriver component test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "SpiDriverTester.hpp"

TEST(Nominal, ConfigureSoftwareChipSelect) {
    Samd21::SpiDriverTester tester;
    tester.testConfigureSoftwareChipSelect();
}

TEST(Nominal, ConfigureHardwareChipSelect) {
    Samd21::SpiDriverTester tester;
    tester.testConfigureHardwareChipSelect();
}

TEST(Nominal, Transaction) {
    Samd21::SpiDriverTester tester;
    tester.testTransaction();
}

TEST(Nominal, HardwareChipSelectTransaction) {
    Samd21::SpiDriverTester tester;
    tester.testHardwareChipSelectTransaction();
}

TEST(Nominal, CalculateBaud) {
    Samd21::SpiDriverTester tester;
    tester.testCalculateBaud();
}

TEST(Nominal, RandomTransactions) {
    Samd21::SpiDriverTester tester;
    tester.testRandomTransactions();
}

TEST(OffNominal, Busy) {
    Samd21::SpiDriverTester tester;
    tester.testBusy();
}

TEST(OffNominal, ConfigureTwice) {
    Samd21::SpiDriverTester tester;
    tester.testConfigureTwice();
}

TEST(OffNominal, RequestBeforeConfigure) {
    Samd21::SpiDriverTester tester;
    tester.testRequestBeforeConfigure();
}

TEST(OffNominal, SizeMismatch) {
    Samd21::SpiDriverTester tester;
    tester.testSizeMismatch();
}

TEST(OffNominal, UnexpectedDmaReply) {
    Samd21::SpiDriverTester tester;
    tester.testUnexpectedDmaReply();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}
