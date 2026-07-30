//! robotkernel module pdrouting
/*!
 * author: Robert Burger
 *
 * $Id$
 */

// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab:

/*
 * This file is part of module_pdrouting.
 *
 * module_pdrouting is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * module_pdrouting is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with module_pdrouting; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "pdrouting.h"
#include "robotkernel/exceptions.h"
#include "robotkernel/helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>


using namespace robotkernel;
using namespace robotkernel::helpers;
using namespace std;
using namespace module_pdrouting;
                
pdrouting::one_to_many::one_to_many(std::shared_ptr<pdrouting> parent, const YAML::Node& node) :
    parent(parent) 
{
    name = get_as<string>(node, "name");
    pdin.name = get_as<std::string>(node, "pd_input_device");

    YAML::Node pd_output_devices_node = get_as<YAML::Node>(node, "pd_output_devices");

    for (const auto& pd_node : pd_output_devices_node) {
        std::string pdout_name = pd_node.as<std::string>();
        pdout.push_back({ pdout_name, 0, nullptr });
    }

}

pdrouting::one_to_many::~one_to_many() {
}

//! creating process data output and trigger
void pdrouting::one_to_many::start() {
    parent->log(info, "event=one_to_many_get_pd name=%s pd_device=%s\n", name.c_str(), pdin.name.c_str());

    pdin.dev  = robotkernel::get_device<process_data>(pdin.name);
    pdin.consumer = make_shared<pd_consumer>(string_printf("%s.%s", parent->name.c_str(), name.c_str()));
    pdin.dev->set_consumer(pdin.consumer);

    size_t in_length = pdin.dev->length;

    for (auto& tmp_pdout : pdout) {
        tmp_pdout.dev = robotkernel::get_device<process_data>(tmp_pdout.name);

        if (tmp_pdout.dev->length != in_length) {
            throw std::runtime_error(string_printf("event=one_to_many_get_pd name=%s pd_device=%s "
                        "message=\"wrong length, need %zu bytes, got %zu bytes.\"", 
                    name.c_str(), tmp_pdout.name.c_str(), tmp_pdout.dev->length, in_length));
        }

        tmp_pdout.provider = make_shared<pd_provider>(string_printf("%s.%s", parent->name.c_str(), name.c_str()));
        tmp_pdout.dev->set_provider(tmp_pdout.provider);
    }

    pdin.dev->trigger_dev->add_trigger(shared_from_this_as<trigger_base>());
}
                
//! trigger tick
void pdrouting::one_to_many::tick() {
    auto buf = pdin.dev->pop(pdin.consumer);

    for (auto& tmp_pdout : pdout) {
        tmp_pdout.dev->write(tmp_pdout.provider, 0, buf, pdin.dev->length);
        tmp_pdout.dev->trigger();
    }
}

//! destroying process data output and trigger
void pdrouting::one_to_many::stop() {
    pdin.dev->trigger_dev->remove_trigger(shared_from_this_as<trigger_base>());

    for (auto& tmp_pdout : pdout) {
        tmp_pdout.dev->reset_provider(tmp_pdout.provider);
        tmp_pdout.provider = nullptr;
        tmp_pdout.dev = nullptr;
    }
    
    pdin.dev->reset_consumer(pdin.consumer);
    pdin.consumer = nullptr;
    pdin.dev = nullptr;
}


pdrouting::pd_demux::pd_demux(std::shared_ptr<pdrouting> parent, const YAML::Node& node) :
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

    config = YAML::Clone(node);

    pdin.name = get_as<string>(node, "pd_input_device");
    zero_copy = get_as<bool>(node, "zero_copy", false);

    parent->log(verbose, "event=demux_get_pd_input name=%s pd_device=%s\n", name.c_str(), pdin.name.c_str());

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
    parent->log(info, "event=demux_start name=%s pd_device=%s\n", name.c_str(), pdin.name.c_str());

    pdin.dev  = robotkernel::get_device<process_data>(pdin.name);
    pdin.consumer = make_shared<pd_consumer>(string_printf("%s.%s", parent->name.c_str(), name.c_str()));
    pdin.dev->set_consumer(pdin.consumer);

    size_t act_len = 0;
    for (auto& output : outputs)
        act_len += output.len;

    if (act_len > pdin.dev->length)
        throw std::runtime_error(string_printf("event=demux_start name=%s pd_device=%s message=\"length mismatch: got %zu bytes, "
                "we need %zu bytes.\"\n", name.c_str(), pdin.name.c_str(), pdin.dev->length, act_len));

    size_t skip_len = 0;
    bool gen_abort = false;

    for (auto& output : outputs) {
        size_t cur_skip = 0;
        act_len = 0;

        auto pd_def = robotkernel::get_pd_definition(pdin.dev->process_data_definition);
        parent->log(verbose, "event=demux_start name=%s pd_device=%s pd_definition=\"%s\"\n", 
                name.c_str(), pdin.name.c_str(), pd_def.c_str());
        YAML::Node pddef_node = YAML::Load(pd_def);
        YAML::Emitter desc_emitter;
        desc_emitter << YAML::BeginMap;

        bool do_break = false; 

        for (const auto& entry : pddef_node) {
            string value = entry.first.as<string>();
            size_t dt_size = get_dt_size(get_as<string>(entry.second, "type"));
            size_t arr_size = get_as<bool>(entry.second, "array", false) ? 
                get_as<int>(entry.second, "size", 1) : 1;
            dt_size *= arr_size;

            if (skip_len > cur_skip) {
                cur_skip += dt_size;
                parent->log(verbose, "event=demux_start name=%s pd_device=%s skip=%s\n",
                        name.c_str(), pdin.name.c_str(), value.c_str());
                continue;
            }

            desc_emitter << YAML::Key << entry.first << YAML::Value << entry.second;
            act_len += dt_size;

            if (act_len == output.len) {
                // split at boundary, everything ok
                skip_len += act_len;
                do_break = true;
            } else if (act_len > output.len) {
                // did not split at desc boundary, abort generation
                parent->log(warning, "event=demux_start name=%s pd_device=%s message=\"did not split at "
                        "pd desc boundaries, abort!\"\n", name.c_str(), pdin.dev->id().c_str());
                gen_abort = true;
                do_break = true;
            }

            if (do_break) {
                break;
            }
        }

        if (gen_abort) { break; }

        desc_emitter << YAML::EndMap;
        
        output.gen_desc = desc_emitter.c_str();
        parent->log(verbose, "event=demux_start name=%s pd_device=%s output_definition=\"%s\"\n", 
                name.c_str(), pdin.name.c_str(), output.gen_desc.c_str());
    }

    for (auto& output : outputs) {
        if ((output.desc == "") && !gen_abort) {
            output.desc = output.gen_desc;
        }

        string tmp = string_printf("%s.%s.%s", parent->name.c_str(), name.c_str(), output.name.c_str());
        string pd_desc = output.desc == "" ? string_printf("data: { type: uint8_t, array: true, size: %d }", output.len) : output.desc;
        string pd_desc_name = tmp + ".output.definition";
        if (zero_copy) {
            output.pdout = make_shared<pointer_buffer>(output.len, nullptr, tmp, string("inputs"), pd_desc_name);
        } else {
            output.pdout = make_shared<triple_buffer>(output.len, tmp, string("inputs"), pd_desc_name);
            //parent->log(info, "created triple_buffer with len %d, definition\n%s\n", output.len, output.pdout->process_data_definition.c_str());
        }
        output.provider = make_shared<pd_provider>(string_printf("%s.%s", parent->name.c_str(), name.c_str()));
        output.pdout->set_provider(output.provider);
        //parent->log(info, "add %s with\n%s\n", pd_desc_name.c_str(), pd_desc.c_str());
        robotkernel::add_pd_definition(pd_desc_name, pd_desc);
        robotkernel::add_device(output.pdout);
        output.pdout_inspection = make_shared<service_provider_process_data_inspection::pd_inspection>(tmp, "inputs", output.pdout); 
        robotkernel::add_device(output.pdout_inspection);
    }

    if (config["trigger"]) {
        trg = make_shared<triggerable>(config["trigger"], std::bind(&pdrouting::pd_demux::tick, this));
    } else if (config["trigger_name"]) {
        YAML::Node tmp_node;
        tmp_node["dev_name"] = config["trigger_name"];
        trg = make_shared<triggerable>(tmp_node, std::bind(&pdrouting::pd_demux::tick, this));

        parent->log(warning, "used 'trigger_name' from config file, please update to modern trigger syntax:\n"
                "#########################################################\n"
                "# Trigger device\n"
                "trigger:\n"
                "  # Trigger device name, must be registered to robotkernel\n"
                "  # before switching to SAFEOP\n"
                "  dev_name: timer.main.trigger\n"
                "\n"
                "  # Optional priority with which we are triggerd\n"
                "  #prio: 50\n"
                "\n"
                "  # Optional cpu affinity on which cpu when run on.\n"
                "  #affinity: [ 2, 3 ]\n"
                "\n"
                "  # Optional trigger mode, direct mode means in callers\n"
                "  # thread context, no direct mode uses worker thread.\n"
                "  #direct_mode: True\n");
    } else {
        YAML::Node tmp_node;
        tmp_node["dev_name"] = pdin.dev->trigger_dev->id();
        trg = make_shared<triggerable>(tmp_node, std::bind(&pdrouting::pd_demux::tick, this));
    }

    trg->acquire();
}

//! destroying process data output and trigger
void pdrouting::pd_demux::stop() {
    parent->log(info, "event=demux_stop name=%s\n", name.c_str());
    
    trg->release();
    trg = nullptr;

    for (auto& output : outputs) {
        robotkernel::remove_device(output.pdout_inspection);
        output.pdout_inspection = nullptr;
    
        robotkernel::remove_device(output.pdout);

        try {
            output.pdout->reset_provider(output.provider);
        } catch (exception& e) {
            parent->log(warning, "event=demux_stop name=%s message=\"reseting provider failed, ignoring: %s\"\n", name.c_str(), e.what()); 
        }

        output.pdout = nullptr;
        output.provider = nullptr;
    }
    
    try {
        pdin.dev->reset_consumer(pdin.consumer);
    } catch (exception& e) {
        parent->log(warning, "event=demux_stop name=%s message=\"reseting consumer failed, ignoring: %s\"\n", name.c_str(), e.what()); 
    }

    pdin.consumer = nullptr;
    pdin.dev  = nullptr;
}
                
//! trigger tick
void pdrouting::pd_demux::tick() {
    off_t pos = 0;
    auto buf = pdin.dev->pop(pdin.consumer);

    for (auto& output : outputs) {
        if (zero_copy) {
            dynamic_pointer_cast<pointer_buffer>(output.pdout)->set_ptr(output.provider, &buf[pos]);
        } else {
            output.pdout->write(output.provider, 0, &buf[pos], output.len);
        }

        pos += output.len;
    }
}

pdrouting::pd_mux::pd_mux(std::shared_ptr<pdrouting> parent, const YAML::Node& node) :
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
    config = YAML::Clone(node);

    pdout.name = get_as<string>(node, "pd_output_device");
    name = get_as<string>(node, "name");
    expected_rate = get_as<int>(node, "expected_rate", 1);
    zero_copy = get_as<bool>(node, "zero_copy", false);

    parent->log(verbose, "event=mux_get_pd_output name=%s pd_device=%s\n", name.c_str(), pdout.name.c_str());

    for (const auto& input_node : node["inputs"]) {
        std::string desc = "";
        if (input_node["desc"]) {
            YAML::Emitter emitter;
            emitter << input_node["desc"];
            desc = emitter.c_str();
        }

        inputs.push_back(input(get_as<string>(input_node, "name"), 
                    get_as<uint32_t>(input_node, "len", 0), desc));
    }
}
                        
//! creating process data input and trigger
void pdrouting::pd_mux::start() {
    pdout.dev  = robotkernel::get_device<process_data>(pdout.name);
    pdout.provider = make_shared<pd_provider>(string_printf("%s.%s", parent->name.c_str(), name.c_str()));
    pdout.dev->set_provider(pdout.provider);

    size_t act_len = 0;
    for (auto& input : inputs)
        act_len += input.len;

    if (act_len > pdout.dev->length)
        throw std::runtime_error(string_printf("event=mux_start name=%s pd_device=%s message=\"length mismatch: got %zu bytes, "
                "we need %zu bytes.\"\n", name.c_str(), pdout.name.c_str(), pdout.dev->length, act_len));
    
    size_t skip_len = 0;
    bool gen_abort = false;

    for (unsigned idx = 0; idx < inputs.size(); ++idx) {
        pd_mux::input& input = inputs[idx];
        size_t cur_skip = 0;
        act_len = 0;

        auto pd_def = robotkernel::get_pd_definition(pdout.dev->process_data_definition);
        YAML::Node pddef_node = YAML::Load(pd_def);
        YAML::Emitter desc_emitter;
        desc_emitter << YAML::BeginMap;

        parent->log(verbose, "event=mux_start name=%s pd_device %s input_len=%d\n", name.c_str(), pdout.name.c_str(), input.len);

        bool do_break = false;

        for (const auto& entry : pddef_node) {
            string value = entry.first.as<string>();
            size_t dt_size = get_dt_size(get_as<string>(entry.second, "type"));
            size_t arr_size = get_as<bool>(entry.second, "array", false) ? 
                get_as<int>(entry.second, "size", 1) : 1;
            dt_size *= arr_size;

            if (skip_len > cur_skip) {
                cur_skip += dt_size;
                continue;
            }

            desc_emitter << YAML::Key << entry.first << YAML::Value << entry.second;
            act_len += dt_size;

            if (act_len == input.len) {
                // split at boundary, everything ok
                skip_len += act_len;
                do_break = true;
            } else if (act_len > input.len) {
                // did not split at desc boundary, abort generation
                parent->log(warning, "event=mux_start name=%s pd_device=%s message=\"did not split at pd desc boundaries, abort!\"\n", 
                        name.c_str(), pdout.dev->id().c_str());

                gen_abort = true;
                do_break = true;
            }

            if (do_break) {
                break;
            }
        }

        if (gen_abort) { break; }

        desc_emitter << YAML::EndMap;
        
        input.gen_desc = desc_emitter.c_str();
    }

    if (config["trigger"]) {
        trg = make_shared<triggerable>(config["trigger"], std::bind(&pdrouting::pd_mux::tick, this));
    } else if (config["trigger_name"]) {
        YAML::Node tmp_node;
        tmp_node["dev_name"] = config["trigger_name"];
        trg = make_shared<triggerable>(tmp_node, std::bind(&pdrouting::pd_mux::tick, this));

        parent->log(warning, "used 'trigger_name' from config file, please update to modern trigger syntax:\n"
                "#########################################################\n"
                "# Trigger device\n"
                "trigger:\n"
                "  # Trigger device name, must be registered to robotkernel\n"
                "  # before switching to SAFEOP\n"
                "  dev_name: timer.main.trigger\n"
                "\n"
                "  # Optional priority with which we are triggerd\n"
                "  #prio: 50\n"
                "\n"
                "  # Optional cpu affinity on which cpu when run on.\n"
                "  #affinity: [ 2, 3 ]\n"
                "\n"
                "  # Optional trigger mode, direct mode means in callers\n"
                "  # thread context, no direct mode uses worker thread.\n"
                "  #direct_mode: True\n");
    } else {
        // using trigger_collector
        collector_trigger = make_shared<trigger>(parent->name, name);
        collector = make_shared<trigger_collector>(inputs.size(), 1.0/expected_rate, 
                std::bind(&trigger::do_trigger, collector_trigger));
    }

    off_t pos = 0;
    auto buf = pdout.dev->next(pdout.provider);
    for (unsigned idx = 0; idx < inputs.size(); ++idx) {
        pd_mux::input& input = inputs[idx];
        if ((input.desc == "") && !gen_abort) {
            input.desc = input.gen_desc;
        }

        string tmp = string_printf("%s.%s.%s", parent->name.c_str(), name.c_str(), input.name.c_str());
        string pd_desc = input.desc == "" ? string_printf("data: { type: uint8_t, array: true, size: %d }", input.len) : input.desc;
        string pd_desc_name = tmp + "input.definition";
        if (zero_copy) {
            input.pdin  = make_shared<pointer_buffer>(input.len, &buf[pos], tmp, string("outputs"), pd_desc_name);
            pos += input.len;
        } else {
            input.pdin  = make_shared<triple_buffer>(input.len, tmp, string("outputs"), pd_desc_name);
        }
        input.consumer = make_shared<pd_consumer>(string_printf("%s.%s", parent->name.c_str(), name.c_str()));
        input.pdin->set_consumer(input.consumer);
        robotkernel::add_pd_definition(pd_desc_name, pd_desc);
        robotkernel::add_device(input.pdin);

        input.pdin_inspection = make_shared<service_provider_process_data_inspection::pd_inspection>(tmp, "outputs", input.pdin); 
        robotkernel::add_device(input.pdin_inspection);

        if (!trg) {
            input.collector_trigger_cb = make_shared<trigger_cb>(std::bind(&trigger_collector::trigger_collect, collector, idx));
            input.pdin->trigger_dev->add_trigger(input.collector_trigger_cb);
        }
    }

    if (trg) {
        trg->acquire();
    } else {
        collector_trigger->add_trigger(shared_from_this_as<trigger_base>());
    }
}

//! destroying process data input and trigger
void pdrouting::pd_mux::stop() {
    if (trg) {
        trg->release();
        trg = nullptr;
    } else {
        collector_trigger->remove_trigger(shared_from_this_as<trigger_base>());
    }

    for (auto& input : inputs) {
        robotkernel::remove_device(input.pdin_inspection);
        input.pdin_inspection = nullptr;
    
        robotkernel::remove_device(input.pdin);

        try {
            input.pdin->reset_consumer(input.consumer);
        } catch (exception& e) {
            parent->log(warning, "event=mux_stop name=%s pd_device=%s message=\"reseting consumer failed, ignoring: %s\"\n", 
                    name.c_str(), input.name.c_str(), e.what()); 
        }

        input.pdin  = nullptr;
        input.consumer  = nullptr;
    }
    
    try {
        pdout.dev->reset_provider(pdout.provider);
    } catch (exception& e) {
        parent->log(warning, "event=mux_stop name=%s pd_device=%s message=\"reseting provider failed, ignoring: %s\"\n",
                    name.c_str(), pdout.name.c_str(), e.what()); 
    }

    pdout.provider = nullptr;
    pdout.dev  = nullptr;
}
                
//! trigger tick
void pdrouting::pd_mux::tick() {
    off_t pos = 0;

    if (zero_copy) {
        pdout.dev->push(pdout.provider);
        auto buf = pdout.dev->next(pdout.provider);
    
        for (auto& input : inputs) {
            dynamic_pointer_cast<pointer_buffer>(input.pdin)->set_ptr(input.consumer, &buf[pos]);
            pos += input.len;
        }
    } else {
        for (auto& input : inputs) {
            auto buf = input.pdin->pop(input.consumer);
            pdout.dev->write(pdout.provider, pos, buf, input.len, false);

            pos += input.len;
        }
    
        pdout.dev->push(pdout.provider);
    }
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
        if (config["demux"].Type() == YAML::NodeType::Sequence) {
            for (const auto& demux_node : config["demux"]) {
                auto d = std::make_shared<pd_demux>(shared_from_this_as<pdrouting>(), demux_node);
                demux.push_back(d);
            }
        } else if (config["demux"].Type() == YAML::NodeType::Map) {
            std::list<YAML::Node> instances;
            parse_templates(config["demux"], instances);
            for (const auto& demux_node : instances) {
                demux.push_back(std::make_shared<pd_demux>(shared_from_this_as<pdrouting>(), demux_node));
            }
        }
    }
    
    if (config["mux"]) {
        if (config["mux"].Type() == YAML::NodeType::Sequence) {
            for (const auto& mux_node : config["mux"]) {
                auto d = std::make_shared<pd_mux>(shared_from_this_as<pdrouting>(), mux_node);
                mux.push_back(d);
            }
        } else if (config["mux"].Type() == YAML::NodeType::Map) {
            std::list<YAML::Node> instances;
            parse_templates(config["mux"], instances);
            for (const auto& mux_node : instances) {
                printf("processing ...\n");
                mux.push_back(std::make_shared<pd_mux>(shared_from_this_as<pdrouting>(), mux_node));
            }
        }
    }

    if (config["one_to_many"]) {
        for (const auto& o2m_node : config["one_to_many"]) {
            auto d = std::make_shared<one_to_many>(shared_from_this_as<pdrouting>(), o2m_node);
            o2m.push_back(d);
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
            for (auto& d : o2m) {
                d->stop();
            }

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

            for (auto& d : o2m) {
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


MODULE_DEF(pdrouting, module_pdrouting::pdrouting)
