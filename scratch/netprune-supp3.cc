/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Supp 3: Bandwidth Breakdown - COMPREHENSIVE FIX (Compilation Error Solved) */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "netprune-common.h"
#include <fstream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetPruneSupp3Fixed");

// ==================== BANDWIDTH STATISTICS ====================
struct BandwidthStats {
    uint64_t vipBytes;           // VIP tokens (useful)
    uint64_t validDraftBytes;    // Valid drafts (useful)
    uint64_t deadDraftBytes;     // Dead drafts full payload (waste in Legacy)
    uint64_t headerBytes;        // Pruned headers (small overhead in NetPrune)
    uint64_t droppedBytes;       // Tail drops due to congestion
    
    uint32_t vipPackets;
    uint32_t validDraftPackets;
    uint32_t deadDraftPackets;
    uint32_t prunedPackets;
    uint32_t droppedPackets;
    
    BandwidthStats() : vipBytes(0), validDraftBytes(0), deadDraftBytes(0),
                       headerBytes(0), droppedBytes(0),
                       vipPackets(0), validDraftPackets(0), 
                       deadDraftPackets(0), prunedPackets(0), droppedPackets(0) {}
};

std::map<std::string, BandwidthStats> g_bwStats;

struct Supp3Config {
    std::string accessBW = ACCESS_LINK_BW;      // 20Gbps
    std::string bottleneckBW = BOTTLENECK_BW;   // 10Gbps
    std::string sendingRate = SENDING_RATE;     // 12Gbps
    uint32_t bufferSize = 100;
    double duration = 3.0;
    double startTime = 0.5;
    double stopTime = 2.5;
};

// ==================== BANDWIDTH TRACKING QUEUE ====================
class BandwidthQueue : public Queue<Packet> {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::BandwidthQueue")
            .SetParent<Queue<Packet>>()
            .AddConstructor<BandwidthQueue>();
        return tid;
    }
    
    BandwidthQueue() : m_maxPackets(100), m_enableNetPrune(false), 
                       m_stats(nullptr), m_packetCount(0) {}
    
    void Setup(std::string mode, BandwidthStats* stats) {
        m_mode = mode;
        m_stats = stats;
    }
    
    void EnableNetPrune(bool enable) {
        m_enableNetPrune = enable;
    }
    
    bool Enqueue(Ptr<Packet> packet) override {
    if (!packet) return false;
    
    uint32_t originalSize = packet->GetSize();
    m_packetCount++;
    
    // 解析Tag确定包类型
    NetPruneTag tag;
    bool hasTag = packet->PeekPacketTag(tag);
    
    bool isVIP = false;
    bool isDraft = false;
    bool isDeadDraft = false;
    
    if (hasTag) {
        isDraft = tag.IsDraft();
        if (isDraft) {
            // 50%的Draft是Dead的
            isDeadDraft = ((double)rand() / RAND_MAX) < 0.5; 
        } else {
            isVIP = true;
        }
    } else {
        // Fallback: 无Tag时按分布生成
        int typeRand = rand() % 10;
        if (typeRand < 2) isVIP = true;           // 20% VIP
        else if (typeRand < 6) {                   // 40% Valid Draft
            isDraft = true; 
            isDeadDraft = false; 
        }
        else {                                     // 40% Dead Draft
            isDraft = true; 
            isDeadDraft = true; 
        }
    }
    
    // ==================== 关键修复 ====================
    // ✅ NetPrune: 无条件剪枝所有Dead Draft
    if (m_enableNetPrune && isDeadDraft) {
        Ptr<Packet> pruned = packet->CreateFragment(0, PRUNED_HEADER_SIZE);
        
        // 检查队列是否满（剪枝后的包也可能被丢弃）
        if (m_packets.size() >= m_maxPackets) {
            if (m_stats) {
                m_stats->droppedBytes += PRUNED_HEADER_SIZE;
                m_stats->droppedPackets++;
            }
            return false;
        }
        
        m_packets.push_back(pruned);
        
        // ✅ 只统计到headerBytes，不统计deadDraftBytes
        if (m_stats) {
            m_stats->headerBytes += PRUNED_HEADER_SIZE;
            m_stats->prunedPackets++;
            // ❌ 删除了 m_stats->deadDraftPackets++
        }
        return true;
    }
    
    // ==================== 正常入队逻辑 ====================
    // 队列满则丢包
    if (m_packets.size() >= m_maxPackets) {
        if (m_stats) {
            m_stats->droppedBytes += originalSize;
            m_stats->droppedPackets++;
        }
        return false;
    }
    
    // 正常入队
    m_packets.push_back(packet);
    
    // 统计带宽（只有成功入队的才算）
    if (m_stats) {
        if (isVIP) {
            m_stats->vipBytes += originalSize;
            m_stats->vipPackets++;
        } else if (isDraft && !isDeadDraft) {
            m_stats->validDraftBytes += originalSize;
            m_stats->validDraftPackets++;
        } else if (isDeadDraft) {
            // ✅ 只有Legacy模式下的Dead Draft才会走到这里
            m_stats->deadDraftBytes += originalSize;
            m_stats->deadDraftPackets++;
        }
    }
    
    return true;
}
    
    Ptr<Packet> Dequeue(void) override {
        if (m_packets.empty()) return 0;
        Ptr<Packet> p = m_packets.front();
        m_packets.pop_front();
        return p;
    }
    
    Ptr<Packet> Remove(void) override { return Dequeue(); }
    Ptr<const Packet> Peek(void) const override {
        return m_packets.empty() ? 0 : m_packets.front();
    }
    
private:
    std::deque<Ptr<Packet>> m_packets;
    uint32_t m_maxPackets;
    bool m_enableNetPrune;
    std::string m_mode;
    BandwidthStats* m_stats;
    uint32_t m_packetCount;
};

