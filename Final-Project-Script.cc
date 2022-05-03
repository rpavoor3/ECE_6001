/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2018 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Jaume Nin <jaume.nin@cttc.cat>
 *          Manuel Requena <manuel.requena@cttc.es>
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include <vector>
//#include "ns3/gtk-config-store.h"

using namespace ns3;
using namespace std;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeBs,
 * attaches one UE per eNodeB starts a flow for each UE to and from a remote host.
 * It also starts another flow between each UE pair.
 */

int
main (int argc, char *argv[])
{
  double simTime = 2.0;
  double distance = 1000.0;
  Time interPacketInterval = MilliSeconds (100);
  uint16_t numCenterUes = 1;
  uint16_t numEdgeUes = 1;
  uint16_t numRandomUes = 1;

  // Command line arguments
  CommandLine cmd (__FILE__);
  cmd.AddValue ("numCenterUes", "Number of center UEs per cell", numCenterUes);
  cmd.AddValue ("numEdgeUes", "Number of edge UEs per cell", numEdgeUes);
  cmd.AddValue ("numRandomUes", "Number of random UEs per cell", numRandomUes);
  cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
  cmd.AddValue ("distance", "Distance between eNBs [m]", distance);
  cmd.AddValue ("interPacketInterval", "Inter packet interval", interPacketInterval);
  cmd.Parse (argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  // parse again so you can override default values from the command line
  cmd.Parse(argc, argv);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

   // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // Create Nodes: eNodeB and UE
  NodeContainer enbNodes;
  NodeContainer centerUeNodes;
  NodeContainer edgeUeNodes;
  NodeContainer randomUeNodes;
  enbNodes.Create (3);
  centerUeNodes.Create (numCenterUes * 3);
  edgeUeNodes.Create (numEdgeUes * 3);
  randomUeNodes.Create(numRandomUes * 3);

  Box leftBound = Box (-distance * 0.5, distance * 0.5, -distance * 0.5, distance * 0.5, 1.5, 1.5);
  Box rightBound = Box (distance * 0.5, distance * 1.5, -distance * 0.5, distance * 0.5, 1.5, 1.5);
  Box topBound = Box (distance * 0.28867, distance * 0.866, -distance * 1.5, -distance * 0.5, 1.5, 1.5);

  vector<Box> bounds;

  bounds.push_back(leftBound);
  bounds.push_back(rightBound);
  bounds.push_back(topBound);
  
  /*   the topology is the following:
  *                 eNB3
  *                /     \
  *               /       \
  *              /         \
  *             /           \
  *   distance /             \ distance
  *           /      UEs      \
  *          /                 \
  *         /                   \
  *        /                     \
  *       /                       \
  *   eNB1-------------------------eNB2
  *                  distance
  */

  // Install Mobility Model
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 0.0));                       // eNB1
  enbPositionAlloc->Add (Vector (distance,  0.0, 0.0));                 // eNB2
  enbPositionAlloc->Add (Vector (distance * 0.5, distance * 0.866, 0.0));   // eNB3
  
  mobility.SetPositionAllocator (enbPositionAlloc);
  mobility.Install (enbNodes);
 
  if (centerUeNodes.GetN() > 0) {
    Ptr<ListPositionAllocator> centerUePositionAlloc = CreateObject<ListPositionAllocator> ();
    for (int i = 0; i < numCenterUes; i++) {
      centerUePositionAlloc->Add (Vector (0.0, 0.0, 0.0));                               // centerUE1
    }
    for (int i = 0; i < numCenterUes; i++) {
      centerUePositionAlloc->Add (Vector (distance, 0.0, 0.0));                         // centerUE2
    }
    for (int i = 0; i < numCenterUes; i++) {
      centerUePositionAlloc->Add (Vector (distance * 0.5, distance * 0.866, 0.0));      // centerUE3
    }
    mobility.SetPositionAllocator (centerUePositionAlloc);
    mobility.Install (centerUeNodes);
  }

  if (edgeUeNodes.GetN() > 0) {
    Ptr<ListPositionAllocator> edgeUePositionAlloc = CreateObject<ListPositionAllocator> ();
    for (int i = 0; i < numEdgeUes; i++) {
      edgeUePositionAlloc->Add (Vector (distance * 0.5, distance * 0.28867, 0.0));  // edgeUE1
      edgeUePositionAlloc->Add (Vector (distance * 0.5, distance * 0.28867, 0.0));  // edgeUE2
      edgeUePositionAlloc->Add (Vector (distance * 0.5, distance * 0.28867, 0.0));  // edgeUE3
    }
    mobility.SetPositionAllocator (edgeUePositionAlloc);
    mobility.Install (edgeUeNodes);
  }
 
  if (randomUeNodes.GetN() > 0) {
    for (int i = 0; i < 3; ++i) {
      Box macroUeBox = bounds[i];
      Ptr<RandomBoxPositionAllocator> randomUePositionAlloc = CreateObject<RandomBoxPositionAllocator> ();
      Ptr<UniformRandomVariable> xVal = CreateObject<UniformRandomVariable> ();
      xVal->SetAttribute ("Min", DoubleValue (macroUeBox.xMin));
      xVal->SetAttribute ("Max", DoubleValue (macroUeBox.xMax));
      randomUePositionAlloc->SetAttribute ("X", PointerValue (xVal));
      Ptr<UniformRandomVariable> yVal = CreateObject<UniformRandomVariable> ();
      yVal->SetAttribute ("Min", DoubleValue (macroUeBox.yMin));
      yVal->SetAttribute ("Max", DoubleValue (macroUeBox.yMax));
      randomUePositionAlloc->SetAttribute ("Y", PointerValue (yVal));
      Ptr<UniformRandomVariable> zVal = CreateObject<UniformRandomVariable> ();
      zVal->SetAttribute ("Min", DoubleValue (macroUeBox.zMin));
      zVal->SetAttribute ("Max", DoubleValue (macroUeBox.zMax));
      randomUePositionAlloc->SetAttribute ("Z", PointerValue (zVal));

      mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                  "PositionAllocator", PointerValue (randomUePositionAlloc));    
      for (int j = 0; j < numRandomUes; j++) {
        mobility.Install (randomUeNodes.Get(i * numRandomUes + j));
      }
    }
  }

  // Install LTE Devices to the nodes
  // Install the IP stack on the UEs
  // Assign IP address to UEs
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer centerUeLteDevs, edgeUeLteDevs, randomUeLteDevs;
  if (centerUeNodes.GetN() > 0) {
    centerUeLteDevs = lteHelper->InstallUeDevice (centerUeNodes);
    internet.Install (centerUeNodes);
    epcHelper->AssignUeIpv4Address (NetDeviceContainer (centerUeLteDevs));
  }
  if (edgeUeNodes.GetN() > 0) {
    edgeUeLteDevs = lteHelper->InstallUeDevice (edgeUeNodes);
    internet.Install (edgeUeNodes);
    epcHelper->AssignUeIpv4Address (NetDeviceContainer (edgeUeLteDevs));
  }
  if (randomUeNodes.GetN() > 0) {
    randomUeLteDevs = lteHelper->InstallUeDevice (randomUeNodes);
    internet.Install (randomUeNodes);
    epcHelper->AssignUeIpv4Address (NetDeviceContainer (randomUeLteDevs));

  }
  
  // Install center applications
  for (uint32_t i = 0; i < numCenterUes * 3; ++i) {
      Ptr<Node> centerUeNode = centerUeNodes.Get (i);

      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (centerUeNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
  }

  // Install edge applications
  for (uint32_t i = 0; i < numEdgeUes * 3; ++i) {
      Ptr<Node> edgeUeNode = edgeUeNodes.Get (i);

      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (edgeUeNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
  }

  // Install random applications
  for (uint32_t i = 0; i < numRandomUes * 3; ++i) {
    Ptr<Node> randomUeNode = randomUeNodes.Get (i);

    // Set the default gateway for the UE
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (randomUeNode->GetObject<Ipv4> ());
    ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
  }

  // Attach UEs to eNodeBs
  for (uint16_t i = 0; i < 3; i++) {
    for (int j = 0; j < numCenterUes; ++j) {
      lteHelper->Attach (centerUeLteDevs.Get(i * numCenterUes + j), enbLteDevs.Get(i));
    }
    for (int j = 0; j < numEdgeUes; ++j) {
      lteHelper->Attach (edgeUeLteDevs.Get(i * numEdgeUes + j), enbLteDevs.Get(i));
    }
    for (int j = 0; j < numRandomUes; ++j) {
      lteHelper->Attach (randomUeLteDevs.Get(i * numRandomUes + j), enbLteDevs.Get(i));
    }
    // side effect: the default EPS bearer will be activated
  }

  // Install and start applications on UEs and remote host
  uint16_t ulPort = 2000;
  ApplicationContainer clientApps;
  ApplicationContainer serverCenterApps;
  ApplicationContainer serverEdgeApps;
  ApplicationContainer serverRandomApps;

  for (uint32_t i = 0; i < 3; ++i) {
    
    //add apps for center ues
    for (int j = 0; j < numCenterUes; ++j) {
      ++ulPort;
      PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
      serverCenterApps.Add (ulPacketSinkHelper.Install (remoteHost));

      UdpClientHelper ulClient (remoteHostAddr, ulPort);
      ulClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
      ulClient.SetAttribute ("MaxPackets", UintegerValue (10000));
      clientApps.Add (ulClient.Install (centerUeNodes.Get(i * numCenterUes + j)));
    }

    //add apps for edge ues
    for (int j = 0; j < numEdgeUes; ++j) {
      cout << "hi\n";
      ++ulPort;
      PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
      serverEdgeApps.Add (ulPacketSinkHelper.Install (remoteHost));

      UdpClientHelper ulClient (remoteHostAddr, ulPort);
      ulClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
      ulClient.SetAttribute ("MaxPackets", UintegerValue (10000));
      clientApps.Add (ulClient.Install (edgeUeNodes.Get(i * numEdgeUes + j)));
    }

    //add apps for random ues
    for (int j = 0; j < numRandomUes; ++j) {
      ++ulPort;
      PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
      serverRandomApps.Add (ulPacketSinkHelper.Install (remoteHost));

      UdpClientHelper ulClient (remoteHostAddr, ulPort);
      ulClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
      ulClient.SetAttribute ("MaxPackets", UintegerValue (10000));
      clientApps.Add (ulClient.Install (randomUeNodes.Get(i * numRandomUes + j)));
    }
  }
  
  if (serverCenterApps.GetN() > 0)
    serverCenterApps.Start (MilliSeconds (500));
  if (serverEdgeApps.GetN() > 0)
    serverEdgeApps.Start (MilliSeconds (500));
  if (serverRandomApps.GetN() > 0)
    serverRandomApps.Start (MilliSeconds (500));

  clientApps.Start (MilliSeconds (500));
  lteHelper->EnableTraces ();
  // Uncomment to enable PCAP tracing
  //p2ph.EnablePcapAll("lena-simple-epc");

  Simulator::Stop (Seconds(simTime));
  Simulator::Run ();

  /*GtkConfigStore config;
  config.ConfigureAttributes();*/

  Simulator::Destroy ();

  // calculate goodputs
  uint64_t total_sum = 0;

  for (int i = 0; i < 3; i++) {
    uint64_t pair_sum = 0;
    uint64_t center_sum = 0;
    uint64_t edge_sum = 0;
    uint64_t random_sum = 0;

    cout << "EnB " << i << "\n\n";

    // center ue goodputs
    for (int j = 0; j < numCenterUes; ++j) {
      uint64_t center = DynamicCast<PacketSink> (serverCenterApps.Get(i * numCenterUes + j))->GetTotalRx ();
      double center_goodput = center * 8 / (simTime - .5);
      cout << "Center " << j << " Goodput: " << center_goodput << "\n";
      center_sum += center_goodput;
    }
    cout << "Sum Center Goodput: " << center_sum << "\n\n";
    pair_sum += center_sum;

    // edge ue goodputs
    for (int j = 0; j < numEdgeUes; ++j) {
      uint64_t edge = DynamicCast<PacketSink> (serverEdgeApps.Get(i * numEdgeUes + j))->GetTotalRx ();
      double edge_goodput = edge * 8 / (simTime - .5);
      cout << "Edge " << j << " Goodput: " << edge_goodput << "\n";
      edge_sum += edge_goodput;
    }
    cout << "Sum Edge Goodput: " << edge_sum << "\n\n";
    pair_sum += edge_sum;

    // random ue goodputs
    for (int j = 0; j < numRandomUes; ++j) {
      uint64_t random = DynamicCast<PacketSink> (serverRandomApps.Get(i * numRandomUes + j))->GetTotalRx ();
      double random_goodput = random * 8 / (simTime - .5);
      cout << "Random " << j << " Goodput: " << random_goodput << "\n";
      random_sum += random_goodput;
    }
    cout << "Sum Random Goodput: " << random_sum << "\n\n";
    pair_sum += random_sum;

    cout << "EnB " << i << " Goodput: " << pair_sum << "\n\n";
    total_sum += pair_sum;
  }
  cout << "Total Goodput " << total_sum << "\n";

  return 0;
}
