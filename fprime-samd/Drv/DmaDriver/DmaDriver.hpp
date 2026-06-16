// ======================================================================
// \title  DmaDriver.hpp
// \author tumbar
// \brief  hpp file for DmaDriver component implementation class
// ======================================================================

#ifndef Samd21_DmaDriver_HPP
#define Samd21_DmaDriver_HPP

#include "Samd21/Drv/DmaDriver/DmaChannel.hpp"
#include "Samd21/Drv/DmaDriver/DmaDriverComponentAc.hpp"

namespace Samd21 {

class DmaDriver final : public DmaDriverComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct DmaDriver object
    DmaDriver(const char* const compName  //!< The component name
    );

    //! Destroy DmaDriver object
    ~DmaDriver();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for configure
    //!
    //! Configure the DMA controller on a channel determined by the port number
    //! If this channel was already configured, overwrite the previous
    //! configuration
    void configure_handler(
        FwIndexType portNum,                              //!< The port number
        const Samd21::DmaDriver_TriggerSource& trigger,   //!< DMA controller trigger source
        const Samd21::DmaDriver_TransactionType& action,  //!< DMA controller behavior on this channel given memory
                                                          //!< and a trigger source
        const Samd21::DmaDriver_BeatSize& beatSize,       //!< Size of each beat. Controls the width of each DMA
                                                          //!< action
        const Samd21::DmaDriver_Priority& priority,       //!< DMA transaction priority
        bool incrementSource,                             //!< Whether the DMA controller should increment
                                                          //!< the source pointer after each action signal
        bool incrementDestination,                        //!< Whether the DMA controller should
                                                          //!< increment the destination pointer after
                                                          //!< each action signal
        const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,  //!< Size to increment address (source/dest when
                                                                     //!< enabled) on each beat
        const Samd21::DmaDriver_StepSelection& stepSelection  //!< Determines whether the step size setting is
                                                              //!< applied to the source or destination address
        ) override;

    //! Handler implementation for start
    //!
    //! Start DMA on a configured channel
    //! The channel must already be configured before this is done
    void start_handler(FwIndexType portNum,  //!< The port number
                       U32 sourceAddr,       //!< The source address to move data from
                       U32 destAddr,         //!< The destination address to move data to
                       FwSizeType len        //!< Number of bytes to copy from source to destination
                       ) override;

  private:
    DmaChannel m_channels[DmaDriver::NUM_CONFIGURE_INPUT_PORTS];
};

}  // namespace Samd21

#endif
