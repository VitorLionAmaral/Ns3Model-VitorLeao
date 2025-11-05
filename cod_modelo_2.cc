/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Versão refeita de um experimento ns-3 (mantém funcionalidade original).
 * Autor: adaptado automaticamente
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Lab2Part1Refactored");

// armazena stream de saída por socketId
static std::unordered_map<uint32_t, Ptr<OutputStreamWrapper>> g_cwndOutputs;
static std::unordered_map<uint32_t, bool> g_firstWrite;

// função que escreve evento de mudança de cwnd
static void
TraceCwnd (uint32_t sockId, uint32_t oldCwnd, uint32_t newCwnd)
{
  if (g_firstWrite[sockId])
    {
      // escreve ponto inicial (tempo 0.0 e valor antigo)
      *g_cwndOutputs[sockId]->GetStream () << "0.0 " << oldCwnd << std::endl;
      g_firstWrite[sockId] = false;
    }
  *g_cwndOutputs[sockId]->GetStream () << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

// Conecta o trace de CongestionWindow para um socket (identificado por socketIndex)
// Conecta o trace de CongestionWindow para um socket (identificado por socketIndex)
// Conecta o trace de CongestionWindow para um socket (identificado por socketIndex)
static void
AttachCwndLogger (uint32_t socketIndex, const std::string &outfilePrefix,
                  const std::string &tcpName, uint32_t totalFlows,
                  const std::string &delayStr)   // <---- adiciona o delay como parâmetro
{
  AsciiTraceHelper asciiHelper;
  std::ostringstream fname;

  // adiciona o delay no nome do arquivo
  fname << outfilePrefix << "-" << tcpName << "-" << totalFlows
        << "flows-" << delayStr << "-sock" << socketIndex << "-cwnd.dat";

  g_cwndOutputs[socketIndex] = asciiHelper.CreateFileStream(fname.str().c_str());
  g_firstWrite[socketIndex] = true;

  std::string configPath = "/NodeList/0/$ns3::TcpL4Protocol/SocketList/" +
                           std::to_string(socketIndex) + "/CongestionWindow";

  if (!Config::ConnectWithoutContextFailSafe(configPath, MakeBoundCallback(&TraceCwnd, socketIndex)))
    {
      NS_LOG_WARN("Falha ao conectar trace de cwnd para socket " << socketIndex);
    }
}



// helper que cria aplicações sink e origem (BulkSend) para cada fluxo
static void
CreateTcpFlows (Ptr<Node> srcNode, Ptr<Node> dstNode, const Ipv4InterfaceContainer &dstIfaces,
                uint16_t basePort, uint16_t nFlows, uint32_t segSize,
                ApplicationContainer &installedSinks, ApplicationContainer &installedSources)
{
  for (uint16_t i = 0; i < nFlows; ++i)
    {
      uint16_t port = basePort + i;

      // Sink (ouvinte no destino)
      Address anyAddr (InetSocketAddress (Ipv4Address::GetAny (), port));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", anyAddr);
      ApplicationContainer sinkApp = sinkHelper.Install (dstNode);
      installedSinks.Add (sinkApp);

      // Client BulkSend apontando para o IP do destino (índice 1 da interface do link destino)
      Address remote (InetSocketAddress (dstIfaces.GetAddress (1), port));
      BulkSendHelper sender ("ns3::TcpSocketFactory", Address ());
      sender.SetAttribute ("Remote", AddressValue (remote));
      sender.SetAttribute ("SendSize", UintegerValue (segSize));
      sender.SetAttribute ("MaxBytes", UintegerValue (0)); // infinito
      ApplicationContainer clientApp = sender.Install (srcNode);
      installedSources.Add (clientApp);
    }
}

int
main (int argc, char *argv[])
{
  // --- parâmetros padrão (podem ser alterados pela linha de comando)
  std::string bottleneckRate = "1Mbps";
  std::string bottleneckDelay = "20ms";
  double linkErrorRate = 1e-5;
  uint16_t numberOfFlows = 1;
  std::string tcpProtocol = "TcpCubic"; // TcpCubic ou TcpNewReno
  uint32_t mtu = 1500;
  uint32_t runIndex = 0;
  bool enableTracing = true;
  std::string outPrefix = "lab2-part1-ref";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("dataRate", "Taxa do enlace de gargalo", bottleneckRate);
  cmd.AddValue ("delay", "Atraso do enlace de gargalo", bottleneckDelay);
  cmd.AddValue ("errorRate", "Taxa de erro do enlace", linkErrorRate);
  cmd.AddValue ("nFlows", "Número de fluxos TCP (max 20)", numberOfFlows);
  cmd.AddValue ("transport_prot", "Protocolo TCP (TcpCubic ou TcpNewReno)", tcpProtocol);
  cmd.AddValue ("mtu", "MTU em bytes", mtu);
  cmd.AddValue ("run", "Índice da execução (seed)", runIndex);
  cmd.AddValue ("prefix_name", "Prefixo dos arquivos de saída", outPrefix);
  cmd.AddValue ("tracing", "Ativa rastreamento da cwnd", enableTracing);
  cmd.Parse (argc, argv);

  if (numberOfFlows > 20)
    {
      std::cerr << "Erro: máximo de 20 fluxos permitido." << std::endl;
      return 1;
    }

  if (tcpProtocol.rfind ("ns3::", 0) != 0)
    {
      tcpProtocol = std::string ("ns3::") + tcpProtocol;
    }

  // sementes
  SeedManager::SetSeed (123456789);
  SeedManager::SetRun (runIndex);

  // calcular MSS (ADU) a partir de headers (IP + TCP)
  Header *tmpIp = new Ipv4Header ();
  uint32_t ipSize = tmpIp->GetSerializedSize ();
  delete tmpIp;
  Header *tmpTcp = new TcpHeader ();
  uint32_t tcpSize = tmpTcp->GetSerializedSize ();
  delete tmpTcp;
  uint32_t payloadSize = mtu - (ipSize + tcpSize);

  // durações
  double simTime = 20.0;
  double sinkStart = 0.0;
  double flowStart = 1.0;

  // configurar tipo TCP global se existir
  TypeId tcpTid;
  NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (tcpProtocol, &tcpTid),
                       "TypeId " << tcpProtocol << " não encontrado");
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                      TypeIdValue (TypeId::LookupByName (tcpProtocol)));

  // criar nós: source, r1, r2, dest
  NodeContainer netNodes;
  netNodes.Create (5); // criar 5 nós: 0..4
  Ptr<Node> nodeSrc  = netNodes.Get (0);
  Ptr<Node> nodeR1   = netNodes.Get (1);
  Ptr<Node> nodeR2   = netNodes.Get (2);
  Ptr<Node> nodeDst1 = netNodes.Get (3);
  Ptr<Node> nodeDst2 = netNodes.Get (4);

  // modelo de erro para o enlace gargalo (aplicado em um dos dispositivos)
  Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel> ();
  errorModel->SetAttribute ("ErrorRate", DoubleValue (linkErrorRate));

  // helpers de enlaces
  PointToPointHelper fastLink;
  fastLink.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  fastLink.SetChannelAttribute ("Delay", StringValue ("0.01ms"));

  PointToPointHelper bottleneckLink;
  bottleneckLink.SetDeviceAttribute ("DataRate", StringValue (bottleneckRate));
  bottleneckLink.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));

  // links do r2 para cada destino com delays diferentes
  PointToPointHelper linkR2Dst1;
  linkR2Dst1.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  linkR2Dst1.SetChannelAttribute ("Delay", StringValue ("0.01ms")); // dest1 = 00.1ms

  PointToPointHelper linkR2Dst2;
  linkR2Dst2.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  linkR2Dst2.SetChannelAttribute ("Delay", StringValue ("50ms")); // dest2 = 50ms

  // instalar dispositivos
  NetDeviceContainer devSrcR1 = fastLink.Install (nodeSrc, nodeR1);
  NetDeviceContainer devR1R2  = bottleneckLink.Install (nodeR1, nodeR2);
  NetDeviceContainer devR2Dst1 = linkR2Dst1.Install (nodeR2, nodeDst1);
  NetDeviceContainer devR2Dst2 = linkR2Dst2.Install (nodeR2, nodeDst2);

  // aplicar modelo de erro no dispositivo do lado do r2 (recebimento)
  devR1R2.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (errorModel));

  // pilha IP
  InternetStackHelper inet;
  inet.InstallAll ();

  // endereçamento (cada par de dispositivo recebe sua sub-rede)
  Ipv4AddressHelper ipHelper;
  ipHelper.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifSrcR1 = ipHelper.Assign (devSrcR1);

  ipHelper.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifR1R2 = ipHelper.Assign (devR1R2);

  ipHelper.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifR2Dst1 = ipHelper.Assign (devR2Dst1);

  ipHelper.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer ifR2Dst2 = ipHelper.Assign (devR2Dst2);

  // popular tabela de roteamento
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // preparar containers de aplicações
  ApplicationContainer sinks;
  ApplicationContainer sources;

  // ajustar MSS globalmente
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));

  // dividir fluxos igualmente
  uint16_t halfFlows = numberOfFlows / 2;
  uint16_t portBase1 = 50000;
  uint16_t portBase2 = portBase1 + halfFlows; // evita conflito de portas

  // Primeira metade dos fluxos → dest1
  CreateTcpFlows (nodeSrc, nodeDst1, ifR2Dst1, portBase1, halfFlows, payloadSize, sinks, sources);

  // Segunda metade dos fluxos → dest2
  CreateTcpFlows (nodeSrc, nodeDst2, ifR2Dst2, portBase2, halfFlows, payloadSize, sinks, sources);
   
  // iniciar/parar aplicações
  sinks.Start (Seconds (sinkStart));
  sinks.Stop (Seconds (simTime));
  sources.Start (Seconds (flowStart));
  sources.Stop (Seconds (simTime));

  // conectar rastreamento de cwnd (se ativado)
  if (enableTracing)
    {
      // extrai nome legível do tipo (removendo "ns3::")
      std::string tcpShort = tcpProtocol;
      if (tcpShort.find ("ns3::") == 0)
        {
          tcpShort = tcpShort.substr (5);
        }

      for (uint32_t idx = 0; idx < numberOfFlows; ++idx)
        {
          // agendar um pouco após o início do fluxo para conectar o trace
      Simulator::Schedule (Seconds (flowStart + 0.00001),
                     &AttachCwndLogger, idx, outPrefix, tcpShort, numberOfFlows, bottleneckDelay);
        }
    }

  NS_LOG_INFO ("Iniciando simulação refeita");
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  NS_LOG_INFO ("Simulação finalizada");
  Simulator::Destroy ();
  
  // calcular e imprimir goodputs
 double activeDuration = simTime - flowStart;
  uint64_t allBytes = 0;
  uint64_t bytesDest1 = 0, bytesDest2 = 0;


  for (uint32_t i = 0; i < sinks.GetN(); ++i)
    {
      Ptr<PacketSink> psink = DynamicCast<PacketSink>(sinks.Get(i));
      uint64_t rx = psink->GetTotalRx();
      allBytes += rx;

      if (i < halfFlows)
        bytesDest1 += rx;
      else
        bytesDest2 += rx;

      double goodput = (rx * 8.0) / activeDuration; // bits/s
      std::cout << "Fluxo " << i << " Goodput: " << (goodput / 1e6) << " Mbps" << std::endl;
    }

  // Cálculos de goodput médio
  double avgGoodputDest1 = ((bytesDest1 * 8.0) / activeDuration) / halfFlows / 1e6;
  double avgGoodputDest2 = ((bytesDest2 * 8.0) / activeDuration) / halfFlows / 1e6;

  double aggGoodput = (allBytes * 8.0) / activeDuration;
  double aggGoodputMbps = aggGoodput / 1e6;

  std::cout << "\n=== Resultados ===" << std::endl;
  std::cout << "Goodput médio Dest1: " << avgGoodputDest1 << " Mbps" << std::endl;
  std::cout << "Goodput médio Dest2: " << avgGoodputDest2 << " Mbps" << std::endl;
  std::cout << "Goodput agregado: " << aggGoodputMbps << " Mbps" << std::endl;
  std::cout << "Bytes totais recebidos: " << allBytes << std::endl;

  // === salvar resultado agregado em arquivo ===
  std::ostringstream fname;
  fname << outPrefix << "-" << tcpProtocol.substr(5) << "-"
        << numberOfFlows << "flows-" << linkErrorRate << "erroRate.txt";

  std::ofstream outFile(fname.str());
  if (outFile.is_open())
    {
      outFile << "Protocolo: " << tcpProtocol.substr(5) << std::endl;
      outFile << "Número de fluxos: " << numberOfFlows << std::endl;
      outFile << "Taxa de erro: " << linkErrorRate << std::endl;
      outFile << "Taxa de enlace: " << bottleneckRate << std::endl;
      outFile << "Atraso: " << bottleneckDelay << std::endl;
      outFile << "Goodput médio Dest1 (Mbps): " << avgGoodputDest1 << std::endl;
      outFile << "Goodput médio Dest2 (Mbps): " << avgGoodputDest2 << std::endl;
      outFile << "Goodput agregado (Mbps): " << aggGoodputMbps << std::endl;
      outFile << "Bytes totais recebidos: " << allBytes << std::endl;
      outFile.close();
      std::cout << "Arquivo de resultados salvo em: " << fname.str() << std::endl;
    }
  else
    {
      std::cerr << "Erro ao criar arquivo de resultados!" << std::endl;
    }

  return 0;
}
