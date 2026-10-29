// ======================================================================
// \title  StaticPrmDb.hpp
// \author tumbar
// \brief  hpp file for StaticPrmDb component implementation class
// ======================================================================

#ifndef Samd21_StaticPrmDb_HPP
#define Samd21_StaticPrmDb_HPP

#include "fprime-samd/Svc/StaticPrmDb/StaticPrmDbComponentAc.hpp"

namespace Samd21 {

class StaticPrmDb final : public StaticPrmDbComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct StaticPrmDb object
    StaticPrmDb(const char* const compName  //!< The component name
    );

    //! Destroy StaticPrmDb object
    ~StaticPrmDb();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for getPrm
    //!
    //! Port to get parameter values
    Fw::ParamValid getPrm_handler(FwIndexType portNum,  //!< The port number
                                  FwPrmIdType id,       //!< Parameter ID
                                  Fw::ParamBuffer& val  //!< Buffer containing serialized parameter value.
                                                        //!< Unmodified if param not found.
                                  ) override;

    //! Handler implementation for readReturnIn
    //!
    //! Asynchronous reply for read completion.
    void readReturnIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& buffer,
                              const Drv::ByteStreamStatus& status) override;

    //! Handler implementation for setPrm
    //!
    //! Port to update parameters
    void setPrm_handler(FwIndexType portNum,  //!< The port number
                        FwPrmIdType id,       //!< Parameter ID
                        Fw::ParamBuffer& val  //!< Buffer containing serialized parameter value
                        ) override;

    //! Handler implementation for writeReturnIn
    //!
    //! Asynchronous reply for write completion.
    void writeReturnIn_handler(FwIndexType portNum,  //!< The port number
                               Fw::Buffer& buffer,
                               const Drv::ByteStreamStatus& status) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SAVE
    //!
    //! Save the current parameters to non-volatile memory
    void SAVE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                         U32 cmdSeq            //!< The command sequence number
                         ) override;

    //! Handler implementation for command LOAD
    //!
    //! Load parameter values from non-volatile memory
    void LOAD_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                         U32 cmdSeq            //!< The command sequence number
                         ) override;

  private:
    // ----------------------------------------------------------------------
    // Auto-generated helpers
    // ----------------------------------------------------------------------

    //! Writes parameter value to RAM given its ID
    static void writeParam(FwPrmIdType id, const Fw::ParamBuffer& val);

    //! Reads the parameter value from RAM given its ID
    static Fw::ParamValid readParam(FwPrmIdType id, Fw::ParamBuffer& val);

    //! Validate each record and count the valid/invalid records
    static void validate(U32& validRecords, U32& invalidRecords);

    //! Get a buffer representing the parameter database
    static Fw::Buffer getDatabaseBuffer();

    //! Call the deployment level load parameters to push all parameters to their components
    static void loadParameters();

    //! A statically generated hash for the current dictionary/deployment version
    //! This hash will guard the layout of a deployment's parameters so that mismatched
    //! layouts are detected and the storage read will drop the DB.
    static U32 getDeploymentParameterHash();

  private:
    //! Current state of the IO transaction
    StaticPrmDb_IoState::T m_ioState;

    //! Active command opcode in progress
    FwOpcodeType m_opCode;

    //! Active command cmdSeq in progress
    U32 m_cmdSeq;
};

}  // namespace Samd21

#endif
