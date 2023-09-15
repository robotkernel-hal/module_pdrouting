//! robotkernel module pdrouting
/*!
 * author: Robert Burger
 *
 * $Id$
 */

// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab:

/*
 * This file is part of robotkernel.
 *
 * robotkernel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * robotkernel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with robotkernel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string_util/string_util.h>

#include "pdrouting.h"
#include "robotkernel/exceptions.h"
#include "robotkernel/helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>

MODULE_DEF(pdrouting, module_pdrouting::pdrouting)

#define min(a, b) ((a) < (b) ? (a) : (b))
using namespace robotkernel;
using namespace std;
using namespace module_pdrouting;
using namespace string_util;

pdrouting::pd_demux::pd_demux(std::shared_ptr<pdrouting> parent, const YAML::Node& node) :
    pd_provider(format_string("%s.%s", parent->name.c_str(), get_as<string>(node, "name").c_str())),
    pd_consumer(format_string("%s.%s", parent->name.c_str(), get_as<string>(node, "name").c_str())),
    service_provider::process_data_inspection::base(parent->name, get_as<string>(node, "name")),
    parent(parent)
{
    /* we will get sth like:
      
        name: first_demux
        pd_input_device: <name>
        outputs:
        - { name: left, len: 8 }
        - { name: right, len: 8 }
    */

    name = get_as<string>(node, "name");
    pdin.trigger_name = get_as<string>(node, "trigger_name", "");
    pdin.name = get_as<string>(node, "pd_input_device");

    parent->log(verbose, "%s got pd_input_device %s\n", name.c_str(), pdin.name.c_str());

    for (const auto& output_node : node["outputs"]) {
        std::string desc = "";
        if (output_node["desc"]) {
            YAML::Emitter emitter;
            emitter << output_node["desc"];
            desc = emitter.c_str();
        }

        outputs.push_back(output(get_as<string>(output_node, "name"), 
                    get_as<uint32_t>(output_node, "len"), desc));
    }
}
                        
size_t get_dt_size(const std::string& dt) {
    if ((dt == "uint8_t") || (dt == "int8_t") || (dt == "char")) {
        return (size_t)1u;
    } 

    if ((dt == "uint16_t") || (dt == "int16_t") || (dt == "short")) {
        return (size_t)2u;
    }

    if ((dt == "uint32_t") || (dt == "int32_t") || (dt == "long") || (dt == "float")) {
        return (size_t)4u;
    }

    if ((dt == "uint64_t") || (dt == "int64_t") || (dt == "double")) {
        return (size_t)8u;
    }

    return (size_t)0u;
}

