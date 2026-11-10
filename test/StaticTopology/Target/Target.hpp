// ======================================================================
// \title  Target.hpp
// \brief  Test harness command and telemetry endpoint of the StaticTopology
// ======================================================================

#ifndef StaticTopology_Target_HPP
#define StaticTopology_Target_HPP

#include "test/StaticTopology/Target/TargetComponentAc.hpp"

namespace StaticTopology {

class Target final : public TargetComponentBase {
  public:
    //! Local opcodes (Target.fpp), public for the tests
    static constexpr FwOpcodeType LOCAL_OPCODE_SET_VALUE = 0;
    static constexpr FwOpcodeType LOCAL_OPCODE_RESPOND = 1;
    static constexpr FwOpcodeType LOCAL_OPCODE_HOLD = 2;

    //! Held commands (HOLD) this instance can track
    static constexpr FwSizeType MAX_HELD = 32;

    explicit Target(const char* const compName);
    ~Target();

    //! Value of the last SET_VALUE
    U32 getValue() const;

    //! SET_VALUE commands received
    U32 getSetCount() const;

    //! HOLD commands not yet released
    FwSizeType getHeldCount() const;

    //! Complete the oldest held command with response
    void release(Fw::CmdResponse response);

    //! Send a response for a command this instance never received
    void respondUnexpected(U32 cmdSeq);

    //! Write every channel
    void writeChannels(U8 counter,
                       bool flag,
                       const StaticTopology::Mode& mode,
                       F32 ratio,
                       I64 big,
                       const StaticTopology::Pair& pair,
                       const StaticTopology::Triple& triple,
                       U16 hidden);

  private:
    void SET_VALUE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 value) override;
    void RESPOND_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::CmdResponse& response) override;
    void HOLD_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    struct Held {
        FwOpcodeType opCode;
        U32 cmdSeq;
    };

    U32 m_value;
    U32 m_setCount;
    U8 m_counter;
    Held m_held[MAX_HELD];
    FwSizeType m_heldCount;
};

}  // namespace StaticTopology

#endif
