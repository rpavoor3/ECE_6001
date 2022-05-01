/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Piotr Gawlowicz
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
 * Author: Piotr Gawlowicz <gawlowicz.p@gmail.com>
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store.h"
#include <ns3/buildings-helper.h>
#include <ns3/spectrum-module.h>
#include <ns3/log.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LenaFrequencyReuse");

void
PrintGnuplottableNodeListToFile (std::string filename)
{
  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ios_base::out | std::ios_base::trunc);
  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }
  outFile << "set view map" << std::endl;
  outFile << "set xlabel \"X\" " << std::endl;
  outFile << "set ylabel \"Y\" " << std::endl;
  outFile << "set cblabel \"SINR (dB)\" " << std::endl;
  for (NodeList::Iterator it = NodeList::Begin (); it != NodeList::End (); ++it)
    {
      Ptr<Node> node = *it;
      int nDevs = node->GetNDevices ();
      for (int j = 0; j < nDevs; j++)
        {
          Ptr<LteUeNetDevice> uedev = node->GetDevice (j)->GetObject <LteUeNetDevice> ();
          Ptr<LteEnbNetDevice> enbdev = node->GetDevice (j)->GetObject <LteEnbNetDevice> ();
          if (uedev)
            {
              Vector pos = node->GetObject<MobilityModel> ()->GetPosition ();
              outFile << "set label \"" << uedev->GetImsi ()
                      << "\" at " << pos.x << "," << pos.y << " left font \"Helvetica,4\" textcolor rgb \"grey\" front point pt 1 ps 0.3 lc rgb \"grey\" offset 0,0"
                      << std::endl;
            }

          else if (enbdev)
            {
              Vector pos = node->GetObject<MobilityModel> ()->GetPosition ();
              outFile << "set label \"" << enbdev->GetCellId ()
                      << "\" at " << pos.x << "," << pos.y
                      << " left font \"Helvetica,4\" textcolor rgb \"white\" front  point pt 2 ps 0.3 lc rgb \"white\" offset 0,0"
                      << std::endl;
            }
        }
    }
    outFile << "plot \"lena-frequency-reuse.rem\" using ($1):($2):(10*log10($4)) with image" << std::endl;
}

