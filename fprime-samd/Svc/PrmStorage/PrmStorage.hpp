// ======================================================================
// \title  PrmStorage.hpp
// \author tumbar
// \brief  hpp file for PrmStorage component implementation class
// ======================================================================

#ifndef Samd21_PrmStorage_HPP
#define Samd21_PrmStorage_HPP

#include "fprime-samd/Svc/PrmStorage/PrmStorageComponentAc.hpp"
#include "samd-config/PrmStorageConfig.hpp"

namespace Samd21 {

//! \brief F-RAM backing store of the Samd21::StaticPrmDb image
//!
//! Converts a whole-image save or load into a run of Fram requests at the
//! offset of the selected bank and returns the image to the database when the
//! last one completes. The image is not interpreted: the database detects a
//! missing or corrupted image, and the deployment sequences any fallback
//! between banks with SELECT_BANK. The bank layout is compile-time
//! configuration (PrmStorageConfig.hpp), checked against the image size by the
//! autocoded database (PrmStorageLayout.hpp).
class PrmStorage final : public PrmStorageComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PrmStorage object
    PrmStorage(const char* const compName  //!< The component name
    );

    //! Destroy PrmStorage object
    ~PrmStorage();

  private:
    static_assert(PrmBank::NUM_CONSTANTS == 2, "bankOffset() maps every PrmBank");

    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for writeIn
    //!
    //! Write the database image to the selected bank
    void writeIn_handler(FwIndexType portNum,  //!< The port number
                         Fw::Buffer& fwBuffer  //!< The buffer
                         ) override;

    //! Handler implementation for readIn
    //!
    //! Read the selected bank into the database image
    void readIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& fwBuffer  //!< The buffer
                        ) override;

    //! Handler implementation for framReadComplete
    //!
    //! Completion of an accepted framRead
    void framReadComplete_handler(FwIndexType portNum,        //!< The port number
                                  U32 offset,                 //!< Byte offset of the request
                                  Fw::Buffer& buffer,         //!< The buffer given with the request
                                  const Fram::Status& status  //!< OK or SPI_ERROR
                                  ) override;

    //! Handler implementation for framWriteComplete
    //!
    //! Completion of an accepted framWrite
    void framWriteComplete_handler(FwIndexType portNum,        //!< The port number
                                   U32 offset,                 //!< Byte offset of the request
                                   Fw::Buffer& buffer,         //!< The buffer given with the request
                                   const Fram::Status& status  //!< OK or SPI_ERROR
                                   ) override;

    //! Handler implementation for schedIn
    //!
    //! Retries an F-RAM request that was rejected BUSY
    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SELECT_BANK
    //!
    //! Point the following saves and loads at a bank
    void SELECT_BANK_cmdHandler(FwOpcodeType opCode,         //!< The opcode
                                U32 cmdSeq,                  //!< The command sequence number
                                const Samd21::PrmBank& bank  //!< Bank of the following saves and loads
                                ) override;

  private:
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Start a save or load of the image at the selected bank
    void start(Fram::Op op, Fw::Buffer& image);

    //! Issue the request for the next chunk, or finish the transfer
    void nextChunk();

    //! Act on the completion of the outstanding request
    void complete(Fram::Op op, Fram::Status status);

    //! Send the current chunk's request, retrying a BUSY rejection on schedIn
    void send();

    //! Report a failed request and end the transfer
    void fail(Fram::Status status);

    //! End the transfer and return the image to the database
    void finish(Drv::ByteStreamStatus status);

    //! F-RAM offset of the selected bank
    U32 bankOffset() const;

  private:
    // ----------------------------------------------------------------------
    // State
    // ----------------------------------------------------------------------

    PrmBank m_bank;  //!< Bank of the next transfer

    bool m_busy;          //!< A transfer is in progress
    bool m_retryPending;  //!< The request was rejected BUSY; schedIn retries it
    Fram::Op m_op;        //!< Direction of the transfer
    Fw::Buffer m_image;   //!< The database image handed in by StaticPrmDb
    FwSizeType m_cursor;  //!< Image bytes done
    Fw::Buffer m_chunk;   //!< View of the image for the outstanding request
    U32 m_retries;        //!< BUSY rejections of the request
};

}  // namespace Samd21

#endif
