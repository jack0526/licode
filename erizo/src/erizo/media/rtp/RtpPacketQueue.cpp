#include <cstring>

#include "RtpPacketQueue.h"
#include "../../MediaDefinitions.h"
#include "RtpHeader.h"


namespace erizo{

DEFINE_LOGGER(RtpPacketQueue, "RtpPacketQueue");

RtpPacketQueue::RtpPacketQueue() : poppedData_(false)
{
}

RtpPacketQueue::~RtpPacketQueue(void)
{
    queue_.clear();
}

void RtpPacketQueue::pushPacket(const char *data, int length)
{
    const RTPHeader *currentHeader = reinterpret_cast<const RTPHeader*>(data);
    uint16_t currentSequenceNumber = currentHeader->getSeqNumber();

    if(poppedData_ && (rtpSequenceLessThan(currentSequenceNumber, lastSequenceNumberGiven_) || currentSequenceNumber == lastSequenceNumberGiven_)) {
        // this sequence number is less than the stuff we've already handed out, which means it's too late to be of any value.
        ELOG_WARN("RTPPacketQueue -- discarding very late sample %d", currentSequenceNumber);
        return;
    }

    // TODO this should be a secret of the dataPacket class.  It should maintain its own memory
    // and copy stuff as necessary.
    boost::shared_ptr<dataPacket> packet(new dataPacket());
    memcpy(packet->data, data, length);
    packet->length = length;

    // let's insert this packet where it belongs in the queue.
    std::list<boost::shared_ptr<dataPacket> >::iterator it;
    for (it=queue_.begin(); it != queue_.end(); ++it) {
        const RTPHeader *header = reinterpret_cast<const RTPHeader*>((*it)->data);
        uint16_t sequenceNumber = header->getSeqNumber();

        if (sequenceNumber == currentSequenceNumber) {
            // We already have this sequence number in the queue.
            ELOG_INFO("RTPPacketQueue -- discarding duplicate sample %d", currentSequenceNumber);
            break;
        }

        if (this->rtpSequenceLessThan(sequenceNumber, currentSequenceNumber)) {
            queue_.insert(it, packet);
            break;
        }
    }

    if (it == queue_.end()) {
        // something old, or queue is empty.
        queue_.push_back(packet);
    }

    // Enforce our max queue size.
    while(queue_.size() >= this->MAX_SIZE) {
        ELOG_DEBUG("RtpPacketQueue - Discarding a sample due to hitting MAX_SIZE");
        queue_.pop_back();  // remove oldest samples.
    }
}

// It's a party foul to call this without checking size first
// TODO fix that....lame.
boost::shared_ptr<dataPacket> RtpPacketQueue::popPacket()
{
    boost::shared_ptr<dataPacket> packet = queue_.back();
    queue_.pop_back();

    const RTPHeader *header = reinterpret_cast<const RTPHeader*>(packet->data);
    this->lastSequenceNumberGiven_ = header->getSeqNumber();
    poppedData_ = true;

    return packet;
}

int RtpPacketQueue::getSize(){
    return queue_.size();
}

// Implements x < y, taking into account RTP sequence number wrap
// The general idea is if there's a very large difference between
// x and y, that implies that the larger one is actually "less than"
// the smaller one.
//
// I picked 0x8000 as my "very large" threshold because it splits
// 0xffff, so it seems like a logical choice.
bool RtpPacketQueue::rtpSequenceLessThan(uint16_t x, uint16_t y) {
    int diff = y - x;
    if (diff > 0) {
        return (diff < 0x8000);
    } else if (diff < 0) {
        return (diff < -0x8000);
    } else { // diff == 0
        return false;
    }
}
} /* namespace erizo */