// ==================== SENDER APPLICATION ====================
class Supp3Sender : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::Supp3Sender")
            .SetParent<Application>()
            .AddConstructor<Supp3Sender>();
        return tid;
    }
    
    Supp3Sender() : m_socket(0), m_sequenceId(0), m_stopTime(Seconds(0)) {}
    
    void Setup(Ipv4Address dest, uint16_t port, uint32_t pktSize, DataRate rate, Time stopTime) {
        m_dest = dest;
        m_port = port;
        m_pktSize = pktSize;
        m_rate = rate;
        m_stopTime = stopTime;
    }
    
private:
    virtual void StartApplication(void) {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Bind();
        m_socket->Connect(InetSocketAddress(m_dest, m_port));
        ScheduleTx();
    }
    
    virtual void StopApplication(void) {
        if (m_socket) {
            m_socket->Close();
            m_socket = 0;
        }
    }
    
    void ScheduleTx(void) {
        // ✅ Fix: Use local m_stopTime instead of GetStopTime()
        if (Simulator::Now() < m_stopTime) {
            Time nextTime = Seconds(m_pktSize * 8.0 / m_rate.GetBitRate());
            m_sendEvent = Simulator::Schedule(nextTime, &Supp3Sender::SendPacket, this);
        }
    }
    
    void SendPacket(void) {
        if (!m_socket) return; 
        
        Ptr<Packet> packet = Create<Packet>(m_pktSize);
        NetPruneTag tag;
        
        double r = (double)rand() / RAND_MAX;
        if (r < 0.2) tag.SetDraft(false);
        else tag.SetDraft(true);
        
        tag.SetSequenceId(m_sequenceId);
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
    Time m_stopTime; // ✅ Added local stop time tracking
};

// ==================== RUN EXPERIMENT ====================
void RunExperiment(Supp3Config config, std::string mode) {
    std::cout << "\n>>> Running Bandwidth Analysis: " << mode << std::endl;
    
    g_bwStats[mode] = BandwidthStats();
    
    NodeContainer sender, receiver, switches;
    sender.Create(1);
    receiver.Create(1);
    switches.Create(2);
    
    InternetStackHelper internet;
    internet.Install(sender);
    internet.Install(receiver);
    internet.Install(switches);
    
    PointToPointHelper p2pAccess, p2pBottleneck;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue(config.accessBW));
    p2pAccess.SetChannelAttribute("Delay", StringValue("1us"));
    
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(config.bottleneckBW));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue("1us"));
    
    NetDeviceContainer d_sender_sw0 = p2pAccess.Install(sender.Get(0), switches.Get(0));
    NetDeviceContainer d_sw0_sw1 = p2pBottleneck.Install(switches.Get(0), switches.Get(1));
    NetDeviceContainer d_sw1_receiver = p2pAccess.Install(switches.Get(1), receiver.Get(0));
    
    Ptr<BandwidthQueue> queue = CreateObject<BandwidthQueue>();
    queue->Setup(mode, &g_bwStats[mode]);
    queue->EnableNetPrune(mode == "NetPrune");
    DynamicCast<PointToPointNetDevice>(d_sw0_sw1.Get(0))->SetQueue(queue);
    
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(d_sender_sw0);
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ipv4.Assign(d_sw0_sw1);
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer receiverIf = ipv4.Assign(d_sw1_receiver);
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    uint16_t port = 9;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(receiver.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(config.duration));
    
    Ptr<Supp3Sender> senderApp = CreateObject<Supp3Sender>();
    // ✅ Pass stop time to Setup
    senderApp->Setup(receiverIf.GetAddress(1), port, 1024, DataRate(config.sendingRate), Seconds(config.stopTime));
    sender.Get(0)->AddApplication(senderApp);
    senderApp->SetStartTime(Seconds(config.startTime));
    senderApp->SetStopTime(Seconds(config.stopTime));
    
    Simulator::Stop(Seconds(config.duration));
    Simulator::Run();
    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    Supp3Config config;
    CommandLine cmd;
    cmd.Parse(argc, argv);
    
    system("mkdir -p results");
    
    g_bwStats.clear(); // Clear stats before run
    
    RunExperiment(config, "Legacy");
    RunExperiment(config, "NetPrune");
    
    std::ofstream csvFile("results/supp3-bandwidth-breakdown-FIXED.csv");
    csvFile << "Scheme,Category,Bytes_MB,Packets,Percentage\n";
    
    for (const auto& [scheme, stats] : g_bwStats) {
        uint64_t total = stats.vipBytes + stats.validDraftBytes + 
                        stats.deadDraftBytes + stats.headerBytes;
        
        if (total > 0) {
            csvFile << scheme << ",VIP Data," << (stats.vipBytes / 1e6) << "," 
                   << stats.vipPackets << "," << (100.0 * stats.vipBytes / total) << "\n";
            csvFile << scheme << ",Valid Drafts," << (stats.validDraftBytes / 1e6) << "," 
                   << stats.validDraftPackets << "," << (100.0 * stats.validDraftBytes / total) << "\n";
            csvFile << scheme << ",Dead Draft Payload," << (stats.deadDraftBytes / 1e6) << "," 
                   << stats.deadDraftPackets << "," << (100.0 * stats.deadDraftBytes / total) << "\n";
            csvFile << scheme << ",Pruning Overhead," << (stats.headerBytes / 1e6) << "," 
                   << stats.prunedPackets << "," << (100.0 * stats.headerBytes / total) << "\n";
        }
    }
    
    csvFile.close();
    std::cout << "\n✅ DONE! Results saved to results/supp3-bandwidth-breakdown-FIXED.csv" << std::endl;
    return 0;
}