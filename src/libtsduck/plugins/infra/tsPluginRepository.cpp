//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2025, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------

#include "tsPluginRepository.h"
#include "tsApplicationSharedLibrary.h"
#include "tsEnvironment.h"
#include "tsAlgorithm.h"
#include "tsCerrReport.h"

TS_DEFINE_SINGLETON(ts::PluginRepository);
ts::PluginRepository::PluginRepository() {}


//----------------------------------------------------------------------------
// Options for --list-processor.
//----------------------------------------------------------------------------

const ts::Names& ts::PluginRepository::ListProcessorEnum()
{
    // Thread-safe init-safe static data patterns.
    static const Names data({
        {u"all",          LIST_ALL},
        {u"input",        LIST_INPUT  | LIST_COMPACT},
        {u"output",       LIST_OUTPUT | LIST_COMPACT},
        {u"packet",       LIST_PACKET | LIST_COMPACT},
        {u"names-input",  LIST_INPUT  | LIST_NAMES},
        {u"names-output", LIST_OUTPUT | LIST_NAMES},
        {u"names-packet", LIST_PACKET | LIST_NAMES},
    });
    return data;
}


//----------------------------------------------------------------------------
// Plugin registration.
//----------------------------------------------------------------------------

void ts::PluginRepository::registerInput(const UString& name, InputPluginFactory allocator)
{
    CERR.debug(u"registering input plugin \"%s\", status: %s", name, allocator != nullptr ? u"ok" : u"error, no allocator");
    if (allocator != nullptr) {
        if (_inputPlugins[name] == nullptr) {
            _inputPlugins[name] = allocator;
        }
        else {
            CERR.debug(u"duplicated input plugin \"%s\" ignored", name);
        }
    }
}

void ts::PluginRepository::registerProcessor(const UString& name, ProcessorPluginFactory allocator)
{
    CERR.debug(u"registering processor plugin \"%s\", status: %s", name, allocator != nullptr ? u"ok" : u"error, no allocator");
    if (allocator != nullptr) {
        if (_processorPlugins[name] == nullptr) {
            _processorPlugins[name] = allocator;
        }
        else {
            CERR.debug(u"duplicated packet processor plugin \"%s\" ignored", name);
        }
    }
}

void ts::PluginRepository::registerOutput(const UString& name, OutputPluginFactory allocator)
{
    CERR.debug(u"registering output plugin \"%s\", status: %s", name, allocator != nullptr ? u"ok" : u"error, no allocator");
    if (allocator != nullptr) {
        if (_outputPlugins[name] == nullptr) {
            _outputPlugins[name] = allocator;
        }
        else {
            CERR.debug(u"duplicated output plugin \"%s\" ignored", name);
        }
    }
}

ts::PluginRepository::Register::Register(const UString& name, InputPluginFactory allocator)
{
    PluginRepository::Instance().registerInput(name, allocator);
}

ts::PluginRepository::Register::Register(const UString& name, ProcessorPluginFactory allocator)
{
    PluginRepository::Instance().registerProcessor(name, allocator);
}

ts::PluginRepository::Register::Register(const UString& name, OutputPluginFactory allocator)
{
    PluginRepository::Instance().registerOutput(name, allocator);
}


//----------------------------------------------------------------------------
// Get plugins by name.
//----------------------------------------------------------------------------

template<typename FACTORY>
FACTORY ts::PluginRepository::getFactory(const UString& plugin_name, const UString& plugin_type, const std::map<UString,FACTORY>& plugin_map, Report& report)
{
    // Search plugin in current cache.
    auto it = plugin_map.find(plugin_name);

    // Load a shared library if not found and allowed.
    if (it == plugin_map.end() && _sharedLibraryAllowed) {
        // Load shareable library. Use name resolution. Use permanent mapping to keep
        // the shareable image in memory after returning from this function. Also make
        // sure to include the plugin's directory in the shared library search path:
        // an extension may install a library in the same directory as the plugin.
        ApplicationSharedLibrary shlib(plugin_name, u"tsplugin_", PLUGINS_PATH_ENVIRONMENT_VARIABLE, SharedLibraryFlags::PERMANENT, report);
        if (shlib.isLoaded()) {
            // Search again if the shareable library was loaded.
            // The shareable library is supposed to register its plugins on initialization.
            it = plugin_map.find(plugin_name);
        }
        else {
            report.error(shlib.errorMessage());
            // If a shared library was loaded but registered its plugin with the wrong name,
            // then plugin_map was modified but the previous 'plugin_map.end()' in invalidated.
            // So, just to make sure  we don't fail on invalid plugins, reassign it.
            it = plugin_map.end();
        }
    }

    // Return the factory function if one was found.
    if (it != plugin_map.end()) {
        assert(it->second != nullptr);
        return it->second;
    }
    else {
        report.error(u"%s plugin %s not found", plugin_type, plugin_name);
        return nullptr;
    }
}

ts::PluginRepository::InputPluginFactory ts::PluginRepository::getInput(const UString& name, Report& report)
{
    return getFactory(name, u"input", _inputPlugins, report);
}

ts::PluginRepository::ProcessorPluginFactory ts::PluginRepository::getProcessor(const UString& name, Report& report)
{
    return getFactory(name, u"processor", _processorPlugins, report);
}

ts::PluginRepository::OutputPluginFactory ts::PluginRepository::getOutput(const UString& name, Report& report)
{
    return getFactory(name, u"output", _outputPlugins, report);
}


