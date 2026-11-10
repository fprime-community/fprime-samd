// ======================================================================
// \title  StaticPrmDb.hpp
// \author tumbar
// \brief  hpp file for StaticPrmDb component implementation class
// ======================================================================

#ifndef Samd21_StaticPrmDb_HPP
#define Samd21_StaticPrmDb_HPP

#include "fprime-samd/Svc/StaticPrmDb/StaticPrmDbComponentAc.hpp"
#include "fprime-samd/Svc/Types/PrmRecord.hpp"
#include "samd-config/StaticPrmDbConfig.hpp"

namespace Samd21 {

class StaticPrmDb final : public StaticPrmDbComponentBase {
  public:
    //! Bytes of the scratch buffer one transfer is packed into or read into
    static constexpr FwSizeType SCRATCH_SIZE = StaticPrmDbConfig::SCRATCH_RECORDS * PrmRecord::SIZE;

    //! Bytes of the persistent store
    static constexpr FwSizeType STORE_SIZE = StaticPrmDbConfig::STORE_RECORDS * PrmRecord::SIZE;

    static_assert(StaticPrmDbConfig::SCRATCH_RECORDS > 0, "StaticPrmDbConfig::SCRATCH_RECORDS must be at least 1");
    static_assert(StaticPrmDbConfig::STORE_RECORDS > 0, "StaticPrmDbConfig::STORE_RECORDS must be at least 1");

    //! What loading one valid persistent record did
    enum class RecordLoad : U8 {
        LOADED,   //!< The record's segment was copied into memory and marked valid
        KEPT,     //!< MERGE: the segment was already valid in memory and was kept
        UNKNOWN,  //!< The deployment has no record with this ID
    };

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
    void LOAD_cmdHandler(FwOpcodeType opCode,              //!< The opcode
                         U32 cmdSeq,                       //!< The command sequence number
                         const StaticPrmDb_LoadMode& mode  //!< How the store combines with memory
                         ) override;

  private:
    // ----------------------------------------------------------------------
    // Auto-generated helpers
    // ----------------------------------------------------------------------

    //! Writes parameter value to RAM given its ID and marks all of its records valid
    static void writeParam(FwPrmIdType id, const Fw::ParamBuffer& val);

    //! Reads the parameter value from RAM given its ID, INVALID unless all of its records are valid
    static Fw::ParamValid readParam(FwPrmIdType id, Fw::ParamBuffer& val);

    //! Load a record (CRC already checked) into RAM. With merge, a segment already valid is kept.
    static RecordLoad loadRecord(const U8* record, bool merge);

    //! Pack record index (0 .. numRecords() - 1, in parameter order) of the deployment.
    //! False, and nothing packed, when its parameter has no valid value.
    static bool saveRecord(FwSizeType index, U8* record);

    //! Clear every validity bit
    static void invalidateAll();

    //! Parameters of the deployment without a valid value
    static U32 countInvalidParams();

    //! Parameters of the deployment
    static U32 numParams();

    //! Records of the deployment
    static FwSizeType numRecords();

    //! Call the deployment level load parameters to push all parameters to their components
    static void loadParameters();

  private:
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Records in a buffer returned by storage, checked to be the scratch buffer sent out
    FwSizeType scratchRecords(const Fw::Buffer& buffer) const;

    //! Run transfers until one completes asynchronously or the command finishes
    void transferLoop();

    //! Continue after a completed transfer
    void transferDone();

    //! Pack the next run of records into the scratch buffer and write it out, or finish the save
    void saveNext();

    //! Read the next run of records into the scratch buffer, or finish the load
    void loadNext();

    //! Merge the records read into the scratch buffer into memory
    void unpackScratch(FwSizeType records);

    //! Report a finished load, push the parameters to their components and complete LOAD
    void finishLoad(Fw::CmdResponse response);

    //! Leave the transfer and complete the command in progress
    void finish(Fw::CmdResponse response);

  private:
    //! Current state of the IO transaction
    StaticPrmDb_IoState::T m_ioState;

    //! Active command opcode in progress
    FwOpcodeType m_opCode;

    //! Active command cmdSeq in progress
    U32 m_cmdSeq;

    //! Mode of the load in progress
    StaticPrmDb_LoadMode m_loadMode;

    //! A transfer is being started by transferLoop
    bool m_inCall;

    //! The transfer started by transferLoop completed inside the call
    bool m_completedInCall;

    //! Store slot the next transfer starts at
    FwSizeType m_slot;

    //! Save: record of the deployment (saveRecord index) packed next
    FwSizeType m_record;

    //! Save: parameters left out because they had no valid value
    U32 m_skippedParams;

    //! Load: records that passed their CRC, failed it, or belong to no parameter of the deployment
    U32 m_validRecords;
    U32 m_corruptRecords;
    U32 m_unknownRecords;

    //! Records of the transfer in flight, packed for or read from the store
    U8 m_scratch[SCRATCH_SIZE];
};

}  // namespace Samd21

#endif
