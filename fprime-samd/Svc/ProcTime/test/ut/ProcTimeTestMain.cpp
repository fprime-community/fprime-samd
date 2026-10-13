// ======================================================================
// \title  ProcTimeTestMain.cpp
// \author tumbar
// \brief  cpp file for ProcTime component test main function
// ======================================================================

#include "fprime-samd/Svc/ProcTime/test/ut/ProcTimeTester.hpp"

TEST(Nominal, testTimeBase) {
    Samd21::ProcTimeTester tester;
    tester.testTimeBase();
}

TEST(Nominal, testTimeTracksRawTime) {
    Samd21::ProcTimeTester tester;
    tester.testTimeTracksRawTime();
}

TEST(Nominal, testTimeIsMonotonic) {
    Samd21::ProcTimeTester tester;
    tester.testTimeIsMonotonic();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