//! creating process data output and trigger
void pdrouting::pd_demux::start() {
    kernel& k = *kernel::get_instance();

    parent->log(info, "%s try to get process data: %s\n", name.c_str(), pdin.name.c_str());

    pdin.dev  = k.get_process_data(pdin.name);
    pdin.hash = pdin.dev->set_consumer(shared_from_this());

    size_t act_len = 0;
    for (auto& output : outputs)
        act_len += output.len;

    if (act_len > pdin.dev->length)
        throw str_exception("demuxer %s length mismatch: pd %s has %u bytes, "
                "we need %u bytes\n", name.c_str(), pdin.name.c_str(), pdin.dev->length, act_len);

    size_t skip_len = 0;
    bool gen_abort = false;

    for (auto& output : outputs) {
        size_t cur_skip = 0;
        act_len = 0;


        YAML::Node pddef_node = YAML::Load(pdin.dev->process_data_definition);
        YAML::Emitter desc_emitter;
        desc_emitter << YAML::BeginSeq;

        for (const auto& entry : pddef_node) {
            for (const auto& kv : entry) {
                string key   = kv.first.as<string>();
                string value = kv.second.as<string>();

                size_t dt_size = get_dt_size(key);

                if (skip_len > cur_skip) {
                    cur_skip += dt_size;
                    continue;
                }

                desc_emitter << YAML::BeginMap << YAML::Key << key << YAML::Value << value << YAML::EndMap;
                act_len += dt_size;

                if (act_len == output.len) {
                    // split at boundary, everything ok
                    skip_len += act_len;
                    break;
                } else if (act_len > output.len) {
                    // did not split at desc boundary, abort generation
                    parent->log(warning, "did not split \"%s\" at pd desc boundaries, abort!\n", pdin.dev->id().c_str());
                    gen_abort = true;
                    break;
                }
            }
        }

        if (gen_abort) { break; }

        desc_emitter << YAML::EndSeq;
        
        output.gen_desc = desc_emitter.c_str();
    }

    for (auto& output : outputs) {
        if ((output.desc == "") && !gen_abort) {
            output.desc = output.gen_desc;
        }

        string pd_desc = output.desc == "" ? format_string("- uint8_t[%d]: data\n", output.len) : output.desc;
        string tmp = format_string("%s.%s.%s", parent->name.c_str(), name.c_str(), output.name.c_str());
        output.pdtr  = make_shared<trigger>(tmp, "inputs");
        output.pdout = make_shared<triple_buffer>(output.len, tmp, string("inputs"), pd_desc, output.pdtr->id());
        output.hash  = output.pdout->set_provider(shared_from_this());

        k.add_device(output.pdtr);
        k.add_device(output.pdout);
    }

    if (pdin.trigger_name == "") {
        pdin.trigger_name = pdin.dev->clk_device;
    }

    parent->log(info, "%s try to get trigger: %s\n", name.c_str(), pdin.trigger_name.c_str());
    auto trigger_dev = k.get_trigger(pdin.trigger_name);
    trigger_dev->add_trigger(shared_from_this());
}

//! destroying process data output and trigger
void pdrouting::pd_demux::stop() {
    kernel& k = *kernel::get_instance();
    
    auto trigger_dev = k.get_trigger(pdin.dev->clk_device);
    trigger_dev->remove_trigger(shared_from_this());
    
    for (auto& output : outputs) {
        k.remove_device(output.pdout);
        k.remove_device(output.pdtr);

        try {
            output.pdout->reset_provider(output.hash);
        } catch (exception& e) {
            parent->log(warning, "reseting provider failed, ignoring: %s\n", e.what()); 
        }

        output.pdout = nullptr;
        output.pdtr  = nullptr;
        output.hash  = 0;
    }
    
    try {
        pdin.dev->reset_consumer(pdin.hash);
    } catch (exception& e) {
        parent->log(warning, "reseting consumer failed, ignoring: %s\n", e.what()); 
    }

    pdin.hash = 0;
    pdin.dev  = nullptr;
}
                
//! trigger tick
void pdrouting::pd_demux::tick() {
    off_t pos = 0;
    auto buf = pdin.dev->pop(pdin.hash);

    for (auto& output : outputs) {
        output.pdout->write(output.hash, 0, &buf[pos], output.len);
        output.pdtr->trigger_modules();
        pos += output.len;
    }
}

pdrouting::pd_mux::pd_mux(std::shared_ptr<pdrouting> parent, const YAML::Node& node) :
    pd_provider(format_string("%s.%s", parent->name.c_str(), get_as<string>(node, "name").c_str())),
    pd_consumer(format_string("%s.%s", parent->name.c_str(), get_as<string>(node, "name").c_str())),
    service_provider::process_data_inspection::base(parent->name, get_as<string>(node, "name")),
    parent(parent)
{
    /* we will get sth like:
      
        name: first_mux
        pd_output_device: <name>
        trigger_name: <modname>.left.outputs.trigger
        inputs:
        - { name: left, len: 8 }
        - { name: right, len: 8 }
    */

    pdout.name = get_as<string>(node, "pd_output_device");
    name = get_as<string>(node, "name");
    trigger_name = get_as<string>(node, "trigger_name", "");
    expected_rate = get_as<int>(node, "expected_rate", 1);

    parent->log(verbose, "%s got pd_output_device %s trigger_name %s\n", name.c_str(), 
            pdout.name.c_str(), trigger_name.c_str());

    for (const auto& input_node : node["inputs"]) {
        std::string desc = "";
        if (input_node["desc"]) {
            YAML::Emitter emitter;
            emitter << input_node["desc"];
            desc = emitter.c_str();
        }

        inputs.push_back(input(get_as<string>(input_node, "name"), 
                    get_as<uint32_t>(input_node, "len"), desc));
    }

    if (trigger_name == "") {
        // using trigger_collector
        collector_trigger = make_shared<trigger>(parent->name, name);
        collector = make_shared<trigger_collector>(inputs.size(), 1.0/expected_rate, 
                std::bind(&trigger::trigger_modules, collector_trigger));

        collector_trigger_cbs.resize(inputs.size());
        for (unsigned i = 0; i < collector_trigger_cbs.size(); ++i) {
            collector_trigger_cbs[i] = make_shared<trigger_cb>(std::bind(&trigger_collector::trigger_collect, collector, i));
        }
    }
}
                        
