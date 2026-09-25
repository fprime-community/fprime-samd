// ----------------------------------------------------------------------
// ThinBufferManagerTestMain.cpp
// ----------------------------------------------------------------------

#include "ThinBufferManagerTester.hpp"

TEST(Nominal, Setup) {
    Samd21::ThinBufferManagerTester tester;
    tester.testSetup();
}

TEST(Nominal, OneSize) {
    Samd21::ThinBufferManagerTester tester;
    tester.oneBufferSize();
}

TEST(Nominal, MultSize) {
    Samd21::ThinBufferManagerTester tester;
    tester.multBuffSize();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
