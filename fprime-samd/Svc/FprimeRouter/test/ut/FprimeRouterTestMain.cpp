// ======================================================================
// \title  FprimeRouterTestMain.cpp
// \author tumbar
// \brief  cpp file for FprimeRouter component test main function
// ======================================================================

#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"
#include "fprime-samd/Svc/FprimeRouter/test/ut/FprimeRouterTester.hpp"

TEST(Nominal, testRouteCommand) {
    COMMENT("A command packet is copied to commandOut and its buffer returned at once");
    REQUIREMENT("SAMD21-ROUTER-001");
    REQUIREMENT("SAMD21-ROUTER-004");
    Samd21::FprimeRouterTester tester;
    tester.testRouteCommand();
}

TEST(Nominal, testRouteCommandMaxSize) {
    COMMENT("A command packet of FW_COM_BUFFER_MAX_SIZE bytes is forwarded whole");
    REQUIREMENT("SAMD21-ROUTER-001");
    Samd21::FprimeRouterTester tester;
    tester.testRouteCommandMaxSize();
}

TEST(OffNominal, testRouteCommandOversize) {
    COMMENT("A command packet too large for a ComBuffer is dropped with an event and returned");
    REQUIREMENT("SAMD21-ROUTER-002");
    REQUIREMENT("SAMD21-ROUTER-004");
    Samd21::FprimeRouterTester tester;
    tester.testRouteCommandOversize();
}

TEST(OffNominal, testRouteCommandOversizeThrottle) {
    COMMENT("Repeated oversize commands are reported at most 5 times per 60 s and every buffer is still returned");
    REQUIREMENT("SAMD21-ROUTER-002");
    REQUIREMENT("SAMD21-ROUTER-004");
    Samd21::FprimeRouterTester tester;
    tester.testRouteCommandOversizeThrottle();
}

TEST(Nominal, testRouteFile) {
    COMMENT("A file packet is handed off on fileOut");
    REQUIREMENT("SAMD21-ROUTER-003");
    Samd21::FprimeRouterTester tester;
    tester.testRouteFile();
}

TEST(OffNominal, testRouteFileUnconnected) {
    COMMENT("A file packet with fileOut unconnected is returned with its context");
    REQUIREMENT("SAMD21-ROUTER-005");
    Samd21::FprimeRouterTester tester(Samd21::FprimeRouterTester::Optional::UNCONNECTED);
    tester.testRouteFileUnconnected();
}

TEST(Nominal, testRouteUnknown) {
    COMMENT("Every other APID is handed off on unknownDataOut with its context");
    REQUIREMENT("SAMD21-ROUTER-003");
    Samd21::FprimeRouterTester tester;
    tester.testRouteUnknown();
}

TEST(OffNominal, testRouteUnknownUnconnected) {
    COMMENT("An unknown packet with unknownDataOut unconnected is returned with its context");
    REQUIREMENT("SAMD21-ROUTER-005");
    Samd21::FprimeRouterTester tester(Samd21::FprimeRouterTester::Optional::UNCONNECTED);
    tester.testRouteUnknownUnconnected();
}

TEST(Nominal, testBufferReturn) {
    COMMENT("Handed-off buffers returned on fileBufferReturnIn go back on dataReturnOut");
    REQUIREMENT("SAMD21-ROUTER-004");
    Samd21::FprimeRouterTester tester;
    tester.testBufferReturn();
}

TEST(Nominal, testCommandResponse) {
    COMMENT("Command responses are accepted and ignored");
    REQUIREMENT("SAMD21-ROUTER-006");
    Samd21::FprimeRouterTester tester;
    tester.testCommandResponse();
}

TEST(Random, testRandomRouting) {
    COMMENT("Random packet types: every buffer is returned exactly once");
    REQUIREMENT("SAMD21-ROUTER-004");
    Samd21::FprimeRouterTester tester;
    tester.testRandomRouting();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}
