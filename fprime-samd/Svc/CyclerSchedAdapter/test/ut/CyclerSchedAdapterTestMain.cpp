// ======================================================================
// \title  CyclerSchedAdapterTestMain.cpp
// \author lestarch-autobot
// \brief  cpp file for CyclerSchedAdapter component test main function
// ======================================================================

#include "CyclerSchedAdapterTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"

TEST(Nominal, ForwardOnce) {
    COMMENT("UT-001: one activeIn call forwards exactly one schedOut call and returns false");
    REQUIREMENT("REQ-CSA-001");
    REQUIREMENT("REQ-CSA-004");
    Samd21::CyclerSchedAdapterTester tester;
    tester.testForwardOnce();
}

TEST(Nominal, ContextPassThrough) {
    COMMENT("UT-002: contexts 0, 1, 7, 0xFFFFFFFF arrive at schedOut unchanged");
    REQUIREMENT("REQ-CSA-002");
    Samd21::CyclerSchedAdapterTester tester;
    tester.testContextPassThrough();
}

TEST(Nominal, NestedCompletion) {
    COMMENT("UT-003: schedOut handler has returned when activeIn returns; no call outside activeIn");
    REQUIREMENT("REQ-CSA-003");
    REQUIREMENT("REQ-CSA-009");
    Samd21::CyclerSchedAdapterTester tester;
    tester.testNestedCompletion();
}

TEST(OffNominal, Unconnected) {
    COMMENT("UT-004: activeIn with schedOut unconnected dies by FW_ASSERT; connected control forwards once");
    REQUIREMENT("REQ-CSA-005");
    Samd21::CyclerSchedAdapterTester tester;
    tester.testUnconnected();
}

TEST(Nominal, HistoryIndependence) {
    COMMENT("UT-005: 100 interleaved calls on two instances each forward once with their own context");
    REQUIREMENT("REQ-CSA-001");
    REQUIREMENT("REQ-CSA-004");
    REQUIREMENT("REQ-CSA-008");
    Samd21::CyclerSchedAdapterTester tester;
    tester.testHistoryIndependence();
}

TEST(Nominal, NoIdleOutput) {
    COMMENT("UT-006: no schedOut call before the first activeIn");
    REQUIREMENT("REQ-CSA-009");
    Samd21::CyclerSchedAdapterTester tester;
    tester.testNoIdleOutput();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}
