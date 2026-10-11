#pragma once

#include <SerialTransfer.h>
#include <protocol/JtagProtocol.hpp>

#if defined(ARDUJTAG_DEBUG)
#error "ARDUJTAG_DEBUG text output must not share the SerialTransfer channel"
#endif

// Optional transport adapter: only users of this header need SerialTransfer.
class SerialTransferJtag
{
public:
  explicit SerialTransferJtag(Jtag &jtag) : handler_(jtag) {}
  void begin(Stream &stream) { transfer_.begin(stream, false); }
  void update()
  {
    if (!transfer_.available()) return;
    const size_t size = handler_.handle(transfer_.packet.rxBuff, transfer_.bytesRead,
                                       transfer_.packet.txBuff, MAX_PACKET_SIZE);
    if (size) transfer_.sendData(size, transfer_.currentPacketID());
  }
private:
  SerialTransfer transfer_;
  ArduJtagProtocol::Handler handler_;
};
