/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Experiment 1: Topology and Traffic Generation
 * 
 * Creates a Dumbbell topology and verifies traffic patterns
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netprune-common.h"
#include "ns3/speculative-app.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetPruneExp1");

// Global statistics
// NetPruneStats g_legacyStats; 
// NetPruneStats g_netpruneStats; 

// Configuration parameters
struct SimConfig {
    std::string bandwidth = "10Gbps";
    std::string delay = "50us";       // Half of RTT
    uint32_t bufferSize = 100;        // In packets (KB)
    uint32_t packetSize = 1024;       // 1KB per token
    double duration = 10.0;           // Simulation duration in seconds
    uint32_t numFlows = 5;            // Number of sender-receiver pairs
    uint32_t vipPerSeq = 1;           // VIP tokens per sequence
    uint32_t draftPerSeq = 4;         // Draft tokens per sequence
};

// Packet receive callback
void ReceivePacket(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    
    while ((packet = socket->RecvFrom(from))) {
        // Extract header information (simplified)
        uint32_t size = packet->GetSize();
        
        // In real implementation, parse NetPruneHeader
        // For now, just count packets
        g_legacyStats.totalPacketsReceived++;
        g_legacyStats.totalBytesReceived += size;
        
        NS_LOG_INFO("Received packet of size " << size << " at " << Simulator::Now().GetSeconds());
    }
}

// Print buffer occupancy periodically
class BufferMonitor {
public:
    BufferMonitor(Ptr<Queue<Packet>> queue, std::string name, double interval)
        : m_queue(queue), m_name(name), m_interval(interval) {
        m_file.open("results/exp1-" + name + "-buffer.txt");
        m_file << "Time(s),Occupancy(packets),Occupancy(%)\n";
    }
    
    ~BufferMonitor() {
        if (m_file.is_open()) m_file.close();
    }
    
    void Start() {
        PrintOccupancy();
    }
    
private:
    void PrintOccupancy() {
        double time = Simulator::Now().GetSeconds();
        uint32_t nPackets = m_queue->GetNPackets();
        uint32_t maxPackets = m_queue->GetMaxSize().GetValue();
        double percentage = (double)nPackets / maxPackets * 100.0;
        
        m_file << time << "," << nPackets << "," << percentage << "\n";
        
        NS_LOG_INFO("Buffer " << m_name << " at " << time << "s: " 
                    << nPackets << "/" << maxPackets << " (" << percentage << "%)");
        
        // Schedule next check
        Simulator::Schedule(Seconds(m_interval), &BufferMonitor::PrintOccupancy, this);
    }
    
    Ptr<Queue<Packet>> m_queue;
    std::string m_name;
    double m_interval;
    std::ofstream m_file;
};

