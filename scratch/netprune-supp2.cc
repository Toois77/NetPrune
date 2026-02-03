/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Supp 2: Queue Depth Dynamics - COMPREHENSIVE FIX (Compilation Error Solved) */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "netprune-common.h"
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetPruneSupp2Fixed");

struct QueueSample { double time; uint32_t depth; };
std::vector<QueueSample> g_legacySamples;
std::vector<QueueSample> g_netpruneSamples;

struct Supp2Config {
    std::string accessBW = ACCESS_LINK_BW;      
    std::string bottleneckBW = BOTTLENECK_BW;   
    std::string sendingRate = SENDING_RATE;     
    std::string delay = "50us";
    uint32_t bufferSize = 100;
    double duration = 5.0;      
    double startTime = 1.0;
    double stopTime = 4.0;
    uint32_t sampleInterval = 20; 
};

class MonitoredQueue : public Queue<Packet> {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::MonitoredQueue").SetParent<Queue<Packet>>().AddConstructor<MonitoredQueue>();
        return tid;
    }
    MonitoredQueue() : m_maxPackets(100), m_mode("Legacy"), m_samples(nullptr), m_packetCount(0) {
        Simulator::Schedule(MicroSeconds(20), &MonitoredQueue::SampleQueue, this);
    }
    void Setup(std::string mode, std::vector<QueueSample>* samples) { m_mode = mode; m_samples = samples; }
    bool Enqueue(Ptr<Packet> p) override {
        if (!p) return false;
        m_packetCount++;
        uint32_t pruneThreshold = static_cast<uint32_t>(m_maxPackets * PRUNE_THRESHOLD);
        if (m_mode == "NetPrune" && m_packets.size() > pruneThreshold) {
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
    void SampleQueue() {
        if (m_samples) {
            QueueSample sample; sample.time = Simulator::Now().GetSeconds();
            sample.depth = static_cast<uint32_t>(m_packets.size());
            m_samples->push_back(sample);
        }
        Simulator::Schedule(MicroSeconds(20), &MonitoredQueue::SampleQueue, this);
    }
private:
    std::deque<Ptr<Packet>> m_packets;
    uint32_t m_maxPackets;
    std::string m_mode;
    std::vector<QueueSample>* m_samples;
    uint32_t m_packetCount;
};

// ==================== SENDER APPLICATION ====================
class Supp2Sender : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::Supp2Sender").SetParent<Application>().AddConstructor<Supp2Sender>();
        return tid;
    }
    Supp2Sender() : m_socket(0), m_sequenceId(0), m_stopTime(Seconds(0)) {}
    
    // ✅ Fix: Added stopTime parameter
    void Setup(Ipv4Address dest, uint16_t port, uint32_t pktSize, DataRate rate, Time stopTime) {
        m_dest = dest; m_port = port; m_pktSize = pktSize; m_rate = rate; m_stopTime = stopTime;
    }
    
private:
    virtual void StartApplication(void) {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Bind(); m_socket->Connect(InetSocketAddress(m_dest, m_port));
        ScheduleTx();
    }
    virtual void StopApplication(void) { if (m_socket) { m_socket->Close(); m_socket = 0; } }
    void ScheduleTx(void) {
        // ✅ Fix: Use m_stopTime
        if (Simulator::Now() < m_stopTime) {
            Time nextTime = Seconds(m_pktSize * 8.0 / m_rate.GetBitRate());
            m_sendEvent = Simulator::Schedule(nextTime, &Supp2Sender::SendPacket, this);
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
    EventId m_sendEvent;
    Time m_stopTime; // ✅ Added
};

void RunExperiment(Supp2Config config, std::string mode) {
    std::cout << "\n>>> Running Queue Dynamics Test: " << mode << std::endl;
    NodeContainer sender, receiver, switches;
    sender.Create(1); receiver.Create(1); switches.Create(2);
    InternetStackHelper internet; internet.Install(sender); internet.Install(receiver); internet.Install(switches);
    
    PointToPointHelper p2pAccess, p2pBottleneck;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue(config.accessBW));
    p2pAccess.SetChannelAttribute("Delay", StringValue("1us"));
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(config.bottleneckBW));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue(config.delay));
    
    NetDeviceContainer d_sender_sw0 = p2pAccess.Install(sender.Get(0), switches.Get(0));
    NetDeviceContainer d_sw0_sw1 = p2pBottleneck.Install(switches.Get(0), switches.Get(1));
    NetDeviceContainer d_sw1_receiver = p2pAccess.Install(switches.Get(1), receiver.Get(0));
    
    Ptr<MonitoredQueue> queue = CreateObject<MonitoredQueue>();
    queue->Setup(mode, (mode == "Legacy") ? &g_legacySamples : &g_netpruneSamples);
    DynamicCast<PointToPointNetDevice>(d_sw0_sw1.Get(0))->SetQueue(queue);
    
    Ipv4AddressHelper ipv4; ipv4.SetBase("10.1.1.0", "255.255.255.0"); ipv4.Assign(d_sender_sw0);
    ipv4.SetBase("10.1.2.0", "255.255.255.0"); ipv4.Assign(d_sw0_sw1);
    ipv4.SetBase("10.1.3.0", "255.255.255.0"); Ipv4InterfaceContainer receiverIf = ipv4.Assign(d_sw1_receiver);
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    uint16_t port = 9;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(receiver.Get(0));
    sinkApp.Start(Seconds(0.0)); sinkApp.Stop(Seconds(config.duration));
    
    Ptr<Supp2Sender> senderApp = CreateObject<Supp2Sender>();
    // ✅ Fix: Pass stop time
    senderApp->Setup(receiverIf.GetAddress(1), port, 1024, DataRate(config.sendingRate), Seconds(config.stopTime));
    sender.Get(0)->AddApplication(senderApp);
    senderApp->SetStartTime(Seconds(config.startTime));
    senderApp->SetStopTime(Seconds(config.stopTime));
    
    Simulator::Stop(Seconds(config.duration)); Simulator::Run(); Simulator::Destroy();
    std::cout << "    Completed. Collected " << ((mode == "Legacy") ? g_legacySamples.size() : g_netpruneSamples.size()) << " queue samples." << std::endl;
}

int main(int argc, char *argv[]) {
    Supp2Config config;
    CommandLine cmd; cmd.Parse(argc, argv);
    system("mkdir -p results");
    g_legacySamples.reserve(500000); g_netpruneSamples.reserve(500000);
    RunExperiment(config, "Legacy");
    RunExperiment(config, "NetPrune");
    std::ofstream csvFile("results/supp2-queue-depth-FIXED.csv");
    csvFile << "Time_s,Scheme,QueueDepth_packets\n";
    for (const auto& sample : g_legacySamples) csvFile << sample.time << ",Legacy," << sample.depth << "\n";
    for (const auto& sample : g_netpruneSamples) csvFile << sample.time << ",NetPrune," << sample.depth << "\n";
    csvFile.close();
    std::cout << "\n✅ DONE! Results saved to results/supp2-queue-depth-FIXED.csv" << std::endl;
    return 0;
}