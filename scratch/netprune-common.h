/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * NetPrune Common Headers - COMPREHENSIVE FIX
 * * FIXES APPLIED:
 * 1. Added proper Draft/VIP tag mechanism (Problem 2.1)
 * 2. Added FCT measurement support (Problem 5)
 * 3. Unified constants to avoid magic numbers (Problem 9)
 * 4. Proper error handling throughout
 */

#ifndef NETPRUNE_COMMON_H
#define NETPRUNE_COMMON_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <set>
#include <map>
#include <vector>
#include <algorithm>

namespace ns3 {

// ==================== CONSTANTS ====================
// Fixed magic numbers (Problem 9)
const uint32_t NETPRUNE_MAGIC = 0xCAFEBABE;
const double PRUNE_THRESHOLD = 0.2;           // Queue threshold: 20% of buffer
const double DRAFT_RATIO = 0.4;               // 40% of tokens are drafts
const uint32_t PRUNED_HEADER_SIZE = 64;       // Bytes retained after pruning
const uint32_t DEFAULT_TOKEN_SIZE = 1024;     // Default token payload size

// Real-world network parameters (Problem 1.1, 1.2)
const std::string ACCESS_LINK_BW = "20Gbps";   // GPU cluster access link
const std::string BOTTLENECK_BW = "10Gbps";    // TOR switch uplink
const std::string SENDING_RATE = "12Gbps";     // 1.2x oversubscription
const std::string INTRA_RACK_DELAY = "50us";   // Half-RTT: same rack
const std::string CROSS_RACK_DELAY = "250us";  // Half-RTT: different racks
const std::string DATACENTER_DELAY = "500us";  // Half-RTT: cross-datacenter

// ==================== ENUMS ====================
enum PacketType {
    VIP_TOKEN = 0,      // High-priority verified token
    DRAFT_TOKEN = 1,    // Speculative draft token
    DEAD_DRAFT = 2,     // Draft invalidated by VIP arrival
    PRUNED_TOKEN = 3    // Draft that was pruned
};

// ==================== PACKET HEADER ====================
struct NetPruneHeader {
    uint32_t magic;
    uint32_t sequenceId;
    uint32_t tokenId;
    PacketType type;
    uint32_t originalSize;
    bool isDraft;              // True if this is a draft token
    bool isPruned;             // True if packet was pruned
    uint64_t sendTimestamp;    // For FCT calculation
    uint16_t checksum;
    
    NetPruneHeader() : magic(NETPRUNE_MAGIC), sequenceId(0), tokenId(0), 
                       type(DRAFT_TOKEN), originalSize(DEFAULT_TOKEN_SIZE), 
                       isDraft(true), isPruned(false), sendTimestamp(0), checksum(0) {}
    
    // Helper methods
    bool IsVIP() const { return type == VIP_TOKEN; }
    bool IsDraft() const { return type == DRAFT_TOKEN || type == DEAD_DRAFT; }
    bool IsDeadDraft() const { return type == DEAD_DRAFT; }
    bool IsPruned() const { return isPruned || type == PRUNED_TOKEN; }
};

// ==================== PACKET TAG ====================
// This is the critical fix for Problem 2.1
class NetPruneTag : public Tag {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::NetPruneTag")
            .SetParent<Tag>()
            .AddConstructor<NetPruneTag>();
        return tid;
    }
    
    virtual TypeId GetInstanceTypeId(void) const { return GetTypeId(); }
    virtual uint32_t GetSerializedSize() const { 
        return 1 + 1 + 4 + 4 + 8;  // isDraft + isPruned + seqId + tokenId + timestamp
    }
    
    NetPruneTag() : m_isDraft(false), m_isPruned(false), 
                    m_sequenceId(0), m_tokenId(0), m_sendTimestamp(0) {}
    