//! creating process data input and trigger
void pdrouting::pd_mux::start() {
    kernel& k = *kernel::get_instance();

    pdout.dev  = k.get_process_data(pdout.name);
    pdout.hash = pdout.dev->set_provider(shared_from_this());

    size_t act_len = 0;
    for (auto& input : inputs)
        act_len += input.len;

    if (act_len > pdout.dev->length)
        throw str_exception("muxer %s length mismatch: pd %s has %u bytes, "
                "we need %u bytes\n", name.c_str(), pdout.name.c_str(), pdout.dev->length, act_len);
    
    size_t skip_len = 0;
    bool gen_abort = false;

    for (auto& input : inputs) {
        size_t cur_skip = 0;
        act_len = 0;


        YAML::Node pddef_node = YAML::Load(pdout.dev->process_data_definition);
        YAML::Emitter desc_emitter;
        desc_emitter << YAML::BeginSeq;

        for (const auto& entry : pddef_node) {
            for (const auto& kv : entry) {
                string key   = kv.first.as<string>();
                string value = kv.second.as<string>();

                size_t dt_size = get_dt_size(key);

                if (skip_len > cur_skip) {
                    cur_skip += dt_size;
                    continue;
                }

                desc_emitter << YAML::BeginMap << YAML::Key << key << YAML::Value << value << YAML::EndMap;
                act_len += dt_size;
                
                if (act_len == input.len) {
                    // split at boundary, everything ok
                    skip_len += act_len;
                    break;
                } else if (act_len > input.len) {
                    // did not split at desc boundary, abort generation
                    parent->log(warning, "did not split \"%s\" at pd desc boundaries, abort!\n", pdout.dev->id().c_str());
                    gen_abort = true;
                    break;
                }
            }
        }

        if (gen_abort) { break; }

        desc_emitter << YAML::EndSeq;
        
        input.gen_desc = desc_emitter.c_str();
    }

    int trigger_cbs_idx = 0;
    for (auto& input : inputs) {
        if ((input.desc == "") && !gen_abort) {
            input.desc = input.gen_desc;
        }

        string pd_desc = input.desc == "" ? format_string("- uint8_t[%d]: data\n", input.len) : input.desc;
        string tmp = format_string("%s.%s.%s", parent->name.c_str(), name.c_str(), input.name.c_str());
        input.pdtr  = make_shared<trigger>(tmp, "outputs");
        input.pdin  = make_shared<triple_buffer>(input.len, tmp, string("outputs"), pd_desc, input.pdtr->id());
        input.hash  = input.pdin->set_consumer(shared_from_this());

        k.add_device(input.pdtr);
        k.add_device(input.pdin);

        if (trigger_name == "") {
            input.pdtr->add_trigger(collector_trigger_cbs[trigger_cbs_idx++]);
        }
    }
    
    if (trigger_name != "") {
        auto trigger_dev = k.get_trigger(trigger_name);
        trigger_dev->add_trigger(shared_from_this());
    } else {
        collector_trigger->add_trigger(shared_from_this());
    }
}

