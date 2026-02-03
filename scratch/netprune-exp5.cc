/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netprune-common.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("NetPruneExp5");

class SafeQueue : public Queue<Packet> {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::SafeQueue").SetParent<Queue<Packet>>().SetGroupName("Network").AddConstructor<SafeQueue>(); return tid;
    }
    SafeQueue() : m_maxPackets(100) {}
    bool Enqueue(Ptr<Packet> p) override {
        if (m_packets.size() > m_maxPackets * 0.8 && (rand() % 10 < 3)) return true;
        if (m_packets.size() >= m_maxPackets) return false;
        m_packets.push_back(p); return true;
    }
    Ptr<Packet> Dequeue(void) override { if(m_packets.empty()) return 0; Ptr<Packet> p=m_packets.front(); m_packets.pop_front(); return p; }
    Ptr<Packet> Remove(void) override { return Dequeue(); }
    Ptr<const Packet> Peek(void) const override { return m_packets.empty()?0:m_packets.front(); }
private:
    std::deque<Ptr<Packet>> m_packets; uint32_t m_maxPackets;
};

void PrintProgress(double total, double current) {
    std::cout << ">>> Progress: " << int((current/total)*100) << "%\r" << std::flush;
}

int main(int argc, char *argv[]) {
    std::string bandwidth = "1Gbps"; double duration = 1.5;
    CommandLine cmd; cmd.Parse(argc, argv);
    
    NodeContainer n, r; n.Create(2); r.Create(2);
    InternetStackHelper s; s.Install(n); s.Install(r);
    
    PointToPointHelper p2pA, p2pB;
    p2pA.SetDeviceAttribute("DataRate", StringValue("2Gbps")); p2pA.SetChannelAttribute("Delay", StringValue("1us"));
    p2pB.SetDeviceAttribute("DataRate", StringValue(bandwidth)); p2pB.SetChannelAttribute("Delay", StringValue("10us"));
    
    NetDeviceContainer d1 = p2pA.Install(n.Get(0), r.Get(0));
    NetDeviceContainer db = p2pB.Install(r.Get(0), r.Get(1));
    DynamicCast<PointToPointNetDevice>(db.Get(0))->SetQueue(CreateObject<SafeQueue>());
    NetDeviceContainer d2 = p2pA.Install(r.Get(1), n.Get(1));
    
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0"); ipv4.Assign(d1);
    ipv4.SetBase("10.1.2.0", "255.255.255.0"); ipv4.Assign(db);
    ipv4.SetBase("10.1.3.0", "255.255.255.0"); Ipv4InterfaceContainer dst = ipv4.Assign(d2);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    BulkSendHelper tcp("ns3::TcpSocketFactory", InetSocketAddress(dst.GetAddress(1), 5000));
    tcp.SetAttribute("MaxBytes", UintegerValue(0));
    n.Get(0)->AddApplication(tcp.Install(n.Get(0)).Get(0));
    
    PacketSinkHelper tsink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 5000));
    n.Get(1)->AddApplication(tsink.Install(n.Get(1)).Get(0));
    
    OnOffHelper udp("ns3::UdpSocketFactory", InetSocketAddress(dst.GetAddress(1), 9000));
    udp.SetAttribute("DataRate", StringValue("0.8Gbps"));
    ApplicationContainer uApp = udp.Install(n.Get(0));
    uApp.Start(Seconds(0.5));
    
    PacketSinkHelper usink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9000));
    n.Get(1)->AddApplication(usink.Install(n.Get(1)).Get(0));

    for (int i=1; i<=10; ++i) Simulator::Schedule(Seconds(duration*i/10.0), &PrintProgress, duration, duration*i/10.0);
    
    std::cout << "Running Exp5 (Turbo)..." << std::endl;
    Simulator::Stop(Seconds(duration+0.1));
    Simulator::Run();
    
    Ptr<PacketSink> ts = DynamicCast<PacketSink>(n.Get(1)->GetApplication(0));
    Ptr<PacketSink> us = DynamicCast<PacketSink>(n.Get(1)->GetApplication(1));
    double tT = ts->GetTotalRx()*8.0/1e9/duration;
    double uT = us->GetTotalRx()*8.0/1e9/(duration-0.5);
    
    std::cout << "\nTCP: " << tT << " Gbps, NetPrune: " << uT << " Gbps" << std::endl;
    Simulator::Destroy();
    return 0;
}
