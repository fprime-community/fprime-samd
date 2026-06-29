// ======================================================================
// \title  DmaDriver.hpp
// \author tumbar
// \brief  hpp file for DmaDriver component implementation class
// ======================================================================

#ifndef Samd21_DmaDriver_HPP
#define Samd21_DmaDriver_HPP

#include "config/DmaDriverConfig.hpp"
#include "fprime-samd/Drv/DmaDriver/DmaChannel.hpp"
#include "fprime-samd/Drv/DmaDriver/DmaDriverComponentAc.hpp"

namespace Samd21 {

class DmaDriver final : public DmaDriverComponentBase {
    static_assert(DmaDriverConfig::DMA_DESCRIPTOR_N <= 32, "Bit-field used to track DMAC Descriptor usage");

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
    void configure();

    //! Handle DMA interrupt (called from DMAC_Handler)
    void handleInterrupt();

    //! Singleton instance pointer for ISR access (public for extern "C" ISR)
    static DmaDriver* s_instance;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for linkToFrontIn
    //!
    //! Updates the final DmaDescriptor in the chain configured on this channel to link
    //! back to the first. This allows this channel to loop to the first transaction continuously.
    void linkToFrontIn_handler(FwIndexType portNum  //!< The port number
                               ) override;

    //! Handler implementation for popFrontIn
    //!
    //! Suspend the channel, read writeback, and advance to the next descriptor.
    //! Used for IDLE frame detection: process partial buffer and move to alternate.
    Dma::Writeback popFrontIn_handler(FwIndexType portNum  //!< The port number
                                      ) override;

    //! Handler implementation for readWritebackIn
    //!
    //! Read the writeback register on the active DMA transfer for this channel
    Dma::Writeback readWritebackIn_handler(FwIndexType portNum  //!< The port number
                                           ) override;

    //! Handler implementation for sendTransactionIn
    //!
    //! Queue a DMA transaction on a channel.
    //! If the channel is idle, the transaction starts immediately.
    //! If the channel is busy, the transaction is appended to the linked descriptor chain.
    void sendTransactionIn_handler(
        FwIndexType portNum,                //!< The port number
        const Dma::TriggerSource& trigger,  //!< DMA controller trigger source
        const Dma::TransactionType&
            action,                     //!< DMA controller behavior on this channel given memory and a trigger source
        const Dma::Priority& priority,  //!< DMA transaction priority
        U32 sourceAddr,                 //!< The source address to move data from
        U32 destAddr,                   //!< The destination address to move data to
        U32 len,                        //!< Number of bytes to copy from source to destination
        const Dma::BeatSize& beatSize,  //!< Size of each beat. Controls the width of each DMA action
        bool incrementSource,  //!< Whether the DMA controller should increment the source pointer after each action
                               //!< signal
        bool incrementDestination,  //!< Whether the DMA controller should increment the destination pointer after each
                                    //!< action signal
        const Dma::AddressIncrementStepSize&
            stepSize,  //!< Size to increment address (source/dest when enabled) on each beat
        const Dma::StepSelection&
            stepSelection  //!< Determines whether the step size setting is applied to the source or destination address
        ) override;

  private:
    //! Free a descriptor back to the pool
    void freeDescriptor(DmacDescriptor* desc);

    //! Free all descriptors in the chain starting from the given descriptor
    void freeChain(DmacDescriptor* chainStart);

    //! Storage of DMA descriptors for queuing jobs on the DMAC
    DmacDescriptor m_descriptors[DmaDriverConfig::DMA_DESCRIPTOR_N];

    //! Bitfield tracking if the descriptor is in use
    U32 m_descriptors_used;

    DmaChannel m_channels[NUM_SENDTRANSACTIONIN_INPUT_PORTS];

    //! Track the currently executing descriptor for each channel (for incremental freeing)
    //! Points to the descriptor currently loaded in hardware (dmac_base or from m_descriptors)
    DmacDescriptor* m_currentExecutingDesc[NUM_SENDTRANSACTIONIN_INPUT_PORTS];

    bool m_initialized;
};

}  // namespace Samd21

#endif
