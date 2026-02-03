/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Supp 1: P99 Latency CDF - COMPREHENSIVE FIX (Compilation Error Solved) */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "netprune-common.h"
#include <fstream>
#include <algorithm>
#include <vector>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetPruneSupp1Fixed");

namespace ns3 {
    std::map<std::string, NetPruneStats> g_schemeStats;
}

struct Supp1Config {
    std::string bandwidth = "10Gbps";
    std::string delay = "50us";        
    uint32_t bufferSize = 100;
    uint32_t packetSize = 1024;
    double duration = 2.0;
    uint32_t numFlows = 5;
    std::string sendingRate = "12Gbps"; 
};

// ==================== QUEUE CLASSES ====================
class LegacyQueue : public Queue<Packet> {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::LegacyQueue").SetParent<Queue<Packet>>().AddConstructor<LegacyQueue>();
        return tid;
    }
    LegacyQueue() : m_maxPackets(100) {}
    bool Enqueue(Ptr<Packet> p) override { 
        if (!p || m_packets.size() >= m_maxPackets) return false;
        m_packets.push_back(p); return true;
    }
    Ptr<Packet> Dequeue(void) override { 
        if (m_packets.empty()) return 0;
        Ptr<Packet> p = m_packets.front(); m_packets.pop_front(); return p;
    }
    Ptr<Packet> Remove(void) override { return Dequeue(); }
    Ptr<const Packet> Peek(void) const override { return m_packets.empty() ? 0 : m_packets.front(); }
private:
    std::deque<Ptr<Packet>> m_packets;
    uint32_t m_maxPackets;
};

class REDQueue : public Queue<Packet> {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::REDQueue").SetParent<Queue<Packet>>().AddConstructor<REDQueue>();
        return tid;
    }
    REDQueue() : m_maxPackets(100), m_minTh(30), m_maxTh(70), m_avgQueueSize(0.0), m_qWeight(0.002), m_count(0) {}
    bool Enqueue(Ptr<Packet> p) override {
        if (!p) return false;
        double instantQ = static_cast<double>(m_packets.size());
        m_avgQueueSize = (1.0 - m_qWeight) * m_avgQueueSize + m_qWeight * instantQ;
        bool drop = false;
        if (m_avgQueueSize >= m_minTh) {
            if (m_avgQueueSize >= m_maxTh) drop = true;
            else {
                m_count++;
                double pb = ((m_avgQueueSize - m_minTh) / (m_maxTh - m_minTh)) * 0.1;
                double pa = pb / (1.0 - m_count * pb);
                if ((double)rand() / RAND_MAX < pa) { drop = true; m_count = 0; }
            }
        } else m_count = 0;
        
        if (drop || m_packets.size() >= m_maxPackets) return false;
        m_packets.push_back(p); return true;
    }
    Ptr<Packet> Dequeue(void) override {
        if (m_packets.empty()) return 0;
        Ptr<Packet> p = m_packets.front(); m_packets.pop_front(); return p;
    }
    Ptr<Packet> Remove(void) override { return Dequeue(); }
    Ptr<const Packet> Peek(void) const override { return m_packets.empty() ? 0 : m_packets.front(); }
private:
    std::deque<Ptr<Packet>> m_packets;
    uint32_t m_maxPackets, m_minTh, m_maxTh;
    double m_avgQueueSize, m_qWeight;
    int32_t m_count;
};

class NetPruneQueue : public Queue<Packet> {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::NetPruneQueue").SetParent<Queue<Packet>>().AddConstructor<NetPruneQueue>();
        return tid;
    }
    NetPruneQueue() : m_maxPackets(100) {}
    bool Enqueue(Ptr<Packet> p) override {
        if (!p) return false;
        uint32_t pruneThreshold = static_cast<uint32_t>(m_maxPackets * PRUNE_THRESHOLD);
        if (m_packets.size() > pruneThreshold) {
            NetPruneTag tag;
            if (p->PeekPacketTag(tag) && tag.IsDraft()) {
                Ptr<Packet> pruned = p->CreateFragment(0, PRUNED_HEADER_SIZE);
                NetPruneTag prunedTag;
                prunedTag.SetDraft(true); prunedTag.SetPruned(true); prunedTag.SetSequenceId(tag.GetSequenceId());
                pruned->ReplacePacketTag(prunedTag); 
                m_packets.push_back(pruned); return true;
            }
        }
        if (m_packets.size() >= m_maxPackets) return false;
        m_packets.push_back(p); return true;
    }
    Ptr<Packet> Dequeue(void) override {
        if (m_packets.empty()) return 0;
        Ptr<Packet> p = m_packets.front(); m_packets.pop_front(); return p;
    }
    Ptr<Packet> Remove(void) override { return Dequeue(); }
    Ptr<const Packet> Peek(void) const override { return m_packets.empty() ? 0 : m_packets.front(); }
