// ======================================================================
// \title  I2cDriverTestMain.cpp
// \author tumbar
// \brief  cpp file for I2cDriver component test main function
// ======================================================================

#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"
#include "fprime-samd/Drv/I2cDriver/test/ut/I2cDriverTester.hpp"

// ----------------------------------------------------------------------
// configure()
// ----------------------------------------------------------------------

TEST(Configure, Nominal) {
    COMMENT("configure() programs the SERCOM once with the requested parameters");
    Samd21::I2cDriverTester tester;
    tester.testConfigureNominal();
}

TEST(Configure, AllParameters) {
    COMMENT("configure() forwards every parameter to the HAL");
    Samd21::I2cDriverTester tester;
    tester.testConfigureAllParameters();
}

TEST(Configure, RegistersIsr) {
    COMMENT("configure() registers the component's ISR trampoline");
    Samd21::I2cDriverTester tester;
    tester.testConfigureRegistersIsr();
}

// ----------------------------------------------------------------------
// read()
// ----------------------------------------------------------------------

TEST(Read, Nominal) {
    COMMENT("read() queues an RX DMA transfer and kicks off the peripheral");
    Samd21::I2cDriverTester tester;
    tester.testReadNominal();
}

TEST(Read, Completion) {
    COMMENT("RX DMA completion returns the buffer OK and frees the driver");
    Samd21::I2cDriverTester tester;
    tester.testReadCompletion();
}

TEST(Read, Busy) {
    COMMENT("read() while busy is rejected with OTHER_ERR");
    Samd21::I2cDriverTester tester;
    tester.testReadBusy();
}

// ----------------------------------------------------------------------
// write()
// ----------------------------------------------------------------------

TEST(Write, Nominal) {
    COMMENT("write() queues a TX DMA transfer with an automatic STOP");
    Samd21::I2cDriverTester tester;
    tester.testWriteNominal();
}

TEST(Write, Completion) {
    COMMENT("TX DMA completion returns the buffer OK and frees the driver");
    Samd21::I2cDriverTester tester;
    tester.testWriteCompletion();
}

TEST(Write, Busy) {
    COMMENT("write() while busy is rejected with OTHER_ERR");
    Samd21::I2cDriverTester tester;
    tester.testWriteBusy();
}

// ----------------------------------------------------------------------
// writeRead()
// ----------------------------------------------------------------------

TEST(WriteRead, Nominal) {
    COMMENT("writeRead() queues both DMA transfers and writes without a STOP");
    Samd21::I2cDriverTester tester;
    tester.testWriteReadNominal();
}

TEST(WriteRead, FullSequence) {
    COMMENT("write-read walks write DMA -> master-on-bus -> read DMA -> OK");
    Samd21::I2cDriverTester tester;
    tester.testWriteReadFullSequence();
}

TEST(WriteRead, Busy) {
    COMMENT("writeRead() while busy is rejected with OTHER_ERR");
    Samd21::I2cDriverTester tester;
    tester.testWriteReadBusy();
}

TEST(WriteRead, PointerNack) {
    COMMENT("a client NACK of the register pointer fails the write-read with WRITE_ERR, no read issued");
    Samd21::I2cDriverTester tester;
    tester.testWriteReadPointerNack();
}

TEST(WriteRead, MbAlreadyLatched) {
    COMMENT("MB already latched at write-DMA completion services the handoff inline (no lost edge)");
    Samd21::I2cDriverTester tester;
    tester.testWriteReadMbAlreadyLatched();
}

TEST(WriteRead, MbAlreadyLatchedSpuriousIsr) {
    COMMENT("the stale SERCOM interrupt after an inline handoff is a benign no-op, not an assert");
    Samd21::I2cDriverTester tester;
    tester.testWriteReadMbAlreadyLatchedSpuriousIsr();
}

// ----------------------------------------------------------------------
// ISR error handling
// ----------------------------------------------------------------------

TEST(Isr, ErrorIdleUnexpected) {
    COMMENT("an error interrupt while IDLE logs the error and an unexpected warning");
    Samd21::I2cDriverTester tester;
    tester.testIsrErrorIdleUnexpected();
}

TEST(Isr, ErrorDuringRead) {
    COMMENT("an error during a read aborts the DMA and replies READ_ERR");
    Samd21::I2cDriverTester tester;
    tester.testIsrErrorDuringRead();
}

TEST(Isr, ErrorDuringWrite) {
    COMMENT("an error during a write aborts the DMA and replies WRITE_ERR");
    Samd21::I2cDriverTester tester;
    tester.testIsrErrorDuringWrite();
}

TEST(Isr, ErrorDuringWriteReadWriting) {
    COMMENT("an error while writing a write-read aborts both channels, replies WRITE_ERR");
    Samd21::I2cDriverTester tester;
    tester.testIsrErrorDuringWriteReadWriting();
}

TEST(Isr, ErrorDuringWriteReadReading) {
    COMMENT("an error while reading a write-read aborts the read channel, replies READ_ERR");
    Samd21::I2cDriverTester tester;
    tester.testIsrErrorDuringWriteReadReading();
}

TEST(Isr, AllErrorFlags) {
    COMMENT("every error STATUS bit maps to its own event and increments the counter");
    Samd21::I2cDriverTester tester;
    tester.testIsrAllErrorFlags();
}

TEST(Isr, MasterOnBusUnexpected) {
    COMMENT("a master-on-bus interrupt outside write-read is flagged unexpected");
    Samd21::I2cDriverTester tester;
    tester.testIsrMasterOnBusUnexpected();
}

// ----------------------------------------------------------------------
// dmaReplyIn channel validation
// ----------------------------------------------------------------------

TEST(DmaReply, WrongChannelRead) {
    COMMENT("a WRITE-channel reply during a read is flagged InvalidDmaReply");
    Samd21::I2cDriverTester tester;
    tester.testDmaReplyWrongChannelRead();
}

TEST(DmaReply, WrongChannelWrite) {
    COMMENT("a READ-channel reply during a write is flagged InvalidDmaReply");
    Samd21::I2cDriverTester tester;
    tester.testDmaReplyWrongChannelWrite();
}

TEST(DmaReply, IdleIgnored) {
    COMMENT("a DMA reply while IDLE is silently ignored");
    Samd21::I2cDriverTester tester;
    tester.testDmaReplyIdleIgnored();
}

// ----------------------------------------------------------------------
// Telemetry
// ----------------------------------------------------------------------

TEST(Telemetry, BusStatus) {
    COMMENT("reportTelemetryIn maps the bus status registers to channels");
    Samd21::I2cDriverTester tester;
    tester.testReportTelemetry();
}

TEST(Telemetry, DeviceOnBus) {
    COMMENT("both SB and MB set maps to MASTER_AND_SLAVE_ON_BUS");
    Samd21::I2cDriverTester tester;
    tester.testReportTelemetryDeviceOnBus();
}

// ----------------------------------------------------------------------
// Command
// ----------------------------------------------------------------------

TEST(Command, ClearErrors) {
    COMMENT("CLEAR_ERRORS resets the error counter and responds OK");
    Samd21::I2cDriverTester tester;
    tester.testClearErrors();
}

int main(int argc, char** argv) {
    STest::Random::seed();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
