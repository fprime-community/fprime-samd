// ======================================================================
// \title  ThinBufferManagerComponentImpl.hpp
// \brief  hpp file for ThinBufferManager component implementation class
//
// Functionally equivalent to Svc::BufferManagerComponentImpl, but tracks
// allocations internally with Samd21::ThinBuffer (12 bytes: data pointer +
// size + context) instead of Fw::Buffer (44 bytes, dominated by the embedded
// Fw::ExternalSerializeBuffer used for (de)serialization, which this
// component never needs). This drops AllocatedBuffer from 56 bytes/slot to
// 24 bytes/slot on this platform/ABI (measured), while keeping the public
// Fw.BufferGet/Fw.BufferSend port boundary identical, so this is a drop-in
// replacement for Svc::BufferManager anywhere buffers are shared. A single
// transient Fw::Buffer is still constructed per call at the port boundary
// (unavoidable -- that's the fixed Fw.BufferGet/Fw.BufferSend port type),
// but it never lives in the persistent per-slot array, so the savings scale
// with the configured buffer count instead of being paid once per call.
//
// ======================================================================

#ifndef Samd21_ThinBufferManager_HPP
#define Samd21_ThinBufferManager_HPP

#include <Fw/Types/MemAllocator.hpp>
#include <fprime-samd/Drv/Types/ThinBuffer.hpp>
#include "fprime-samd/Svc/ThinBufferManager/ThinBufferManagerComponentAc.hpp"
#include "samd-config/ThinBufferManagerConfig.hpp"

namespace Samd21 {

// To use the class, instantiate an instance of the BufferBins struct below. This
// table specifies N buffers of M size per bin. Up to
// ThinBufferManagerConfig::THINBUFFERMGR_MAX_NUM_BINS bins can be specified. The
// table is copied when setup() is called, so it does not need to be retained after
// the call.
//
// The rules for specifying bins:
// 1. For each bin (BufferBins.bins[n]), specify the size of the buffers (bufferSize) in the
//    bin and how many buffers for that bin (numBuffers).
// 2. The bins should be ordered based on an increasing bufferSize to allow ThinBufferManager to
//    search for available buffers. When receiving a request for a buffer, the component will
//    search for the first buffer from the bins that is equal to or greater
//    than the requested size, starting at the beginning of the table.
// 3. Any unused bins should have numBuffers set to 0.
// 4. A single bin can be specified if a single size is needed.
//
// If a buffer is requested that can't be found among available buffers, the call will
//    return an Fw::Buffer with a size of zero. It is expected that the user will notice
//    and have the appropriate response for the design. If an empty buffer is returned to
//    the ThinBufferManager instance, a warning event will be issued but no other action will
//    be taken.
//
// ThinBufferManager will assert under the following conditions:
// 1. A returned buffer has the incorrect manager ID.
// 2. A returned buffer has an incorrect buffer ID.
// 3. A returned buffer is returned with a correct buffer ID, but it isn't already allocated.
// 4. A returned buffer has an indicated size larger than originally allocated.
// 5. A returned buffer has a pointer different than the one originally allocated.
//
// Note that a pointer to the Fw::MemAllocator used in setup() is stored for later memory cleanup.
// The instance of the allocator must persist beyond calling the cleanup() function or the
// destructor of ThinBufferManager if cleanup() is not called. On this platform, a static,
// non-heap allocator such as Samd21::StaticMallocator is preferred over Fw::MallocAllocator.

class ThinBufferManagerComponentImpl final : public ThinBufferManagerComponentBase {
    friend class ThinBufferManagerTester;

  public:
    // ----------------------------------------------------------------------
    // Construction, initialization, and destruction
    // ----------------------------------------------------------------------

    //! Construct object ThinBufferManager
    //!
    ThinBufferManagerComponentImpl(const char* const compName /*!< The component name*/
    );

    // Defines a buffer bin
    struct BufferBin {
        FwSizeType bufferSize;  //!< size of the buffers in this bin. Set to zero for unused bins.
        U16 numBuffers;         //!< number of buffers in this bin. Set to zero for unused bins.
    };

    // Set of bins for the ThinBufferManager
    struct BufferBins {
        BufferBin bins[ThinBufferManagerConfig::THINBUFFERMGR_MAX_NUM_BINS];  //!< set of bins to define buffers
    };

    //! set up configuration

    void setup(U16 mgrID,                    //!< ID of manager for buffer checking
               FwEnumStoreType memID,        //!< Memory segment identifier
               Fw::MemAllocator& allocator,  //!< memory allocator. MUST be persistent for later deallocation.
                                             //!  MUST persist past destructor if cleanup() not called explicitly.
               const BufferBins& bins        //!< Set of user bins
    );

    void cleanup();  // Free memory prior to end of program if desired. Otherwise,
                     // will be deleted in destructor

    //! Destroy object ThinBufferManager
    //!
    ~ThinBufferManagerComponentImpl();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for bufferSendIn
    //!
    void bufferSendIn_handler(const FwIndexType portNum, /*!< The port number*/
                              Fw::Buffer& fwBuffer);

    //! Handler implementation for bufferGetCallee
    //!
    Fw::Buffer bufferGetCallee_handler(const FwIndexType portNum, /*!< The port number*/
                                       Fw::Buffer::SizeType size);

    //! Handler implementation for schedIn
    //!
    void schedIn_handler(const FwIndexType portNum, /*!< The port number*/
                         U32 context                /*!< The call order*/
    );

    bool m_setup;    //!< flag to indicate component has been setup
    bool m_cleaned;  //!< flag to indicate memory has been cleaned up
    U16 m_mgrId;     //!< stored manager ID for buffer checking

    BufferBins m_bufferBins;  //!< copy of bins supplied by user

    struct AllocatedBuffer {
        Samd21::ThinBuffer buff;  //!< Thin buffer descriptor; converted to/from Fw::Buffer only at the port boundary
        U8* memory;               //!< pointer to memory buffer
        FwSizeType size;          //!< size of the buffer
        bool allocated;           //!< this buffer has been allocated
    };

    AllocatedBuffer* m_buffers;     //!< pointer to allocated buffer space
    Fw::MemAllocator* m_allocator;  //!< allocator for memory
    FwEnumStoreType m_memId;        //!< identifier for allocator
    U16 m_numStructs;               //!< number of allocated structs

    // stats
    U32 m_highWater;   //!< high watermark for allocations
    U32 m_currBuffs;   //!< number of currently allocated buffers
    U32 m_noBuffs;     //!< number of failures to allocate a buffer
    U32 m_emptyBuffs;  //!< number of empty buffers returned
};

}  // end namespace Samd21

#endif