private:
    std::deque<Ptr<Packet>> m_packets;
    uint32_t m_maxPackets;
};

// ==================== SENDER APPLICATION ====================
class NetPruneSender : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::NetPruneSender").SetParent<Application>().AddConstructor<NetPruneSender>();
        return tid;
    }
    NetPruneSender() : m_socket(0), m_sequenceId(0), m_stats(nullptr), m_stopTime(Seconds(0)) {}
    
    // ✅ Fix: Added stopTime parameter
    void Setup(Ipv4Address dest, uint16_t port, uint32_t pktSize, 
               DataRate rate, NetPruneStats* stats, Time stopTime) {
        m_dest = dest; m_port = port; m_pktSize = pktSize;
        m_rate = rate; m_stats = stats; m_stopTime = stopTime;
    }
    
private:
    virtual void StartApplication(void) {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Bind(); m_socket->Connect(InetSocketAddress(m_dest, m_port));
        ScheduleTx();
    }
    virtual void StopApplication(void) {
        if (m_socket) { m_socket->Close(); m_socket = 0; }
    }
    void ScheduleTx(void) {
        // ✅ Fix: Use m_stopTime
        if (Simulator::Now() < m_stopTime) {
            Time nextTime = Seconds(m_pktSize * 8.0 / m_rate.GetBitRate());
            m_sendEvent = Simulator::Schedule(nextTime, &NetPruneSender::SendPacket, this);
        }
    }
    void SendPacket(void) {
        if (!m_socket) return;
        Ptr<Packet> packet = Create<Packet>(m_pktSize);
        NetPruneTag tag;
        bool isDraft = ((double)rand() / RAND_MAX) < DRAFT_RATIO;
        tag.SetDraft(isDraft); tag.SetSequenceId(m_sequenceId);
        tag.SetSendTimestamp(Simulator::Now().GetNanoSeconds());
        packet->AddPacketTag(tag);
        if (m_stats) {
            m_stats->RecordSendTime(m_sequenceId, Simulator::Now().GetNanoSeconds());
            m_stats->totalPacketsSent++; m_stats->totalBytesSent += m_pktSize;
        }
        m_socket->Send(packet);
        m_sequenceId++;
        ScheduleTx();
    }
    Ptr<Socket> m_socket;
    Ipv4Address m_dest;
    uint16_t m_port;
    uint32_t m_pktSize;
    DataRate m_rate;
    uint32_t m_sequenceId;
    NetPruneStats* m_stats;
    EventId m_sendEvent;
    Time m_stopTime; // ✅ Added
};

// ==================== RECEIVER APPLICATION ====================
class NetPruneReceiver : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::NetPruneReceiver").SetParent<Application>().AddConstructor<NetPruneReceiver>();
        return tid;
    }
    NetPruneReceiver() : m_socket(0), m_stats(nullptr) {}
    void Setup(uint16_t port, NetPruneStats* stats) { m_port = port; m_stats = stats; }
private:
    virtual void StartApplication(void) {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_port));
        m_socket->SetRecvCallback(MakeCallback(&NetPruneReceiver::HandleRead, this));
    }
    virtual void StopApplication(void) { if (m_socket) { m_socket->Close(); m_socket = 0; } }
    void HandleRead(Ptr<Socket> socket) {
        if (!socket || !m_stats) return;
        Ptr<Packet> packet; Address from;
        while ((packet = socket->RecvFrom(from))) {
            if (!packet) break;
            uint32_t pktSize = packet->GetSize();
            m_stats->totalBytesReceived += pktSize;
            NetPruneTag tag;
            if (packet->PeekPacketTag(tag)) {
                m_stats->RecordReceiveTime(tag.GetSequenceId(), Simulator::Now().GetNanoSeconds());
                if (tag.IsPruned()) { m_stats->prunedPackets++; m_stats->headerBytes += pktSize; }
                else if (tag.IsDraft()) { m_stats->draftPacketsReceived++; m_stats->effectiveBytesReceived += pktSize; }
                else { m_stats->vipPacketsReceived++; m_stats->effectiveBytesReceived += pktSize; }
                m_stats->totalPacketsReceived++;
            }
        }
    }
    Ptr<Socket> m_socket;
    uint16_t m_port;
    NetPruneStats* m_stats;
};

