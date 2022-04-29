#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/mobility-module.h>
#include <ns3/lte-module.h>
#include <fstream>

using namespace ns3;

int main (int argc, char *argv[])
{
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();



  
  NodeContainer enbNodes;
  int count = 4;
  std::cout << count << std::endl;
  enbNodes.Create (count);
  NodeContainer ueNodes;
  ueNodes.Create (count);
  
  // can consider experimenting with the mobility model
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (int i = 0; i < count ; i++)
  {
    positionAlloc -> Add (Vector(i*10.0,0.0,0.0));

  }
  for (int i = 0; i < count ; i ++)
  {
    positionAlloc -> Add(Vector(5.0 + i*10.0,0.0,0.0));

  }


 


  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator(positionAlloc);

  mobility.Install (enbNodes);
  mobility.Install (ueNodes);


  NetDeviceContainer enbDevs;
  enbDevs = lteHelper->InstallEnbDevice (enbNodes);

  NetDeviceContainer ueDevs;
  ueDevs = lteHelper->InstallUeDevice (ueNodes);
  for (int i = 0; i < count; i++)
  {
        lteHelper->Attach (ueDevs.Get(i), enbDevs.Get (i));

  }

  for  (int i = 0; i < count; i++)
  {
    enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer (q);
    lteHelper->ActivateDataRadioBearer (ueDevs.Get(i), bearer);

  }



  //enables traces to get SNR values and other values, outputs to different text files
  lteHelper->EnablePhyTraces ();
  lteHelper->EnableRlcTraces ();
  Ptr<RadioBearerStatsCalculator> rlcStats = lteHelper->GetRlcStats ();
  rlcStats->SetAttribute ("StartTime", TimeValue (Seconds (0)));
  rlcStats->SetAttribute ("EpochDuration", TimeValue (Seconds (0.2)));

  Simulator::Stop (Seconds (0.2));
  Simulator::Run ();




// getting the SNR vals
  std::string a;
  std::ifstream infile;
  infile.open("DlRsrpSinrStats.txt");//open the text file for DownLink data

    for (int i = 0; i < 14; i++) // get the 6th column value in the file to get SNR val in Downlink
    {
        infile >> a;  

    }
    std::cout << "SNR for DL: " << a << std::endl;
    infile.close();

    infile.open("UlSinrStats.txt");
    for(int i = 0; i < 12; i++) // get the 5th column value in the file to get the SNR val in Uplink
    {
        infile >> a;
    
    }
     std::cout << "SNR for UL: " << a << std::endl;

     infile.close();

     // get the interferance vals

     infile.open("UlInterferenceStats.txt");
     for(int i = 0; i < count+1; i++)
     {
         std::getline(infile, a,'\n');
         std::cout << "Interferance: " << a << std::endl;
     }



  Simulator::Destroy ();
  return 0;
}