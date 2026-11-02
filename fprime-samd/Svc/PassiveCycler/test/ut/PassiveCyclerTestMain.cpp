// ======================================================================
// \title  PassiveCyclerTestMain.cpp
// \brief  cpp file for PassiveCycler component test main function
// ======================================================================

#include "PassiveCyclerTester.hpp"

TEST(Nominal, Idle) {
    Svc::PassiveCyclerTester tester;
    tester.testIdle();
}

TEST(Nominal, EarlyTrue) {
    Svc::PassiveCyclerTester tester;
    tester.testEarlyTrue();
}

TEST(Nominal, LateTrue) {
    Svc::PassiveCyclerTester tester;
    tester.testLateTrue();
}

TEST(Nominal, RepeatedWork) {
    Svc::PassiveCyclerTester tester;
    tester.testRepeatedWork();
}

TEST(Idle, Sleeps) {
    Svc::PassiveCyclerTester tester;
    tester.testIdleSleeps();
}

TEST(Idle, InterruptDuringPass) {
    Svc::PassiveCyclerTester tester;
    tester.testIdleInterruptDuringPass();
}

TEST(Idle, StaleInterrupt) {
    Svc::PassiveCyclerTester tester;
    tester.testIdleStaleInterrupt();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
