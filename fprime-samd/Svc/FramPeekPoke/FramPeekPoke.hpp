// ======================================================================
// \title  FramPeekPoke.hpp
// \author tumbar
// \brief  hpp file for FramPeekPoke component implementation class
// ======================================================================

#ifndef Samd21_FramPeekPoke_HPP
#define Samd21_FramPeekPoke_HPP

#include "fprime-samd/Svc/FramPeekPoke/FramPeekPokeComponentAc.hpp"

namespace Samd21 {

//! \brief Ground PEEK / POKE of U8, U16 and U32 words in F-RAM
//!
//! One command at a time: a PEEK or POKE is completed from the F-RAM
//! completion, and a second command while one is outstanding is BUSY. Words
//! are big endian.
class FramPeekPoke final : public FramPeekPokeComponentBase {
  public:
    //! Largest word: U32
    static constexpr FwSizeType MAX_WORD_SIZE = 4;

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct FramPeekPoke object
    FramPeekPoke(const char* const compName  //!< The component name
    );

    //! Destroy FramPeekPoke object
    ~FramPeekPoke();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

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

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command PEEK
    //!
    //! Read a word and report it with the Peeked event
    void PEEK_cmdHandler(FwOpcodeType opCode,                //!< The opcode
                         U32 cmdSeq,                         //!< The command sequence number
                         U32 offset,                         //!< F-RAM byte offset of the word
                         const Samd21::FramWordWidth& width  //!< Word width
                         ) override;

    //! Handler implementation for command POKE
    //!
    //! Write a word
    void POKE_cmdHandler(FwOpcodeType opCode,                 //!< The opcode
                         U32 cmdSeq,                          //!< The command sequence number
                         U32 offset,                          //!< F-RAM byte offset of the word
                         const Samd21::FramWordWidth& width,  //!< Word width
                         U32 value                            //!< Value to write; must fit the width
                         ) override;

  private:
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Record the command and send the request; a rejection completes it here
    void start(Fram::Op op, FwOpcodeType opCode, U32 cmdSeq, U32 offset, FramWordWidth width);

    //! Report a failure and complete the command with EXECUTION_ERROR
    void fail(Fram::Status status);

    //! Value of the big-endian word in m_word
    U32 wordValue() const;

    //! Complete the outstanding command
    void respond(Fw::CmdResponse response);

  private:
    // ----------------------------------------------------------------------
    // State
    // ----------------------------------------------------------------------

    bool m_busy;               //!< A command is outstanding
    Fram::Op m_op;             //!< Its direction
    FwOpcodeType m_opCode;     //!< Its opcode
    U32 m_cmdSeq;              //!< Its sequence number
    U32 m_offset;              //!< Its F-RAM offset
    FramWordWidth m_width;     //!< Its word width
    U8 m_word[MAX_WORD_SIZE];  //!< The word, big endian, MSB first
};

}  // namespace Samd21

#endif
