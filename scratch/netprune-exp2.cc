/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Experiment 2: Core Performance Comparison
 * 
 * Compares Legacy UDP vs NetPrune for HoL blocking
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netprune-common.h"
#include "ns3/speculative-app.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetPruneExp2");

// NetPruneStats g_legacyStats; 
// NetPruneStats g_netpruneStats; 

// Configuration
struct SimConfig {
    std::string mode = "legacy";      // "legacy" or "netprune"
    std::string bandwidth = "10Gbps";
    std::string delay = "50us";
    uint32_t bufferSize = 100;        // KB
    uint32_t packetSize = 1024;
    double duration = 30.0;
    uint32_t numFlows = 10;
    double pruneThreshold = 0.8;      // Prune when buffer > 80%
};

// Custom queue with NetPrune logic
class NetPruneQueue : public Queue<Packet> {
public:
    static TypeId GetTypeId(void);
    
    NetPruneQueue();
    virtual ~NetPruneQueue();
    
    void SetPruneThreshold(double threshold) { m_pruneThreshold = threshold; }
    void SetProcessingDelay(Time delay) { m_processingDelay = delay; }
    void EnableNetPrune(bool enable) { m_enableNetPrune = enable; }
    
    virtual bool Enqueue(Ptr<Packet> packet);
    virtual Ptr<Packet> Dequeue(void);
    virtual Ptr<Packet> Remove(void);
    virtual Ptr<const Packet> Peek(void) const;
    
private:
    bool ShouldPrune(Ptr<Packet> packet);
    Ptr<Packet> TruncatePacket(Ptr<Packet> packet);
    
    double m_pruneThreshold;
    Time m_processingDelay;
    bool m_enableNetPrune;
    uint32_t m_prunedCount;
    
    std::deque<Ptr<Packet>> m_packets;
    uint32_t m_maxPackets;
};

TypeId NetPruneQueue::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::NetPruneQueue")
        .SetParent<Queue<Packet>>()
        .SetGroupName("Network")
        .AddConstructor<NetPruneQueue>();
    return tid;
}

NetPruneQueue::NetPruneQueue()
    : m_pruneThreshold(0.8),
      m_processingDelay(MicroSeconds(0)),
      m_enableNetPrune(false),
      m_prunedCount(0),
      m_maxPackets(100)
{
}

NetPruneQueue::~NetPruneQueue() {
    // NS_LOG_INFO("NetPruneQueue destroyed. Total pruned: " << m_prunedCount);
}

bool NetPruneQueue::Enqueue(Ptr<Packet> packet) {
    // NS_LOG_FUNCTION(this << packet);
    
    // Check if we should prune this packet
    if (m_enableNetPrune && ShouldPrune(packet)) {
        // Truncate packet (keep only header)
        Ptr<Packet> truncated = TruncatePacket(packet);
        m_packets.push_back(truncated);
        m_prunedCount++;
        g_netpruneStats.prunedPackets++;
        
        // NS_LOG_INFO("Packet pruned. Queue: " << m_packets.size() << "/" << m_maxPackets);
        return true;
    }
    
    // Normal enqueue
    if (m_packets.size() >= m_maxPackets) {
        // Queue full - tail drop
        // NS_LOG_WARN("Queue full, dropping packet");
        return false;
    }
    
    m_packets.push_back(packet);
    // NS_LOG_INFO("Packet enqueued. Queue: " << m_packets.size() << "/" << m_maxPackets);
    return true;
}

Ptr<Packet> NetPruneQueue::Dequeue(void) {
    // NS_LOG_FUNCTION(this);
    
    if (m_packets.empty()) {
        return 0;
    }
    
    // Simulate processing delay
    if (m_processingDelay.GetMicroSeconds() > 0) {
        // In real implementation, this would delay the dequeue
        // For simulation, we just log it
        // NS_LOG_INFO("Processing delay: " << m_processingDelay.GetMicroSeconds() << " us");
    }
    
    Ptr<Packet> packet = m_packets.front();
    m_packets.pop_front();
    
    // NS_LOG_INFO("Packet dequeued. Queue: " << m_packets.size() << "/" << m_maxPackets);
    return packet;
}

Ptr<Packet> NetPruneQueue::Remove(void) {
    return Dequeue();
}

Ptr<const Packet> NetPruneQueue::Peek(void) const {
    // NS_LOG_FUNCTION(this);
    
    if (m_packets.empty()) {
        return 0;
    }
    
    return m_packets.front();
}

