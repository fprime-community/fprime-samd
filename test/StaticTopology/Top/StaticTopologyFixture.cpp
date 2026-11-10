// ======================================================================
// \title  StaticTopologyFixture.cpp
// \brief  gtest fixture setting up the StaticTopology for the Static* components' tests
// ======================================================================

#include "test/StaticTopology/Top/StaticTopologyFixture.hpp"
#include "Os/Os.hpp"

namespace StaticTopology {

TopologyState StaticTopologyFixture::s_state;

void StaticTopologyFixture::SetUpTestSuite() {
    Os::init();
    setup(s_state);
}

void StaticTopologyFixture::TearDownTestSuite() {
    teardown(s_state);
}

void StaticTopologyFixture::SetUp() {
    ground.reset();
    sequencer.reset();
    eventLog.reset();
}

}  // namespace StaticTopology