void RunScheme(Supp1Config config, std::string scheme) {
    std::cout << ">>> Running Scheme: " << scheme << "..." << std::endl;
    g_schemeStats[scheme] = NetPruneStats();
    NetPruneStats* stats = &g_schemeStats[scheme];
    
    NodeContainer senders, receivers, switches;
    senders.Create(config.numFlows); receivers.Create(config.numFlows); switches.Create(2);
    InternetStackHelper internet; internet.Install(senders); internet.Install(receivers); internet.Install(switches);
    
    PointToPointHelper p2pAccess, p2pBottleneck;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue(ACCESS_LINK_BW));
    p2pAccess.SetChannelAttribute("Delay", StringValue("10us"));
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(config.bandwidth));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue(config.delay));
    
    NetDeviceContainer bottleneck = p2pBottleneck.Install(switches.Get(0), switches.Get(1));
    Ptr<PointToPointNetDevice> bottleneckDev = DynamicCast<PointToPointNetDevice>(bottleneck.Get(0));
    
    if (scheme == "Legacy") bottleneckDev->SetQueue(CreateObject<LegacyQueue>());
    else if (scheme == "RED") bottleneckDev->SetQueue(CreateObject<REDQueue>());
    else if (scheme == "NetPrune") bottleneckDev->SetQueue(CreateObject<NetPruneQueue>());
    
    std::vector<NetDeviceContainer> senderLinks, receiverLinks;
    for (uint32_t i = 0; i < config.numFlows; i++) {
        senderLinks.push_back(p2pAccess.Install(senders.Get(i), switches.Get(0)));
        receiverLinks.push_back(p2pAccess.Install(switches.Get(1), receivers.Get(i)));
    }
    
    Ipv4AddressHelper ipv4; ipv4.SetBase("10.1.1.0", "255.255.255.0"); ipv4.Assign(bottleneck);
    for (uint32_t i = 0; i < config.numFlows; i++) {
        std::stringstream ss; ss << "10.2." << (i + 1) << ".0"; ipv4.SetBase(ss.str().c_str(), "255.255.255.0"); ipv4.Assign(senderLinks[i]);
        ss.str(""); ss << "10.3." << (i + 1) << ".0"; ipv4.SetBase(ss.str().c_str(), "255.255.255.0"); 
        Ipv4InterfaceContainer receiverIf = ipv4.Assign(receiverLinks[i]);
        
        Ptr<NetPruneReceiver> receiver = CreateObject<NetPruneReceiver>();
        receiver->Setup(9, stats); receivers.Get(i)->AddApplication(receiver);
        receiver->SetStartTime(Seconds(0.0)); receiver->SetStopTime(Seconds(config.duration));
        
        Ptr<NetPruneSender> sender = CreateObject<NetPruneSender>();
        // ✅ Fix: Pass stop time
        sender->Setup(receiverIf.GetAddress(1), 9, config.packetSize, DataRate(config.sendingRate), stats, Seconds(config.duration - 0.1));
        senders.Get(i)->AddApplication(sender);
        sender->SetStartTime(Seconds(0.1)); sender->SetStopTime(Seconds(config.duration - 0.1));
    }
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Simulator::Stop(Seconds(config.duration)); Simulator::Run(); Simulator::Destroy();
    stats->PrintSummary(scheme, config.duration);
}

int main(int argc, char *argv[]) {
    Supp1Config config;
    CommandLine cmd; cmd.AddValue("rtt", "Half-RTT delay", config.delay); cmd.Parse(argc, argv);
    system("mkdir -p results");
    std::vector<std::string> schemes = {"Legacy", "RED", "NetPrune"};
    std::cout << "=== NetPrune Supp1: P99 Latency CDF (FIXED) ===" << std::endl;
    std::ofstream csvFile("results/supp1-latency-cdf-FIXED.csv");
    csvFile << "Scheme,Latency_ms,P99_ms\n";
    for (const auto& scheme : schemes) {
        RunScheme(config, scheme);
        NetPruneStats& stats = g_schemeStats[scheme];
        double p99 = stats.GetP99Latency();
        for (double lat : stats.latencyList) csvFile << scheme << "," << lat << "," << p99 << "\n";
    }
    csvFile.close();
    std::cout << "\n✅ DONE! Results saved to results/supp1-latency-cdf-FIXED.csv" << std::endl;
    return 0;
}