int main(int argc, char *argv[]) {
    SimConfig config;
    
    // Command line arguments
    CommandLine cmd;
    cmd.AddValue("bandwidth", "Bottleneck bandwidth", config.bandwidth);
    cmd.AddValue("rtt", "Round trip time", config.delay);
    cmd.AddValue("bufferSize", "Buffer size in KB", config.bufferSize);
    cmd.AddValue("duration", "Simulation duration", config.duration);
    cmd.AddValue("numFlows", "Number of flows", config.numFlows);
    cmd.Parse(argc, argv);
    
    // Convert RTT to one-way delay
    Time rtt = Time(config.delay);
    Time oneWayDelay = Time(rtt.GetTimeStep() / 2);
    
    NS_LOG_INFO("=== Experiment 1: Topology and Traffic Generation ===");
    NS_LOG_INFO("Bandwidth: " << config.bandwidth);
    NS_LOG_INFO("RTT: " << config.delay);
    NS_LOG_INFO("Buffer Size: " << config.bufferSize << " KB");
    NS_LOG_INFO("Duration: " << config.duration << " s");
    NS_LOG_INFO("Number of Flows: " << config.numFlows);
    
    // Create results directory
    system("mkdir -p results");
    
    // Create nodes
    NodeContainer senders;
    senders.Create(config.numFlows);
    
    NodeContainer receivers;
    receivers.Create(config.numFlows);
    
    NodeContainer switches;
    switches.Create(2);  // Left and right switches
    
    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(senders);
    internet.Install(receivers);
    internet.Install(switches);
    
    // Create point-to-point links
    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue("40Gbps"));  // Access links
    p2pAccess.SetChannelAttribute("Delay", StringValue("10us"));
    
    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(config.bandwidth));
    p2pBottleneck.SetChannelAttribute("Delay", TimeValue(oneWayDelay));
    
    // Set buffer size (shallow buffer)
    uint32_t bufferSizeBytes = config.bufferSize * 1024;  // KB to bytes
    p2pBottleneck.SetQueue("ns3::DropTailQueue<Packet>",
                           "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, bufferSizeBytes)));
    
    // Connect senders to left switch
    std::vector<NetDeviceContainer> senderDevices;
    for (uint32_t i = 0; i < config.numFlows; i++) {
        NetDeviceContainer link = p2pAccess.Install(senders.Get(i), switches.Get(0));
        senderDevices.push_back(link);
    }
    
    // Connect left switch to right switch (bottleneck)
    NetDeviceContainer bottleneck = p2pBottleneck.Install(switches.Get(0), switches.Get(1));
    
    // Connect right switch to receivers
    std::vector<NetDeviceContainer> receiverDevices;
    for (uint32_t i = 0; i < config.numFlows; i++) {
        NetDeviceContainer link = p2pAccess.Install(switches.Get(1), receivers.Get(i));
        receiverDevices.push_back(link);
    }
    
    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> senderInterfaces;
    
    for (uint32_t i = 0; i < config.numFlows; i++) {
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        senderInterfaces.push_back(ipv4.Assign(senderDevices[i]));
    }
    
    ipv4.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterfaces = ipv4.Assign(bottleneck);
    
    std::vector<Ipv4InterfaceContainer> receiverInterfaces;
    for (uint32_t i = 0; i < config.numFlows; i++) {
        std::ostringstream subnet;
        subnet << "10.3." << (i + 1) << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        receiverInterfaces.push_back(ipv4.Assign(receiverDevices[i]));
    }
    
    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    // Install receiver applications
    uint16_t port = 9;
    for (uint32_t i = 0; i < config.numFlows; i++) {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), port + i));
        ApplicationContainer sinkApp = sinkHelper.Install(receivers.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(config.duration));
    }
    
    // Install sender applications (Speculative Inference)
    for (uint32_t i = 0; i < config.numFlows; i++) {
        Ptr<SpeculativeInferenceApp> app = CreateObject<SpeculativeInferenceApp>();
        senders.Get(i)->AddApplication(app);
        
        Address receiverAddr(InetSocketAddress(receiverInterfaces[i].GetAddress(1), port + i));
        app->Setup(receiverAddr, config.packetSize, 
                   config.vipPerSeq, config.draftPerSeq,
                   DataRate(config.bandwidth), false);  // Legacy mode first
        
        app->SetStartTime(Seconds(1.0 + i * 0.1));  // Stagger start times
        app->SetStopTime(Seconds(config.duration - 1.0));
    }
    
    // Monitor bottleneck queue
    Ptr<PointToPointNetDevice> bottleneckDevice = bottleneck.Get(0)->GetObject<PointToPointNetDevice>();
    Ptr<Queue<Packet>> queue = bottleneckDevice->GetQueue();
    
    BufferMonitor* monitor = new BufferMonitor(queue, "bottleneck", 0.01);  // Sample every 10ms
    Simulator::Schedule(Seconds(1.0), &BufferMonitor::Start, monitor);
    
    // Enable PCAP tracing
    p2pBottleneck.EnablePcapAll("results/exp1", true);
    
    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor_flow = flowmon.InstallAll();
    
    NS_LOG_INFO("Starting simulation...");
    
    // Run simulation
    Simulator::Stop(Seconds(config.duration));
    Simulator::Run();
    
    // Print flow statistics
    monitor_flow->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor_flow->GetFlowStats();
    
    std::ofstream outfile("results/exp1-traffic-stats.txt");
    outfile << "=== Flow Statistics ===\n\n";
    
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        outfile << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        outfile << "  Tx Packets: " << it->second.txPackets << "\n";
        outfile << "  Rx Packets: " << it->second.rxPackets << "\n";
        outfile << "  Tx Bytes: " << it->second.txBytes << "\n";
        outfile << "  Rx Bytes: " << it->second.rxBytes << "\n";
        outfile << "  Throughput: " << it->second.rxBytes * 8.0 / config.duration / 1e9 << " Gbps\n";
        outfile << "  Mean Delay: " << it->second.delaySum.GetSeconds() / it->second.rxPackets * 1000 << " ms\n";
        outfile << "  Packet Loss Ratio: " << (double)(it->second.txPackets - it->second.rxPackets) / it->second.txPackets * 100 << " %\n";
        outfile << "\n";
    }
    
    outfile.close();
    
    // Print summary
    NS_LOG_INFO("Simulation completed!");
    NS_LOG_INFO("Results saved to results/exp1-traffic-stats.txt");
    NS_LOG_INFO("Buffer occupancy saved to results/exp1-bottleneck-buffer.txt");
    NS_LOG_INFO("PCAP files saved to results/exp1-*.pcap");
    
    // Cleanup
    delete monitor;
    Simulator::Destroy();
    
    return 0;
}
