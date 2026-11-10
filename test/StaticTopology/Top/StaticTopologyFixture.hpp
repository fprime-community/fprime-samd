// ======================================================================
// \title  StaticTopologyFixture.hpp
// \brief  gtest fixture setting up the StaticTopology for the Static* components' tests
// ======================================================================

#ifndef StaticTopology_StaticTopologyFixture_HPP
#define StaticTopology_StaticTopologyFixture_HPP

#include "gtest/gtest.h"

#include "Fw/Types/Serializable.hpp"
#include "test/StaticTopology/Top/TopTopologyAc.hpp"

namespace StaticTopology {

//! Sets the topology up once per test suite (the instances are globals) and clears the
//! harness histories before each test
class StaticTopologyFixture : public ::testing::Test {
  public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

  protected:
    void SetUp() override;

    //! Serialize one value big endian, returning its size
    template <typename T>
    static FwSizeType serialize(U8* out, FwSizeType capacity, const T& value) {
        Fw::ExternalSerializeBuffer buffer(out, capacity);
        const Fw::SerializeStatus status = buffer.serializeFrom(value);
        EXPECT_EQ(status, Fw::FW_SERIALIZE_OK);
        return buffer.getSize();
    }

    //! Send a command with one argument from the ground
    template <typename T>
    static void command(FwOpcodeType opCode, const T& arg, U32 context = 0) {
        U8 args[sizeof(T) + 8];
        const FwSizeType size = serialize(args, sizeof(args), arg);
        ground.sendCommand(opCode, args, size, context);
    }

    //! Send a command without arguments from the ground
    static void commandNoArgs(FwOpcodeType opCode, U32 context = 0) { ground.sendCommand(opCode, nullptr, 0, context); }

    //! Global event id of an instance's local event id
    static FwEventIdType eventId(const Fw::PassiveComponentBase& instance, FwEventIdType local) {
        return instance.getIdBase() + local;
    }

  private:
    static TopologyState s_state;
};

}  // namespace StaticTopology

#endif