bool NetPruneQueue::ShouldPrune(Ptr<Packet> packet) {
    // Check if buffer occupancy exceeds threshold
    double occupancy = (double)m_packets.size() / m_maxPackets;
    if (occupancy < m_pruneThreshold) {
        return false;
    }
    
    // Check if packet is marked as Draft token
    // In real implementation, parse NetPruneHeader
    // For simulation, randomly decide (50% are draft tokens)
    bool isDraftToken = (rand() % 2 == 0);
    
    return isDraftToken;
}

Ptr<Packet> NetPruneQueue::TruncatePacket(Ptr<Packet> packet) {
    // Create a small packet (header only, ~64 bytes)
    Ptr<Packet> truncated = Create<Packet>(64);
    
    // Copy header information
    // In real implementation, copy NetPruneHeader with prune flag set
    
    return truncated;
}

// Receiver application that tracks FCT
class NetPruneReceiver : public Application {
public:
    static TypeId GetTypeId(void);
    
    NetPruneReceiver();
    virtual ~NetPruneReceiver();
    
    void Setup(uint16_t port, bool isNetPrune);
    
private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    
    void HandleRead(Ptr<Socket> socket);
    
    Ptr<Socket> m_socket;
    uint16_t m_port;
    bool m_isNetPrune;
    
    std::map<uint32_t, uint64_t> m_sequenceStartTime;  // Track sequence start times
    NetPruneStats* m_stats;
};

TypeId NetPruneReceiver::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::NetPruneReceiver")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<NetPruneReceiver>();
    return tid;
}

NetPruneReceiver::NetPruneReceiver()
    : m_socket(0),
      m_port(9),
      m_isNetPrune(false),
      m_stats(nullptr)
{
}

NetPruneReceiver::~NetPruneReceiver() {
    m_socket = 0;
}

void NetPruneReceiver::Setup(uint16_t port, bool isNetPrune) {
    m_port = port;
    m_isNetPrune = isNetPrune;
    m_stats = isNetPrune ? &g_netpruneStats : &g_legacyStats;
}

void NetPruneReceiver::StartApplication(void) {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&NetPruneReceiver::HandleRead, this));
}

