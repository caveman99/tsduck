//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2025, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------
//
//  Transport stream processor shared library:
//  Extract PID's containing PSI/SI
//
//----------------------------------------------------------------------------

#include "tsPluginRepository.h"
#include "tsCASSelectionArgs.h"
#include "tsBinaryTable.h"
#include "tsSectionDemux.h"
#include "tsPAT.h"


//----------------------------------------------------------------------------
// Plugin definition
//----------------------------------------------------------------------------

namespace ts {
    class SIFilterPlugin: public ProcessorPlugin, private TableHandlerInterface
    {
        TS_PLUGIN_CONSTRUCTORS(SIFilterPlugin);
    public:
        // Implementation of plugin API
        virtual bool start() override;
        virtual Status processPacket(TSPacket&, TSPacketMetadata&) override;

    private:
        CASSelectionArgs _cas_args {};            // CAS selection
        bool             _pass_pmt = false;       // Pass PIDs containing PMT
        Status           _drop_status = TSP_DROP; // Status for dropped packets
        PIDSet           _pass_pids {};           // List of PIDs to pass
        SectionDemux     _demux {duck, this};     // Section filter

        // Invoked by the demux when a complete table is available.
        virtual void handleTable(SectionDemux&, const BinaryTable&) override;

        // Process specific tables
        void processPAT(const PAT&);
    };
}

TS_REGISTER_PROCESSOR_PLUGIN(u"sifilter", ts::SIFilterPlugin);


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------

ts::SIFilterPlugin::SIFilterPlugin(TSP* tsp_) :
    ProcessorPlugin(tsp_, u"Extract PID's containing the specified PSI/SI", u"[options]")
{
    option(u"bat");
    help(u"bat", u"Extract PID 0x0011 (SDT/BAT).");

    option(u"cat");
    help(u"cat", u"Extract PID 0x0001 (CAT).");

    option(u"eit");
    help(u"eit", u"Extract PID 0x0012 (EIT).");

    option(u"nit");
    help(u"nit", u"Extract PID 0x0010 (NIT).");

    option(u"pat");
    help(u"pat", u"Extract PID 0x0000 (PAT).");

    option(u"pmt", 'p');
    help(u"pmt", u"Extract all PMT PID's.");

    option(u"rst");
    help(u"rst", u"Extract PID 0x0013 (RST).");

    option(u"sdt");
    help(u"sdt", u"Extract PID 0x0011 (SDT/BAT).");

    option(u"stuffing", 's');
    help(u"stuffing",
         u"Replace excluded packets with stuffing (null packets) instead\n"
         u"of removing them. Useful to preserve bitrate.");

    option(u"tdt");
    help(u"tdt", u"Extract PID 0x0014 (TDT/TOT).");

    option(u"tot");
    help(u"tot", u"Extract PID 0x0014 (TDT/TOT).");

    option(u"tsdt");
    help(u"tsdt", u"Extract PID 0x0002 (TSDT).");

    // CAS filtering options.
    _cas_args.defineArgs(*this);
}


//----------------------------------------------------------------------------
// Start method
//----------------------------------------------------------------------------

bool ts::SIFilterPlugin::start()
{
    // Get command line arguments
    _cas_args.loadArgs(duck, *this);
    _pass_pmt = present(u"pmt");
    _drop_status = present(u"stuffing") ? TSP_NULL : TSP_DROP;

    _pass_pids.reset();
    if (present(u"bat")) {
        _pass_pids.set(PID_BAT);
    }
    if (present(u"cat")) {
        _pass_pids.set(PID_CAT);
    }
    if (present(u"eit")) {
        _pass_pids.set(PID_EIT);
    }
    if (present(u"nit")) {
        _pass_pids.set(PID_NIT);
    }
    if (present(u"pat")) {
        _pass_pids.set(PID_PAT);
    }
    if (present(u"rst")) {
        _pass_pids.set(PID_RST);
    }
    if (present(u"sdt")) {
        _pass_pids.set(PID_SDT);
    }
    if (present(u"tdt")) {
        _pass_pids.set(PID_TDT);
    }
    if (present(u"tot")) {
        _pass_pids.set(PID_TOT);
    }
    if (present(u"tsdt")) {
        _pass_pids.set(PID_TSDT);
    }

    // Reinitialize the demux
    _demux.reset();
    _demux.addPID(PID_PAT);
    if (_cas_args.pass_emm) {
        _demux.addPID(PID_CAT);
    }

    return true;
}


//----------------------------------------------------------------------------
// Invoked by the demux when a complete table is available.
//----------------------------------------------------------------------------

void ts::SIFilterPlugin::handleTable(SectionDemux& demux, const BinaryTable& table)
{
    switch (table.tableId()) {

        case TID_PAT: {
            PAT pat(duck, table);
            if (pat.isValid()) {
                processPAT(pat);
            }
            break;
        }

        case TID_CAT: {
            CAT cat(duck, table);
            if (cat.isValid()) {
                _cas_args.addMatchingPIDs(_pass_pids, cat, *this);
            }
            break;
        }

        case TID_PMT: {
            PMT pmt(duck, table);
            if (pmt.isValid()) {
                _cas_args.addMatchingPIDs(_pass_pids, pmt, *this);
            }
            break;
        }

        default: {
            break;
        }
    }
}


//----------------------------------------------------------------------------
//  This method processes a Program Association Table (PAT).
//----------------------------------------------------------------------------

void ts::SIFilterPlugin::processPAT(const PAT& pat)
{
    for (const auto& it : pat.pmts) {
        // Add PMT PID to section filter if ECM are required
        if (_cas_args.pass_ecm) {
            _demux.addPID(it.second);
        }
        // Pass this PMT PID if PMT are required
        if (_pass_pmt && !_pass_pids[it.second]) {
            verbose(u"Filtering PMT PID %n", it.second);
            _pass_pids.set(it.second);
        }
    }
}


//----------------------------------------------------------------------------
// Packet processing method
//----------------------------------------------------------------------------

ts::ProcessorPlugin::Status ts::SIFilterPlugin::processPacket(TSPacket& pkt, TSPacketMetadata& pkt_data)
{
    _demux.feedPacket(pkt);
    return _pass_pids[pkt.getPID()] ? TSP_OK : _drop_status;
}
