// ======================================================================
// \title Samd21/Drv/RtcDriver/RawTime.hpp
// \brief Realtime clock implementation for Os::RawTime
// ======================================================================
#ifndef OS_DRV_RTC_RAWTIME_HPP
#define OS_DRV_RTC_RAWTIME_HPP

#include "Os/RawTime.hpp"

namespace Os {
namespace Samd21 {

struct Samd21RawTimeHandle : public RawTimeHandle {
    U32 m_seconds = 0;
    U32 m_micros = 0;
};

//! Get the current raw time handle by reading the realtime clock and tick counter
Samd21RawTimeHandle timeNow();

//! \brief stub implementation of Os::RawTime
//!
//! Stub implementation of `RawTimeInterface`.
//!
class Samd21RawTime : public RawTimeInterface {
  public:
    //! \brief constructor
    //!
    Samd21RawTime() = default;

    //! \brief destructor
    //!
    ~Samd21RawTime() override = default;

    //! \brief return the underlying RawTime handle (implementation specific)
    //! \return internal RawTime handle representation
    RawTimeHandle* getHandle() override;

    // ------------------------------------------------------------
    // Implementation-specific RawTime overrides
    // ------------------------------------------------------------
    //! \brief Get the current time.
    //!
    //! This function retrieves the current time and stores it in the RawTime object.
    //! Each implementation should define its RawTimeHandle type for storing the time.
    //!
    //! \return Status indicating the result of the operation.
    Status now() override;

    //! \brief Calculate the time interval between this and another raw time.
    //!
    //! This function calculates the time interval between the current raw time and another
    //! specified raw time. The result is stored in the provided (output) interval object.
    //!
    //! \param other The other RawTimeHandle to compare against.
    //! \param interval Output parameter to store the calculated time interval.
    //! \return Status indicating the result of the operation.
    Status getTimeInterval(const Os::RawTime& other, Fw::TimeInterval& interval) const override;

    //! \brief Serialize the contents of the RawTimeInterface object into a buffer.
    //!
    //! This function serializes the contents of the RawTimeInterface object into the provided
    //! buffer.
    //!
    //! \note The serialization must fit within `FW_RAW_TIME_SERIALIZATION_MAX_SIZE` bytes. This value is
    //! defined in FpConfig.h. For example, Posix systems use a pair of U32 (sec, nanosec) and can therefore
    //! serialize in 8 bytes. Should an OSAL implementation require more than this, the project must increase
    //! that value in its config/ folder.
    //!
    //! \param buffer The buffer to serialize the contents into.
    //! \param mode Endianness to use when serializing to buffer.
    //! \return Fw::SerializeStatus indicating the result of the serialization.
    Fw::SerializeStatus serializeTo(Fw::SerialBufferBase& buffer,
                                    Fw::Endianness mode = Fw::Endianness::BIG) const override;

    //! \brief Deserialize the contents of the RawTimeInterface object from a buffer.
    //!
    //! This function deserializes the contents of the RawTimeInterface object from the provided
    //! buffer.
    //!
    //! \note The serialization must fit within `FW_RAW_TIME_SERIALIZATION_MAX_SIZE` bytes. This value is
    //! defined in FpConfig.h. For example, Posix systems use a pair of U32 (sec, nanosec) and can therefore
    //! serialize in 8 bytes. Should an OSAL implementation require more than this, the project must increase
    //! that value in its config/ folder.
    //!
    //! \param buffer The buffer to deserialize the contents from.
    //! \param mode Endianness to use when deserializing from the buffer.
    //! \return Fw::SerializeStatus indicating the result of the deserialization.
    Fw::SerializeStatus deserializeFrom(Fw::SerialBufferBase& buffer,
                                        Fw::Endianness mode = Fw::Endianness::BIG) override;

  private:
    //! Handle for StubRawTime
    Samd21RawTimeHandle m_handle;
};

}  // namespace Samd21
}  // namespace Os
#endif  // OS_STUB_RAWTIME_HPP
