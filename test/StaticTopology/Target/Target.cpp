// ======================================================================
// \title  Target.cpp
// \brief  Test harness command and telemetry endpoint of the StaticTopology
// ======================================================================

#include "test/StaticTopology/Target/Target.hpp"
#include "Fw/Types/Assert.hpp"

namespace StaticTopology {

constexpr FwOpcodeType Target::LOCAL_OPCODE_SET_VALUE;
constexpr FwOpcodeType Target::LOCAL_OPCODE_RESPOND;
constexpr FwOpcodeType Target::LOCAL_OPCODE_HOLD;
constexpr FwSizeType Target::MAX_HELD;

Target::Target(const char* const compName)
    : TargetComponentBase(compName), m_value(0), m_setCount(0), m_counter(0), m_held(), m_heldCount(0) {}

Target::~Target() {}

U32 Target::getValue() const {
    return this->m_value;
}

U32 Target::getSetCount() const {
    return this->m_setCount;
}

FwSizeType Target::getHeldCount() const {
    return this->m_heldCount;
}

void Target::release(Fw::CmdResponse response) {
    FW_ASSERT(this->m_heldCount > 0);
    const Held held = this->m_held[0];
    for (FwSizeType i = 1; i < this->m_heldCount; i++) {
        this->m_held[i - 1] = this->m_held[i];
    }
    this->m_heldCount--;
    this->cmdResponse_out(held.opCode, held.cmdSeq, response);
}

void Target::respondUnexpected(U32 cmdSeq) {
    this->cmdResponse_out(this->getIdBase() + LOCAL_OPCODE_HOLD, cmdSeq, Fw::CmdResponse::OK);
}

void Target::writeChannels(U8 counter,
                           bool flag,
                           const StaticTopology::Mode& mode,
                           F32 ratio,
                           I64 big,
                           const StaticTopology::Pair& pair,
                           const StaticTopology::Triple& triple,
                           U16 hidden) {
    this->tlmWrite_Counter(counter);
    this->tlmWrite_Flag(flag);
    this->tlmWrite_Mode(mode);
    this->tlmWrite_Ratio(ratio);
    this->tlmWrite_Big(big);
    this->tlmWrite_Pair(pair);
    this->tlmWrite_Triple(triple);
    this->tlmWrite_Hidden(hidden);
}

void Target::SET_VALUE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 value) {
    this->m_value = value;
    this->m_setCount++;
    this->m_counter++;
    this->tlmWrite_Value(value);
    this->tlmWrite_Counter(this->m_counter);
    this->log_ACTIVITY_LO_ValueSet(value);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Target::RESPOND_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::CmdResponse& response) {
    this->cmdResponse_out(opCode, cmdSeq, response);
}

void Target::HOLD_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    FW_ASSERT(this->m_heldCount < MAX_HELD, static_cast<FwAssertArgType>(this->m_heldCount));
    this->m_held[this->m_heldCount].opCode = opCode;
    this->m_held[this->m_heldCount].cmdSeq = cmdSeq;
    this->m_heldCount++;
}

}  // namespace StaticTopology