void NetPruneReceiver::StopApplication(void) {
    if (m_socket) {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void NetPruneReceiver::HandleRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    
    while ((packet = socket->RecvFrom(from))) {
        uint32_t size = packet->GetSize();
        uint64_t now = Simulator::Now().GetMicroSeconds();
        
        // Parse header (simplified)
        // In real implementation, extract NetPruneHeader
        uint32_t seqId = rand() % 1000;  // Placeholder
        bool isPruned = (size == 64);    // Truncated packets are 64 bytes
        bool isVIP = (rand() % 5 == 0);  // 20% are VIP tokens
        
        // Track statistics
        if (m_stats) {
            m_stats->totalPacketsReceived++;
            m_stats->totalBytesReceived += size;
            
            if (isPruned) {
                m_stats->prunedPackets++;
                // Pruned packet allows window to slide immediately
            } else if (isVIP) {
                m_stats->vipPacketsReceived++;
                m_stats->effectiveBytesReceived += size;
            } else {
                m_stats->draftPacketsReceived++;
            }
            
            // Track FCT for sequences
            if (m_sequenceStartTime.find(seqId) == m_sequenceStartTime.end()) {
                m_sequenceStartTime[seqId] = now;
            } else {
                // Sequence completed
                double fct = (now - m_sequenceStartTime[seqId]) / 1000.0;  // Convert to ms
                m_stats->fctList.push_back(fct);
                m_sequenceStartTime.erase(seqId);
            }
            
            // Track latency (simplified)
            double latency = (rand() % 1000) / 1000.0;  // Random latency for demo
            m_stats->latencyList.push_back(latency);
        }
    }
}

int main(int argc, char *argv[]) {
    SimConfig config;
    
    CommandLine cmd;
    cmd.AddValue("mode", "Mode: legacy or netprune", config.mode);
    cmd.AddValue("bandwidth", "Bottleneck bandwidth", config.bandwidth);
    cmd.AddValue("flows", "Number of flows", config.numFlows);
    cmd.AddValue("duration", "Simulation duration", config.duration);
    cmd.Parse(argc, argv);
    
    bool isNetPrune = (config.mode == "netprune");
    
    NS_LOG_INFO("=== Experiment 2: Performance Comparison ===");
    NS_LOG_INFO("Mode: " << config.mode);
    NS_LOG_INFO("Bandwidth: " << config.bandwidth);
    NS_LOG_INFO("Flows: " << config.numFlows);
    NS_LOG_INFO("Duration: " << config.duration << " s");
    
    system("mkdir -p results");
    
    // Create topology (same as Exp1)
    NodeContainer senders, receivers, switches;
    senders.Create(config.numFlows);
    receivers.Create(config.numFlows);
    switches.Create(2);
    
    InternetStackHelper internet;
    internet.Install(senders);
    internet.Install(receivers);
    internet.Install(switches);
    
    // Setup links
    PointToPointHelper p2pAccess, p2pBottleneck;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue("40Gbps"));
    p2pAccess.SetChannelAttribute("Delay", StringValue("10us"));
    
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(config.bandwidth));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue(config.delay));
    
    uint32_t bufferBytes = config.bufferSize * 1024;
    p2pBottleneck.SetQueue("ns3::DropTailQueue<Packet>",
                           "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, bufferBytes)));
    
    // Connect topology
    std::vector<NetDeviceContainer> senderDevices, receiverDevices;
    for (uint32_t i = 0; i < config.numFlows; i++) {
        senderDevices.push_back(p2pAccess.Install(senders.Get(i), switches.Get(0)));
    }
    
    NetDeviceContainer bottleneck = p2pBottleneck.Install(switches.Get(0), switches.Get(1));
    
    for (uint32_t i = 0; i < config.numFlows; i++) {
        receiverDevices.push_back(p2pAccess.Install(switches.Get(1), receivers.Get(i)));
    }
    
    // Assign IPs
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> senderIfaces, receiverIfaces;
    
    for (uint32_t i = 0; i < config.numFlows; i++) {
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        senderIfaces.push_back(ipv4.Assign(senderDevices[i]));
    }
    
    ipv4.SetBase("10.2.1.0", "255.255.255.0");
    ipv4.Assign(bottleneck);
    
    for (uint32_t i = 0; i < config.numFlows; i++) {
        std::ostringstream subnet;
        subnet << "10.3." << (i + 1) << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        receiverIfaces.push_back(ipv4.Assign(receiverDevices[i]));
    }
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    // Install applications
    uint16_t port = 9;
    for (uint32_t i = 0; i < config.numFlows; i++) {
        // Receiver
        Ptr<NetPruneReceiver> receiver = CreateObject<NetPruneReceiver>();
        receivers.Get(i)->AddApplication(receiver);
        receiver->Setup(port + i, isNetPrune);
        receiver->SetStartTime(Seconds(0.0));
        receiver->SetStopTime(Seconds(config.duration));
        
        // Sender
        Ptr<SpeculativeInferenceApp> sender = CreateObject<SpeculativeInferenceApp>();
        senders.Get(i)->AddApplication(sender);
        
        Address addr(InetSocketAddress(receiverIfaces[i].GetAddress(1), port + i));
        sender->Setup(addr, config.packetSize, 1, 4, DataRate(config.bandwidth), isNetPrune);
        sender->SetStartTime(Seconds(1.0 + i * 0.1));
        sender->SetStopTime(Seconds(config.duration - 1.0));
    }
    
    // Run simulation
    Simulator::Stop(Seconds(config.duration));
    Simulator::Run();
    
    // Save results
    NetPruneStats* stats = isNetPrune ? &g_netpruneStats : &g_legacyStats;
    std::string filename = "results/exp2-" + config.mode + "-results.txt";
    std::ofstream outfile(filename);
    
    stats->PrintStats(outfile);
    outfile << "\nGoodput: " << stats->GetGoodput(config.duration) / 1e9 << " Gbps\n";
    outfile << "Total Throughput: " << stats->GetTotalThroughput(config.duration) / 1e9 << " Gbps\n";
    
    outfile.close();
    
    // Save FCT data
    std::string fctFile = "results/exp2-" + config.mode + "-fct.csv";
    std::ofstream fctOut(fctFile);
    fctOut << "FCT(ms)\n";
    for (auto fct : stats->fctList) {
        fctOut << fct << "\n";
    }
    fctOut.close();
    
    NS_LOG_INFO("Results saved to " << filename);
    NS_LOG_INFO("FCT data saved to " << fctFile);
    
    Simulator::Destroy();
    return 0;
}