//----------------------------------------------------------------------------
// Get the names of all registered plugins.
//----------------------------------------------------------------------------

ts::UStringList ts::PluginRepository::inputNames() const
{
    return MapKeysList(_inputPlugins);
}

ts::UStringList ts::PluginRepository::processorNames() const
{
    return MapKeysList(_processorPlugins);
}

ts::UStringList ts::PluginRepository::outputNames() const
{
    return MapKeysList(_outputPlugins);
}


//----------------------------------------------------------------------------
// A minimal implementation of TSP which only acts as a Report.
//----------------------------------------------------------------------------

namespace {
    class ReportTSP: public ts::TSP
    {
        TS_NOBUILD_NOCOPY(ReportTSP);
    public:
        ReportTSP(Report& report) : ts::TSP(report.maxSeverity(), ts::UString(), &report) {}
        virtual ts::UString pluginName() const override { return ts::UString(); }
        virtual ts::Plugin* plugin() const override { return nullptr; }
        virtual size_t pluginIndex() const override { return 0; }
        virtual size_t pluginCount() const override { return 0; }
        virtual void signalPluginEvent(uint32_t, ts::Object*) const override {}
        virtual void useJointTermination(bool on) override {}
        virtual void jointTerminate() override {}
        virtual bool useJointTermination() const override { return false; }
        virtual bool thisJointTerminated() const override { return false; }
    };
}


//----------------------------------------------------------------------------
// Load all available tsp processors.
//----------------------------------------------------------------------------

void ts::PluginRepository::loadAllPlugins(Report& report)
{
    // Do nothing if loading dynamic libraries is disallowed.
    if (!_sharedLibraryAllowed) {
        return;
    }

    // Get list of shared library files
    UStringVector files;
    ApplicationSharedLibrary::GetPluginList(files, u"tsplugin_", PLUGINS_PATH_ENVIRONMENT_VARIABLE);

    // Load all plugins, let them register their plugins.
    for (size_t i = 0; i < files.size(); ++i) {
        // Permanent load.
        SharedLibrary shlib(files[i], SharedLibraryFlags::PERMANENT, report);
        CERR.debug(u"loaded plugin file \"%s\", status: %s", files[i], shlib.isLoaded());
    }
}


//----------------------------------------------------------------------------
// List all tsp processors.
//----------------------------------------------------------------------------

ts::UString ts::PluginRepository::listPlugins(bool loadAll, Report& report, int flags)
{
    // Output text, use some preservation.
    UString out;
    out.reserve(5000);

    // Load all shareable plugins first.
    if (loadAll) {
        loadAllPlugins(report);
    }

    // Compute max name width of all plugins.
    size_t name_width = 0;
    if ((flags & (LIST_COMPACT | LIST_NAMES)) == 0) {
        if ((flags & LIST_INPUT) != 0) {
            for (const auto& it : _inputPlugins) {
                name_width = std::max(name_width, it.first.width());
            }
        }
        if ((flags & LIST_PACKET) != 0) {
            for (const auto& it : _processorPlugins) {
                name_width = std::max(name_width, it.first.width());
            }
        }
        if ((flags & LIST_OUTPUT) != 0) {
            for (const auto& it : _outputPlugins) {
                name_width = std::max(name_width, it.first.width());
            }
        }
    }

    // A minimal TSP, used to build temporary plugins.
    ReportTSP tsp(report);

    // List capabilities.
    if ((flags & LIST_INPUT) != 0) {
        if ((flags & (LIST_COMPACT | LIST_NAMES)) == 0) {
            out += u"\nList of tsp input plugins:\n\n";
        }
        for (const auto& it : _inputPlugins) {
            Plugin* p = it.second(&tsp);
            ListOnePlugin(out, it.first, p, name_width, flags);
            delete p;
        }
    }

    if ((flags & LIST_OUTPUT) != 0) {
        if ((flags & (LIST_COMPACT | LIST_NAMES)) == 0) {
            out += u"\nList of tsp output plugins:\n\n";
        }
        for (const auto& it : _outputPlugins) {
            Plugin* p = it.second(&tsp);
            ListOnePlugin(out, it.first, p, name_width, flags);
            delete p;
        }
    }

    if ((flags & LIST_PACKET) != 0) {
        if ((flags & (LIST_COMPACT | LIST_NAMES)) == 0) {
            out += u"\nList of tsp packet processor plugins:\n\n";
        }
        for (const auto& it : _processorPlugins) {
            Plugin* p = it.second(&tsp);
            ListOnePlugin(out, it.first, p, name_width, flags);
            delete p;
        }
    }

    return out;
}


//----------------------------------------------------------------------------
// List one plugin.
//----------------------------------------------------------------------------

void ts::PluginRepository::ListOnePlugin(UString& out, const UString& name, Plugin* plugin, size_t name_width, int flags)
{
    if ((flags & LIST_NAMES) != 0) {
        out += name;
        out += u"\n";
    }
    else if ((flags & LIST_COMPACT) != 0) {
        out += name;
        out += u":";
        out += plugin->getDescription();
        out += u"\n";
    }
    else {
        out += u"  ";
        out += name.toJustifiedLeft(name_width + 1, u'.', false, 1);
        out += u" ";
        out += plugin->getDescription();
        out += u"\n";
    }
}