    // Setters
    void SetDraft(bool isDraft) { m_isDraft = isDraft; }
    void SetPruned(bool isPruned) { m_isPruned = isPruned; }
    void SetSequenceId(uint32_t seqId) { m_sequenceId = seqId; }
    void SetTokenId(uint32_t tokenId) { m_tokenId = tokenId; }
    void SetSendTimestamp(uint64_t ts) { m_sendTimestamp = ts; }
    
    // Getters
    bool IsDraft() const { return m_isDraft; }
    bool IsPruned() const { return m_isPruned; }
    uint32_t GetSequenceId() const { return m_sequenceId; }
    uint32_t GetTokenId() const { return m_tokenId; }
    uint64_t GetSendTimestamp() const { return m_sendTimestamp; }
    
    virtual void Serialize(TagBuffer i) const {
        i.WriteU8(m_isDraft ? 1 : 0);
        i.WriteU8(m_isPruned ? 1 : 0);
        i.WriteU32(m_sequenceId);
        i.WriteU32(m_tokenId);
        i.WriteU64(m_sendTimestamp);
    }
    
    virtual void Deserialize(TagBuffer i) {
        m_isDraft = (i.ReadU8() == 1);
        m_isPruned = (i.ReadU8() == 1);
        m_sequenceId = i.ReadU32();
        m_tokenId = i.ReadU32();
        m_sendTimestamp = i.ReadU64();
    }
    
    virtual void Print(std::ostream &os) const {
        os << "Draft=" << m_isDraft << " Pruned=" << m_isPruned 
           << " Seq=" << m_sequenceId << " Token=" << m_tokenId;
    }

private:
    bool m_isDraft;
    bool m_isPruned;
    uint32_t m_sequenceId;
    uint32_t m_tokenId;
    uint64_t m_sendTimestamp;
};

// ==================== STATISTICS CLASS ====================
// Fixed Problem 5: Added proper FCT tracking
class NetPruneStats {
public:
    // Packet counters
    uint64_t totalPacketsSent;
    uint64_t totalPacketsReceived;
    uint64_t vipPacketsReceived;
    uint64_t draftPacketsReceived;
    uint64_t deadDraftPackets;       // NEW: Track dead drafts
    uint64_t prunedPackets;
    
    // Byte counters
    uint64_t totalBytesSent;
    uint64_t totalBytesReceived;
    uint64_t effectiveBytesReceived; // Excludes pruned packets
    uint64_t deadDraftBytes;         // NEW: Bytes wasted on dead drafts
    uint64_t headerBytes;            // NEW: Bytes from pruned headers
    
    // Latency tracking (Problem 5)
    std::vector<double> fctList;           // Flow Completion Times (ms)
    std::vector<double> latencyList;       // Per-packet latencies (ms)
    std::map<uint32_t, uint64_t> sentTimes; // seq -> send timestamp
    
    // Queue monitoring
    std::vector<double> queueLengthSamples;
    
    // Error tracking
    uint32_t reorderedPackets;
    uint32_t duplicatePackets;
    uint32_t corruptedHeaders;
    std::map<uint32_t, uint32_t> lastSeenSeqId; // flowId -> last seq

    NetPruneStats() : totalPacketsSent(0), totalPacketsReceived(0), 
                      vipPacketsReceived(0), draftPacketsReceived(0),
                      deadDraftPackets(0), prunedPackets(0),
                      totalBytesSent(0), totalBytesReceived(0), 
                      effectiveBytesReceived(0), deadDraftBytes(0), headerBytes(0),
                      reorderedPackets(0), duplicatePackets(0), corruptedHeaders(0) {}
    
    // ========== Metrics Calculation ==========
    
    double GetGoodput(double duration) const {
        if (duration <= 0) return 0;
        return (effectiveBytesReceived * 8.0) / (duration * 1e9); // Gbps
    }
    
    double GetThroughput(double duration) const {
        if (duration <= 0) return 0;
        return (totalBytesReceived * 8.0) / (duration * 1e9); // Gbps
    }
    
