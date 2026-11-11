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
        LOADED,   //!< The record's segment was copied into memory
        KEPT,     //!< The segment was already loaded (earlier slot) or valid (MERGE), and was kept
        UNKNOWN,  //!< The deployment has no record with this ID
    };

    //! Where a parameter lives in the deployment's generated storage. The generated code holds
    //! one constant per parameter and passes it to the shared helpers below.
    struct ParamLayout {
        U8* value;            //!< Serialized value, size bytes
        FwSizeType size;      //!< Bytes of the value
        U16* check;           //!< Value check, nullptr unless the parameter takes more than one record
        U32* validity;        //!< Validity word holding the parameter's bits
        U32* loaded;          //!< Word of the load in progress holding the parameter's bits
        U32 mask;             //!< The parameter's bits, one per record, in both words
        FwSizeType firstBit;  //!< Bit of the parameter's first record
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

    //! Handler implementation for activeIn
    //!
    //! Acts on the completed transfer (if any), then issues the next one (if any)
    bool activeIn_handler(FwIndexType portNum,  //!< The port number
                          U32 context           //!< The call order
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

    //! Start a load: no record has been loaded into RAM yet
    static void beginLoad();

    //! Load a record (CRC already checked) into RAM unless this load already loaded its segment,
    //! or, with merge, its parameter is already valid. Validity is only updated by commitLoad.
    static RecordLoad loadRecord(const U8* record, bool merge);

    //! End a load (complete: every slot was read) by updating every parameter's validity:
    //!  - none of its records loaded: unchanged, except invalid after a complete REPLACE
    //!  - some but not all of its records loaded: invalid (its value in RAM is partly overwritten)
    //!  - all of its records loaded: valid, unless it fails its value check (mixed saves)
    //! Returns how many failed their value check.
    static U32 commitLoad(bool complete, bool merge);

    //! Pack record index (0 .. numRecords() - 1, in parameter order) of the deployment.
    //! False, and nothing packed, when its parameter has no valid value.
    static bool saveRecord(FwSizeType index, U8* record);

    //! Parameters of the deployment without a valid value
    static U32 countInvalidParams();

    //! Records of the deployment
    static FwSizeType numRecords();

    //! Call the deployment level load parameters to push all parameters to their components
    static void loadParameters();

  private:
    // ----------------------------------------------------------------------
    // Shared helpers of the generated code, one call per parameter or record
    // ----------------------------------------------------------------------

    //! The parameter's value, INVALID unless all of its records are valid
    static Fw::ParamValid readValue(const ParamLayout& layout, Fw::ParamBuffer& val);

    //! Set the parameter's value (and value check) and mark all of its records valid
    static void writeValue(const ParamLayout& layout, const Fw::ParamBuffer& val);

    //! Copy segment of the parameter from a record into memory and mark it loaded, unless this
    //! load already loaded it or, with merge, the parameter is already valid
    static RecordLoad loadSegment(const U8* record, bool merge, const ParamLayout& layout, FwSizeType segment);

    //! Pack segment of the parameter into a record with recordId. False, and nothing packed,
    //! when the parameter has no valid value.
    static bool saveSegment(U8* record, U16 recordId, const ParamLayout& layout, FwSizeType segment);

    //! Set the parameter's validity from the load (see commitLoad). 1 when all of its records
    //! loaded but failed the value check.
    static U32 commitValue(bool complete, bool merge, const ParamLayout& layout);

  private:
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Where the transfer started by the main context stands
    enum class Transfer : U8 {
        NONE,         //!< No transfer: the next one may be issued
        OUTSTANDING,  //!< Issued; storage holds the scratch buffer
        COMPLETED,    //!< Storage returned the scratch buffer; activeIn acts on it
    };

    //! Start a save or load command: storage must be connected
    bool startCommand(FwOpcodeType opCode, U32 cmdSeq, StaticPrmDb_IoState::T state);

    //! Records storage returned in a completion, checked to be the transfer outstanding
    FwSizeType transferReturned(const Fw::Buffer& buffer, StaticPrmDb_IoState::T state) const;

    //! Act on the completed transfer: advance, or finish the command
    void transferCompleted();

    //! Pack the next run of records into the scratch buffer and write it out
    void saveNext();

    //! Read the next run of records into the scratch buffer
    void loadNext();

    //! Merge the records read into the scratch buffer into memory
    void unpackScratch(FwSizeType records);

    //! Report a finished load, push the parameters to their components and complete LOAD
    void finishLoad(StaticPrmDb_LoadResult result);

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

    //! The transfer in flight
    Transfer m_transfer;

    //! Status and records of the transfer that completed
    Drv::ByteStreamStatus m_transferStatus;
    FwSizeType m_transferRecords;

    //! Store slot the next transfer starts at
    FwSizeType m_slot;

    //! Save: record of the deployment (saveRecord index) packed next
    FwSizeType m_record;

    //! Load: records that passed their CRC, failed it, or belong to no parameter of the deployment
    U32 m_validRecords;
    U32 m_corruptRecords;
    U32 m_unknownRecords;

    //! Records of the transfer in flight, packed for or read from the store
    U8 m_scratch[SCRATCH_SIZE];
};

}  // namespace Samd21

#endif
