// Copyright (c) 2015-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <iostream>

POTHOS_TEST_BLOCK("/comms/tests", test_simple_mac)
{
    //create test blocks
    auto feeder = Pothos::BlockRegistry::make("/blocks/feeder_source", "uint8");
    auto collector = Pothos::BlockRegistry::make("/blocks/collector_sink", "uint8");
    auto mac = Pothos::BlockRegistry::make("/comms/simple_mac");
    const unsigned short macId = std::rand() & 0xffff;
    mac.call("setMacId", macId);

    //create a test packet
    Pothos::Packet pkt0;
    pkt0.payload = Pothos::BufferChunk("uint8", 100);
    for (size_t i = 0; i < pkt0.payload.elements(); i++)
        pkt0.payload.as<unsigned char *>()[i] = std::rand() & 0xff;
    pkt0.metadata["recipient"] = Pothos::Object(macId);
    feeder.call("feedPacket", pkt0);

    //setup the topology
    Pothos::Topology topology;
    topology.connect(feeder, 0, mac, "macIn");
    topology.connect(mac, "macOut", collector, 0);
    topology.connect(mac, "phyOut", mac, "phyIn"); //loopback phy data path

    //run the design
    topology.commit();
    POTHOS_TEST_TRUE(topology.waitInactive());

    //check the result
    POTHOS_TEST_EQUAL(mac.call<unsigned long long>("getErrorCount"), 0);
    const std::vector<Pothos::Packet> packets = collector.call("getPackets");
    POTHOS_TEST_EQUAL(packets.size(), 1);
    const auto pktOut0 = packets.at(0);
    POTHOS_TEST_EQUAL(pktOut0.metadata.count("sender"), 1);
    POTHOS_TEST_EQUAL(pktOut0.metadata.at("sender").convert<unsigned short>(), macId);
    POTHOS_TEST_EQUAL(pktOut0.metadata.count("recipient"), 1);
    POTHOS_TEST_EQUAL(pktOut0.metadata.at("recipient").convert<unsigned short>(), macId);
    POTHOS_TEST_EQUAL(pkt0.payload.dtype, pktOut0.payload.dtype);
    POTHOS_TEST_EQUAL(pkt0.payload.elements(), pktOut0.payload.elements());
    POTHOS_TEST_EQUALA(pkt0.payload.as<const unsigned char *>(),
        pktOut0.payload.as<const unsigned char *>(), pktOut0.payload.elements());

    //now try a packet with the wrong recipient -- should cause an error
    const unsigned short otherId = ~macId;
    pkt0.metadata["recipient"] = Pothos::Object(otherId);
    feeder.call("feedPacket", pkt0);
    topology.commit();
    POTHOS_TEST_TRUE(topology.waitInactive());
    POTHOS_TEST_EQUAL(mac.call<unsigned long long>("getErrorCount"), 1);
}