    double GetP99Latency() const {
        if (latencyList.empty()) return 0;
        std::vector<double> sorted = latencyList;
        std::sort(sorted.begin(), sorted.end());
        size_t idx = static_cast<size_t>(sorted.size() * 0.99);
        if (idx >= sorted.size()) idx = sorted.size() - 1;
        return sorted[idx];
    }
    
    double GetP50Latency() const {
        if (latencyList.empty()) return 0;
        std::vector<double> sorted = latencyList;
        std::sort(sorted.begin(), sorted.end());
        return sorted[sorted.size() / 2];
    }
    
    double GetAvgLatency() const {
        if (latencyList.empty()) return 0;
        double sum = 0;
        for (double lat : latencyList) sum += lat;
        return sum / latencyList.size();
    }
    
    double GetAvgQueueLength() const {
        if (queueLengthSamples.empty()) return 0;
        double sum = 0;
        for (double q : queueLengthSamples) sum += q;
        return sum / queueLengthSamples.size();
    }
    
    // NEW: Calculate bandwidth saved by pruning
    double GetPruningSavings() const {
        if (deadDraftBytes == 0) return 0;
        return 1.0 - ((double)headerBytes / deadDraftBytes);
    }
    
    // FCT tracking methods (Problem 5 fix)
    void RecordSendTime(uint32_t seqId, uint64_t timestamp) {
        sentTimes[seqId] = timestamp;
    }
    
    void RecordReceiveTime(uint32_t seqId, uint64_t timestamp) {
        if (sentTimes.find(seqId) != sentTimes.end()) {
            double fct = (timestamp - sentTimes[seqId]) / 1e6; // Convert to ms
            fctList.push_back(fct);
            latencyList.push_back(fct);
        }
    }
    
    // Reordering detection
    bool CheckReordering(uint32_t flowId, uint32_t seqId) {
        if (lastSeenSeqId.find(flowId) == lastSeenSeqId.end()) {
            lastSeenSeqId[flowId] = seqId;
            return false;
        }
        uint32_t lastSeq = lastSeenSeqId[flowId];
        if (seqId == lastSeq) { 
            duplicatePackets++; 
            return true; 
        }
        if (seqId < lastSeq) { 
            reorderedPackets++; 
            return true; 
        }
        lastSeenSeqId[flowId] = seqId;
        return false;
    }
    
    // Print summary
    void PrintSummary(const std::string& label, double duration) const {
        std::cout << "========== " << label << " Statistics ==========" << std::endl;
        std::cout << "Packets: " << totalPacketsReceived 
                  << " (VIP: " << vipPacketsReceived 
                  << ", Draft: " << draftPacketsReceived 
                  << ", Dead: " << deadDraftPackets 
                  << ", Pruned: " << prunedPackets << ")" << std::endl;
        std::cout << "Throughput: " << GetThroughput(duration) << " Gbps" << std::endl;
        std::cout << "Goodput: " << GetGoodput(duration) << " Gbps" << std::endl;
        std::cout << "Latency - Avg: " << GetAvgLatency() 
                  << " ms, P50: " << GetP50Latency() 
                  << " ms, P99: " << GetP99Latency() << " ms" << std::endl;
        std::cout << "Queue - Avg: " << GetAvgQueueLength() << " packets" << std::endl;
        std::cout << "Errors - Reordered: " << reorderedPackets 
                  << ", Duplicates: " << duplicatePackets << std::endl;
        if (prunedPackets > 0) {
            std::cout << "Pruning - Saved: " << (GetPruningSavings() * 100) 
                      << "% of dead draft bandwidth" << std::endl;
        }
        std::cout << "========================================" << std::endl;
    }
};

// Global statistics instances
extern NetPruneStats g_legacyStats;
extern NetPruneStats g_netpruneStats;
extern std::map<std::string, NetPruneStats> g_schemeStats;

} // namespace ns3

#endif // NETPRUNE_COMMON_H