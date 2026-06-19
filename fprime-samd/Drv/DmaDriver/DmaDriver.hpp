// ======================================================================
// \title  DmaDriver.hpp
// \author tumbar
// \brief  hpp file for DmaDriver component implementation class
// ======================================================================

#ifndef Samd21_DmaDriver_HPP
#define Samd21_DmaDriver_HPP

#include "fprime-samd/Drv/DmaDriver/DmaChannel.hpp"
#include "fprime-samd/Drv/DmaDriver/DmaDriverComponentAc.hpp"

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

    //! Initialize the DMAC hardware (must be called before first use)
    void init();

    //! Handle DMA interrupt (called from DMAC_Handler)
    void handleInterrupt();

    //! Singleton instance pointer for ISR access (public for extern "C" ISR)
    static DmaDriver* s_instance;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for readWritebackIn
    //!
    //! Read the writeback register on the active DMA transfer for this channel
    Samd21::DmaWriteback readWritebackIn_handler(FwIndexType portNum  //!< The port number
                                                 ) override;

    //! Handler implementation for sendTransactionIn
    //!
    //! Queue a DMA transaction on a channel.
    //! If the channel is idle, the transaction starts immediately.
    //! If the channel is busy, the transaction is appended to the linked descriptor chain.
    void sendTransactionIn_handler(
        FwIndexType portNum,                             //!< The port number
        const Samd21::DmaDriver_TriggerSource& trigger,  //!< DMA controller trigger source
        const Samd21::DmaDriver_TransactionType&
            action,  //!< DMA controller behavior on this channel given memory and a trigger source
        const Samd21::DmaDriver_Priority& priority,  //!< DMA transaction priority
        U32 sourceAddr,                              //!< The source address to move data from
        U32 destAddr,                                //!< The destination address to move data to
        U32 len,                                     //!< Number of bytes to copy from source to destination
        const Samd21::DmaDriver_BeatSize& beatSize,  //!< Size of each beat. Controls the width of each DMA action
        bool incrementSource,  //!< Whether the DMA controller should increment the source pointer after each action
                               //!< signal
        bool incrementDestination,  //!< Whether the DMA controller should increment the destination pointer after each
                                    //!< action signal
        const Samd21::DmaDriver_AddressIncrementStepSize&
            stepSize,  //!< Size to increment address (source/dest when enabled) on each beat
        const Samd21::DmaDriver_StepSelection&
            stepSelection  //!< Determines whether the step size setting is applied to the source or destination address
        ) override;

    //! Handler implementation for suspendIn
    //!
    //! Suspend a DMA channel.
    //! This will trigger suspendIsrOut after the DMA channel has finished transferring its current block
    void suspendIn_handler(FwIndexType portNum  //!< The port number
                           ) override;

  private:
    DmaChannel m_channels[DmaDriver::NUM_SENDTRANSACTIONIN_INPUT_PORTS];
    bool m_initialized{false};
};

}  // namespace Samd21

#endif