//! destroying process data input and trigger
void pdrouting::pd_mux::stop() {
    kernel& k = *kernel::get_instance();
    
    if (trigger_name != "") {
        auto trigger_dev = k.get_trigger(trigger_name);
        trigger_dev->remove_trigger(shared_from_this());
    } else {
        collector_trigger->remove_trigger(shared_from_this());
    }
    
    for (auto& input : inputs) {
        k.remove_device(input.pdin);
        k.remove_device(input.pdtr);

        try {
            input.pdin->reset_consumer(input.hash);
        } catch (exception& e) {
            parent->log(warning, "reseting consumer failed, ignoring: %s\n", e.what()); 
        }

        input.pdin  = nullptr;
        input.pdtr  = nullptr;
        input.hash  = 0;
    }
    
    try {
        pdout.dev->reset_provider(pdout.hash);
    } catch (exception& e) {
        parent->log(warning, "reseting provider failed, ignoring: %s\n", e.what()); 
    }

    pdout.hash = 0;
    pdout.dev  = nullptr;
}
                
//! trigger tick
void pdrouting::pd_mux::tick() {
    off_t pos = 0;

    for (auto& input : inputs) {
        auto buf = input.pdin->pop(input.hash);
        pdout.dev->write(pdout.hash, pos, buf, input.len, false);
        pos += input.len;
    }

    pdout.dev->push(pdout.hash);
}

//! construction
/*!
 * \param node yaml intialization node
 */
pdrouting::pdrouting(const std::string& name, const YAML::Node& node) : 
    module_base("module_pdrouting", name, node)
{
    config = YAML::Clone(node);
}

//! destruction 
pdrouting::~pdrouting() {
    set_state(module_state_init);
}

void pdrouting::init() {
    if (config["demux"]) {
        for (const auto& demux_node : config["demux"]) {
            auto d = std::make_shared<pd_demux>(shared_from_this(), demux_node);
            demux.push_back(d);
        }
    }
    
    if (config["mux"]) {
        for (const auto& mux_node : config["mux"]) {
            auto d = std::make_shared<pd_mux>(shared_from_this(), mux_node);
            mux.push_back(d);
        }
    }
}
        
//! set module state machine to defined state
/*!
 * \param state requested state
 * \return success or failure
 */
int pdrouting::set_state(module_state_t state) {
    // get transition
    uint32_t transition = GEN_STATE(this->state, state);

    switch (transition) {
        case op_2_safeop:
        case op_2_preop:
        case op_2_init:
        case op_2_boot:
            // ====> stop sending commands
            for (auto& d : demux) {
                d->stop();
            }

            for (auto& d : mux) {
                d->stop();
            }
            if (state == module_state_safeop)
                break;
        case safeop_2_preop:
        case safeop_2_init:
        case safeop_2_boot:
            // ====> stop receiving measurements
            if (state == module_state_preop)
                break;
        case preop_2_init:
        case preop_2_boot:
            // ====> deinit devices
        case init_2_init:
            // ====> re-/open ethercat device
            if (state == module_state_init)
                break;
        case init_2_boot:
            break;
        case boot_2_init:
        case boot_2_preop:
        case boot_2_safeop:
        case boot_2_op:
            // ====> re-/open ethercat device
            if (state == module_state_init)
                break;
        case init_2_op:
        case init_2_safeop:
        case init_2_preop:
            // ====> initial devices            
            if (state == module_state_preop)
                break;
        case preop_2_op:
        case preop_2_safeop: {
            // ====> start receiving measurements
            if (state == module_state_safeop)
                break;
        }
        case safeop_2_op: {
            // ====> start sending commands
            for (auto& d : demux) {
                d->start();
            }

            for (auto& d : mux) {
                d->start();
            }
            break;
        }
        case op_2_op:
        case safeop_2_safeop:
        case preop_2_preop:
            // ====> do nothing
            break;

        default:
            break;
    }

    return (this->state = state);
}