int main (int argc, char *argv[])
{
  Config::SetDefault ("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue (true));
  Config::SetDefault ("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue (true));
  Config::SetDefault ("ns3::LteHelper::UseIdealRrc", BooleanValue (true));
  Config::SetDefault ("ns3::LteHelper::UsePdschForCqiGeneration", BooleanValue (true));

  //Uplink Power Control
  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (true));
  Config::SetDefault ("ns3::LteUePowerControl::ClosedLoop", BooleanValue (true));
  Config::SetDefault ("ns3::LteUePowerControl::AccumulationEnabled", BooleanValue (false));

  uint32_t runId = 3;
  uint16_t numberOfRandomUes = 3;
  double simTime = 2.500;
  bool generateSpectrumTrace = true;
  bool generateRem = true;
  int32_t remRbId = -1;
  uint16_t bandwidth = 25;
  double distance = 1000;
  Box macroUeBox = Box (-distance * 0.5, distance * 1.5, -distance * 0.5, distance * 1.5, 1.5, 1.5);

  // Command line arguments
  CommandLine cmd (__FILE__);
  cmd.AddValue ("numberOfUes", "Number of random UEs", numberOfRandomUes);
  cmd.AddValue ("simTime", "Total duration of the simulation (in seconds)", simTime);
  cmd.AddValue ("generateSpectrumTrace", "if true, will generate a Spectrum Analyzer trace", generateSpectrumTrace);
  cmd.AddValue ("generateRem", "if true, will generate a REM and then abort the simulation", generateRem);
  cmd.AddValue ("remRbId", "Resource Block Id, for which REM will be generated,"
                "default value is -1, what means REM will be averaged from all RBs", remRbId);
  cmd.AddValue ("runId", "runId", runId);
  cmd.Parse (argc, argv);

  RngSeedManager::SetSeed (1);
  RngSeedManager::SetRun (runId);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  // Create Nodes: eNodeB and UE
  NodeContainer enbNodes;
  NodeContainer centerUeNodes;
  NodeContainer edgeUeNodes;
  NodeContainer randomUeNodes;
  enbNodes.Create (3);
  centerUeNodes.Create (3);
  edgeUeNodes.Create (3);
  randomUeNodes.Create (numberOfRandomUes);


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
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 0.0));                       // eNB1
  enbPositionAlloc->Add (Vector (distance,  0.0, 0.0));                 // eNB2
  enbPositionAlloc->Add (Vector (distance * 0.5, distance * 0.866, 0.0));   // eNB3
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator (enbPositionAlloc);
  mobility.Install (enbNodes);


  Ptr<ListPositionAllocator> edgeUePositionAlloc = CreateObject<ListPositionAllocator> ();
  edgeUePositionAlloc->Add (Vector (distance * 0.5, distance * 0.28867, 0.0));  // edgeUE1
  edgeUePositionAlloc->Add (Vector (distance * 0.5, distance * 0.28867, 0.0));  // edgeUE2
  edgeUePositionAlloc->Add (Vector (distance * 0.5, distance * 0.28867, 0.0));  // edgeUE3
  mobility.SetPositionAllocator (edgeUePositionAlloc);
  mobility.Install (edgeUeNodes);


  Ptr<ListPositionAllocator> centerUePositionAlloc = CreateObject<ListPositionAllocator> ();
  centerUePositionAlloc->Add (Vector (0.0, 0.0, 0.0));                                      // centerUE1
  centerUePositionAlloc->Add (Vector (distance,  0.0, 0.0));                            // centerUE2
  centerUePositionAlloc->Add (Vector (distance * 0.5, distance * 0.866, 0.0));      // centerUE3
  mobility.SetPositionAllocator (centerUePositionAlloc);
  mobility.Install (centerUeNodes);


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
  mobility.SetPositionAllocator (randomUePositionAlloc);
  mobility.Install (randomUeNodes);


  // Create Devices and install them in the Nodes (eNB and UE)
  NetDeviceContainer enbDevs;
  NetDeviceContainer edgeUeDevs;
  NetDeviceContainer centerUeDevs;
  NetDeviceContainer randomUeDevs;
  lteHelper->SetSchedulerType ("ns3::PfFfMacScheduler");
  lteHelper->SetSchedulerAttribute ("UlCqiFilter", EnumValue (FfMacScheduler::PUSCH_UL_CQI));
  lteHelper->SetEnbDeviceAttribute ("DlBandwidth", UintegerValue (bandwidth));
  lteHelper->SetEnbDeviceAttribute ("UlBandwidth", UintegerValue (bandwidth));


  std::string frAlgorithmType = lteHelper->GetFfrAlgorithmType ();
  NS_LOG_DEBUG ("FrAlgorithmType: " << frAlgorithmType);
  lteHelper->SetFfrAlgorithmType ("ns3::LteFrNoOpAlgorithm");

  lteHelper->SetFfrAlgorithmAttribute ("FrCellTypeId", UintegerValue (1));
  enbDevs.Add (lteHelper->InstallEnbDevice (enbNodes.Get (0)));

  lteHelper->SetFfrAlgorithmAttribute ("FrCellTypeId", UintegerValue (2));
  enbDevs.Add (lteHelper->InstallEnbDevice (enbNodes.Get (1)));

  lteHelper->SetFfrAlgorithmAttribute ("FrCellTypeId", UintegerValue (3));
  enbDevs.Add (lteHelper->InstallEnbDevice (enbNodes.Get (2)));

  //FR algorithm reconfiguration if needed
  PointerValue tmp;
  enbDevs.Get (0)->GetAttribute ("LteFfrAlgorithm", tmp);
  Ptr<LteFfrAlgorithm> ffrAlgorithm = DynamicCast<LteFfrAlgorithm> (tmp.GetObject ());
  ffrAlgorithm->SetAttribute ("FrCellTypeId", UintegerValue (1));


  //Install Ue Device
  edgeUeDevs = lteHelper->InstallUeDevice (edgeUeNodes);
  centerUeDevs = lteHelper->InstallUeDevice (centerUeNodes);
  randomUeDevs = lteHelper->InstallUeDevice (randomUeNodes);

  // Attach edge UEs to eNbs
  for (uint32_t i = 0; i < edgeUeDevs.GetN (); i++)
    {
      lteHelper->Attach (edgeUeDevs.Get (i), enbDevs.Get (i));
    }
  // Attach center UEs to eNbs
  for (uint32_t i = 0; i < centerUeDevs.GetN (); i++)
    {
      lteHelper->Attach (centerUeDevs.Get (i), enbDevs.Get (i));
    }

  // Attach UE to a eNB
  lteHelper->AttachToClosestEnb (randomUeDevs, enbDevs);

  // Activate a data radio bearer
  enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
  EpsBearer bearer (q);
  lteHelper->ActivateDataRadioBearer (edgeUeDevs, bearer);
  lteHelper->ActivateDataRadioBearer (centerUeDevs, bearer);
  lteHelper->ActivateDataRadioBearer (randomUeDevs, bearer);

  //Spectrum analyzer
  NodeContainer spectrumAnalyzerNodes;
  spectrumAnalyzerNodes.Create (1);
  SpectrumAnalyzerHelper spectrumAnalyzerHelper;

  if (generateSpectrumTrace)
    {
      Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
      //position of Spectrum Analyzer
//	  positionAlloc->Add (Vector (0.0, 0.0, 0.0));                              // eNB1
//	  positionAlloc->Add (Vector (distance,  0.0, 0.0));                        // eNB2
      positionAlloc->Add (Vector (distance * 0.5, distance * 0.866, 0.0));          // eNB3

      MobilityHelper mobility;
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.SetPositionAllocator (positionAlloc);
      mobility.Install (spectrumAnalyzerNodes);

      Ptr<LteSpectrumPhy> enbDlSpectrumPhy = enbDevs.Get (0)->GetObject<LteEnbNetDevice> ()->GetPhy ()->GetDownlinkSpectrumPhy ()->GetObject<LteSpectrumPhy> ();
      Ptr<SpectrumChannel> dlChannel = enbDlSpectrumPhy->GetChannel ();

      spectrumAnalyzerHelper.SetChannel (dlChannel);
      Ptr<SpectrumModel> sm = LteSpectrumValueHelper::GetSpectrumModel (100, bandwidth);
      spectrumAnalyzerHelper.SetRxSpectrumModel (sm);
      spectrumAnalyzerHelper.SetPhyAttribute ("Resolution", TimeValue (MicroSeconds (10)));
      spectrumAnalyzerHelper.SetPhyAttribute ("NoisePowerSpectralDensity", DoubleValue (1e-15));     // -120 dBm/Hz
      spectrumAnalyzerHelper.EnableAsciiAll ("spectrum-analyzer-output");
      spectrumAnalyzerHelper.Install (spectrumAnalyzerNodes);
    }

  Ptr<RadioEnvironmentMapHelper> remHelper;
  if (generateRem)
    {
      PrintGnuplottableNodeListToFile ("SINR_graph.p");

      remHelper = CreateObject<RadioEnvironmentMapHelper> ();
      remHelper->SetAttribute ("ChannelPath", StringValue ("/ChannelList/0"));
      remHelper->SetAttribute ("OutputFile", StringValue ("lena-frequency-reuse.rem"));
      remHelper->SetAttribute ("XMin", DoubleValue (macroUeBox.xMin));
      remHelper->SetAttribute ("XMax", DoubleValue (macroUeBox.xMax));
      remHelper->SetAttribute ("YMin", DoubleValue (macroUeBox.yMin));
      remHelper->SetAttribute ("YMax", DoubleValue (macroUeBox.yMax));
      remHelper->SetAttribute ("Z", DoubleValue (1.5));
      remHelper->SetAttribute ("XRes", UintegerValue (500));
      remHelper->SetAttribute ("YRes", UintegerValue (500));
      if (remRbId >= 0)
        {
          remHelper->SetAttribute ("UseDataChannel", BooleanValue (true));
          remHelper->SetAttribute ("RbId", IntegerValue (remRbId));
        }

      remHelper->Install ();
      // simulation will stop right after the REM has been generated


    }

  else
    {
      Simulator::Stop (Seconds (simTime));
    }

  lteHelper->EnablePhyTraces ();
  lteHelper->EnableMacTraces ();
  lteHelper->EnableRlcTraces ();
  lteHelper->EnablePdcpTraces ();
  Ptr<RadioBearerStatsCalculator> rlcStats = lteHelper->GetRlcStats ();
  rlcStats->SetAttribute ("StartTime", TimeValue (Seconds (0)));
  rlcStats->SetAttribute ("EpochDuration", TimeValue (Seconds (0.2)));

  Simulator::Run ();



    
  Simulator::Destroy ();
  return 0;
}
